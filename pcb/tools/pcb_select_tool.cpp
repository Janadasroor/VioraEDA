#include "pcb_select_tool.h"
#include "pcb_view.h"
#include "pcb_item.h"
#include "pad_item.h"
#include "trace_item.h"
#include "copper_pour_item.h"
#include "image_item.h"
#include "shape_item.h"
#include "pcb_commands.h"
#include "../analysis/pcb_ratsnest_manager.h"
#include "theme_manager.h"
#include "../drc/pcb_drc.h"
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsPathItem>
#include <QGraphicsEllipseItem>
#include <QPainterPathStroker>
#include <QUndoStack>
#include <QMouseEvent>
#include <QPen>
#include <QBrush>
#include <QDebug>
#include <QSet>
#include <QMap>
#include <QMainWindow>
#include <QStatusBar>
#include <cmath>

namespace {
PCBItem* resolveSelectableTarget(QGraphicsItem* item) {
    PCBItem* target = nullptr;
    QGraphicsItem* current = item;
    while (current) {
        if (PCBItem* candidate = dynamic_cast<PCBItem*>(current)) {
            if (candidate->flags() & QGraphicsItem::ItemIsSelectable) {
                // Keep climbing so we prefer the highest selectable owner
                // (e.g. footprint/component instead of its pad child).
                target = candidate;
            }
        }
        current = current->parentItem();
    }
    return target;
}

PCBItem* hitSelectableTargetAt(PCBView* view, const QPointF& scenePos) {
    if (!view || !view->scene()) return nullptr;

    const QList<QGraphicsItem*> hits = view->scene()->items(
        scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder, view->transform());

    for (QGraphicsItem* hit : hits) {
        PCBItem* target = resolveSelectableTarget(hit);
        if (!target) continue;

        PCBItem* deepestItem = nullptr;
        QGraphicsItem* current = hit;
        while (current) {
            if (PCBItem* candidate = dynamic_cast<PCBItem*>(current)) {
                if ((candidate->flags() & QGraphicsItem::ItemIsSelectable) &&
                    candidate->contains(candidate->mapFromScene(scenePos))) {
                    if (!deepestItem) deepestItem = candidate;
                }
            }
            current = current->parentItem();
        }

        if (deepestItem && deepestItem != target) {
            if (target->isSelected() || deepestItem->isSelected()) {
                return deepestItem;
            }
        }

        if (target->contains(target->mapFromScene(scenePos))) {
            return target;
        }
    }

    return nullptr;
}

void selectItemsInRect(QGraphicsScene* scene, const QRectF& rect) {
    if (!scene) return;

    QSet<PCBItem*> targets;
    // Only capture items FULLY CONTAINED within the selection rectangle
    const QList<QGraphicsItem*> hits = scene->items(rect, Qt::ContainsItemShape);
    for (QGraphicsItem* hit : hits) {
        if (PCBItem* target = resolveSelectableTarget(hit)) {
            targets.insert(target);
        }
    }

    for (PCBItem* item : targets) {
        item->setSelected(true);
    }
}

QPointF constrainTo45(const QPointF& fixed, const QPointF& rawTarget) {
    QPointF delta = rawTarget - fixed;
    const double len = std::hypot(delta.x(), delta.y());
    if (len < 1e-9) return rawTarget;

    const double angle = std::atan2(delta.y(), delta.x()) * 180.0 / M_PI;
    const double snappedAngle = std::round(angle / 45.0) * 45.0;
    const double rad = snappedAngle * M_PI / 180.0;
    return QPointF(fixed.x() + std::cos(rad) * len,
                   fixed.y() + std::sin(rad) * len);
}

QString tracePointKey(const QPointF& p, double grid = 1e-3) {
    const qint64 x = static_cast<qint64>(std::llround(p.x() / grid));
    const qint64 y = static_cast<qint64>(std::llround(p.y() / grid));
    return QString::number(x) + ":" + QString::number(y);
}

constexpr int kJunctionDotTagKey = 0x534A44; // "SJD"
constexpr int kResizeHandleTagKey = 0x52485A; // "RHZ"

PCBSelectTool::ResizeHandleCorner resizeCornerFromVariant(const QVariant& data) {
    switch (data.toInt()) {
    case 1: return PCBSelectTool::ResizeHandleCorner::TopLeft;
    case 2: return PCBSelectTool::ResizeHandleCorner::TopRight;
    case 3: return PCBSelectTool::ResizeHandleCorner::BottomLeft;
    case 4: return PCBSelectTool::ResizeHandleCorner::BottomRight;
    default: return PCBSelectTool::ResizeHandleCorner::None;
    }
}

}

