#include "seven_segment_display_item.h"

#include <QJsonObject>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace {
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

double lookupNodeVoltage(const QMap<QString, double>& nodeVoltages, const QString& rawNet) {
    const QString net = rawNet.trimmed();
    if (net.isEmpty()) return 0.0;

    if (nodeVoltages.contains(net)) return nodeVoltages.value(net, 0.0);
    const QString lower = net.toLower();
    if (nodeVoltages.contains(lower)) return nodeVoltages.value(lower, 0.0);
    const QString upper = net.toUpper();
    if (nodeVoltages.contains(upper)) return nodeVoltages.value(upper, 0.0);

    // Probe/runtime paths sometimes format names as V(net).
    if (net.startsWith("V(", Qt::CaseInsensitive) && net.endsWith(')') && net.size() > 3) {
        return lookupNodeVoltage(nodeVoltages, net.mid(2, net.size() - 3));
    }

    // Final fallback: case-insensitive scan.
    for (auto it = nodeVoltages.constBegin(); it != nodeVoltages.constEnd(); ++it) {
        if (it.key().compare(net, Qt::CaseInsensitive) == 0) return it.value();
    }
    return 0.0;
}
} // namespace

SevenSegmentDisplayItem::SevenSegmentDisplayItem(QPointF pos, QGraphicsItem* parent)
    : SchematicItem(parent) {
    setPos(pos);
    setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
    setReference("DS1");
    setValue("CC");
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
    return QRectF(-75, -95, 150, 190);
}

QList<QPointF> SevenSegmentDisplayItem::connectionPoints() const {
    // Left pins: a,b,c,d,e,f,g
    // Right pins: dp,com1,com2
    return {
        QPointF(-75, -60), QPointF(-75, -40), QPointF(-75, -20), QPointF(-75, 0),
        QPointF(-75, 20), QPointF(-75, 40), QPointF(-75, 60),
        QPointF(75, -60), QPointF(75, 40), QPointF(75, 60)
    };
}

bool SevenSegmentDisplayItem::segmentOn(int idx) const {
    return idx >= 0 && idx < kSegmentCount && m_segmentDrive[idx] > 0.01;
}

