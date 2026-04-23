#include "seven_segment_display_item.h"

#include <QJsonObject>
#include <QPainter>
#include <QRadialGradient>
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

QColor segmentColorForDrive(double drive, const QColor& baseColor) {
    const double t = std::clamp(drive, 0.0, 1.0);
    QColor c = baseColor.isValid() ? baseColor : QColor(255, 72, 60);
    const int minMix = 48;
    const int maxMix = 255;
    const int mix = static_cast<int>(minMix + (maxMix - minMix) * t);
    c.setRed(std::clamp((c.red() * mix) / 255, 0, 255));
    c.setGreen(std::clamp((c.green() * mix) / 255, 0, 255));
    c.setBlue(std::clamp((c.blue() * mix) / 255, 0, 255));
    return c;
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

QRectF unitedPathBounds(const QPainterPath* paths, int count) {
    QRectF bounds;
    for (int i = 0; i < count; ++i) {
        const QRectF r = paths[i].controlPointRect();
        bounds = bounds.isNull() ? r : bounds.united(r);
    }
    return bounds;
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
    // Canonical 16-seg order must match the reference layout exactly:
    // a1 a2
    // f  h i j  b
    // g1     g2
    // e  m l k  c
    // d1 d2
    // dp
    //
    // Sensitive: the inner six are easy to mis-map. The correct order is:
    // h = upper-left diagonal
    // i = upper center vertical
    // j = upper-right diagonal
    // k = lower-right diagonal
    // l = lower center vertical
    // m = lower-left diagonal
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
        R"SVG(M 242.57 319.76 C243.46,318.88 244.30,304.36 247.00,244.00 C248.12,218.98 249.46,190.18 249.98,180.00 C250.50,169.82 250.95,156.23 250.97,149.79 C251.00,139.21 250.81,137.97 249.07,137.04 C246.00,135.39 214.81,135.68 212.50,137.38 C210.81,138.61 210.48,141.24 209.35,162.13 C207.89,189.20 206.73,215.60 204.38,275.40 C202.74,317.42 202.74,318.34 204.60,319.71 C206.10,320.81 210.23,321.04 224.17,320.80 C233.89,320.64 242.17,320.17 242.57,319.76 Z)SVG",
        R"SVG(M 262.87 316.25 C266.08,310.95 273.81,297.07 279.45,286.50 C282.09,281.55 285.77,274.96 287.63,271.86 C289.49,268.76 292.21,263.59 293.69,260.36 C295.16,257.14 298.94,249.98 302.07,244.46 C305.21,238.93 309.81,230.16 312.29,224.96 L 316.81 215.50 L 317.89 188.50 C319.82,140.14 319.81,138.54 317.50,138.24 C315.90,138.04 314.06,140.46 308.35,150.24 C304.42,156.99 298.84,167.00 295.94,172.50 C293.04,178.00 289.16,185.20 287.33,188.50 C285.49,191.80 281.46,199.45 278.37,205.50 C275.28,211.55 269.81,221.90 266.21,228.50 L 259.66 240.50 L 258.79 261.00 C258.31,272.27 257.58,289.15 257.16,298.50 C256.74,307.85 256.75,316.51 257.17,317.75 C258.31,321.03 260.29,320.51 262.87,316.25 Z)SVG",
        R"SVG(M 302.22 564.25 C302.47,563.29 302.77,556.88 302.90,550.00 C303.02,543.12 303.31,536.60 303.54,535.50 C303.77,534.40 304.44,524.14 305.03,512.70 L 306.11 491.90 L 303.11 485.20 C301.46,481.51 297.56,472.88 294.45,466.00 C291.34,459.12 287.08,449.40 284.99,444.40 C282.90,439.39 278.73,429.94 275.72,423.40 C269.50,409.85 263.89,397.25 261.63,391.74 C259.85,387.39 257.26,385.92 255.54,388.27 C254.79,389.30 253.87,400.92 252.96,420.68 C250.90,465.52 250.81,463.90 255.63,474.00 C257.86,478.67 260.27,484.08 260.98,486.00 C262.46,489.99 265.56,496.95 271.02,508.50 C273.10,512.90 276.89,521.45 279.44,527.50 C281.99,533.55 287.09,544.69 290.78,552.25 C296.29,563.56 297.86,566.00 299.62,566.00 C300.83,566.00 301.96,565.23 302.22,564.25 Z)SVG",
        R"SVG(M 233.25 565.85 C234.54,565.17 235.00,563.69 235.00,560.21 C235.00,557.62 235.43,546.05 235.95,534.50 C240.15,441.46 241.90,389.67 240.89,387.79 C240.02,386.17 238.19,386.00 221.59,386.00 C208.12,386.00 202.98,386.33 202.24,387.25 C201.09,388.69 199.73,413.31 196.98,482.50 C196.39,497.35 195.48,518.12 194.95,528.66 C193.63,555.22 193.71,564.52 195.28,565.82 C195.99,566.41 199.25,567.07 202.53,567.30 C211.05,567.88 231.18,566.93 233.25,565.85 Z)SVG",
        R"SVG(M 133.21 559.92 C135.25,556.86 141.27,546.28 146.58,536.43 C151.90,526.57 156.58,518.05 156.99,517.50 C157.41,516.95 163.75,505.25 171.08,491.50 L 184.42 466.50 L 185.65 438.50 C186.98,408.06 187.30,388.63 186.48,387.81 C186.19,387.52 184.95,387.48 183.72,387.71 C182.07,388.03 179.95,391.02 175.50,399.31 C163.66,421.35 157.64,432.71 150.75,446.00 C146.91,453.42 143.20,460.40 142.52,461.50 C141.84,462.60 138.26,469.25 134.56,476.27 C128.46,487.86 127.80,489.71 127.37,496.27 C126.53,509.18 125.25,563.46 125.76,564.80 C126.70,567.29 129.53,565.44 133.21,559.92 Z)SVG",
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

const std::array<QPainterPath, 15>& exactSeg14Paths() {
    // Canonical 14-seg order must match the reference layout exactly:
    //    a
    // f  h i j  b
    //   g1   g2
    // e  m l k  c
    //    d
    // dp
    //
    // Sensitive: the inner six are easy to mis-map. The correct order is:
    // h = upper-left diagonal
    // i = upper center vertical
    // j = upper-right diagonal
    // k = lower-right diagonal
    // l = lower center vertical
    // m = lower-left diagonal
    static const std::array<QString, 15> kSeg14PathData = {
        R"SVG(M 119.35 21.58 C124.59,16.78 124.63,15.76 119.84,11.34 C117.54,9.22 116.75,9.17 73.94,8.57 L 30.39 7.95 L 25.69 11.61 C23.11,13.62 21.00,15.94 21.00,16.75 C21.00,17.57 22.75,19.98 24.88,22.12 L 28.76 26.00 L 71.65 26.00 L 114.54 26.00 L 119.35 21.58 Z)SVG",
        R"SVG(M 127.21 118.25 C129.69,115.61 129.82,114.88 130.47,99.50 C130.85,90.70 131.79,70.90 132.55,55.50 C133.88,28.72 133.86,27.39 132.11,25.00 C131.11,23.62 129.63,22.50 128.83,22.50 C128.03,22.50 124.87,24.75 121.81,27.50 L 116.23 32.50 L 115.13 48.50 C114.52,57.30 113.72,74.69 113.35,87.14 L 112.69 109.79 L 117.59 115.38 C120.29,118.46 122.98,120.98 123.56,120.99 C124.15,120.99 125.79,119.76 127.21,118.25 Z)SVG",
        R"SVG(M 123.74 224.80 C124.79,223.54 125.84,207.36 128.04,158.56 L 129.08 135.61 L 126.13 132.67 L 123.18 129.72 L 117.25 135.67 L 111.31 141.62 L 110.59 157.06 C110.20,165.55 109.47,182.40 108.98,194.50 L 108.08 216.50 L 113.70 222.20 C119.45,228.01 120.80,228.36 123.74,224.80 Z)SVG",
        R"SVG(M 109.97 237.90 C115.98,233.55 116.06,232.40 110.72,227.35 L 106.12 223.00 L 64.31 223.00 L 22.50 223.01 L 17.75 227.32 C15.14,229.69 13.00,232.07 13.00,232.62 C13.00,233.16 14.47,235.03 16.26,236.77 C20.08,240.47 18.57,240.37 73.18,240.69 L 105.85 240.89 L 109.97 237.90 Z)SVG",
        R"SVG(M 18.85 218.30 C20.09,217.65 20.98,205.74 22.08,175.17 L 23.32 140.84 L 17.61 135.17 L 11.90 129.50 L 9.13 132.56 L 6.37 135.62 L 5.16 161.56 C4.50,175.83 3.67,195.44 3.32,205.14 L 2.69 222.78 L 5.43 225.52 L 8.17 228.26 L 13.31 223.38 C16.14,220.70 18.63,218.41 18.85,218.30 Z)SVG",
        R"SVG(M 18.53 115.75 C22.80,111.56 23.98,109.69 24.38,106.50 C24.65,104.30 25.43,86.88 26.11,67.78 L 27.34 33.06 L 22.92 28.15 C17.21,21.80 16.16,21.39 13.23,24.31 C10.47,27.08 10.05,31.89 7.94,85.80 L 6.75 116.10 L 9.05 118.55 C10.31,119.90 11.76,121.00 12.26,121.00 C12.76,121.00 15.58,118.64 18.53,115.75 Z)SVG",
        R"SVG(M 64.50 125.50 L 64.50 117.50 L 45.13 117.23 L 25.76 116.96 L 21.38 120.54 C18.97,122.52 17.00,124.78 17.00,125.57 C17.00,126.36 18.68,128.59 20.73,130.52 L 24.46 134.04 L 44.48 133.77 L 64.50 133.50 L 64.50 125.50 Z)SVG",
        R"SVG(M 115.00 129.50 L 119.41 125.01 L 115.50 121.00 L 111.59 117.00 L 91.43 117.00 L 71.27 117.00 L 70.64 121.64 C69.81,127.64 69.83,132.49 70.67,133.33 C71.03,133.70 80.16,134.00 90.96,134.00 L 110.58 134.00 L 115.00 129.50 Z)SVG",
        R"SVG(M 54.67 94.75 L 55.62 78.50 L 49.81 65.19 C46.61,57.87 44.00,51.74 44.00,51.58 C44.00,49.78 34.65,31.97 34.04,32.62 C33.59,33.10 32.93,39.80 32.56,47.50 C32.20,55.20 31.67,62.25 31.40,63.18 C30.85,65.07 33.13,70.83 43.26,93.00 C50.45,108.75 51.67,111.00 52.98,111.00 C53.39,111.00 54.15,103.69 54.67,94.75 Z)SVG",
        R"SVG(M 76.64 110.70 C77.22,110.12 80.77,33.73 80.27,32.75 C80.06,32.34 76.15,32.00 71.59,32.00 L 63.30 32.00 L 62.69 40.25 C61.36,58.50 59.67,108.70 60.34,110.43 C60.97,112.08 61.87,112.22 68.62,111.71 C72.79,111.40 76.39,110.94 76.64,110.70 Z)SVG",
        R"SVG(M 97.04 88.64 L 108.79 66.88 L 109.51 50.53 C109.91,41.54 109.92,33.67 109.53,33.04 C108.76,31.80 105.39,36.68 100.58,46.00 C99.01,49.03 94.67,57.18 90.93,64.11 L 84.13 76.72 L 83.41 94.00 C82.83,107.91 82.95,111.18 83.99,110.84 C84.70,110.60 90.57,100.61 97.04,88.64 Z)SVG",
        R"SVG(M 103.30 200.79 L 104.11 185.24 L 96.65 168.37 C85.80,143.81 83.96,140.00 82.93,140.00 C82.42,140.00 81.98,141.46 81.95,143.25 C81.93,145.04 81.52,152.35 81.05,159.50 C80.08,174.28 79.01,170.33 91.78,199.00 C99.88,217.18 99.89,217.20 101.32,216.73 C102.11,216.47 102.76,211.24 103.30,200.79 Z)SVG",
        R"SVG(M 72.61 216.82 C73.46,215.45 74.48,196.51 75.51,163.25 L 76.23 140.00 L 67.76 140.00 L 59.28 140.00 L 58.68 146.25 C58.35,149.69 57.53,166.82 56.86,184.33 C55.80,211.81 55.83,216.28 57.07,217.06 C59.21,218.42 71.74,218.22 72.61,216.82 Z)SVG",
        R"SVG(M 29.76 214.25 C32.12,210.82 42.40,192.16 47.79,181.50 L 51.84 173.50 L 52.37 156.75 C52.66,147.54 52.53,140.00 52.07,140.00 C50.68,140.00 49.63,141.78 38.20,163.50 L 27.16 184.50 L 26.45 200.75 C25.71,217.86 26.12,219.53 29.76,214.25 Z)SVG",
        R"SVG(M 146.33 239.64 C153.35,236.70 155.27,228.12 150.08,222.92 C142.07,214.91 128.50,224.44 133.37,234.66 C134.36,236.73 136.31,238.67 138.26,239.51 C142.33,241.28 142.39,241.28 146.33,239.64 Z)SVG"
    };
    static const std::array<QPainterPath, 15> kPaths = [] {
        std::array<QPainterPath, 15> out;
        for (int i = 0; i < static_cast<int>(out.size()); ++i) {
            out[i] = parseSvgPathDataAbsolute(kSeg14PathData[i]);
        }
        return out;
    }();
    return kPaths;
}

const std::array<QPainterPath, 8>& exactSeg7Paths() {
    static const std::array<QString, 8> kSeg7PathData = {
        R"SVG(M 116.57 17.44 C119.21,15.21 121.54,12.89 121.74,12.29 C121.94,11.68 120.68,9.57 118.95,7.59 L 115.79 4.00 L 71.15 4.01 L 26.50 4.02 L 22.75 7.57 C20.69,9.53 19.00,11.62 19.00,12.22 C19.00,12.82 20.87,15.03 23.15,17.14 L 27.30 20.97 L 55.40 21.23 C70.85,21.38 89.86,21.50 97.64,21.50 L 111.78 21.50 L 116.57 17.44 Z)SVG",
        R"SVG(M 125.23 113.25 L 127.83 110.50 L 129.89 66.50 C131.89,23.81 131.89,22.43 130.12,20.00 C129.11,18.62 127.58,17.50 126.73,17.50 C125.87,17.50 122.72,19.84 119.73,22.70 L 114.28 27.89 L 113.53 37.70 C113.12,43.09 112.26,60.30 111.64,75.95 L 110.50 104.41 L 115.50 110.20 C118.25,113.39 120.98,116.00 121.56,116.00 C122.15,116.00 123.80,114.76 125.23,113.25 Z)SVG",
        R"SVG(M 120.74 220.75 C123.04,218.28 123.61,211.25 126.08,155.61 L 127.18 130.72 L 124.19 127.72 L 121.19 124.73 L 115.85 130.46 C112.91,133.62 110.21,137.25 109.85,138.54 C109.50,139.83 108.70,153.17 108.09,168.19 C107.48,183.21 106.77,199.09 106.50,203.48 L 106.03 211.47 L 111.73 217.23 C114.87,220.41 117.71,223.00 118.04,223.00 C118.38,223.00 119.59,221.99 120.74,220.75 Z)SVG",
        R"SVG(M 108.51 232.55 C110.98,230.52 113.00,228.26 113.00,227.51 C113.00,226.77 110.86,224.32 108.25,222.08 L 103.50 218.01 L 61.96 218.01 L 20.42 218.00 L 15.51 222.99 L 10.59 227.98 L 13.94 231.44 C15.79,233.34 18.24,235.03 19.40,235.20 C20.55,235.36 40.07,235.66 62.76,235.86 L 104.02 236.23 L 108.51 232.55 Z)SVG",
        R"SVG(M 18.20 209.61 C18.59,207.90 19.47,190.62 20.15,171.21 L 21.40 135.92 L 15.65 130.21 L 9.90 124.50 L 7.46 127.30 C6.11,128.84 4.81,131.09 4.57,132.30 C4.05,134.87 0.96,203.76 0.99,212.30 C1.00,217.15 1.41,218.50 3.53,220.62 L 6.06 223.15 L 11.78 217.93 C15.43,214.61 17.75,211.59 18.20,209.61 Z)SVG",
        R"SVG(M 17.02 110.48 L 22.46 104.97 L 23.67 77.73 C24.33,62.76 24.89,45.33 24.92,39.00 L 24.98 27.50 L 19.89 22.25 C17.09,19.36 14.48,17.00 14.08,17.00 C12.23,17.00 9.05,21.91 8.55,25.51 C8.25,27.71 7.55,41.20 6.99,55.50 C6.44,69.80 5.71,88.03 5.37,96.00 C4.78,109.90 4.85,110.61 6.95,113.25 C8.16,114.76 9.69,116.00 10.36,116.00 C11.02,116.00 14.02,113.52 17.02,110.48 Z)SVG",
        R"SVG(M 112.19 125.72 C114.22,124.09 116.12,122.13 116.42,121.36 C116.71,120.59 115.34,118.17 113.36,115.98 L 109.77 112.00 L 67.03 112.00 L 24.29 112.00 L 19.64 115.55 C17.09,117.50 15.00,119.78 15.00,120.63 C15.00,121.48 16.61,123.59 18.57,125.34 L 22.15 128.50 L 61.82 129.00 C83.65,129.27 103.07,129.31 105.00,129.09 C106.93,128.86 110.16,127.34 112.19,125.72 Z)SVG",
        R"SVG(M 148.25 231.99 C154.25,225.40 150.00,215.00 141.30,215.00 C130.37,215.00 126.42,229.39 136.00,234.29 C140.56,236.62 144.74,235.84 148.25,231.99 Z)SVG"
    };
    static const std::array<QPainterPath, 8> kPaths = [] {
        std::array<QPainterPath, 8> out;
        for (int i = 0; i < static_cast<int>(out.size()); ++i) {
            out[i] = parseSvgPathDataAbsolute(kSeg7PathData[i]);
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

void SevenSegmentDisplayItem::setSegmentOnColor(const QColor& color) {
    if (!color.isValid()) return;
    m_segmentOnColor = color;
    update();
}

void SevenSegmentDisplayItem::setSegmentOffColor(const QColor& color) {
    if (!color.isValid()) return;
    m_segmentOffColor = color;
    update();
}

void SevenSegmentDisplayItem::setBodyColor(const QColor& color) {
    if (!color.isValid()) return;
    m_bodyColor = color;
    update();
}

void SevenSegmentDisplayItem::setBezelColor(const QColor& color) {
    if (!color.isValid()) return;
    m_bezelColor = color;
    update();
}

void SevenSegmentDisplayItem::setGlowStrength(double value) {
    m_glowStrength = std::clamp(value, 0.0, 2.0);
    update();
}

void SevenSegmentDisplayItem::setFitScale(double value) {
    m_fitScale = std::clamp(value, 0.7, 1.25);
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
    const auto& segPaths = exactSeg7Paths();
    const QRectF srcBounds = unitedPathBounds(segPaths.data(), static_cast<int>(segPaths.size()));
    const QRectF fitZone = fitRectPreserveAspect(srcBounds.size(), bezel.adjusted(4, 4, -4, -4));
    const QSizeF scaled(fitZone.width() * m_fitScale, fitZone.height() * m_fitScale);
    const QRectF exactRect(fitZone.center().x() - scaled.width() * 0.5,
                           fitZone.center().y() - scaled.height() * 0.5,
                           scaled.width(), scaled.height());
    QTransform xf;
    xf.translate(exactRect.left(), exactRect.top());
    xf.scale(exactRect.width() / srcBounds.width(), exactRect.height() / srcBounds.height());
    xf.translate(-srcBounds.left(), -srcBounds.top());
    for (int i = 0; i < static_cast<int>(segPaths.size()); ++i) {
        const QColor on = segmentColorForDrive(m_segmentDrive.value(baseSegmentIndex + i, 0.0), m_segmentOnColor).lighter(130);
        const QColor off = m_segmentOffColor;
        painter->setPen(QPen(QColor(25, 25, 25), 0.8));
        if (m_glowStrength > 0.01 && m_segmentDrive.value(baseSegmentIndex + i, 0.0) > 0.01) {
            const QRectF r = xf.map(segPaths[i]).controlPointRect();
            const qreal radius = std::max(r.width(), r.height()) * (0.45 + 0.25 * m_glowStrength);
            QRadialGradient glow(r.center(), radius);
            QColor glowColor = on;
            glowColor.setAlphaF(std::clamp(0.18 * m_glowStrength, 0.0, 0.5));
            glow.setColorAt(0.0, glowColor);
            glow.setColorAt(1.0, QColor(0, 0, 0, 0));
            painter->setPen(Qt::NoPen);
            painter->setBrush(glow);
            painter->drawEllipse(r.center(), radius, radius);
            painter->setPen(QPen(QColor(25, 25, 25), 0.8));
        }
        painter->setBrush(m_segmentDrive.value(baseSegmentIndex + i, 0.0) > 0.01 ? on : off);
        painter->drawPath(xf.map(segPaths[i]));
    }
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
    if (m_variant == Variant::Seg16) {
        const auto& segPaths = exactSeg16Paths();
        const QRectF srcBounds = unitedPathBounds(segPaths.data(), static_cast<int>(segPaths.size()));
        const QRectF fitZone = fitRectPreserveAspect(srcBounds.size(), bezel.adjusted(2, 2, -2, -2));
        const QSizeF scaled(fitZone.width() * m_fitScale, fitZone.height() * m_fitScale);
        const QRectF exactRect(fitZone.center().x() - scaled.width() * 0.5,
                               fitZone.center().y() - scaled.height() * 0.5,
                               scaled.width(), scaled.height());
        QTransform xf;
        xf.translate(exactRect.left(), exactRect.top());
        xf.scale(exactRect.width() / srcBounds.width(), exactRect.height() / srcBounds.height());
        xf.translate(-srcBounds.left(), -srcBounds.top());
        for (int idx = 0; idx < static_cast<int>(segPaths.size()); ++idx) {
            const bool on = segmentOn(idx);
            const QColor onColor = segmentColorForDrive(m_segmentDrive.value(idx, 0.0), m_segmentOnColor).lighter(125);
            const QColor offColor = m_segmentOffColor;
            painter->setPen(QPen(QColor(24, 24, 24), 0.8));
            if (m_glowStrength > 0.01 && on) {
                const QRectF r = xf.map(segPaths[idx]).controlPointRect();
                const qreal radius = std::max(r.width(), r.height()) * (0.42 + 0.22 * m_glowStrength);
                QRadialGradient glow(r.center(), radius);
                QColor gc = onColor;
                gc.setAlphaF(std::clamp(0.16 * m_glowStrength, 0.0, 0.45));
                glow.setColorAt(0.0, gc);
                glow.setColorAt(1.0, QColor(0, 0, 0, 0));
                painter->setPen(Qt::NoPen);
                painter->setBrush(glow);
                painter->drawEllipse(r.center(), radius, radius);
                painter->setPen(QPen(QColor(24, 24, 24), 0.8));
            }
            painter->setBrush(on ? onColor : offColor);
            painter->drawPath(xf.map(segPaths[idx]));
        }
        return;
    }

    const auto& segPaths = exactSeg14Paths();
    const QRectF srcBounds = unitedPathBounds(segPaths.data(), static_cast<int>(segPaths.size()));
    const QRectF fitZone = fitRectPreserveAspect(srcBounds.size(), bezel.adjusted(8, 8, -8, -8));
    const QSizeF scaled(fitZone.width() * m_fitScale, fitZone.height() * m_fitScale);
    const QRectF exactRect(fitZone.center().x() - scaled.width() * 0.5,
                           fitZone.center().y() - scaled.height() * 0.5,
                           scaled.width(), scaled.height());
    QTransform xf;
    xf.translate(exactRect.left(), exactRect.top());
    xf.scale(exactRect.width() / srcBounds.width(), exactRect.height() / srcBounds.height());
    xf.translate(-srcBounds.left(), -srcBounds.top());
    for (int idx = 0; idx < static_cast<int>(segPaths.size()); ++idx) {
        const bool on = segmentOn(idx);
        const QColor onColor = segmentColorForDrive(m_segmentDrive.value(idx, 0.0), m_segmentOnColor).lighter(125);
        const QColor offColor = m_segmentOffColor;
        painter->setPen(QPen(QColor(24, 24, 24), 0.8));
        if (m_glowStrength > 0.01 && on) {
            const QRectF r = xf.map(segPaths[idx]).controlPointRect();
            const qreal radius = std::max(r.width(), r.height()) * (0.42 + 0.22 * m_glowStrength);
            QRadialGradient glow(r.center(), radius);
            QColor gc = onColor;
            gc.setAlphaF(std::clamp(0.16 * m_glowStrength, 0.0, 0.45));
            glow.setColorAt(0.0, gc);
            glow.setColorAt(1.0, QColor(0, 0, 0, 0));
            painter->setPen(Qt::NoPen);
            painter->setBrush(glow);
            painter->drawEllipse(r.center(), radius, radius);
            painter->setPen(QPen(QColor(24, 24, 24), 0.8));
        }
        painter->setBrush(on ? onColor : offColor);
        painter->drawPath(xf.map(segPaths[idx]));
    }
}

void SevenSegmentDisplayItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);

    const QRectF bounds = boundingRect();
    const qreal bodyMargin = 8.0;
    QRectF body = bounds.adjusted(bodyMargin, bodyMargin, -bodyMargin, -bodyMargin);
    painter->setPen(QPen(isSelected() ? QColor("#55aaff") : QColor("#d9d9d9"), 2.0));
    painter->setBrush(m_bodyColor);
    painter->drawRoundedRect(body, 8, 8);

    QRectF bezel = body.adjusted(22, 24, -22, -24);
    if (m_variant == Variant::Dual7) {
        bezel = body.adjusted(18, 24, -18, -24);
    }
    painter->setPen(QPen(QColor("#aab0bb"), 1.0));
    painter->setBrush(m_bezelColor);
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
    j["segmentOnColor"] = m_segmentOnColor.name(QColor::HexArgb);
    j["segmentOffColor"] = m_segmentOffColor.name(QColor::HexArgb);
    j["bodyColor"] = m_bodyColor.name(QColor::HexArgb);
    j["bezelColor"] = m_bezelColor.name(QColor::HexArgb);
    j["glowStrength"] = m_glowStrength;
    j["fitScale"] = m_fitScale;
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
    if (json.contains("segmentOnColor")) {
        const QColor color(json["segmentOnColor"].toString());
        if (color.isValid()) m_segmentOnColor = color;
    }
    if (json.contains("segmentOffColor")) {
        const QColor color(json["segmentOffColor"].toString());
        if (color.isValid()) m_segmentOffColor = color;
    }
    if (json.contains("bodyColor")) {
        const QColor color(json["bodyColor"].toString());
        if (color.isValid()) m_bodyColor = color;
    }
    if (json.contains("bezelColor")) {
        const QColor color(json["bezelColor"].toString());
        if (color.isValid()) m_bezelColor = color;
    }
    if (json.contains("glowStrength")) {
        m_glowStrength = std::clamp(json["glowStrength"].toDouble(0.0), 0.0, 2.0);
    }
    if (json.contains("fitScale")) {
        m_fitScale = std::clamp(json["fitScale"].toDouble(1.0), 0.7, 1.25);
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