TraceItem* PCBSelectTool::selectedSingleTrace() const {
    if (!view() || !view()->scene()) return nullptr;
    const QList<QGraphicsItem*> selected = view()->scene()->selectedItems();
    if (selected.size() != 1) return nullptr;
    return dynamic_cast<TraceItem*>(selected.first());
}

PCBItem* PCBSelectTool::selectedSingleResizableItem() const {
    if (!view() || !view()->scene()) return nullptr;
    const QList<QGraphicsItem*> selected = view()->scene()->selectedItems();
    if (selected.size() != 1) return nullptr;
    PCBItem* item = dynamic_cast<PCBItem*>(selected.first());
    if (!item || item->isLocked()) return nullptr;
    if (dynamic_cast<CopperPourItem*>(item) || dynamic_cast<PCBImageItem*>(item) ||
        dynamic_cast<PCBShapeItem*>(item)) {
        return item;
    }
    return nullptr;
}

void PCBSelectTool::cleanupTraceEditHandles() {
    if (!view() || !view()->scene()) return;
    if (m_traceStartHandle) {
        view()->scene()->removeItem(m_traceStartHandle);
        delete m_traceStartHandle;
        m_traceStartHandle = nullptr;
    }
    if (m_traceEndHandle) {
        view()->scene()->removeItem(m_traceEndHandle);
        delete m_traceEndHandle;
        m_traceEndHandle = nullptr;
    }
}

void PCBSelectTool::cleanupResizeHandles() {
    if (!view() || !view()->scene()) return;
    for (QGraphicsRectItem* handle : std::as_const(m_resizeHandles)) {
        if (!handle) continue;
        view()->scene()->removeItem(handle);
        delete handle;
    }
    m_resizeHandles.clear();
}

void PCBSelectTool::updateTraceEditHandles() {
    if (!view() || !view()->scene()) return;
    TraceItem* trace = selectedSingleTrace();
    if (!trace) {
        cleanupTraceEditHandles();
        return;
    }

    const QPointF startScene = trace->mapToScene(trace->startPoint());
    const QPointF endScene = trace->mapToScene(trace->endPoint());
    const double r = 0.6;

    if (!m_traceStartHandle) {
        m_traceStartHandle = new QGraphicsEllipseItem();
        m_traceStartHandle->setPen(QPen(QColor(255, 215, 0, 230), 0));
        m_traceStartHandle->setBrush(QBrush(QColor(255, 215, 0, 80)));
        m_traceStartHandle->setZValue(2100);
        m_traceStartHandle->setAcceptedMouseButtons(Qt::NoButton);
        view()->scene()->addItem(m_traceStartHandle);
    }
    if (!m_traceEndHandle) {
        m_traceEndHandle = new QGraphicsEllipseItem();
        m_traceEndHandle->setPen(QPen(QColor(255, 215, 0, 230), 0));
        m_traceEndHandle->setBrush(QBrush(QColor(255, 215, 0, 80)));
        m_traceEndHandle->setZValue(2100);
        m_traceEndHandle->setAcceptedMouseButtons(Qt::NoButton);
        view()->scene()->addItem(m_traceEndHandle);
    }

    m_traceStartHandle->setRect(QRectF(startScene.x() - r, startScene.y() - r, 2.0 * r, 2.0 * r));
    m_traceEndHandle->setRect(QRectF(endScene.x() - r, endScene.y() - r, 2.0 * r, 2.0 * r));
}

void PCBSelectTool::updateResizeHandles() {
    if (!view() || !view()->scene()) return;

    PCBItem* item = selectedSingleResizableItem();
    if (!item || m_resizeCorner != ResizeHandleCorner::None) {
        cleanupResizeHandles();
        return;
    }

    const QRectF rect = item->sceneBoundingRect();
    if (!rect.isValid() || rect.width() < 1e-6 || rect.height() < 1e-6) {
        cleanupResizeHandles();
        return;
    }

    if (m_resizeHandles.size() != 4) {
        cleanupResizeHandles();
        PCBTheme* theme = ThemeManager::theme();
        const QColor edge = theme ? theme->selectionBox() : QColor(0, 200, 255);
        QColor fill = edge;
        fill.setAlpha(90);

        for (int i = 0; i < 4; ++i) {
            auto* handle = new QGraphicsRectItem();
            handle->setPen(QPen(edge, 0));
            handle->setBrush(QBrush(fill));
            handle->setZValue(2100);
            handle->setAcceptedMouseButtons(Qt::NoButton);
            handle->setData(kResizeHandleTagKey, i + 1);
            view()->scene()->addItem(handle);
            m_resizeHandles.append(handle);
        }
    }

    const qreal handleSize = 1.0;
    const QPointF corners[4] = {
        rect.topLeft(), rect.topRight(), rect.bottomLeft(), rect.bottomRight()
    };

    for (int i = 0; i < m_resizeHandles.size() && i < 4; ++i) {
        m_resizeHandles[i]->setRect(QRectF(corners[i].x() - handleSize * 0.5,
                                           corners[i].y() - handleSize * 0.5,
                                           handleSize, handleSize));
    }
}

