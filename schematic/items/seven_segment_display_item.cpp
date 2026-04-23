#include "seven_segment_display_item.h"

#include <QJsonObject>
#include <QPainter>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace {
constexpr int kPinPitch = 20;

QColor segmentColorForDrive(double drive) {
    const double t = std::clamp(drive, 0.0, 1.0);
    const int r = static_cast<int>(70 + 185 * t);
    const int g = static_cast<int>(20 + 60 * t);
    const int b = static_cast<int>(20 + 40 * t);
    return QColor(r, g, b);
}

QPolygonF segPolyH(double x, double y, double w, double h, double cut) {
    return QPolygonF{
        QPointF(x + cut, y),
        QPointF(x + w - cut, y),
        QPointF(x + w, y + h * 0.5),
        QPointF(x + w - cut, y + h),
        QPointF(x + cut, y + h),
        QPointF(x, y + h * 0.5)
    };
}

QPolygonF segPolyV(double x, double y, double w, double h, double cut) {
    return QPolygonF{
        QPointF(x, y + cut),
        QPointF(x + w * 0.5, y),
        QPointF(x + w, y + cut),
        QPointF(x + w, y + h - cut),
        QPointF(x + w * 0.5, y + h),
        QPointF(x, y + h - cut)
    };
}

QPolygonF segPolyLine(const QPointF& a, const QPointF& b, qreal width) {
    QLineF axis(a, b);
    const qreal len = axis.length();
    if (len < 1e-3) return QPolygonF();

    const QPointF dir((b.x() - a.x()) / len, (b.y() - a.y()) / len);
    const QPointF n(-dir.y(), dir.x());
    const qreal half = width * 0.5;
    const qreal cut = std::min(len * 0.28, width * 0.95);

    const QPointF p1(a.x() + dir.x() * cut, a.y() + dir.y() * cut);
    const QPointF p2(b.x() - dir.x() * cut, b.y() - dir.y() * cut);

    return QPolygonF{
        QPointF(p1.x() + n.x() * half, p1.y() + n.y() * half),
        QPointF(p2.x() + n.x() * half, p2.y() + n.y() * half),
        QPointF(b.x(), b.y()),
        QPointF(p2.x() - n.x() * half, p2.y() - n.y() * half),
        QPointF(p1.x() - n.x() * half, p1.y() - n.y() * half),
        QPointF(a.x(), a.y())
    };
}

QRectF fitRectPreserveAspect(const QSizeF& src, const QRectF& dst) {
    if (src.width() <= 0.0 || src.height() <= 0.0 || dst.isEmpty()) return dst;
    const double scale = std::min(dst.width() / src.width(), dst.height() / src.height());
    const QSizeF out(src.width() * scale, src.height() * scale);
    return QRectF(dst.left() + (dst.width() - out.width()) * 0.5,
                  dst.top() + (dst.height() - out.height()) * 0.5,
                  out.width(), out.height());
}

QPainterPath parseSvgPathDataAbsolute(const QString& data) {
    const QByteArray bytes = data.toLatin1();
    const char* s = bytes.constData();
    const int n = bytes.size();
    int i = 0;

    auto skipSep = [&]() {
        while (i < n && (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == ',')) ++i;
    };
    auto atNumber = [&]() {
        skipSep();
        return i < n && (s[i] == '-' || s[i] == '+' || s[i] == '.' || std::isdigit(static_cast<unsigned char>(s[i])));
    };
    auto readNumber = [&]() -> double {
        skipSep();
        const int start = i;
        if (i < n && (s[i] == '-' || s[i] == '+')) ++i;
        while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        if (i < n && s[i] == '.') {
            ++i;
            while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        }
        if (i < n && (s[i] == 'e' || s[i] == 'E')) {
            ++i;
            if (i < n && (s[i] == '-' || s[i] == '+')) ++i;
            while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        }
        return QByteArray(s + start, i - start).toDouble();
    };

    QPainterPath path;
    QPointF current;
    QPointF subStart;
    char cmd = 0;

    while (i < n) {
        skipSep();
        if (i >= n) break;
        if (std::isalpha(static_cast<unsigned char>(s[i]))) {
            cmd = s[i++];
            if (cmd == 'Z' || cmd == 'z') {
                path.closeSubpath();
                current = subStart;
                continue;
            }
        }

        switch (cmd) {
        case 'M':
        case 'm': {
            if (!atNumber()) break;
            const double x = readNumber();
            const double y = readNumber();
            current = QPointF(x, y);
            subStart = current;
            path.moveTo(current);
            cmd = (cmd == 'm') ? 'l' : 'L';
            break;
        }
        case 'L':
        case 'l':
            while (atNumber()) {
                const double x = readNumber();
                const double y = readNumber();
                current = QPointF((cmd == 'l') ? current.x() + x : x,
                                  (cmd == 'l') ? current.y() + y : y);
                path.lineTo(current);
            }
            break;
        case 'C':
        case 'c':
            while (atNumber()) {
                const double x1 = readNumber();
                const double y1 = readNumber();
                const double x2 = readNumber();
                const double y2 = readNumber();
                const double x = readNumber();
                const double y = readNumber();
                const QPointF c1((cmd == 'c') ? current.x() + x1 : x1,
                                 (cmd == 'c') ? current.y() + y1 : y1);
                const QPointF c2((cmd == 'c') ? current.x() + x2 : x2,
                                 (cmd == 'c') ? current.y() + y2 : y2);
                current = QPointF((cmd == 'c') ? current.x() + x : x,
                                  (cmd == 'c') ? current.y() + y : y);
                path.cubicTo(c1, c2, current);
            }
            break;
        default:
            ++i;
            break;
        }
    }

    return path;
}

