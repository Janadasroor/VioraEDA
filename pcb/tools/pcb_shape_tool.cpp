#include "pcb_shape_tool.h"

#include "pcb_view.h"
#include "pcb_commands.h"
#include "pcb_layer.h"
#include "../items/shape_item.h"

#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPen>
#include <QUndoStack>
#include <QtMath>

namespace {
PCBShapeItem::ShapeKind toItemKind(PCBShapeTool::ShapeKind kind) {
    switch (kind) {
    case PCBShapeTool::ShapeKind::Circle: return PCBShapeItem::ShapeKind::Circle;
    case PCBShapeTool::ShapeKind::Arc: return PCBShapeItem::ShapeKind::Arc;
    case PCBShapeTool::ShapeKind::Line:
    default:
        return PCBShapeItem::ShapeKind::Line;
    }
}

QRectF shapeRectFromPoints(PCBShapeTool::ShapeKind kind, const QPointF& origin, const QPointF& pos) {
    if (kind == PCBShapeTool::ShapeKind::Circle) {
        const qreal radius = std::max<qreal>(0.1, QLineF(origin, pos).length());
        return QRectF(origin.x() - radius, origin.y() - radius, radius * 2.0, radius * 2.0);
    }

    QRectF rect(origin, pos);
    rect = rect.normalized();
    rect.setWidth(std::max<qreal>(0.2, rect.width()));
    rect.setHeight(std::max<qreal>(0.2, rect.height()));
    rect.moveCenter(QRectF(origin, pos).center());
    return rect;
}
}

PCBShapeTool::PCBShapeTool(const QString& toolName, ShapeKind kind, QObject* parent)
    : PCBTool(toolName, parent)
    , m_shapeKind(kind)
    , m_layerId(PCBLayerManager::instance().activeLayerId()) {
}

QString PCBShapeTool::tooltip() const {
    return QString("Place %1 shape").arg(name().toLower());
}

QMap<QString, QVariant> PCBShapeTool::toolProperties() const {
    QMap<QString, QVariant> props;
    props["Active Layer"] = m_layerId;
    props["Stroke Width (mm)"] = m_strokeWidth;
    if (m_shapeKind == ShapeKind::Arc) {
        props["Arc Start Angle (deg)"] = m_arcStartAngleDeg;
        props["Arc Span Angle (deg)"] = m_arcSpanAngleDeg;
    }
    return props;
}

void PCBShapeTool::setToolProperty(const QString& name, const QVariant& value) {
    if (name == "Active Layer") {
        m_layerId = value.toInt();
    } else if (name == "Stroke Width (mm)") {
        m_strokeWidth = std::max(0.05, value.toDouble());
    } else if (name == "Arc Start Angle (deg)") {
        m_arcStartAngleDeg = value.toDouble();
    } else if (name == "Arc Span Angle (deg)") {
        m_arcSpanAngleDeg = value.toDouble();
    }
}

void PCBShapeTool::activate(PCBView* view) {
    PCBTool::activate(view);
    m_isDrawing = false;
    m_layerId = PCBLayerManager::instance().activeLayerId();
}

void PCBShapeTool::deactivate() {
    if (m_previewPath && view() && view()->scene()) {
        view()->scene()->removeItem(m_previewPath);
        delete m_previewPath;
    }
    m_previewPath = nullptr;
    clearMeasurementLabel();
    clearSnapIndicator();
    m_isDrawing = false;
    PCBTool::deactivate();
}