void PCBSelectTool::rebuildTraceJunctionDots() {
    if (!view() || !view()->scene()) return;
    QGraphicsScene* scene = view()->scene();

    const QList<QGraphicsItem*> existing = scene->items();
    for (QGraphicsItem* item : existing) {
        if (item && item->data(kJunctionDotTagKey).toBool()) {
            scene->removeItem(item);
            delete item;
        }
    }

    QMap<QString, int> endpointCounts;
    QMap<QString, QPointF> keyToPoint;
    const QList<QGraphicsItem*> sceneItems = scene->items();
    for (QGraphicsItem* item : sceneItems) {
        TraceItem* t = dynamic_cast<TraceItem*>(item);
        if (!t || t->netName().isEmpty() || t->netName() == "No Net") continue;
        const QPointF s = t->mapToScene(t->startPoint());
        const QPointF e = t->mapToScene(t->endPoint());
        const QString ks = QString::number(t->layer()) + "|" + t->netName() + "|" + tracePointKey(s);
        const QString ke = QString::number(t->layer()) + "|" + t->netName() + "|" + tracePointKey(e);
        endpointCounts[ks]++;
        endpointCounts[ke]++;
        keyToPoint[ks] = s;
        keyToPoint[ke] = e;
    }

    PCBTheme* theme = ThemeManager::theme();
    QColor dotColor = theme ? theme->accentColor() : QColor(255, 215, 0);
    dotColor.setAlpha(230);

    for (auto it = endpointCounts.constBegin(); it != endpointCounts.constEnd(); ++it) {
        if (it.value() < 3) continue;
        const QPointF p = keyToPoint.value(it.key());
        const double r = 0.45;
        auto* dot = new QGraphicsEllipseItem(p.x() - r, p.y() - r, 2.0 * r, 2.0 * r);
        dot->setPen(QPen(dotColor, 0));
        dot->setBrush(QBrush(dotColor));
        dot->setZValue(2050);
        dot->setAcceptedMouseButtons(Qt::NoButton);
        dot->setData(kJunctionDotTagKey, true);
        scene->addItem(dot);
    }
}

PCBSelectTool::PCBSelectTool(QObject* parent)
    : PCBTool("Select", parent) {
}

void PCBSelectTool::deactivate() {
    if (m_rubberBandItem && view() && view()->scene()) {
        view()->scene()->removeItem(m_rubberBandItem);
        delete m_rubberBandItem;
    }
    m_rubberBandItem = nullptr;
    m_rubberBandActive = false;
    cleanupClearanceVisuals();
    cleanupDragCollisionPreview();
    cleanupTraceEditHandles();
    cleanupResizeHandles();
    m_traceDragItem = nullptr;
    m_traceDragMode = TraceDragMode::None;
    m_resizeItem = nullptr;
    m_resizeCorner = ResizeHandleCorner::None;
    PCBTool::deactivate();
}

