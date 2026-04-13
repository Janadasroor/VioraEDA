#include "diode_item.h"
#include "schematic_text_item.h"
#include "theme_manager.h"
#include <QPainter>
#include <QJsonObject>

DiodeItem::DiodeItem(QPointF pos, QString value, QGraphicsItem *parent)
    : SchematicItem(parent), m_value(value) {
    setPos(pos);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);

    PCBTheme* theme = ThemeManager::theme();
    m_pen = QPen(theme->schematicLine(), 2);
    m_brush = QBrush(theme->schematicComponent());
    
    buildPrimitives();
    createLabels(QPointF(-15, -37.5), QPointF(-22.5, 37.5));
}

void DiodeItem::buildPrimitives() {
    m_primitives.clear();
    
    // Leads
    m_primitives.push_back(std::make_unique<LinePrimitive>(QPointF(-45, 0), QPointF(-15, 0)));
    m_primitives.push_back(std::make_unique<LinePrimitive>(QPointF(15, 0), QPointF(45, 0)));
    
    // Triangle (Anode -> Cathode)
    QList<QPointF> triangle;
    triangle << QPointF(-15, -22.5) << QPointF(-15, 22.5) << QPointF(15, 0);
    m_primitives.push_back(std::make_unique<PolygonPrimitive>(triangle, false));
    
    // Cathode bar
    m_primitives.push_back(std::make_unique<LinePrimitive>(QPointF(15, -22.5), QPointF(15, 22.5)));
    
    // Pins
    m_primitives.push_back(std::make_unique<CirclePrimitive>(QPointF(-45, 0), 3.75, true));
    m_primitives.push_back(std::make_unique<CirclePrimitive>(QPointF(45, 0), 3.75, true));
}

void DiodeItem::setValue(const QString& value) {
    if (m_value != value) {
        m_value = value;
        m_spiceModel = value.trimmed();
        updateLabelText();
        buildPrimitives();
        update();
    }
}

QRectF DiodeItem::boundingRect() const {
    QRectF rect;
    for (const auto& prim : m_primitives) {
        rect = rect.united(prim->boundingRect());
    }
    return rect.adjusted(-5, -5, 5, 5);
}

void DiodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)
    for (const auto& prim : m_primitives) {
        prim->paint(painter, m_pen, m_brush);
    }
    
    // Draw highlighted connection points
    drawConnectionPointHighlights(painter);
    
    if (isSelected()) {
        PCBTheme* theme = ThemeManager::theme();
        painter->setPen(QPen(theme->selectionBox(), 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(boundingRect().adjusted(2, 2, -2, -2));
    }
}

QJsonObject DiodeItem::toJson() const {
    QJsonObject json;
    json["type"] = itemTypeName();
    json["id"] = id().toString();
    json["name"] = name();
    json["value"] = m_value;
    json["reference"] = reference();
    json["footprint"] = footprint();
    json["spiceModel"] = m_spiceModel;
    json["excludeFromSim"] = m_excludeFromSimulation;
    json["excludeFromPcb"] = m_excludeFromPcb;
    json["isLocked"] = m_isLocked;
    json["isMirroredX"] = m_isMirroredX;
    json["isMirroredY"] = m_isMirroredY;
    json["rotation"] = rotation();

    QJsonObject exprs;
    for (auto it = m_paramExpressions.begin(); it != m_paramExpressions.end(); ++it) exprs[it.key()] = it.value();
    json["paramExpressions"] = exprs;

    QJsonObject tols;
    for (auto it = m_tolerances.begin(); it != m_tolerances.end(); ++it) tols[it.key()] = it.value();
    json["tolerances"] = tols;

    json["x"] = pos().x();
    json["y"] = pos().y();

    if (m_refLabelItem) {
        json["refX"] = m_refLabelItem->pos().x();
        json["refY"] = m_refLabelItem->pos().y();
    }
    if (m_valueLabelItem) {
        json["valX"] = m_valueLabelItem->pos().x();
        json["valY"] = m_valueLabelItem->pos().y();
    }
    json["pinPadMapping"] = pinPadMappingToJson();
    return json;
}

bool DiodeItem::fromJson(const QJsonObject& json) {
    if (json["type"].toString() != itemTypeName()) return false;

    if (json.contains("id")) setId(QUuid(json["id"].toString()));
    setName(json["name"].toString());
    m_value = json["value"].toString();
    setReference(json["reference"].toString());
    setFootprint(json["footprint"].toString());
    if (json.contains("spiceModel")) m_spiceModel = json["spiceModel"].toString();
    if (json.contains("excludeFromSim")) m_excludeFromSimulation = json["excludeFromSim"].toBool(false);
    if (json.contains("excludeFromPcb")) m_excludeFromPcb = json["excludeFromPcb"].toBool(false);
    if (json.contains("isLocked")) m_isLocked = json["isLocked"].toBool(false);
    if (json.contains("isMirroredX")) m_isMirroredX = json["isMirroredX"].toBool(false);
    if (json.contains("isMirroredY")) m_isMirroredY = json["isMirroredY"].toBool(false);
    if (json.contains("rotation")) setRotation(json["rotation"].toDouble());

    m_paramExpressions.clear();
    if (json.contains("paramExpressions")) {
        QJsonObject exprs = json["paramExpressions"].toObject();
        for (auto it = exprs.begin(); it != exprs.end(); ++it) m_paramExpressions[it.key()] = it.value().toString();
    }

    m_tolerances.clear();
    if (json.contains("tolerances")) {
        QJsonObject tols = json["tolerances"].toObject();
        for (auto it = tols.begin(); it != tols.end(); ++it) m_tolerances[it.key()] = it.value().toString();
    }

    loadPinPadMappingFromJson(json);
    setPos(QPointF(json["x"].toDouble(), json["y"].toDouble()));
    buildPrimitives();
    createLabels(QPointF(-15, -37.5), QPointF(-22.5, 37.5));
    if (json.contains("refX")) {
        setReferenceLabelPos(QPointF(json["refX"].toDouble(), json["refY"].toDouble()));
    }
    if (json.contains("valX")) {
        setValueLabelPos(QPointF(json["valX"].toDouble(), json["valY"].toDouble()));
    }
    updateLabelText();
    update();
    return true;
}

SchematicItem* DiodeItem::clone() const {
    auto* newItem = new DiodeItem(pos(), m_value, parentItem());
    QJsonObject state = toJson();
    state["id"] = QUuid::createUuid().toString();
    newItem->fromJson(state);
    return newItem;
}

QList<QPointF> DiodeItem::connectionPoints() const {
    return { QPointF(-45, 0), QPointF(45, 0) };
}