void PCBShapeTool::mousePressEvent(QMouseEvent* event) {
    if (!view()) return;

    if (event->button() == Qt::RightButton) {
        view()->setCurrentTool("Select");
        event->accept();
        return; // CRITICAL: Tool is now deactivated and view() is null!
    }

    if (event->button() != Qt::LeftButton) return;

    const QPointF pos = snapToMagneticEndpoint(view()->mapToScene(event->pos()));
    updateSnapIndicator(view()->mapToScene(event->pos()));
    if (!m_isDrawing) {
        m_isDrawing = true;
        m_origin = pos;

        if (!m_previewPath) {
            m_previewPath = new QGraphicsPathItem();
            QColor color = QColor(200, 50, 50, 120);
            if (PCBLayer* layer = PCBLayerManager::instance().layer(m_layerId)) {
                color = layer->color();
                color.setAlpha(140);
            }
            m_previewPath->setPen(QPen(color, 0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            m_previewPath->setZValue(1000);
            view()->scene()->addItem(m_previewPath);
        }

        if (m_shapeKind == ShapeKind::Line && !m_measureLabel && view()->scene()) {
            m_measureLabel = new QGraphicsSimpleTextItem();
            m_measureLabel->setBrush(Qt::white);
            m_measureLabel->setZValue(1001);
            view()->scene()->addItem(m_measureLabel);
        }

        updatePreview(pos);
    } else {
        finishShape(pos);
    }
    event->accept();
}

void PCBShapeTool::mouseMoveEvent(QMouseEvent* event) {
    if (!view()) return;

    const QPointF rawPos = view()->mapToScene(event->pos());
    updateSnapIndicator(rawPos);
    if (m_isDrawing) {
        updatePreview(snapToMagneticEndpoint(rawPos));
    }
    event->accept();
}

void PCBShapeTool::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event)
}

void PCBShapeTool::updatePreview(const QPointF& pos) {
    if (!m_previewPath) return;

    QPainterPath path;
    switch (m_shapeKind) {
    case ShapeKind::Line:
        path.moveTo(m_origin);
        path.lineTo(pos);
        break;
    case ShapeKind::Circle: {
        const QRectF rect = shapeRectFromPoints(m_shapeKind, m_origin, pos);
        path.addEllipse(rect);
        break;
    }
    case ShapeKind::Arc: {
        const QRectF rect = shapeRectFromPoints(m_shapeKind, m_origin, pos);
        path.arcMoveTo(rect, m_arcStartAngleDeg);
        path.arcTo(rect, m_arcStartAngleDeg, m_arcSpanAngleDeg);
        break;
    }
    }
    m_previewPath->setPath(path);

    if (m_shapeKind == ShapeKind::Line) {
        updateMeasurementLabel(pos);
    }
}

void PCBShapeTool::finishShape(const QPointF& pos) {
    if (!view() || !view()->scene()) return;

    if (m_shapeKind == ShapeKind::Line) {
        const QLineF line(m_origin, pos);
        const qreal length = line.length();
        if (length < 0.2) {
            return;
        }

        PCBShapeItem* item = new PCBShapeItem(toItemKind(m_shapeKind), QSizeF(length, std::max(0.2, m_strokeWidth)));
        item->setPos((m_origin + pos) * 0.5);
        item->setRotation(-line.angle());
        item->setLayer(m_layerId);
        item->setStrokeWidth(m_strokeWidth);

        if (view()->undoStack()) {
            view()->undoStack()->push(new PCBAddItemCommand(view()->scene(), item));
        } else {
            view()->scene()->addItem(item);
        }

        m_isDrawing = true;
        m_origin = pos;
        updatePreview(pos);
        return;
    }

    const QRectF rect = shapeRectFromPoints(m_shapeKind, m_origin, pos);
    if (rect.width() < 0.2 || rect.height() < 0.2) {
        // Reset state for next shape
        m_isDrawing = false;
        if (m_previewPath) m_previewPath->hide();
        clearMeasurementLabel();
        return;
    }

    PCBShapeItem* item = new PCBShapeItem(toItemKind(m_shapeKind), rect.size());
    item->setPos(rect.center());
    item->setLayer(m_layerId);
    item->setStrokeWidth(m_strokeWidth);
    if (m_shapeKind == ShapeKind::Arc) {
        item->setStartAngleDeg(m_arcStartAngleDeg);
        item->setSpanAngleDeg(m_arcSpanAngleDeg);
    }

    if (view()->undoStack()) {
        view()->undoStack()->push(new PCBAddItemCommand(view()->scene(), item));
    } else {
        view()->scene()->addItem(item);
    }

    // Reset state for next shape without deactivating tool
    m_isDrawing = false;
    if (m_previewPath) {
        m_previewPath->hide();
    }
    clearMeasurementLabel();
}