void PCBSelectTool::updateClearanceVisuals() {
    if (!view() || !view()->scene()) return;

    cleanupClearanceVisuals();
    
    QList<QGraphicsItem*> selected = view()->scene()->selectedItems();
    // Optimization: Only show halos if a small number of items are selected
    if (selected.size() > 10) return;

    PCBDRC drc;
    double clearance = drc.rules().minClearance();
    
    for (QGraphicsItem* item : selected) {
        PCBItem* pcbItem = dynamic_cast<PCBItem*>(item);
        if (!pcbItem || pcbItem->isLocked()) continue;
        
        // Only show halos for copper-carrying items
        PCBItem::ItemType type = pcbItem->itemType();
        if (type != PCBItem::PadType && type != PCBItem::ViaType && 
            type != PCBItem::TraceType && type != PCBItem::ComponentType) continue;

        // Create the halo shape
        QPainterPath shape = pcbItem->shape();
        if (shape.isEmpty()) continue;

        QTransform transform = pcbItem->sceneTransform();
        QPainterPath sceneShape = transform.map(shape);
        
        QPainterPathStroker stroker;
        stroker.setWidth(clearance * 2.0);
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        
        QPainterPath haloPath = stroker.createStroke(sceneShape);
        // Don't unite with sceneShape for simpler geometry during drag
        // haloPath = haloPath.united(sceneShape); 

        QGraphicsPathItem* halo = new QGraphicsPathItem(haloPath);
        
        // Check for collisions
        bool collision = false;
        
        // Fast bounding box check first
        QRectF searchRect = halo->boundingRect().adjusted(-0.5, -0.5, 0.5, 0.5);
        QList<QGraphicsItem*> candidates = view()->scene()->items(searchRect);
        
        for (QGraphicsItem* other : candidates) {
            PCBItem* otherPcb = dynamic_cast<PCBItem*>(other);
            if (!otherPcb || otherPcb == pcbItem || otherPcb->isSelected()) continue;
            
            // Ignore if 'other' is a child or parent of 'pcbItem' (e.g. pad vs component)
            if (otherPcb->parentItem() == pcbItem || pcbItem->parentItem() == otherPcb) continue;
            
            // Final check: only primitives on the same layer
            if (otherPcb->layer() == pcbItem->layer()) {
                QPointF dummy;
                if (drc.checkItemClearance(pcbItem, otherPcb, clearance, dummy)) {
                    collision = true;
                    break;
                }
            }
        }

        QColor safeColor(0, 240, 255, 30);
        QColor errorColor(255, 50, 50, 90);
        
        halo->setBrush(collision ? errorColor : safeColor);
        halo->setPen(QPen(collision ? QColor(255, 0, 0, 150) : QColor(0, 200, 255, 60), 0.05));
        halo->setZValue(900);
        
        view()->scene()->addItem(halo);
        m_dragHalos.append(halo);
    }
}

void PCBSelectTool::cleanupClearanceVisuals() {
    if (!view() || !view()->scene()) return;
    for (auto* m : m_dragHalos) {
        view()->scene()->removeItem(m);
        delete m;
    }
    m_dragHalos.clear();
}

void PCBSelectTool::cleanupDragCollisionPreview() {
    if (!view() || !view()->scene()) return;
    for (auto* overlay : m_collisionOverlays) {
        view()->scene()->removeItem(overlay);
        delete overlay;
    }
    m_collisionOverlays.clear();
}

QSet<PCBItem*> PCBSelectTool::collectDragComponentCollisions() const {
    QSet<PCBItem*> collisions;
    if (!view() || !view()->scene()) return collisions;

    QSet<PCBItem*> selectedComponents;
    for (QGraphicsItem* item : view()->scene()->selectedItems()) {
        if (PCBItem* pcb = dynamic_cast<PCBItem*>(item)) {
            if (dynamic_cast<PCBItem*>(pcb->parentItem()) == nullptr &&
                pcb->itemType() == PCBItem::ComponentType) {
                selectedComponents.insert(pcb);
            }
        }
    }
    if (selectedComponents.isEmpty()) return collisions;

    QList<PCBItem*> allComponents;
    const QList<QGraphicsItem*> sceneItems = view()->scene()->items();
    for (QGraphicsItem* item : sceneItems) {
        PCBItem* pcb = dynamic_cast<PCBItem*>(item);
        if (!pcb || !pcb->isVisible()) continue;
        if (dynamic_cast<PCBItem*>(pcb->parentItem()) != nullptr) continue;
        if (pcb->itemType() != PCBItem::ComponentType) continue;
        allComponents.append(pcb);
    }

    for (PCBItem* moving : selectedComponents) {
        if (!moving) continue;
        const QRectF movingRect = moving->sceneBoundingRect();
        for (PCBItem* other : allComponents) {
            if (!other || other == moving || selectedComponents.contains(other)) continue;
            if (other->layer() != moving->layer()) continue;
            if (!movingRect.intersects(other->sceneBoundingRect())) continue;
            if (moving->collidesWithItem(other, Qt::IntersectsItemShape)) {
                collisions.insert(moving);
                collisions.insert(other);
            }
        }
    }

    return collisions;
}

void PCBSelectTool::updateDragCollisionPreview() {
    if (!view() || !view()->scene()) return;
    cleanupDragCollisionPreview();

    const QSet<PCBItem*> collidingItems = collectDragComponentCollisions();
    if (collidingItems.isEmpty()) return;

    for (PCBItem* item : collidingItems) {
        QRectF rect = item->sceneBoundingRect().adjusted(-0.2, -0.2, 0.2, 0.2);
        QGraphicsRectItem* overlay = new QGraphicsRectItem(rect);
        overlay->setPen(QPen(QColor(255, 60, 60, 230), 0, Qt::DashLine));
        overlay->setBrush(QBrush(QColor(255, 60, 60, 35)));
        overlay->setZValue(1100);
        overlay->setAcceptedMouseButtons(Qt::NoButton);
        view()->scene()->addItem(overlay);
        m_collisionOverlays.append(overlay);
    }
}

