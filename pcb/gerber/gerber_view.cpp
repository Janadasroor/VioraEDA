#include "gerber_view.h"
#include <QWheelEvent>
#include <QScrollBar>
#include <QGraphicsPathItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsPolygonItem>

namespace {
QString normalizedLayerName(const QString& name) {
    return name.toLower();
}
}

GerberView::GerberView(QWidget* parent)
    : QGraphicsView(parent), m_backgroundColor(Qt::black), m_isPanning(false) {
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setDragMode(QGraphicsView::NoDrag);
    setBackgroundBrush(m_backgroundColor);
    
    // Improved zoom level
    scale(10.0, -10.0); // Y-axis is inverted in Gerber (up is positive)
}

void GerberView::setBackgroundColor(const QColor& color) {
    if (!color.isValid() || color == m_backgroundColor) {
        return;
    }

    m_backgroundColor = color;
    setBackgroundBrush(m_backgroundColor);
    viewport()->update();
}

void GerberView::setMonochrome(bool enabled) {
    if (m_monochrome == enabled) {
        return;
    }
    m_monochrome = enabled;
    rebuildScene();
}

QRectF GerberView::plotBounds() const {
    const QPainterPath outline = boardOutlinePath();
    const QRectF outlineBounds = outline.boundingRect();
    if (outlineBounds.isValid() && !outlineBounds.isEmpty()) {
        return outlineBounds;
    }
    return m_scene ? m_scene->itemsBoundingRect() : QRectF();
}

void GerberView::addLayer(GerberLayer* layer) {
    m_layers.append(layer);
    rebuildScene();
}

void GerberView::setLayers(const QList<GerberLayer*>& layers) {
    m_layers = layers;
    rebuildScene();
}

void GerberView::clear() {
    m_scene->clear();
    m_layers.clear();
}

bool GerberView::isEdgeLayer(const GerberLayer* layer) const {
    const QString name = normalizedLayerName(layer ? layer->name() : QString());
    return name.contains("edge") || name.contains("outline") ||
           name.contains(".gm1") || name.contains(".gko");
}

bool GerberView::isDrillLayer(const GerberLayer* layer) const {
    const QString name = normalizedLayerName(layer ? layer->name() : QString());
    return name.contains("drill") || name.contains(".drl") || name.contains(".drll") ||
           name.contains("excellon") || name.contains(".xln") || name.contains(".txt");
}

QColor GerberView::displayColorForLayer(const GerberLayer* layer) const {
    if (m_monochrome) {
        if (isDrillLayer(layer)) {
            return Qt::black;
        }
        return Qt::black;
    }

    const QString name = normalizedLayerName(layer ? layer->name() : QString());
    if (isDrillLayer(layer)) {
        return QColor(35, 35, 35);
    }
    if (isEdgeLayer(layer)) {
        return QColor(255, 208, 92);
    }
    if (name.contains("gtl") || name.contains("top") && name.contains("copper")) {
        return QColor(232, 119, 34);
    }
    if (name.contains("gbl") || name.contains("bottom") && name.contains("copper")) {
        return QColor(59, 130, 246);
    }
    if (name.contains("gts") || name.contains("mask") && name.contains("top")) {
        return QColor(35, 111, 62, 150);
    }
    if (name.contains("gbs") || name.contains("mask") && name.contains("bottom")) {
        return QColor(27, 83, 49, 150);
    }
    if (name.contains("gto") || name.contains("gbo") || name.contains("silk") || name.contains("legend")) {
        return QColor(238, 238, 232);
    }
    if (name.contains("paste")) {
        return QColor(180, 180, 180, 180);
    }
    if (name.contains("courtyard")) {
        return QColor(125, 180, 255);
    }
    if (name.contains("fabrication")) {
        return QColor(150, 150, 180);
    }
    return layer ? layer->color() : QColor(Qt::white);
}

QPainterPath GerberView::boardOutlinePath() const {
    QPainterPath outline;
    for (GerberLayer* layer : m_layers) {
        if (!layer || !layer->isVisible() || !isEdgeLayer(layer)) {
            continue;
        }
        for (const auto& prim : layer->primitives()) {
            outline.addPath(prim.path);
        }
    }

    if (!outline.isEmpty()) {
        return outline.simplified();
    }

    QRectF bounds;
    bool first = true;
    for (GerberLayer* layer : m_layers) {
        if (!layer || !layer->isVisible() || isDrillLayer(layer)) {
            continue;
        }
        for (const auto& prim : layer->primitives()) {
            QRectF r = prim.type == GerberPrimitive::Flash
                ? QRectF(prim.center.x() - 0.5, prim.center.y() - 0.5, 1.0, 1.0)
                : prim.path.boundingRect();
            if (first) {
                bounds = r;
                first = false;
            } else {
                bounds = bounds.united(r);
            }
        }
    }

    if (first) {
        bounds = QRectF(-50, -50, 100, 100);
    }

    QPainterPath fallback;
    fallback.addRect(bounds.adjusted(-2.0, -2.0, 2.0, 2.0));
    return fallback;
}

