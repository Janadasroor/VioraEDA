#ifndef GERBER_VIEW_H
#define GERBER_VIEW_H

#include <QColor>
#include <QGraphicsView>
#include <QGraphicsScene>
#include "gerber_layer.h"

/**
 * @brief High-performance renderer for Gerber layers
 */
class GerberView : public QGraphicsView {
    Q_OBJECT
public:
    explicit GerberView(QWidget* parent = nullptr);
    
    void addLayer(GerberLayer* layer);
    void setLayers(const QList<GerberLayer*>& layers);
    void clear();
    void zoomFit();
    void setBackgroundColor(const QColor& color);
    void setMonochrome(bool enabled);
    QRectF plotBounds() const;
    QColor backgroundColor() const { return m_backgroundColor; }

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    void rebuildScene();
    bool isEdgeLayer(const GerberLayer* layer) const;
    bool isDrillLayer(const GerberLayer* layer) const;
    QColor displayColorForLayer(const GerberLayer* layer) const;
    QPainterPath boardOutlinePath() const;
    void renderLayer(GerberLayer* layer);

    QGraphicsScene* m_scene;
    QList<GerberLayer*> m_layers;
    QColor m_backgroundColor;
    bool m_monochrome = false;
    
    bool m_isPanning;
    QPoint m_lastPanPoint;
};

#endif // GERBER_VIEW_H