void PCBSelectTool::mousePressEvent(QMouseEvent* event) {
    if (!view()) return;

    QPointF scenePos = view()->mapToScene(event->pos());
    QGraphicsItem* item = view()->itemAt(event->pos());

    if (event->button() == Qt::LeftButton && item && item->data(kResizeHandleTagKey).isValid()) {
        PCBItem* resizeItem = selectedSingleResizableItem();
        const ResizeHandleCorner corner = resizeCornerFromVariant(item->data(kResizeHandleTagKey));
        if (resizeItem && corner != ResizeHandleCorner::None) {
            beginResize(resizeItem, corner);
            event->accept();
            return;
        }
    }

    PCBItem* pcbItem = hitSelectableTargetAt(view(), scenePos);

    // Clear selection if clicking on empty space or non-pcb item
    if (event->button() == Qt::LeftButton) {
        if (!pcbItem) {
            // Start rubber band selection
            m_rubberBandActive = true;
            m_rubberBandOrigin = scenePos;
            
            if (m_rubberBandItem) {
                view()->scene()->removeItem(m_rubberBandItem);
                delete m_rubberBandItem;
            }
            
            m_rubberBandItem = new QGraphicsRectItem();
            QColor selColor = Qt::blue;
            if (ThemeManager::theme()) selColor = ThemeManager::theme()->selectionBox();
            
            QColor fillColor = selColor;
            fillColor.setAlpha(40);
            
            m_rubberBandItem->setPen(QPen(selColor, 0, Qt::DashLine));
            m_rubberBandItem->setBrush(QBrush(fillColor));
            m_rubberBandItem->setZValue(1000); // Always on top
            view()->scene()->addItem(m_rubberBandItem);
            m_rubberBandItem->setRect(QRectF(m_rubberBandOrigin, QSize(0, 0)));

            if (!(event->modifiers() & Qt::ControlModifier)) {
                view()->scene()->clearSelection();
            }
            updateTraceEditHandles();
            updateResizeHandles();
            event->accept();
            return;
        } else {
            // Not a background click: Handle selection and dragging
            bool isCtrlHeld = event->modifiers() & Qt::ControlModifier;
            bool isShiftHeld = event->modifiers() & Qt::ShiftModifier;

            if (isCtrlHeld || isShiftHeld) {
                // Toggle selection
                pcbItem->setSelected(!pcbItem->isSelected());
            } else {
                // Normal click: Select only this item if not already part of selection
                if (!pcbItem->isSelected()) {
                    view()->scene()->clearSelection();
                    pcbItem->setSelected(true);
                }
            }

            updateTraceEditHandles();
            updateResizeHandles();

            // Single-trace edit mode: drag start/end handle or the segment itself.
            if (!isCtrlHeld && !isShiftHeld) {
                if (TraceItem* trace = dynamic_cast<TraceItem*>(pcbItem)) {
                    if (view()->scene()->selectedItems().size() == 1 && trace->isSelected()) {
                        const QPointF startScene = trace->mapToScene(trace->startPoint());
                        const QPointF endScene = trace->mapToScene(trace->endPoint());
                        const double handlePickRadius = 0.9;

                        TraceDragMode mode = TraceDragMode::None;
                        if (QLineF(scenePos, startScene).length() <= handlePickRadius) {
                            mode = TraceDragMode::StartPoint;
                        } else if (QLineF(scenePos, endScene).length() <= handlePickRadius) {
                            mode = TraceDragMode::EndPoint;
                        } else {
                            const QPointF localPos = trace->mapFromScene(scenePos);
                            if (trace->shape().contains(localPos)) {
                                mode = TraceDragMode::Segment;
                            }
                        }

                        if (mode != TraceDragMode::None) {
                            m_traceDragItem = trace;
                            m_traceDragMode = mode;
                            m_traceDragMouseOrigin = scenePos;
                            m_traceDragStartInitialScene = startScene;
                            m_traceDragEndInitialScene = endScene;
                            m_traceDragStartInitialLocal = trace->startPoint();
                            m_traceDragEndInitialLocal = trace->endPoint();
                            event->accept();
                            return;
                        }
                    }
                }
            }

            m_isDragging = true;
            m_lastMousePos = event->pos();
            m_initialPositions.clear();
            m_dragStartScenePos = scenePos;
            m_hasDragAnchor = false;

            // Capture initial positions of ONLY top-level selected items.
            // Child items (e.g. PadItems inside a ComponentItem) must NOT be stored here
            // because createPads() can delete and recreate them, leaving dangling pointers.
            for (QGraphicsItem* selItem : view()->scene()->selectedItems()) {
                if (PCBItem* p = dynamic_cast<PCBItem*>(selItem)) {
                    // Only top-level: no PCBItem parent
                    if (dynamic_cast<PCBItem*>(p->parentItem()) == nullptr) {
                        m_initialPositions[p] = p->pos();
                    }
                }
            }
            if (!m_initialPositions.isEmpty()) {
                m_dragAnchorInitialPos = m_initialPositions.constBegin().value();
                m_hasDragAnchor = true;
            }
            event->accept();
        }
    }

    // Pass event to base implementation
    PCBTool::mousePressEvent(event);
    QGraphicsView* graphicsView = dynamic_cast<QGraphicsView*>(view());
    if (graphicsView) {
        // We need to call the view's mousePressEvent to let QGraphicsScene handle move events
        // But PCBView::mousePressEvent forwards back to us. 
        // We should let the default QGraphicsView behavior handle the move.
    }
}