const std::array<QPainterPath, 17>& exactSeg16Paths() {
    static const std::array<QString, 17> kSeg16PathData = {
        R"SVG(M 224.45 121.98 C225.65,120.78 226.10,116.72 226.56,102.98 C226.94,91.63 226.78,84.97 226.11,84.00 C224.51,81.68 136.64,81.10 132.24,83.37 C127.35,85.91 113.00,99.97 113.00,102.24 C113.00,104.73 130.37,122.71 133.38,123.34 C134.55,123.58 155.17,123.71 179.22,123.64 C216.92,123.52 223.14,123.29 224.45,121.98 Z)SVG",
        R"SVG(M 340.88 113.35 C346.99,107.49 351.99,101.98 351.99,101.10 C351.98,100.22 348.49,95.84 344.24,91.36 L 336.50 83.22 L 325.00 82.52 C318.67,82.14 297.65,81.98 278.28,82.16 C246.21,82.47 242.91,82.66 241.30,84.27 C239.78,85.79 239.48,88.45 239.16,103.31 C238.85,117.25 239.05,120.80 240.19,121.74 C241.96,123.21 248.04,123.44 293.13,123.75 L 329.75 124.00 L 340.88 113.35 Z)SVG",
        R"SVG(M 359.77 336.65 C363.11,333.12 365.00,330.24 365.00,328.71 C365.00,327.38 365.46,317.34 366.01,306.40 C367.90,269.32 369.98,224.67 371.97,178.50 C373.07,153.20 374.14,131.15 374.36,129.50 C374.89,125.52 373.10,122.27 367.62,117.22 C364.21,114.07 362.61,113.22 361.16,113.77 C360.10,114.17 353.39,120.12 346.24,127.00 L 333.25 139.50 L 332.64 149.00 C332.05,158.22 331.16,175.93 328.98,221.50 C328.08,240.47 325.07,311.17 325.02,314.74 C325.00,316.14 336.32,329.42 345.11,338.32 C351.80,345.10 351.78,345.10 359.77,336.65 Z)SVG",
        R"SVG(M 348.61 584.69 L 354.31 579.39 L 354.66 571.44 C354.85,567.07 355.68,549.78 356.51,533.00 C358.65,489.78 361.85,417.92 362.64,395.35 L 363.30 376.20 L 358.01 370.60 C354.42,366.81 351.93,365.00 350.29,365.00 C348.58,365.00 344.81,367.98 337.33,375.25 C322.04,390.12 322.90,388.33 321.89,407.33 C320.57,432.18 318.15,485.61 317.04,514.50 C316.49,528.80 315.78,545.45 315.47,551.50 C314.97,561.07 315.13,562.84 316.70,565.13 C320.70,570.96 339.37,590.00 341.08,590.00 C342.09,590.00 345.47,587.61 348.61,584.69 Z)SVG",
        R"SVG(M 203.81 617.73 C204.43,616.98 204.99,609.13 205.18,598.57 L 205.50 580.67 L 202.53 579.86 C200.90,579.42 181.31,578.75 159.00,578.38 C125.27,577.82 117.98,577.95 115.77,579.11 C111.45,581.37 94.00,597.99 94.00,599.84 C94.00,600.78 97.71,605.24 102.25,609.75 L 110.50 617.96 L 129.50 618.25 C185.30,619.12 202.76,619.00 203.81,617.73 Z)SVG",
        R"SVG(M 314.28 617.51 C318.18,614.80 329.93,602.80 330.53,600.92 C331.02,599.36 329.34,597.05 322.83,590.33 C318.25,585.61 313.05,581.13 311.28,580.37 C308.72,579.28 299.30,579.00 265.03,579.00 C213.42,579.00 218.00,577.60 218.00,593.35 C218.00,599.05 217.75,606.36 217.45,609.60 C217.14,612.93 217.34,616.15 217.91,617.00 C218.77,618.27 224.63,618.58 256.71,619.00 C299.83,619.57 311.76,619.26 314.28,617.51 Z)SVG",
        R"SVG(M 87.00 587.24 C88.38,586.28 94.52,580.55 100.65,574.50 L 111.81 563.50 L 112.36 557.00 C113.01,549.41 114.57,514.84 115.98,476.50 C116.53,461.65 117.51,436.32 118.16,420.21 C119.30,391.89 119.27,390.85 117.42,388.56 C110.75,380.33 96.20,364.89 94.63,364.39 C92.06,363.56 89.21,365.39 84.42,370.95 L 80.34 375.69 L 79.14 399.09 C76.56,449.05 75.04,481.62 74.01,509.00 C73.42,524.67 72.49,546.23 71.95,556.91 C70.86,578.51 70.56,577.08 77.92,584.75 C82.47,589.49 83.41,589.75 87.00,587.24 Z)SVG",
        R"SVG(M 99.62 338.45 C109.41,329.49 119.29,319.33 120.47,317.03 C121.27,315.44 121.95,308.56 122.29,298.50 C122.84,282.07 127.45,173.41 128.45,153.21 C128.89,144.43 128.69,141.32 127.57,139.21 C125.15,134.68 104.66,114.00 102.59,114.00 C101.55,114.00 98.34,116.36 95.45,119.25 C90.25,124.46 90.21,124.56 89.64,132.00 C88.41,148.05 83.51,256.95 81.42,314.45 L 80.80 331.50 L 85.48 336.75 C91.28,343.26 94.01,343.59 99.62,338.45 Z)SVG",
        R"SVG(M 213.47 371.71 C214.90,369.75 216.69,339.76 215.59,336.27 C214.68,333.42 208.22,333.02 163.28,333.01 L 125.07 333.00 L 116.04 340.58 C106.30,348.75 104.00,351.38 104.00,354.36 C104.00,355.43 107.69,359.94 112.42,364.62 L 120.83 372.96 L 140.67 373.27 C151.57,373.45 172.12,373.57 186.33,373.55 C209.96,373.50 212.28,373.35 213.47,371.71 Z)SVG",
        R"SVG(M 322.58 370.51 C330.39,364.56 340.00,354.60 340.00,352.47 C340.00,351.14 336.94,347.38 331.75,342.33 L 323.50 334.31 L 308.09 333.65 C299.62,333.29 279.11,333.00 262.53,333.00 C238.29,333.00 232.01,333.27 230.47,334.40 C228.79,335.63 228.45,337.84 227.64,352.87 C226.93,366.05 227.00,370.29 227.95,371.44 C229.57,373.40 236.13,373.66 281.58,373.57 L 318.66 373.50 L 322.58 370.51 Z)SVG",
        R"SVG(M 190.35 312.75 C190.68,308.76 191.47,291.33 192.13,274.00 L 193.31 242.50 L 190.07 235.00 C188.28,230.88 184.57,222.77 181.83,217.00 C179.08,211.23 174.36,200.43 171.35,193.00 C168.33,185.57 164.37,176.35 162.54,172.50 C157.90,162.69 154.00,154.02 154.00,153.49 C154.00,153.25 152.42,149.66 150.48,145.52 C147.89,140.00 146.45,138.00 145.04,138.00 C143.98,138.00 142.85,138.44 142.51,138.98 C142.18,139.52 141.27,152.90 140.49,168.73 C139.70,184.55 138.77,202.00 138.42,207.50 L 137.78 217.50 L 142.77 228.00 C147.32,237.55 154.53,253.46 165.16,277.38 C173.58,296.36 184.29,318.53 185.37,319.23 C185.99,319.64 187.23,319.98 188.13,319.98 C189.44,320.00 189.88,318.56 190.35,312.75 Z)SVG",
        R"SVG(M 262.87 316.25 C266.08,310.95 273.81,297.07 279.45,286.50 C282.09,281.55 285.77,274.96 287.63,271.86 C289.49,268.76 292.21,263.59 293.69,260.36 C295.16,257.14 298.94,249.98 302.07,244.46 C305.21,238.93 309.81,230.16 312.29,224.96 L 316.81 215.50 L 317.89 188.50 C319.82,140.14 319.81,138.54 317.50,138.24 C315.90,138.04 314.06,140.46 308.35,150.24 C304.42,156.99 298.84,167.00 295.94,172.50 C293.04,178.00 289.16,185.20 287.33,188.50 C285.49,191.80 281.46,199.45 278.37,205.50 C275.28,211.55 269.81,221.90 266.21,228.50 L 259.66 240.50 L 258.79 261.00 C258.31,272.27 257.58,289.15 257.16,298.50 C256.74,307.85 256.75,316.51 257.17,317.75 C258.31,321.03 260.29,320.51 262.87,316.25 Z)SVG",
        R"SVG(M 133.21 559.92 C135.25,556.86 141.27,546.28 146.58,536.43 C151.90,526.57 156.58,518.05 156.99,517.50 C157.41,516.95 163.75,505.25 171.08,491.50 L 184.42 466.50 L 185.65 438.50 C186.98,408.06 187.30,388.63 186.48,387.81 C186.19,387.52 184.95,387.48 183.72,387.71 C182.07,388.03 179.95,391.02 175.50,399.31 C163.66,421.35 157.64,432.71 150.75,446.00 C146.91,453.42 143.20,460.40 142.52,461.50 C141.84,462.60 138.26,469.25 134.56,476.27 C128.46,487.86 127.80,489.71 127.37,496.27 C126.53,509.18 125.25,563.46 125.76,564.80 C126.70,567.29 129.53,565.44 133.21,559.92 Z)SVG",
        R"SVG(M 302.22 564.25 C302.47,563.29 302.77,556.88 302.90,550.00 C303.02,543.12 303.31,536.60 303.54,535.50 C303.77,534.40 304.44,524.14 305.03,512.70 L 306.11 491.90 L 303.11 485.20 C301.46,481.51 297.56,472.88 294.45,466.00 C291.34,459.12 287.08,449.40 284.99,444.40 C282.90,439.39 278.73,429.94 275.72,423.40 C269.50,409.85 263.89,397.25 261.63,391.74 C259.85,387.39 257.26,385.92 255.54,388.27 C254.79,389.30 253.87,400.92 252.96,420.68 C250.90,465.52 250.81,463.90 255.63,474.00 C257.86,478.67 260.27,484.08 260.98,486.00 C262.46,489.99 265.56,496.95 271.02,508.50 C273.10,512.90 276.89,521.45 279.44,527.50 C281.99,533.55 287.09,544.69 290.78,552.25 C296.29,563.56 297.86,566.00 299.62,566.00 C300.83,566.00 301.96,565.23 302.22,564.25 Z)SVG",
        R"SVG(M 242.57 319.76 C243.46,318.88 244.30,304.36 247.00,244.00 C248.12,218.98 249.46,190.18 249.98,180.00 C250.50,169.82 250.95,156.23 250.97,149.79 C251.00,139.21 250.81,137.97 249.07,137.04 C246.00,135.39 214.81,135.68 212.50,137.38 C210.81,138.61 210.48,141.24 209.35,162.13 C207.89,189.20 206.73,215.60 204.38,275.40 C202.74,317.42 202.74,318.34 204.60,319.71 C206.10,320.81 210.23,321.04 224.17,320.80 C233.89,320.64 242.17,320.17 242.57,319.76 Z)SVG",
        R"SVG(M 233.25 565.85 C234.54,565.17 235.00,563.69 235.00,560.21 C235.00,557.62 235.43,546.05 235.95,534.50 C240.15,441.46 241.90,389.67 240.89,387.79 C240.02,386.17 238.19,386.00 221.59,386.00 C208.12,386.00 202.98,386.33 202.24,387.25 C201.09,388.69 199.73,413.31 196.98,482.50 C196.39,497.35 195.48,518.12 194.95,528.66 C193.63,555.22 193.71,564.52 195.28,565.82 C195.99,566.41 199.25,567.07 202.53,567.30 C211.05,567.88 231.18,566.93 233.25,565.85 Z)SVG",
        R"SVG(M 404.26 616.97 C408.47,614.95 414.00,610.68 414.00,609.45 C414.00,609.10 414.95,607.47 416.12,605.83 C417.54,603.84 418.41,600.73 418.76,596.46 C419.20,590.94 418.90,589.32 416.58,584.58 C412.85,576.96 406.30,572.21 398.36,571.39 C390.06,570.53 384.72,572.25 378.95,577.64 C368.48,587.44 368.22,603.04 378.36,612.87 C384.74,619.05 396.15,620.85 404.26,616.97 Z)SVG"
    };
    static const std::array<QPainterPath, 17> kPaths = [] {
        std::array<QPainterPath, 17> out;
        for (int i = 0; i < static_cast<int>(out.size()); ++i) {
            out[i] = parseSvgPathDataAbsolute(kSeg16PathData[i]);
        }
        return out;
    }();
    return kPaths;
}

