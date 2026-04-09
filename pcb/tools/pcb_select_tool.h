#ifndef PCBSELECTTOOL_H
#define PCBSELECTTOOL_H

#include "pcb_tool.h"
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QPolygonF>
#include <QSet>

class PCBSelectTool : public PCBTool {
    Q_OBJECT

public:
    PCBSelectTool(QObject* parent = nullptr);

    // PCBTool interface
    QString tooltip() const override { return "Select and move items"; }
    QString iconName() const override { return "select"; }
    bool isSelectable() const override { return true; }
    void deactivate() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    enum class TraceDragMode {
        None,
        StartPoint,
        EndPoint,
        Segment
    };

    enum class ResizeHandleCorner {
        None,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

private:
    class TraceItem* selectedSingleTrace() const;
    class PCBItem* selectedSingleResizableItem() const;
    void updateTraceEditHandles();
    void cleanupTraceEditHandles();
    void updateResizeHandles();
    void cleanupResizeHandles();
    void rebuildTraceJunctionDots();
    QSet<class PCBItem*> collectDragComponentCollisions() const;
    void updateClearanceVisuals();
    void cleanupClearanceVisuals();
    void updateDragCollisionPreview();
    void cleanupDragCollisionPreview();
    void beginResize(class PCBItem* item, ResizeHandleCorner corner);
    bool applyResizeFromScenePoint(const QPointF& scenePos);

    bool m_isDragging = false;
    QPoint m_lastMousePos;
    QMap<class PCBItem*, QPointF> m_initialPositions;
    QList<class QGraphicsPathItem*> m_dragHalos;
    QList<class QGraphicsRectItem*> m_collisionOverlays;
    class QGraphicsEllipseItem* m_traceStartHandle = nullptr;
    class QGraphicsEllipseItem* m_traceEndHandle = nullptr;
    class TraceItem* m_traceDragItem = nullptr;
    TraceDragMode m_traceDragMode = TraceDragMode::None;
    QPointF m_traceDragMouseOrigin;
    QPointF m_traceDragStartInitialScene;
    QPointF m_traceDragEndInitialScene;
    QPointF m_traceDragStartInitialLocal;
    QPointF m_traceDragEndInitialLocal;
    QPointF m_dragStartScenePos;
    QPointF m_dragAnchorInitialPos;
    bool m_hasDragAnchor = false;
    QList<class QGraphicsRectItem*> m_resizeHandles;
    class PCBItem* m_resizeItem = nullptr;
    ResizeHandleCorner m_resizeCorner = ResizeHandleCorner::None;
    QPointF m_resizeAnchorScenePos;
    QRectF m_resizeInitialSceneRect;
    QPointF m_resizeInitialItemPos;
    QPolygonF m_resizeInitialPolygon;
    QSizeF m_resizeInitialImageSize;
    QJsonObject m_resizeOldState;

    bool m_rubberBandActive = false;
    QPointF m_rubberBandOrigin;
    class QGraphicsRectItem* m_rubberBandItem = nullptr;
};

#endif // PCBSELECTTOOL_H
