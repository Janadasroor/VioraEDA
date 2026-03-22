#ifndef BEHAVIORAL_CURRENT_SOURCE_ITEM_H
#define BEHAVIORAL_CURRENT_SOURCE_ITEM_H

#include "schematic_item.h"
#include <QPainter>

class BehavioralCurrentSourceItem : public SchematicItem {
public:
    BehavioralCurrentSourceItem(QPointF pos = QPointF(), QGraphicsItem* parent = nullptr);

    QString itemTypeName() const override { return "Current_Source_Behavioral"; }
    ItemType itemType() const override { return SchematicItem::ComponentType; }
    QString referencePrefix() const override { return "B"; }

    void setValue(const QString& value) override;

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    QList<QPointF> connectionPoints() const override;

    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject& json) override;
    SchematicItem* clone() const override;
};

#endif // BEHAVIORAL_CURRENT_SOURCE_ITEM_H