double lookupNodeVoltage(const QMap<QString, double>& nodeVoltages, const QString& rawNet) {
    const QString net = rawNet.trimmed();
    if (net.isEmpty()) return 0.0;

    if (nodeVoltages.contains(net)) return nodeVoltages.value(net, 0.0);
    const QString lower = net.toLower();
    if (nodeVoltages.contains(lower)) return nodeVoltages.value(lower, 0.0);
    const QString upper = net.toUpper();
    if (nodeVoltages.contains(upper)) return nodeVoltages.value(upper, 0.0);

    if (net.startsWith("V(", Qt::CaseInsensitive) && net.endsWith(')') && net.size() > 3) {
        return lookupNodeVoltage(nodeVoltages, net.mid(2, net.size() - 3));
    }

    for (auto it = nodeVoltages.constBegin(); it != nodeVoltages.constEnd(); ++it) {
        if (it.key().compare(net, Qt::CaseInsensitive) == 0) return it.value();
    }
    return 0.0;
}

int variantHalfWidth(SevenSegmentDisplayItem::Variant variant) {
    switch (variant) {
    case SevenSegmentDisplayItem::Variant::Dual7: return 140;
    case SevenSegmentDisplayItem::Variant::Seg14: return 100;
    case SevenSegmentDisplayItem::Variant::Seg16: return 100;
    case SevenSegmentDisplayItem::Variant::Single7:
    default: return 100;
    }
}