void SevenSegmentDisplayItem::setSimState(const QMap<QString, double>& nodeVoltages, const QMap<QString, double>&) {
    auto pinNetName = [&](int pinIndex) -> QString {
        return pinNet(pinIndex).trimmed();
    };
    auto readPinVoltage = [&](int pinIndex) -> double {
        return lookupNodeVoltage(nodeVoltages, pinNetName(pinIndex));
    };

    const QString com1Net = pinNetName(8);
    const QString com2Net = pinNetName(9);
    const bool hasCom1 = !com1Net.isEmpty();
    const bool hasCom2 = !com2Net.isEmpty();

    double vCom = 0.0;
    bool hasCommonVoltage = false;
    if (hasCom1 && hasCom2) {
        const double v1 = readPinVoltage(8);
        const double v2 = readPinVoltage(9);
        // Physical 7-seg packages expose duplicated common pins; using min/max is
        // more robust than averaging when one common is left floating/noisy.
        vCom = (m_commonType == CommonType::CommonCathode) ? std::min(v1, v2) : std::max(v1, v2);
        hasCommonVoltage = true;
    } else if (hasCom1) {
        vCom = readPinVoltage(8);
        hasCommonVoltage = true;
    } else if (hasCom2) {
        vCom = readPinVoltage(9);
        hasCommonVoltage = true;
    }

    // If common is not wired/observable in current result vectors, still provide
    // useful visual feedback using an inferred rail.
    double inferredHigh = 0.0;
    for (int i = 0; i < kSegmentCount; ++i) {
        inferredHigh = std::max(inferredHigh, readPinVoltage(i));
    }

    for (int i = 0; i < kSegmentCount; ++i) {
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

void SevenSegmentDisplayItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);

    // Body
    QRectF body(-68, -88, 136, 176);
    painter->setPen(QPen(isSelected() ? QColor("#55aaff") : QColor("#d9d9d9"), 2.0));
    painter->setBrush(QColor("#2a2f3a"));
    painter->drawRoundedRect(body, 7, 7);

    // Bezel/screen
    QRectF bezel(-52, -72, 90, 144);
    painter->setPen(QPen(QColor("#aab0bb"), 1.0));
    painter->setBrush(QColor("#101319"));
    painter->drawRoundedRect(bezel, 5, 5);

    // Segment geometry
    const double sx = bezel.left() + 9.0;
    const double sy = bezel.top() + 9.0;
    const double hW = 44.0;
    const double hH = 10.0;
    const double vW = 10.0;
    const double vH = 44.0;
    const double cut = 3.0;

    const QPolygonF segA = segPolyH(sx + 10, sy + 0, hW, hH, cut);
    const QPolygonF segB = segPolyV(sx + 56, sy + 8, vW, vH, cut);
    const QPolygonF segC = segPolyV(sx + 56, sy + 58, vW, vH, cut);
    const QPolygonF segD = segPolyH(sx + 10, sy + 108, hW, hH, cut);
    const QPolygonF segE = segPolyV(sx + 0, sy + 58, vW, vH, cut);
    const QPolygonF segF = segPolyV(sx + 0, sy + 8, vW, vH, cut);
    const QPolygonF segG = segPolyH(sx + 10, sy + 54, hW, hH, cut);
    const QPointF dpCenter(sx + 73, sy + 112);

    const QPolygonF segments[kSegmentCount] = {segA, segB, segC, segD, segE, segF, segG, QPolygonF()};
    for (int i = 0; i < 7; ++i) {
        const QColor on = segmentColorForDrive(m_segmentDrive[i]).lighter(130);
        const QColor off(65, 26, 26);
        painter->setPen(QPen(QColor(25, 25, 25), 0.8));
        painter->setBrush(segmentOn(i) ? on : off);
        painter->drawPolygon(segments[i]);
    }

    const QColor onDp = segmentColorForDrive(m_segmentDrive[7]).lighter(130);
    const QColor offDp(65, 26, 26);
    painter->setPen(QPen(QColor(25, 25, 25), 0.8));
    painter->setBrush(segmentOn(7) ? onDp : offDp);
    painter->drawEllipse(dpCenter, 4.3, 4.3);

    // Pin labels and pins
    painter->setFont(QFont("Inter", 7));
    painter->setPen(QColor("#e6e8eb"));
    const char* leftLbl[7] = {"a", "b", "c", "d", "e", "f", "g"};
    const int leftY[7] = {-60, -40, -20, 0, 20, 40, 60};
    for (int i = 0; i < 7; ++i) {
        painter->drawLine(-75, leftY[i], -62, leftY[i]);
        painter->drawText(QRectF(-58, leftY[i] - 7, 16, 14), Qt::AlignLeft | Qt::AlignVCenter, leftLbl[i]);
    }

    painter->drawLine(62, -60, 75, -60);
    painter->drawLine(62, 40, 75, 40);
    painter->drawLine(62, 60, 75, 60);
    painter->drawText(QRectF(41, -67, 20, 14), Qt::AlignRight | Qt::AlignVCenter, "dp");
    painter->drawText(QRectF(35, 33, 26, 14), Qt::AlignRight | Qt::AlignVCenter, "com1");
    painter->drawText(QRectF(35, 53, 26, 14), Qt::AlignRight | Qt::AlignVCenter, "com2");

    // Common polarity marker for quick visual distinction.
    painter->setPen(QPen(QColor("#d6d9de"), 1.0));
    painter->setBrush(Qt::NoBrush);
    if (m_commonType == CommonType::CommonAnode) {
        painter->drawEllipse(QPointF(59, 40), 2.1, 2.1);
        painter->drawEllipse(QPointF(59, 60), 2.1, 2.1);
    } else {
        painter->drawLine(56, 40, 61, 40);
        painter->drawLine(56, 60, 61, 60);
    }

    painter->setPen(QColor("#9aa0aa"));
    painter->drawText(QRectF(-66, -87, 132, 14), Qt::AlignCenter,
                      m_commonType == CommonType::CommonCathode ? "7-SEG (CC)" : "7-SEG (CA)");

    drawConnectionPointHighlights(painter);
}

QJsonObject SevenSegmentDisplayItem::toJson() const {
    QJsonObject j = SchematicItem::toJson();
    j["type"] = "7-Segment Display";
    j["commonType"] = static_cast<int>(m_commonType);
    j["threshold"] = m_thresholdV;
    return j;
}

bool SevenSegmentDisplayItem::fromJson(const QJsonObject& json) {
    if (!SchematicItem::fromJson(json)) return false;
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
    auto* item = new SevenSegmentDisplayItem(pos());
    item->fromJson(this->toJson());
    return item;
}