void PCBShapeTool::updateMeasurementLabel(const QPointF& pos) {
    if (!m_measureLabel || !view()) {
        return;
    }

    const double dx = pos.x() - m_origin.x();
    const double dy = pos.y() - m_origin.y();
    const double length = std::hypot(dx, dy);
    const double angleDeg = qRadiansToDegrees(std::atan2(dy, dx));

    m_measureLabel->setText(QString("%1 mm\n%2°")
        .arg(length, 0, 'f', 3)
        .arg(angleDeg, 0, 'f', 2));

    const double viewScale = view()->transform().m11();
    if (viewScale > 0.0) {
        m_measureLabel->setScale(1.2 / viewScale);
    }

    m_measureLabel->setPos(pos + QPointF(6.0 / qMax(0.001, viewScale), 6.0 / qMax(0.001, viewScale)));
}

void PCBShapeTool::clearMeasurementLabel() {
    if (m_measureLabel && view() && view()->scene()) {
        view()->scene()->removeItem(m_measureLabel);
        delete m_measureLabel;
    }
    m_measureLabel = nullptr;
}

bool PCBShapeTool::findMagneticEndpoint(const QPointF& pos, QPointF* snappedPos) const {
    if (!view() || !view()->scene() || m_shapeKind != ShapeKind::Line) {
        return false;
    }

    constexpr qreal snapRadiusMm = 1.25;
    const QRectF searchRect(pos.x() - snapRadiusMm, pos.y() - snapRadiusMm,
                            snapRadiusMm * 2.0, snapRadiusMm * 2.0);

    QPointF bestPoint = pos;
    qreal bestDistance = snapRadiusMm;

    const auto considerCandidate = [&](const QPointF& candidate) {
        const qreal distance = QLineF(pos, candidate).length();
        if (distance < bestDistance) {
            bestDistance = distance;
            bestPoint = candidate;
        }
    };

    const QList<QGraphicsItem*> nearbyItems = view()->scene()->items(searchRect, Qt::IntersectsItemBoundingRect);
    for (QGraphicsItem* graphicsItem : nearbyItems) {
        if (graphicsItem == m_previewPath || graphicsItem == m_measureLabel || graphicsItem == m_snapIndicator) {
            continue;
        }

        if (auto* shapeItem = dynamic_cast<PCBShapeItem*>(graphicsItem)) {
            if (shapeItem->shapeKind() != PCBShapeItem::ShapeKind::Line) {
                continue;
            }

            const qreal halfLength = shapeItem->sizeMm().width() * 0.5;
            considerCandidate(shapeItem->mapToScene(QPointF(-halfLength, 0.0)));
            considerCandidate(shapeItem->mapToScene(QPointF(halfLength, 0.0)));
        }
    }

    if (bestDistance < snapRadiusMm) {
        if (snappedPos) {
            *snappedPos = bestPoint;
        }
        return true;
    }

    return false;
}

QPointF PCBShapeTool::snapToMagneticEndpoint(const QPointF& pos) const {
    QPointF snappedPos;
    if (findMagneticEndpoint(pos, &snappedPos)) {
        return snappedPos;
    }

    if (view()) {
        return view()->snapToGrid(pos);
    }

    return pos;
}

void PCBShapeTool::updateSnapIndicator(const QPointF& pos) {
    if (!view() || !view()->scene() || m_shapeKind != ShapeKind::Line) {
        clearSnapIndicator();
        return;
    }

    QPointF snappedPos;
    if (!findMagneticEndpoint(pos, &snappedPos)) {
        clearSnapIndicator();
        return;
    }

    if (!m_snapIndicator) {
        m_snapIndicator = new QGraphicsEllipseItem();
        m_snapIndicator->setPen(QPen(QColor(255, 220, 120), 0.0));
        m_snapIndicator->setBrush(Qt::NoBrush);
        m_snapIndicator->setZValue(1002);
        view()->scene()->addItem(m_snapIndicator);
    }

    const qreal viewScale = qMax(0.001, view()->transform().m11());
    const qreal radius = 5.0 / viewScale;
    m_snapIndicator->setRect(QRectF(snappedPos.x() - radius, snappedPos.y() - radius,
                                    radius * 2.0, radius * 2.0));
    m_snapIndicator->show();
}

void PCBShapeTool::clearSnapIndicator() {
    if (m_snapIndicator && view() && view()->scene()) {
        view()->scene()->removeItem(m_snapIndicator);
        delete m_snapIndicator;
    }
    m_snapIndicator = nullptr;
}