void PCBSelectTool::mouseMoveEvent(QMouseEvent* event) {
    if (m_resizeCorner != ResizeHandleCorner::None && m_resizeItem && view()) {
        if (applyResizeFromScenePoint(view()->mapToScene(event->pos()))) {
            updateResizeHandles();
        }
        event->accept();
        return;
    }

    if (m_traceDragMode != TraceDragMode::None && m_traceDragItem && view()) {
        QPointF scenePos = view()->mapToScene(event->pos());
        QPointF snappedPos = view()->snapToGrid(scenePos);

        if (m_traceDragMode == TraceDragMode::StartPoint) {
            QPointF constrained = constrainTo45(m_traceDragEndInitialScene, snappedPos);
            constrained = view()->snapToGrid(constrained);
            m_traceDragItem->setStartPoint(m_traceDragItem->mapFromScene(constrained));
        } else if (m_traceDragMode == TraceDragMode::EndPoint) {
            QPointF constrained = constrainTo45(m_traceDragStartInitialScene, snappedPos);
            constrained = view()->snapToGrid(constrained);
            m_traceDragItem->setEndPoint(m_traceDragItem->mapFromScene(constrained));
        } else if (m_traceDragMode == TraceDragMode::Segment) {
            QPointF rawDelta = scenePos - m_traceDragMouseOrigin;
            QPointF snappedAnchor = view()->snapToGrid(m_traceDragStartInitialScene + rawDelta);
            QPointF snappedDelta = snappedAnchor - m_traceDragStartInitialScene;
            QPointF newStart = m_traceDragStartInitialScene + snappedDelta;
            QPointF newEnd = m_traceDragEndInitialScene + snappedDelta;
            m_traceDragItem->setStartPoint(m_traceDragItem->mapFromScene(newStart));
            m_traceDragItem->setEndPoint(m_traceDragItem->mapFromScene(newEnd));
        }

        if (!m_traceDragItem->netName().isEmpty()) {
            PCBRatsnestManager::instance().updateNet(m_traceDragItem->netName());
        }
        updateTraceEditHandles();
        event->accept();
        return;
    }

    if (m_rubberBandActive && m_rubberBandItem && view()) {
        QPointF currentPos = view()->mapToScene(event->pos());
        QRectF rect = QRectF(m_rubberBandOrigin, currentPos).normalized();
        
        // Cache the last rect to avoid redundant selection updates
        static QRectF lastRect;
        if (rect != lastRect) {
            m_rubberBandItem->setRect(rect);
            
            // Real-time selection highlight
            // If Ctrl is held, we should really be toggling or adding, 
            // but for simple rubber-band we clear and re-select what's in the box.
            // Professional behavior: Ctrl adds to existing, no-ctrl clears and selects box.
            if (!(event->modifiers() & Qt::ControlModifier)) {
                view()->scene()->clearSelection();
            }
            
            selectItemsInRect(view()->scene(), rect);
            updateTraceEditHandles();
            updateResizeHandles();
            lastRect = rect;
        }
        
        event->accept();
        return;
    }

    if (m_isDragging && view()) {
        QPointF scenePos = view()->mapToScene(event->pos());
        QPointF rawDelta = scenePos - m_dragStartScenePos;
        QPointF snappedDelta = rawDelta;
        if (m_hasDragAnchor) {
            QPointF snappedAnchor = view()->snapToGrid(m_dragAnchorInitialPos + rawDelta);
            snappedDelta = snappedAnchor - m_dragAnchorInitialPos;
        }

        for (auto it = m_initialPositions.constBegin(); it != m_initialPositions.constEnd(); ++it) {
            PCBItem* p = it.key();
            if (p && !p->isLocked()) {
                p->setPos(it.value() + snappedDelta);
            }
        }
        // Identify all affected nets for targeted update
        QSet<QString> affectedNets;
        for (QGraphicsItem* item : view()->scene()->selectedItems()) {
            if (PCBItem* pcbItem = dynamic_cast<PCBItem*>(item)) {
                if (!pcbItem->netName().isEmpty()) affectedNets.insert(pcbItem->netName());
                
                // Track children (like component pads)
                for (QGraphicsItem* child : pcbItem->childItems()) {
                    if (PCBItem* cp = dynamic_cast<PCBItem*>(child)) {
                        if (!cp->netName().isEmpty()) affectedNets.insert(cp->netName());
                    }
                }
            }
        }

        for (const QString& net : affectedNets) {
            PCBRatsnestManager::instance().updateNet(net);
        }

        updateDragCollisionPreview();
        // updateClearanceVisuals(); // Temporarily disabled to fix crash

        m_lastMousePos = event->pos();
        updateResizeHandles();
        event->accept();
        return;
    }

    PCBTool::mouseMoveEvent(event);
}

