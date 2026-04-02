#ifndef SYMBOL_EDITOR_VIEW_H
#define SYMBOL_EDITOR_VIEW_H

#include <QGraphicsView>
#include <QPointF>

/**
 * @brief Custom view for symbol editor with grid snapping
 */
class SymbolEditorView : public QGraphicsView {
    Q_OBJECT
    
public:
    explicit SymbolEditorView(QWidget* parent = nullptr);
    
    void setCurrentTool(int tool);
    int currentTool() const { return m_currentTool; }
    
    QPointF snapToGrid(QPointF pos) const;
    qreal gridSize() const { return m_gridSize; }
    void setGridSize(qreal size) { m_gridSize = size; viewport()->update(); }
    
    bool snapToGridEnabled() const { return m_snapToGrid; }
    void setSnapToGrid(bool enable) { m_snapToGrid = enable; }

    void setGuideAnchorPoints(const QList<QPointF>& points);
    
    enum GridStyle { Dots, Lines };
    GridStyle gridStyle() const { return m_gridStyle; }
    void setGridStyle(GridStyle style) { m_gridStyle = style; viewport()->update(); }
    void zoomByFactor(qreal factor);
    bool snapCursorCrosshairEnabled() const { return m_snapCursorCrosshair; }
    void setSnapCursorCrosshairEnabled(bool enabled) { m_snapCursorCrosshair = enabled; viewport()->update(); }
    
Q_SIGNALS:
    void pointClicked(QPointF scenePos);
    void mouseMoved(QPointF scenePos);
    void coordinatesChanged(QPointF scenePos);
    void lineDragged(QPointF start, QPointF end);
    void drawingFinished(QPointF start, QPointF end);
    void itemsMoved(QPointF delta);
    void rotateCWRequested();
    void rotateCCWRequested();
    void flipHRequested();
    void flipVRequested();
    void rightClicked();
    void contextMenuRequested(const QPoint& pos);
    void itemErased(QGraphicsItem* item);
    void penPointAdded(QPointF scenePos);
    void penHandleDragged(QPointF handlePos);
    void penPointFinished();
    void penPathClosed();
    void penClicked(QPointF scenePos, int pointIndex = -1, int handleIndex = -1);
    void penDoubleClicked(QPointF scenePos, int pointIndex = -1);
    void bezierEditPointClicked(QPointF scenePos);
    void bezierEditPointDragged(QPointF newPos);
    void rectResizeStarted(const QString& corner, QPointF scenePos);
    void rectResizeUpdated(QPointF scenePos);
    void rectResizeFinished(QPointF scenePos);
    
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
 
private:
    bool applyZoomFactor(qreal factor);
    void updatePinAlignmentGuides();
    void updateDrawGuides(const QPointF& cursor);
    void clearPinAlignmentGuides();

    int m_currentTool = 0;
    qreal m_gridSize = 10.0;
    bool m_snapToGrid = true;
    GridStyle m_gridStyle = Dots;
    bool m_isPanning = false;
    bool m_isDrawing = false;
    QPoint m_lastPanPoint;
    QPointF m_drawStart;
    QPointF m_snapPressPos;

    bool m_showVGuide = false;
    bool m_showHGuide = false;
    qreal m_vGuideX = 0.0;
    qreal m_hGuideY = 0.0;
    QPointF m_penPressPos;
    bool m_penIsDragging = false;
    static constexpr qreal PEN_DRAG_THRESHOLD = 5.0;
    
    // Bezier editing state (Select mode)
    bool m_bezierEditIsDragging = false;
    QPointF m_bezierEditPressPos;
    bool m_rectResizeActive = false;

    // Smart guide anchor points (pins, endpoints, etc.)
    QList<QPointF> m_guideAnchors;
    QPointF m_guideCursorPos;
    qreal m_minZoom = 0.03;
    qreal m_maxZoom = 60.0;
    bool m_snapCursorCrosshair = false;
    bool m_hasMousePos = false;
    QPointF m_lastSnappedMousePos;
};

#endif // SYMBOL_EDITOR_VIEW_H
