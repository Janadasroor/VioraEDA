#ifndef SEVENSEGMENTDISPLAYITEM_H
#define SEVENSEGMENTDISPLAYITEM_H

#include "schematic_item.h"

class SevenSegmentDisplayItem : public SchematicItem {
public:
    enum class CommonType {
        CommonCathode = 0,
        CommonAnode = 1
    };

    explicit SevenSegmentDisplayItem(QPointF pos = QPointF(), QGraphicsItem* parent = nullptr);

    QString itemTypeName() const override { return "7-Segment Display"; }
    ItemType itemType() const override { return SchematicItem::ComponentType; }
    QString referencePrefix() const override { return "DS"; }
    void setValue(const QString& value) override;

    void setSimState(const QMap<QString, double>& nodeVoltages, const QMap<QString, double>& currents) override;

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    QList<QPointF> connectionPoints() const override;

    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject& json) override;
    SchematicItem* clone() const override;

    CommonType commonType() const { return m_commonType; }
    void setCommonType(CommonType type);
    double thresholdVoltage() const { return m_thresholdV; }
    void setThresholdVoltage(double volts);

private:
    static constexpr int kSegmentCount = 8; // a,b,c,d,e,f,g,dp
    bool segmentOn(int idx) const;

    CommonType m_commonType = CommonType::CommonCathode;
    double m_thresholdV = 1.7;
    double m_segmentDrive[kSegmentCount] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
};

#endif // SEVENSEGMENTDISPLAYITEM_H