int variantHalfHeight(SevenSegmentDisplayItem::Variant variant) {
    switch (variant) {
    case SevenSegmentDisplayItem::Variant::Dual7: return 145;
    case SevenSegmentDisplayItem::Variant::Seg14: return 120;
    case SevenSegmentDisplayItem::Variant::Seg16: return 120;
    case SevenSegmentDisplayItem::Variant::Single7:
    default: return 120;
    }
}
} // namespace

SevenSegmentDisplayItem::Variant SevenSegmentDisplayItem::variantFromTypeName(const QString& typeName) {
    const QString t = typeName.trimmed().toLower();
    if (t.contains("dual") && t.contains("7-segment")) return Variant::Dual7;
    if (t.contains("14-segment")) return Variant::Seg14;
    if (t.contains("16-segment")) return Variant::Seg16;
    return Variant::Single7;
}

QString SevenSegmentDisplayItem::typeNameForVariant(Variant variant) {
    switch (variant) {
    case Variant::Dual7: return "Dual 7-Segment Display";
    case Variant::Seg14: return "14-Segment Display";
    case Variant::Seg16: return "16-Segment Display";
    case Variant::Single7:
    default: return "7-Segment Display";
    }
}

bool SevenSegmentDisplayItem::isSegmentDisplayTypeName(const QString& typeName) {
    const QString t = typeName.trimmed().toLower();
    return t == "7-segment display" || t == "dual 7-segment display" ||
           t == "14-segment display" || t == "16-segment display";
}