void PCBSelectTool::mouseReleaseEvent(QMouseEvent* event) {
    if (m_resizeCorner != ResizeHandleCorner::None && m_resizeItem && view()) {
        const QJsonObject newState = m_resizeItem->toJson();
        if (view()->undoStack() && newState != m_resizeOldState) {
            view()->undoStack()->push(new PCBItemStateCommand(
                view()->scene(), m_resizeItem, m_resizeOldState, newState, "Resize Item"));
        }

        if (!m_resizeItem->netName().isEmpty()) {
            PCBRatsnestManager::instance().updateNet(m_resizeItem->netName());
        } else {
            PCBRatsnestManager::instance().update();
        }

        m_resizeItem = nullptr;
        m_resizeCorner = ResizeHandleCorner::None;
        m_resizeOldState = QJsonObject();
        updateResizeHandles();
        event->accept();
        PCBTool::mouseReleaseEvent(event);
        return;
    }

    if (m_traceDragMode != TraceDragMode::None && m_traceDragItem && view()) {
        QPointF oldStart = m_traceDragStartInitialLocal;
        QPointF oldEnd = m_traceDragEndInitialLocal;
        QPointF newStart = m_traceDragItem->startPoint();
        QPointF newEnd = m_traceDragItem->endPoint();

        bool changed = (oldStart != newStart) || (oldEnd != newEnd);
        if (changed && view()->undoStack()) {
            QUndoCommand* editCmd = new QUndoCommand("Edit Trace Segment");
            bool hasChild = false;
            auto addProperty = [&](const QString& name, double oldVal, double newVal) {
                if (qFuzzyCompare(oldVal + 1.0, newVal + 1.0)) return;
                new PCBPropertyCommand(view()->scene(), m_traceDragItem, name, oldVal, newVal, editCmd);
                hasChild = true;
            };
            addProperty("Start X (mm)", oldStart.x(), newStart.x());
            addProperty("Start Y (mm)", oldStart.y(), newStart.y());
            addProperty("End X (mm)", oldEnd.x(), newEnd.x());
            addProperty("End Y (mm)", oldEnd.y(), newEnd.y());

            if (hasChild) view()->undoStack()->push(editCmd);
            else delete editCmd;
        }

        if (!m_traceDragItem->netName().isEmpty()) {
            PCBRatsnestManager::instance().updateNet(m_traceDragItem->netName());
        }
        rebuildTraceJunctionDots();

        m_traceDragMode = TraceDragMode::None;
        m_traceDragItem = nullptr;
        updateTraceEditHandles();
        updateResizeHandles();
        event->accept();
        PCBTool::mouseReleaseEvent(event);
        return;
    }

    if (m_rubberBandActive && m_rubberBandItem && view()) {
        QRectF rect = m_rubberBandItem->rect();
        
        selectItemsInRect(view()->scene(), rect);
        updateTraceEditHandles();
        updateResizeHandles();
        
        view()->scene()->removeItem(m_rubberBandItem);
        delete m_rubberBandItem;
        m_rubberBandItem = nullptr;
        m_rubberBandActive = false;
        event->accept();
    }

    if (m_isDragging && view() && view()->undoStack()) {
        const QSet<PCBItem*> collisions = collectDragComponentCollisions();
        if (!collisions.isEmpty()) {
            QSet<QString> affectedNets;
            for (auto it = m_initialPositions.begin(); it != m_initialPositions.end(); ++it) {
                PCBItem* item = it.key();
                if (!item) continue;
                item->setPos(it.value());
                if (!item->netName().isEmpty()) affectedNets.insert(item->netName());
                for (QGraphicsItem* child : item->childItems()) {
                    if (PCBItem* cp = dynamic_cast<PCBItem*>(child)) {
                        if (!cp->netName().isEmpty()) affectedNets.insert(cp->netName());
                    }
                }
            }
            for (const QString& net : affectedNets) {
                PCBRatsnestManager::instance().updateNet(net);
            }

            if (QMainWindow* mainWindow = qobject_cast<QMainWindow*>(view()->window())) {
                if (mainWindow->statusBar()) {
                    mainWindow->statusBar()->showMessage(
                        "Move blocked: footprint overlap detected.", 3000);
                }
            }
        } else {
            bool hasMoved = false;
            QList<PCBItem*> movedItems;
            QList<QPointF> oldPositions;
            QList<QPointF> newPositions;

            for (auto it = m_initialPositions.begin(); it != m_initialPositions.end(); ++it) {
                if (it.key()->pos() != it.value()) {
                    hasMoved = true;
                    movedItems.append(it.key());
                    oldPositions.append(it.value());
                    newPositions.append(it.key()->pos());
                }
            }

            if (hasMoved) {
                view()->undoStack()->push(new PCBMoveItemCommand(view()->scene(), movedItems, oldPositions, newPositions));
            }
        }
    }

    cleanupClearanceVisuals();
    cleanupDragCollisionPreview();
    m_isDragging = false;
    m_hasDragAnchor = false;
    m_initialPositions.clear();
    updateTraceEditHandles();
    updateResizeHandles();
    PCBTool::mouseReleaseEvent(event);
}

