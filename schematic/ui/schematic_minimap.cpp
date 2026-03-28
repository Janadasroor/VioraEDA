#include "schematic_minimap.h"
#include "../editor/schematic_view.h"
#include <QScrollBar>
#include <QMouseEvent>
#include <QPainter>

SchematicMiniMap::SchematicMiniMap(SchematicView* parentView, QWidget* parent)
    : QGraphicsView(parent), m_mainView(parentView), m_isDragging(false) {
    
    if (m_mainView && m_mainView->scene()) {
        setScene(m_mainView->scene());
    }

    setRenderHint(QPainter::Antialiasing);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    
    // Modern "glassy" sidebar-style background
    setStyleSheet("background: rgba(15, 15, 15, 180); border: 1px solid rgba(80, 80, 80, 100); border-radius: 6px;");

    if (scene()) {
        m_viewportRectItem = new QGraphicsRectItem();
        m_viewportRectItem->setPen(QPen(QColor(59, 130, 246, 220), 1.5)); // Blue accent
        m_viewportRectItem->setBrush(QColor(59, 130, 246, 40));
        m_viewportRectItem->setZValue(10000); 
        scene()->addItem(m_viewportRectItem);
    }
}

void SchematicMiniMap::updateViewportRect() {
    if (!m_mainView || !m_viewportRectItem || !scene()) return;

    // Get the visible area of the main view in scene coordinates
    QRectF viewportRect = m_mainView->mapToScene(m_mainView->viewport()->rect()).boundingRect();
    m_viewportRectItem->setRect(viewportRect);
    
    // Fit the whole scene content in the mini-map
    QRectF contentRect = scene()->itemsBoundingRect();
    if (contentRect.isEmpty()) contentRect = QRectF(0, 0, 1000, 1000);
    
    // Add some margin
    contentRect.adjust(-100, -100, 100, 100);
    fitInView(contentRect, Qt::KeepAspectRatio);
}

void SchematicMiniMap::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    updateViewportRect();
}

void SchematicMiniMap::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        syncToMainView(event->pos());
    }
}

void SchematicMiniMap::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging) {
        syncToMainView(event->pos());
    }
}

void SchematicMiniMap::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    }
}

void SchematicMiniMap::syncToMainView(const QPoint& pos) {
    if (!m_mainView) return;
    QPointF scenePos = mapToScene(pos);
    m_mainView->centerOn(scenePos);
}

void SchematicMiniMap::scrollContentsBy(int dx, int dy) {
    QGraphicsView::scrollContentsBy(dx, dy);
}

void SchematicMiniMap::paintEvent(QPaintEvent* event) {
    if (!scene()) return;
    QGraphicsView::paintEvent(event);
}