SevenSegmentDisplayItem::SevenSegmentDisplayItem(Variant variant, QPointF pos, const QString& typeName, QGraphicsItem* parent)
    : SchematicItem(parent)
    , m_typeName(typeName.trimmed().isEmpty() ? typeNameForVariant(variant) : typeName.trimmed())
    , m_variant(typeName.trimmed().isEmpty() ? variant : variantFromTypeName(typeName)) {
    setPos(pos);
    setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
    setReference("DS1");
    ensureDriveSize();
    setValue("CC");
}

void SevenSegmentDisplayItem::ensureDriveSize() {
    m_segmentDrive.resize(segmentCount());
    for (double& d : m_segmentDrive) d = 0.0;
}

void SevenSegmentDisplayItem::setVariant(Variant variant) {
    m_variant = variant;
    m_typeName = typeNameForVariant(variant);
    ensureDriveSize();
    update();
}

QStringList SevenSegmentDisplayItem::segmentPinLabels() const {
    switch (m_variant) {
    case Variant::Dual7:
        return {"a1", "b1", "c1", "d1", "e1", "f1", "g1", "dp1",
                "a2", "b2", "c2", "d2", "e2", "f2", "g2", "dp2"};
    case Variant::Seg14:
        return {"a", "b", "c", "d", "e", "f", "g1", "g2", "h", "i", "j", "k", "l", "m", "dp"};
    case Variant::Seg16:
        return {"a1", "a2", "b", "c", "d1", "d2", "e", "f", "g1", "g2", "h", "i", "j", "k", "l", "m", "dp"};
    case Variant::Single7:
    default:
        return {"a", "b", "c", "d", "e", "f", "g", "dp"};
    }
}

QStringList SevenSegmentDisplayItem::allPinLabels() const {
    QStringList labels = segmentPinLabels();
    labels << "com1" << "com2";
    return labels;
}

int SevenSegmentDisplayItem::segmentCount() const {
    return segmentPinLabels().size();
}

int SevenSegmentDisplayItem::comPin1Index() const {
    return segmentCount();
}

int SevenSegmentDisplayItem::comPin2Index() const {
    return segmentCount() + 1;
}

QString SevenSegmentDisplayItem::pinName(int index) const {
    const QStringList labels = allPinLabels();
    if (index >= 0 && index < labels.size()) return labels[index];
    return SchematicItem::pinName(index);
}

void SevenSegmentDisplayItem::setValue(const QString& value) {
    const QString v = value.trimmed().toUpper();
    if (v == "CA" || v.contains("ANODE")) {
        m_commonType = CommonType::CommonAnode;
    } else if (v == "CC" || v.contains("CATHODE")) {
        m_commonType = CommonType::CommonCathode;
    }
    SchematicItem::setValue(m_commonType == CommonType::CommonCathode ? "CC" : "CA");
    update();
}

void SevenSegmentDisplayItem::setCommonType(CommonType type) {
    m_commonType = type;
    SchematicItem::setValue(m_commonType == CommonType::CommonCathode ? "CC" : "CA");
    update();
}

void SevenSegmentDisplayItem::setThresholdVoltage(double volts) {
    m_thresholdV = std::max(0.0, volts);
    update();
}

QRectF SevenSegmentDisplayItem::boundingRect() const {
    const int hw = variantHalfWidth(m_variant);
    const int hh = variantHalfHeight(m_variant);
    return QRectF(-hw, -hh, hw * 2, hh * 2);
}

QList<QPointF> SevenSegmentDisplayItem::connectionPoints() const {
    QList<QPointF> points;
    const int hw = variantHalfWidth(m_variant);
    const int segCount = segmentCount();
    const int leftCount = (segCount + 1) / 2;
    const int rightCount = segCount - leftCount;
    const int leftStartY = -(leftCount / 2) * kPinPitch;
    const int rightStartY = -(rightCount / 2) * kPinPitch;

    for (int i = 0; i < leftCount; ++i) {
        points.append(QPointF(-hw, leftStartY + i * kPinPitch));
    }
    for (int i = 0; i < rightCount; ++i) {
        points.append(QPointF(hw, rightStartY + i * kPinPitch));
    }

    const int com1Y = rightStartY + rightCount * kPinPitch + kPinPitch;
    points.append(QPointF(hw, com1Y)); // com1
    points.append(QPointF(hw, com1Y + kPinPitch)); // com2
    return points;
}

bool SevenSegmentDisplayItem::segmentOn(int idx) const {
    return idx >= 0 && idx < m_segmentDrive.size() && m_segmentDrive[idx] > 0.01;
}