void GerberView::rebuildScene() {
    m_scene->clear();

    const QPainterPath boardPath = boardOutlinePath();
    if (!boardPath.isEmpty()) {
        QGraphicsPathItem* boardItem = m_scene->addPath(boardPath);
        if (m_monochrome) {
            boardItem->setBrush(Qt::white);
            boardItem->setPen(QPen(Qt::black, 0.18));
        } else {
            boardItem->setBrush(QColor(140, 168, 108));
            boardItem->setPen(QPen(QColor(108, 134, 82), 0.18));
        }
        boardItem->setZValue(-100.0);
    }

    for (GerberLayer* layer : m_layers) {
        renderLayer(layer);
    }
}

void GerberView::renderLayer(GerberLayer* layer) {
    if (!layer->isVisible()) return;

    const bool drillLayer = isDrillLayer(layer);
    const QColor layerColor = displayColorForLayer(layer);
    const bool edgeLayer = isEdgeLayer(layer);

    for (const auto& prim : layer->primitives()) {
        GerberAperture ap = layer->getAperture(prim.apertureId);
        
        QColor fillColor = layerColor;
        QColor penColor = layerColor;
        bool isClear = (prim.polarity == GerberPrimitive::Clear);

        if (isClear) {
            fillColor = m_monochrome ? Qt::white : m_backgroundColor;
            penColor = fillColor;
        }

        if (prim.type == GerberPrimitive::Line) {
            double width = ap.params.isEmpty() ? 0.2 : ap.params[0];
            QGraphicsPathItem* item = m_scene->addPath(prim.path);
            item->setPen(QPen(penColor, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            item->setBrush(Qt::NoBrush);
            item->setZValue(edgeLayer ? 50.0 : (isClear ? 15.0 : 10.0));
            item->setVisible(true);
        } else if (prim.type == GerberPrimitive::Region) {
            QGraphicsPathItem* item = m_scene->addPath(prim.path);
            item->setPen(Qt::NoPen);
            item->setBrush(fillColor);
            item->setZValue(edgeLayer ? 50.0 : (isClear ? 16.0 : 11.0));
            item->setVisible(true);
        } else if (prim.type == GerberPrimitive::Flash) {
            double d = ap.params.isEmpty() ? 0.5 : ap.params[0];
            const QColor flashFill = (drillLayer || isClear) ? (m_monochrome ? Qt::white : m_backgroundColor) : layerColor;
            const QColor flashStroke = drillLayer ? (m_monochrome ? QColor(Qt::black) : QColor(190, 190, 190, 80)) : 
                                       (isClear ? flashFill : layerColor);
            
            QPainterPath path;
            if (ap.type == GerberAperture::Circle) {
                path.addEllipse(prim.center, d/2, d/2);
                if (ap.params.size() > 1 && ap.params[1] > 0.001) {
                    double holeD = ap.params[1];
                    path.addEllipse(prim.center, holeD/2, holeD/2);
                    path.setFillRule(Qt::OddEvenFill);
                }
            } else if (ap.type == GerberAperture::Rectangle || ap.type == GerberAperture::Obround) {
                double h = ap.params.size() > 1 ? ap.params[1] : d;
                QRectF rect(prim.center.x() - d/2, prim.center.y() - h/2, d, h);
                if (ap.type == GerberAperture::Obround) {
                    qreal r = std::min(d, h) / 2.0;
                    path.addRoundedRect(rect, r, r);
                } else {
                    path.addRect(rect);
                }
                
                // Handle hole in Rect/Obround (3rd param in template)
                if (ap.params.size() > 2 && ap.params[2] > 0.001) {
                    double holeD = ap.params[2];
                    path.addEllipse(prim.center, holeD/2, holeD/2);
                    path.setFillRule(Qt::OddEvenFill);
                }
            }

            if (!path.isEmpty()) {
                QGraphicsPathItem* item = m_scene->addPath(path);
                item->setBrush(flashFill);
                item->setPen(drillLayer ? QPen(flashStroke, 0.08) : Qt::NoPen);
                item->setZValue(drillLayer ? 100.0 : (isClear ? 17.0 : 12.0));
                item->setVisible(true);
            }
        }
    }
}

void GerberView::zoomFit() {
    if (!m_scene->items().isEmpty()) {
        fitInView(m_scene->itemsBoundingRect(), Qt::KeepAspectRatio);
    }
}

void GerberView::wheelEvent(QWheelEvent* event) {
    const double factor = 1.15;
    if (event->angleDelta().y() > 0) scale(factor, factor);
    else scale(1.0 / factor, 1.0 / factor);
}

void GerberView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_isPanning = true;
        m_lastPanPoint = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void GerberView::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastPanPoint;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        m_lastPanPoint = event->pos();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void GerberView::mouseReleaseEvent(QMouseEvent* event) {
    m_isPanning = false;
    setCursor(Qt::ArrowCursor);
    QGraphicsView::mouseReleaseEvent(event);
}

void GerberView::drawBackground(QPainter* painter, const QRectF& rect) {
    painter->fillRect(rect, m_backgroundColor);
}
