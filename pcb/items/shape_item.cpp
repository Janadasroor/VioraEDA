#include "shape_item.h"

#include "theme_manager.h"
#include "../layers/pcb_layer.h"

#include <QPainter>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>

namespace {
QString kindToString(PCBShapeItem::ShapeKind kind) {
    switch (kind) {
    case PCBShapeItem::ShapeKind::Line: return "Line";
    case PCBShapeItem::ShapeKind::Circle: return "Circle";
    case PCBShapeItem::ShapeKind::Arc: return "Arc";
    }
    return "Line";
}

PCBShapeItem::ShapeKind kindFromString(const QString& kind) {
    if (kind.compare("Circle", Qt::CaseInsensitive) == 0) return PCBShapeItem::ShapeKind::Circle;
    if (kind.compare("Arc", Qt::CaseInsensitive) == 0) return PCBShapeItem::ShapeKind::Arc;
    return PCBShapeItem::ShapeKind::Line;
}
}

PCBShapeItem::PCBShapeItem(QGraphicsItem* parent)
    : PCBItem(parent) {
}

PCBShapeItem::PCBShapeItem(ShapeKind kind, const QSizeF& sizeMm, QGraphicsItem* parent)
    : PCBItem(parent)
    , m_kind(kind)
    , m_sizeMm(sizeMm) {
}

QRectF PCBShapeItem::localRect() const {
    return QRectF(-m_sizeMm.width() * 0.5, -m_sizeMm.height() * 0.5, m_sizeMm.width(), m_sizeMm.height());
}

QPainterPath PCBShapeItem::basePath() const {
    QPainterPath path;
    const QRectF rect = localRect();

    switch (m_kind) {
    case ShapeKind::Line:
        path.moveTo(QPointF(-m_sizeMm.width() * 0.5, 0.0));
        path.lineTo(QPointF(m_sizeMm.width() * 0.5, 0.0));
        break;
    case ShapeKind::Circle:
        path.addEllipse(rect);
        break;
    case ShapeKind::Arc:
        path.arcMoveTo(rect, m_startAngleDeg);
        path.arcTo(rect, m_startAngleDeg, m_spanAngleDeg);
        break;
    }

    return path;
}

QRectF PCBShapeItem::boundingRect() const {
    const qreal pad = std::max<qreal>(0.6, m_strokeWidth) + 0.75;
    return localRect().adjusted(-pad, -pad, pad, pad);
}

QPainterPath PCBShapeItem::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(std::max(0.2, m_strokeWidth));
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    return stroker.createStroke(basePath());
}

void PCBShapeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    Q_UNUSED(widget)

    painter->setRenderHint(QPainter::Antialiasing, true);

    PCBTheme* theme = ThemeManager::theme();
    PCBLayer* layerData = PCBLayerManager::instance().layer(layer());
    QColor color = layerData ? layerData->color() : (theme ? theme->trace() : QColor(220, 120, 60));

    painter->setPen(QPen(color, std::max(0.12, m_strokeWidth), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(basePath());

    if (option->state & QStyle::State_Selected) {
        drawSelectionGlow(painter);
    }
}

QJsonObject PCBShapeItem::toJson() const {
    QJsonObject json = PCBItem::toJson();
    json["type"] = itemTypeName();
    json["shapeKind"] = kindToString(m_kind);
    json["widthMm"] = m_sizeMm.width();
    json["heightMm"] = m_sizeMm.height();
    json["strokeWidth"] = m_strokeWidth;
    json["startAngleDeg"] = m_startAngleDeg;
    json["spanAngleDeg"] = m_spanAngleDeg;
    return json;
}

bool PCBShapeItem::fromJson(const QJsonObject& json) {
    PCBItem::fromJson(json);
    m_kind = kindFromString(json.value("shapeKind").toString());
    m_sizeMm.setWidth(std::max(0.2, json.value("widthMm").toDouble(m_sizeMm.width())));
    m_sizeMm.setHeight(std::max(0.2, json.value("heightMm").toDouble(m_sizeMm.height())));
    m_strokeWidth = std::max(0.05, json.value("strokeWidth").toDouble(m_strokeWidth));
    m_startAngleDeg = json.value("startAngleDeg").toDouble(m_startAngleDeg);
    m_spanAngleDeg = json.value("spanAngleDeg").toDouble(m_spanAngleDeg);
    update();
    return true;
}

PCBItem* PCBShapeItem::clone() const {
    PCBShapeItem* item = new PCBShapeItem(m_kind, m_sizeMm);
    item->fromJson(toJson());
    return item;
}

void PCBShapeItem::setShapeKind(ShapeKind kind) {
    if (m_kind == kind) return;
    prepareGeometryChange();
    m_kind = kind;
    update();
}

QString PCBShapeItem::shapeKindName() const {
    return kindToString(m_kind);
}

void PCBShapeItem::setSizeMm(const QSizeF& sizeMm) {
    const QSizeF clamped(std::max(0.2, sizeMm.width()), std::max(0.2, sizeMm.height()));
    if (m_sizeMm == clamped) return;
    prepareGeometryChange();
    m_sizeMm = clamped;
    update();
}

void PCBShapeItem::setStrokeWidth(double width) {
    const double clamped = std::max(0.05, width);
    if (qFuzzyCompare(m_strokeWidth + 1.0, clamped + 1.0)) return;
    prepareGeometryChange();
    m_strokeWidth = clamped;
    update();
}

void PCBShapeItem::setStartAngleDeg(double degrees) {
    if (qFuzzyCompare(m_startAngleDeg + 1.0, degrees + 1.0)) return;
    prepareGeometryChange();
    m_startAngleDeg = degrees;
    update();
}

void PCBShapeItem::setSpanAngleDeg(double degrees) {
    if (qFuzzyCompare(m_spanAngleDeg + 1.0, degrees + 1.0)) return;
    prepareGeometryChange();
    m_spanAngleDeg = degrees;
    update();
}