void SevenSegmentDisplayItem::setSimState(const QMap<QString, double>& nodeVoltages, const QMap<QString, double>&) {
    auto pinNetName = [&](int pinIndex) -> QString { return pinNet(pinIndex).trimmed(); };
    auto readPinVoltage = [&](int pinIndex) -> double { return lookupNodeVoltage(nodeVoltages, pinNetName(pinIndex)); };

    const QString com1Net = pinNetName(comPin1Index());
    const QString com2Net = pinNetName(comPin2Index());
    const bool hasCom1 = !com1Net.isEmpty();
    const bool hasCom2 = !com2Net.isEmpty();

    double vCom = 0.0;
    bool hasCommonVoltage = false;
    if (hasCom1 && hasCom2) {
        const double v1 = readPinVoltage(comPin1Index());
        const double v2 = readPinVoltage(comPin2Index());
        vCom = (m_commonType == CommonType::CommonCathode) ? std::min(v1, v2) : std::max(v1, v2);
        hasCommonVoltage = true;
    } else if (hasCom1) {
        vCom = readPinVoltage(comPin1Index());
        hasCommonVoltage = true;
    } else if (hasCom2) {
        vCom = readPinVoltage(comPin2Index());
        hasCommonVoltage = true;
    }

    double inferredHigh = 0.0;
    for (int i = 0; i < segmentCount(); ++i) {
        inferredHigh = std::max(inferredHigh, readPinVoltage(i));
    }

    for (int i = 0; i < segmentCount(); ++i) {
        const double vSeg = readPinVoltage(i);
        const double dv = hasCommonVoltage
            ? ((m_commonType == CommonType::CommonCathode) ? (vSeg - vCom) : (vCom - vSeg))
            : ((m_commonType == CommonType::CommonCathode) ? vSeg : (inferredHigh - vSeg));
        const double drive = (dv > m_thresholdV)
            ? std::clamp((dv - m_thresholdV) / 1.0, 0.15, 1.0)
            : 0.0;
        m_segmentDrive[i] = drive;
    }

    update();
}

QString SevenSegmentDisplayItem::displayHeader() const {
    QString base;
    switch (m_variant) {
    case Variant::Dual7: base = "DUAL 7-SEG"; break;
    case Variant::Seg14: base = "14-SEG"; break;
    case Variant::Seg16: base = "16-SEG"; break;
    case Variant::Single7:
    default: base = "7-SEG"; break;
    }
    return QString("%1 (%2)").arg(base, m_commonType == CommonType::CommonCathode ? "CC" : "CA");
}

void SevenSegmentDisplayItem::paintClassic7Digit(QPainter* painter, const QRectF& bezel, int baseSegmentIndex) const {
    const double sx = bezel.left() + 6.0;
    const double sy = bezel.top() + 6.0;
    const double hW = 44.0;
    const double hH = 10.0;
    const double vW = 10.0;
    const double vH = 42.0;
    const double cut = 3.0;

    const double yTop = sy + 0.0;
    const double yUpperV = sy + 8.0;
    const double yMid = sy + 52.0;
    const double yLowerV = sy + 54.0;
    const double yBottom = sy + 98.0; // was effectively ~108; tighten gap above bottom

    const QPolygonF segA = segPolyH(sx + 10, yTop, hW, hH, cut);
    const QPolygonF segB = segPolyV(sx + 56, yUpperV, vW, vH, cut);
    const QPolygonF segC = segPolyV(sx + 56, yLowerV, vW, vH, cut);
    const QPolygonF segD = segPolyH(sx + 10, yBottom, hW, hH, cut);
    const QPolygonF segE = segPolyV(sx + 0, yLowerV, vW, vH, cut);
    const QPolygonF segF = segPolyV(sx + 0, yUpperV, vW, vH, cut);
    const QPolygonF segG = segPolyH(sx + 10, yMid, hW, hH, cut);
    const QPointF dpCenter(sx + 73, yBottom + 4.0);

    const QPolygonF segments[7] = {segA, segB, segC, segD, segE, segF, segG};
    for (int i = 0; i < 7; ++i) {
        const QColor on = segmentColorForDrive(m_segmentDrive.value(baseSegmentIndex + i, 0.0)).lighter(130);
        const QColor off(65, 26, 26);
        painter->setPen(QPen(QColor(25, 25, 25), 0.8));
        painter->setBrush(m_segmentDrive.value(baseSegmentIndex + i, 0.0) > 0.01 ? on : off);
        painter->drawPolygon(segments[i]);
    }

    const QColor onDp = segmentColorForDrive(m_segmentDrive.value(baseSegmentIndex + 7, 0.0)).lighter(130);
    const QColor offDp(65, 26, 26);
    painter->setPen(QPen(QColor(25, 25, 25), 0.8));
    painter->setBrush(m_segmentDrive.value(baseSegmentIndex + 7, 0.0) > 0.01 ? onDp : offDp);
    painter->drawEllipse(dpCenter, 4.3, 4.3);
}

void SevenSegmentDisplayItem::paintBarArrayDigit(QPainter* painter, const QRectF& bezel, int baseSegmentIndex, int count) const {
    const int cols = (count > 10) ? 2 : 1;
    const int rows = static_cast<int>(std::ceil(static_cast<double>(count) / cols));
    const double padX = 8.0;
    const double padY = 10.0;
    const double usableW = bezel.width() - 2 * padX;
    const double usableH = bezel.height() - 2 * padY;
    const double cellW = usableW / cols;
    const double cellH = usableH / rows;

    for (int i = 0; i < count; ++i) {
        const int col = i / rows;
        const int row = i % rows;
        const QRectF cell(bezel.left() + padX + col * cellW,
                          bezel.top() + padY + row * cellH,
                          cellW, cellH);
        const QRectF bar = cell.adjusted(3.5, 2.5, -3.5, -2.5);
        const QColor on = segmentColorForDrive(m_segmentDrive.value(baseSegmentIndex + i, 0.0)).lighter(125);
        const QColor off(60, 24, 24);
        painter->setPen(QPen(QColor(22, 22, 22), 0.8));
        painter->setBrush(m_segmentDrive.value(baseSegmentIndex + i, 0.0) > 0.01 ? on : off);
        painter->drawRoundedRect(bar, 2.0, 2.0);
    }
}

