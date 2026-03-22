#ifndef VOLTAGE_CONTROLLED_SWITCH_ITEM_H
#define VOLTAGE_CONTROLLED_SWITCH_ITEM_H

#include "schematic_item.h"
#include <QPainter>

class VoltageControlledSwitchItem : public SchematicItem {
public:
    VoltageControlledSwitchItem(QPointF pos = QPointF(), QGraphicsItem* parent = nullptr);

    QString itemTypeName() const override { return "Voltage Controlled Switch"; }
    ItemType itemType() const override { return SchematicItem::ComponentType; }
    QString referencePrefix() const override { return "S"; }

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QList<QPointF> connectionPoints() const override;

    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject& json) override;
    SchematicItem* clone() const override;

    QString modelName() const { return m_modelName; }
    void setModelName(const QString& name);

    QString ron() const { return m_ron; }
    void setRon(const QString& value);

    QString roff() const { return m_roff; }
    void setRoff(const QString& value);

    QString vt() const { return m_vt; }
    void setVt(const QString& value);

    QString vh() const { return m_vh; }
    void setVh(const QString& value);

private:
    void syncParamExpressions();
    void applyParamExpressions();

    QString m_modelName;
    QString m_ron;
    QString m_roff;
    QString m_vt;
    QString m_vh;
};

#endif // VOLTAGE_CONTROLLED_SWITCH_ITEM_H