void PCBSelectTool::beginResize(PCBItem* item, ResizeHandleCorner corner) {
    if (!item) return;
    m_resizeItem = item;
    m_resizeCorner = corner;
    m_resizeInitialSceneRect = item->sceneBoundingRect();
    m_resizeInitialItemPos = item->pos();
    m_resizeOldState = item->toJson();

    switch (corner) {
    case ResizeHandleCorner::TopLeft:
        m_resizeAnchorScenePos = m_resizeInitialSceneRect.bottomRight();
        break;
    case ResizeHandleCorner::TopRight:
        m_resizeAnchorScenePos = m_resizeInitialSceneRect.bottomLeft();
        break;
    case ResizeHandleCorner::BottomLeft:
        m_resizeAnchorScenePos = m_resizeInitialSceneRect.topRight();
        break;
    case ResizeHandleCorner::BottomRight:
        m_resizeAnchorScenePos = m_resizeInitialSceneRect.topLeft();
        break;
    default:
        break;
    }

    if (CopperPourItem* pour = dynamic_cast<CopperPourItem*>(item)) {
        m_resizeInitialPolygon = pour->polygon();
    } else if (PCBImageItem* image = dynamic_cast<PCBImageItem*>(item)) {
        m_resizeInitialImageSize = image->sizeMm();
    }

    cleanupResizeHandles();
}

bool PCBSelectTool::applyResizeFromScenePoint(const QPointF& scenePos) {
    if (!m_resizeItem || !view()) return false;

    const QPointF snappedPos = view()->snapToGrid(scenePos);
    QRectF targetRect(m_resizeAnchorScenePos, snappedPos);
    targetRect = targetRect.normalized();
    targetRect.setWidth(std::max<qreal>(0.5, targetRect.width()));
    targetRect.setHeight(std::max<qreal>(0.5, targetRect.height()));

    if (CopperPourItem* pour = dynamic_cast<CopperPourItem*>(m_resizeItem)) {
        const QRectF initialBounds = m_resizeInitialPolygon.boundingRect();
        if (!initialBounds.isValid() || initialBounds.width() < 1e-9 || initialBounds.height() < 1e-9) return false;

        QPolygonF scaled = m_resizeInitialPolygon;
        for (QPointF& point : scaled) {
            const qreal nx = (point.x() - initialBounds.left()) / initialBounds.width();
            const qreal ny = (point.y() - initialBounds.top()) / initialBounds.height();
            point.setX(targetRect.left() + nx * targetRect.width());
            point.setY(targetRect.top() + ny * targetRect.height());
        }
        pour->setPolygon(scaled);
        if (pour->scene()) {
            pour->scene()->update();
        }
        return true;
    }

    if (PCBImageItem* image = dynamic_cast<PCBImageItem*>(m_resizeItem)) {
        image->setPos(targetRect.center());
        image->setSizeMm(targetRect.size());
        return true;
    }

    if (PCBShapeItem* shape = dynamic_cast<PCBShapeItem*>(m_resizeItem)) {
        shape->setPos(targetRect.center());
        shape->setSizeMm(targetRect.size());
        return true;
    }

    return false;
}
