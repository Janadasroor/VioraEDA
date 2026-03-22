#include "behavioral_current_source_item.h"
#include "schematic_text_item.h"
#include "theme_manager.h"

BehavioralCurrentSourceItem::BehavioralCurrentSourceItem(QPointF pos, QGraphicsItem* parent)
    : SchematicItem(parent) {
    setPos(pos);
    setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
    setReference("B1");
    setValue("I=0");
    createLabels(QPointF(30, -15), QPointF(30, 15));
}

void BehavioralCurrentSourceItem::setValue(const QString& value) {
    QString v = value.trimmed();
    if (v.isEmpty()) v = "I=0";
    if (!v.startsWith("I=", Qt::CaseInsensitive)) v = "I=" + v;
    SchematicItem::setValue(v);
}

QRectF BehavioralCurrentSourceItem::boundingRect() const { return QRectF(-30, -50, 60, 100); }

void BehavioralCurrentSourceItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);
    PCBTheme* theme = ThemeManager::theme();
    painter->setPen(QPen(theme ? theme->schematicLine() : Qt::white, 2));
    painter->setBrush(Qt::NoBrush);

    // Leads
    painter->drawLine(0, -50, 0, -25);
    painter->drawLine(0, 25, 0, 50);

    // Circle body
    painter->drawEllipse(QPointF(0, 0), 20, 20);

    // Arrow (current source)
    painter->drawLine(0, 10, 0, -6);
    painter->drawLine(0, -6, -5, 0);
    painter->drawLine(0, -6, 5, 0);

    // Pin markers
    painter->drawEllipse(QPointF(0, -50), 4, 4);
    painter->drawEllipse(QPointF(0, 50), 4, 4);

    drawConnectionPointHighlights(painter);
}

QList<QPointF> BehavioralCurrentSourceItem::connectionPoints() const {
    return { QPointF(0, -50), QPointF(0, 50) };
}

QJsonObject BehavioralCurrentSourceItem::toJson() const {
    QJsonObject json = SchematicItem::toJson();
    json["type"] = itemTypeName();
    return json;
}

bool BehavioralCurrentSourceItem::fromJson(const QJsonObject& json) {
    SchematicItem::fromJson(json);
    setValue(value());
    return true;
}

SchematicItem* BehavioralCurrentSourceItem::clone() const {
    auto* item = new BehavioralCurrentSourceItem(pos());
    item->setValue(value());
    return item;
}
