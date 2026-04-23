#ifndef SEVENSEGMENTDISPLAYITEM_H
#define SEVENSEGMENTDISPLAYITEM_H

#include "schematic_item.h"
#include <QVector>
#include <QStringList>

class SevenSegmentDisplayItem : public SchematicItem {
public:
    enum class CommonType {
        CommonCathode = 0,
        CommonAnode = 1
    };

    enum class Variant {
        Single7 = 0,
        Dual7 = 1,
        Seg14 = 2,
        Seg16 = 3
    };

    explicit SevenSegmentDisplayItem(Variant variant = Variant::Single7,
                                     QPointF pos = QPointF(),
                                     const QString& typeName = QString(),
                                     QGraphicsItem* parent = nullptr);

    QString itemTypeName() const override { return m_typeName; }
    ItemType itemType() const override { return SchematicItem::ComponentType; }
    QString referencePrefix() const override { return "DS"; }
    void setValue(const QString& value) override;
    QString pinName(int index) const override;

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
    Variant variant() const { return m_variant; }
    void setVariant(Variant variant);
    static bool isSegmentDisplayTypeName(const QString& typeName);

private:
    bool segmentOn(int idx) const;
    QString displayHeader() const;
    QStringList segmentPinLabels() const;
    QStringList allPinLabels() const;
    int segmentCount() const;
    int comPin1Index() const;
    int comPin2Index() const;
    void ensureDriveSize();
    void paintClassic7Digit(QPainter* painter, const QRectF& bezel, int baseSegmentIndex) const;
    void paintBarArrayDigit(QPainter* painter, const QRectF& bezel, int baseSegmentIndex, int count) const;
    void paintInspired14Or16(QPainter* painter, const QRectF& bezel) const;
    static Variant variantFromTypeName(const QString& typeName);
    static QString typeNameForVariant(Variant variant);

    QString m_typeName;
    Variant m_variant = Variant::Single7;
    CommonType m_commonType = CommonType::CommonCathode;
    double m_thresholdV = 1.7;
    QVector<double> m_segmentDrive;
};

#endif // SEVENSEGMENTDISPLAYITEM_H