void SevenSegmentDisplayItem::paintInspired14Or16(QPainter* painter, const QRectF& bezel) const {
    auto mapPt = [&](double nx, double ny) {
        return QPointF(bezel.left() + nx * bezel.width(), bezel.top() + ny * bezel.height());
    };

    const qreal segWMain = std::max<qreal>(3.8, bezel.width() * 0.078);
    const qreal segWDiag = std::max<qreal>(3.2, bezel.width() * 0.062);
    const qreal segWCenter = std::max<qreal>(3.1, bezel.width() * 0.061);
    auto drawSegPoly = [&](int idx, const QPointF& p1, const QPointF& p2, qreal width) {
        const bool on = segmentOn(idx);
        const QColor onColor = segmentColorForDrive(m_segmentDrive.value(idx, 0.0)).lighter(125);
        const QColor offColor(86, 34, 34); // brighter off-state so no segment appears "missing"
        painter->setPen(QPen(QColor(24, 24, 24), 0.8));
        painter->setBrush(on ? onColor : offColor);
        painter->drawPolygon(segPolyLine(p1, p2, width));
    };

    if (m_variant == Variant::Seg16) {
        const QRectF exactRect = fitRectPreserveAspect(QSizeF(452.0, 694.0), bezel.adjusted(2, 2, -2, -2));
        QTransform xf;
        xf.translate(exactRect.left(), exactRect.top());
        xf.scale(exactRect.width() / 452.0, exactRect.height() / 694.0);
        const auto& segPaths = exactSeg16Paths();
        for (int idx = 0; idx < static_cast<int>(segPaths.size()); ++idx) {
            const bool on = segmentOn(idx);
            const QColor onColor = segmentColorForDrive(m_segmentDrive.value(idx, 0.0)).lighter(125);
            const QColor offColor(86, 34, 34);
            painter->setPen(QPen(QColor(24, 24, 24), 0.8));
            painter->setBrush(on ? onColor : offColor);
            painter->drawPath(xf.map(segPaths[idx]));
        }
        return;
    }

    // Keep 14-seg silhouette close to classic 7-seg proportions.
    const double xL = 0.18, xR = 0.82;
    const double xLM = 0.28, xRM = 0.72;
    const double xC = 0.50;
    const double yT = 0.11, yUT = 0.18, yUM = 0.41, yM = 0.50, yLM = 0.59, yLB = 0.82, yB = 0.89;

    const QPointF aL = mapPt(xL, yT), aR = mapPt(xR, yT);
    const QPointF dL = mapPt(xL, yB), dR = mapPt(xR, yB);
    const QPointF bT = mapPt(xR, yUT), bM = mapPt(xR, yUM);
    const QPointF cM = mapPt(xR, yLM), cB = mapPt(xR, yLB);
    const QPointF fT = mapPt(xL, yUT), fM = mapPt(xL, yUM);
    const QPointF eM = mapPt(xL, yLM), eB = mapPt(xL, yLB);
    const QPointF g1L = mapPt(xL + 0.05, yM), g1R = mapPt(xC - 0.05, yM);
    const QPointF g2L = mapPt(xC + 0.05, yM), g2R = mapPt(xR - 0.05, yM);
    const QPointF h0 = mapPt(xLM, yUT), h1 = mapPt(xC - 0.048, yUM + 0.018);
    const QPointF i0 = mapPt(xRM, yUT), i1 = mapPt(xC + 0.048, yUM + 0.018);
    const QPointF j0 = mapPt(xLM, yLB), j1 = mapPt(xC - 0.048, yLM - 0.018);
    const QPointF k0 = mapPt(xRM, yLB), k1 = mapPt(xC + 0.048, yLM - 0.018);
    const QPointF lT = mapPt(xC, yUT + 0.03), lB = mapPt(xC, yUM + 0.005);
    const QPointF mT = mapPt(xC, yLM - 0.005), mB = mapPt(xC, yLB - 0.03);

    // 14-seg: a b c d e f g1 g2 h i j k l m
    drawSegPoly(0, aL, aR, segWMain);
    drawSegPoly(1, bT, bM, segWMain);
    drawSegPoly(2, cM, cB, segWMain);
    drawSegPoly(3, dL, dR, segWMain);
    drawSegPoly(4, eM, eB, segWMain);
    drawSegPoly(5, fT, fM, segWMain);
    drawSegPoly(6, g1L, g1R, segWCenter);
    drawSegPoly(7, g2L, g2R, segWCenter);
    drawSegPoly(8, h0, h1, segWDiag);
    drawSegPoly(9, i0, i1, segWDiag);
    drawSegPoly(10, j0, j1, segWDiag);
    drawSegPoly(11, k0, k1, segWDiag);
    drawSegPoly(12, lT, lB, segWCenter);
    drawSegPoly(13, mT, mB, segWCenter);

    const bool dpOn = segmentOn(14);
    painter->setPen(QPen(QColor(24, 24, 24), 0.8));
    painter->setBrush(dpOn ? segmentColorForDrive(m_segmentDrive.value(14, 0.0)).lighter(125)
                           : QColor(86, 34, 34));
    painter->drawEllipse(mapPt(0.93, 0.93), segWMain * 0.34, segWMain * 0.34);
}

void SevenSegmentDisplayItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);

    const QRectF bounds = boundingRect();
    const qreal bodyMargin = 8.0;
    QRectF body = bounds.adjusted(bodyMargin, bodyMargin, -bodyMargin, -bodyMargin);
    painter->setPen(QPen(isSelected() ? QColor("#55aaff") : QColor("#d9d9d9"), 2.0));
    painter->setBrush(QColor("#2a2f3a"));
    painter->drawRoundedRect(body, 8, 8);

    QRectF bezel = body.adjusted(22, 24, -22, -24);
    if (m_variant == Variant::Dual7) {
        bezel = body.adjusted(18, 24, -18, -24);
    }
    painter->setPen(QPen(QColor("#aab0bb"), 1.0));
    painter->setBrush(QColor("#101319"));
    painter->drawRoundedRect(bezel, 5, 5);

    if (m_variant == Variant::Single7) {
        paintClassic7Digit(painter, bezel.adjusted(8, 8, -8, -8), 0);
    } else if (m_variant == Variant::Dual7) {
        const QRectF leftBezel = QRectF(bezel.left() + 6, bezel.top() + 8, bezel.width() * 0.46, bezel.height() - 16);
        const QRectF rightBezel = QRectF(bezel.left() + bezel.width() * 0.52, bezel.top() + 8, bezel.width() * 0.46, bezel.height() - 16);
        paintClassic7Digit(painter, leftBezel, 0);
        paintClassic7Digit(painter, rightBezel, 8);
    } else if (m_variant == Variant::Seg14) {
        paintInspired14Or16(painter, bezel.adjusted(6, 6, -6, -6));
    } else {
        paintInspired14Or16(painter, bezel.adjusted(6, 6, -6, -6));
    }

    painter->setFont(QFont("Inter", 6));
    painter->setPen(QColor("#e6e8eb"));
    const QList<QPointF> pts = connectionPoints();
    const QStringList labels = allPinLabels();
    for (int i = 0; i < pts.size(); ++i) {
        const QPointF p = pts[i];
        if (p.x() < 0) {
            painter->drawLine(p, QPointF(p.x() + 13, p.y()));
            painter->drawText(QRectF(p.x() + 15, p.y() - 6, 44, 12), Qt::AlignLeft | Qt::AlignVCenter, labels.value(i));
        } else {
            painter->drawLine(QPointF(p.x() - 13, p.y()), p);
            painter->drawText(QRectF(p.x() - 56, p.y() - 6, 40, 12), Qt::AlignRight | Qt::AlignVCenter, labels.value(i));
        }
    }

    painter->setPen(QPen(QColor("#d6d9de"), 1.0));
    painter->setBrush(Qt::NoBrush);
    const QPointF com1 = pts.value(comPin1Index(), QPointF());
    const QPointF com2 = pts.value(comPin2Index(), QPointF());
    const QPointF m1(com1.x() - 16, com1.y());
    const QPointF m2(com2.x() - 16, com2.y());
    if (m_commonType == CommonType::CommonAnode) {
        painter->drawEllipse(m1, 2.1, 2.1);
        painter->drawEllipse(m2, 2.1, 2.1);
    } else {
        painter->drawLine(m1.x() - 3, m1.y(), m1.x() + 3, m1.y());
        painter->drawLine(m2.x() - 3, m2.y(), m2.x() + 3, m2.y());
    }

    painter->setPen(QColor("#9aa0aa"));
    painter->setFont(QFont("Inter", 7, QFont::DemiBold));
    painter->drawText(QRectF(body.left() + 2, body.top() + 2, body.width() - 4, 14), Qt::AlignCenter, displayHeader());

    drawConnectionPointHighlights(painter);
}

QJsonObject SevenSegmentDisplayItem::toJson() const {
    QJsonObject j = SchematicItem::toJson();
    j["type"] = m_typeName;
    j["variant"] = static_cast<int>(m_variant);
    j["commonType"] = static_cast<int>(m_commonType);
    j["threshold"] = m_thresholdV;
    return j;
}

bool SevenSegmentDisplayItem::fromJson(const QJsonObject& json) {
    if (!SchematicItem::fromJson(json)) return false;

    if (json.contains("variant")) {
        const int raw = json["variant"].toInt(static_cast<int>(Variant::Single7));
        switch (raw) {
        case 1: m_variant = Variant::Dual7; break;
        case 2: m_variant = Variant::Seg14; break;
        case 3: m_variant = Variant::Seg16; break;
        case 0:
        default: m_variant = Variant::Single7; break;
        }
    } else if (json.contains("type")) {
        m_variant = variantFromTypeName(json["type"].toString());
    }
    m_typeName = typeNameForVariant(m_variant);
    ensureDriveSize();

    if (json.contains("commonType")) {
        const int type = json["commonType"].toInt(static_cast<int>(CommonType::CommonCathode));
        m_commonType = (type == static_cast<int>(CommonType::CommonAnode))
            ? CommonType::CommonAnode
            : CommonType::CommonCathode;
    }
    if (json.contains("threshold")) {
        m_thresholdV = json["threshold"].toDouble(1.7);
    }
    SchematicItem::setValue(m_commonType == CommonType::CommonCathode ? "CC" : "CA");
    update();
    return true;
}

SchematicItem* SevenSegmentDisplayItem::clone() const {
    auto* item = new SevenSegmentDisplayItem(m_variant, pos(), m_typeName);
    item->fromJson(this->toJson());
    return item;
}
