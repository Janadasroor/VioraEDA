#include "seven_segment_display_item.h"

#include <QJsonObject>
#include <QPainter>

#include <algorithm>
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

    // Keep 14/16 silhouette close to classic 7-seg proportions.
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

    if (m_variant == Variant::Seg16) {
        // a1 a2 b c d1 d2 e f g1 g2 h i j k l m
        drawSegPoly(0, aL, mapPt(0.48, 0.09), segWMain);
        drawSegPoly(1, mapPt(0.52, 0.09), aR, segWMain);
        drawSegPoly(2, bT, bM, segWMain);
        drawSegPoly(3, cM, cB, segWMain);
        drawSegPoly(4, dL, mapPt(0.48, 0.91), segWMain);
        drawSegPoly(5, mapPt(0.52, 0.91), dR, segWMain);
        drawSegPoly(6, eM, eB, segWMain);
        drawSegPoly(7, fT, fM, segWMain);
        drawSegPoly(8, g1L, g1R, segWCenter);
        drawSegPoly(9, g2L, g2R, segWCenter);
        drawSegPoly(10, h0, h1, segWDiag);
        drawSegPoly(11, i0, i1, segWDiag);
        drawSegPoly(12, j0, j1, segWDiag);
        drawSegPoly(13, k0, k1, segWDiag);
        drawSegPoly(14, lT, lB, segWCenter);
        drawSegPoly(15, mT, mB, segWCenter);

        const bool dpOn = segmentOn(16);
        painter->setPen(QPen(QColor(24, 24, 24), 0.8));
        painter->setBrush(dpOn ? segmentColorForDrive(m_segmentDrive.value(16, 0.0)).lighter(125)
                               : QColor(86, 34, 34));
        painter->drawEllipse(mapPt(0.93, 0.93), segWMain * 0.34, segWMain * 0.34);
        return;
    }

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
