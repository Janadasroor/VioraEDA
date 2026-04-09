#include "gerber_3d_view.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {
QString layerNameLower(const GerberLayer* layer) {
    return layer ? layer->name().toLower() : QString();
}
}

Gerber3DView::Gerber3DView(QWidget* parent)
    : QOpenGLWidget(parent), m_backgroundColor(20, 20, 20) {
    resetView();
}

void Gerber3DView::setLayers(const QList<GerberLayer*>& layers) {
    m_layers = layers;
    fitToBoard(); // Auto-scale when layers change
    update();
}

void Gerber3DView::setComponents(const QList<Model3DInstance>& components) {
    m_components = components;
    update();
}

void Gerber3DView::resetView() {
    m_fov = 45.0f;
    m_cameraPos = QVector3D(0, 0, 100.0f);
    m_rotation = QVector2D(-45.0f, 25.0f); // Pitch and Yaw
    m_boardCenter = QPointF(0, 0);
    updateViewMatrix();
    update();
}

void Gerber3DView::setCameraPos(const QVector3D& pos) {
    m_cameraPos = pos;
    updateViewMatrix();
    update();
}

void Gerber3DView::setCameraRotation(const QVector2D& rot) {
    m_rotation = rot;
    updateViewMatrix();
    update();
}

void Gerber3DView::setBackgroundColor(const QColor& color) {
    if (!color.isValid() || color == m_backgroundColor) {
        return;
    }

    m_backgroundColor = color;
    update();
}

bool Gerber3DView::isEdgeLayer(const GerberLayer* layer) const {
    const QString name = layerNameLower(layer);
    return name.contains("edge") || name.contains("outline") || name.contains(".gm1") || name.contains(".gko");
}

bool Gerber3DView::isBottomLayer(const GerberLayer* layer) const {
    const QString name = layerNameLower(layer);
    return name.contains("gbl") || name.contains("gbo") || name.contains("gbs") ||
           name.contains("bottom");
}

bool Gerber3DView::isMaskLayer(const GerberLayer* layer) const {
    const QString name = layerNameLower(layer);
    return name.contains("mask") || name.contains("gts") || name.contains("gbs");
}

QColor Gerber3DView::layerColor(const GerberLayer* layer) const {
    const QString name = layerNameLower(layer);
    if (name.contains("gbl") || (name.contains("bottom") && name.contains("copper"))) {
        return QColor(59, 130, 246);
    }
    if (name.contains("cu") || name.contains("gtl") || (name.contains("top") && name.contains("copper"))) {
        return QColor(184, 115, 51);
    }
    if (name.contains("silk") || name.contains("legend")) {
        return QColor(245, 245, 240);
    }
    if (name.contains("mask")) {
        return QColor(0, 100, 0, 150);
    }
    if (isEdgeLayer(layer)) {
        return QColor(255, 208, 92);
    }
    if (name.contains("drill") || name.contains(".drl")) {
        return QColor(28, 28, 28);
    }
    return layer ? layer->color() : QColor(Qt::white);
}

void Gerber3DView::updateViewMatrix() {
    m_viewMatrix.setToIdentity();
    m_viewMatrix.translate(0, 0, -m_cameraPos.z());
    m_viewMatrix.rotate(m_rotation.x(), 1, 0, 0); // Pitch
    m_viewMatrix.rotate(m_rotation.y(), 0, 0, 1); // Row/Yaw
    m_viewMatrix.translate(-m_cameraPos.x(), -m_cameraPos.y(), 0);
    m_viewMatrix.translate(-m_boardCenter.x(), -m_boardCenter.y(), 0);
}

void Gerber3DView::fitToBoard() {
    QRectF bounds;
    if (m_layers.isEmpty()) {
        bounds = QRectF(-50, -50, 100, 100);
    } else {
        bool first = true;
        for (auto* layer : m_layers) {
            for (const auto& prim : layer->primitives()) {
                QRectF r = prim.path.boundingRect();
                if (prim.type == GerberPrimitive::Flash) {
                    r = QRectF(prim.center.x()-1, prim.center.y()-1, 2, 2); 
                }
                
                if (first) { bounds = r; first = false; }
                else bounds = bounds.united(r);
            }
        }
    }
    
    if (bounds.width() < 1 || bounds.height() < 1) bounds = QRectF(-50, -50, 100, 100);

    m_boardCenter = bounds.center();

    float maxDim = qMax(bounds.width(), bounds.height());
    float dist = (maxDim * 1.2f) / (2.0f * qTan(qDegreesToRadians(m_fov / 2.0f)));
    
    m_cameraPos.setZ(qBound(10.0f, dist, 5000.0f));
    updateViewMatrix();
    update();
}

void Gerber3DView::zoomIn() {
    m_cameraPos.setZ(m_cameraPos.z() * 0.8f);
    updateViewMatrix();
    update();
}

void Gerber3DView::zoomOut() {
    m_cameraPos.setZ(m_cameraPos.z() * 1.25f);
    updateViewMatrix();
    update();
}

void Gerber3DView::initializeGL() {
}

void Gerber3DView::resizeGL(int w, int h) {
    m_projection.setToIdentity();
    m_projection.perspective(m_fov, (float)w/h, 0.1f, 10000.0f);
}

void Gerber3DView::paintGL() {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    painter.fillRect(rect(), m_backgroundColor);

    // Find the board outline from Edge Cuts layer
    QPainterPath boardOutline;
    GerberLayer* edgeLayer = nullptr;
    
    for (auto* layer : m_layers) {
        QString name = layer->name().toLower();
        if (name.contains("edge") || name.contains("outline") || name.contains(".gm1") || name.contains(".gko")) {
            edgeLayer = layer;
            break;
        }
    }

    if (edgeLayer && !edgeLayer->primitives().isEmpty()) {
        for (const auto& prim : edgeLayer->primitives()) {
            if (boardOutline.isEmpty()) {
                boardOutline = prim.path;
            } else {
                boardOutline.addPath(prim.path);
            }
        }
    } else {
        QRectF bounds;
        bool hasAny = false;
        for (auto* layer : m_layers) {
            for (const auto& prim : layer->primitives()) {
                if (!hasAny) { bounds = prim.path.boundingRect(); hasAny = true; }
                else bounds = bounds.united(prim.path.boundingRect());
            }
        }
        if (hasAny) boardOutline.addRect(bounds.adjusted(-0.5, -0.5, 0.5, 0.5));
        else boardOutline.addRect(QRectF(-50, -50, 100, 100));
    }

    const float boardThickness = 1.6f;
    const float topZ = boardThickness * 0.5f;
    const float bottomZ = -boardThickness * 0.5f;

    QMatrix4x4 rotation;
    rotation.rotate(qBound(-78.0f, m_rotation.x(), -18.0f), 1.0f, 0.0f, 0.0f);
    rotation.rotate(m_rotation.y(), 0.0f, 0.0f, 1.0f);

    auto projectModel = [&](const QVector3D& modelPoint) {
        QVector3D local(modelPoint.x() - float(m_boardCenter.x()) - m_cameraPos.x(),
                        modelPoint.y() - float(m_boardCenter.y()) - m_cameraPos.y(),
                        modelPoint.z());
        return rotation.map(local);
    };

    auto collectBoundsAtZ = [&](float z) {
        QRectF bounds;
        bool first = true;
        const QList<QPolygonF> polys = boardOutline.toFillPolygons();
        for (const QPolygonF& poly : polys) {
            for (const QPointF& pt : poly) {
                const QVector3D p = projectModel(QVector3D(float(pt.x()), float(pt.y()), z));
                const QPointF screen(p.x(), -p.y());
                if (first) {
                    bounds = QRectF(screen, QSizeF(0, 0));
                    first = false;
                } else {
                    bounds = bounds.united(QRectF(screen, QSizeF(0, 0)));
                }
            }
        }
        return first ? QRectF(-50, -50, 100, 100) : bounds;
    };

    const QRectF projectedBounds = collectBoundsAtZ(topZ).united(collectBoundsAtZ(bottomZ));
    const qreal scale = qMin(width() / qMax(1.0, projectedBounds.width() * 1.25),
                             height() / qMax(1.0, projectedBounds.height() * 1.25));
    const QPointF projectedCenter = projectedBounds.center();
    const QPointF viewportCenter(width() * 0.5, height() * 0.5);

    auto projectPoint = [&](const QVector3D& p) {
        const QVector3D rp = projectModel(p);
        return QPointF((rp.x() - projectedCenter.x()) * scale + viewportCenter.x(),
                       (-rp.y() - projectedCenter.y()) * scale + viewportCenter.y());
    };

    auto projectPolygon = [&](const QPolygonF& poly, float z) {
        QPolygonF out;
        out.reserve(poly.size());
        for (const QPointF& pt : poly) {
            out << projectPoint(QVector3D(float(pt.x()), float(pt.y()), z));
        }
        return out;
    };

    auto primitiveFillPath = [](const GerberPrimitive& prim, const GerberLayer* layer) {
        QPainterPath path;
        if (prim.type == GerberPrimitive::Line) {
            GerberAperture ap = layer->getAperture(prim.apertureId);
            const qreal width = ap.params.isEmpty() ? 0.2 : ap.params[0];
            QPainterPathStroker stroker;
            stroker.setWidth(width);
            stroker.setCapStyle(Qt::RoundCap);
            stroker.setJoinStyle(Qt::RoundJoin);
            path = stroker.createStroke(prim.path);
        } else if (prim.type == GerberPrimitive::Region) {
            path = prim.path;
        } else if (prim.type == GerberPrimitive::Flash) {
            GerberAperture ap = layer->getAperture(prim.apertureId);
            const qreal d = ap.params.isEmpty() ? 0.5 : ap.params[0];
            if (ap.type == GerberAperture::Rectangle) {
                const qreal h = ap.params.size() > 1 ? ap.params[1] : d;
                path.addRect(QRectF(prim.center.x() - d / 2, prim.center.y() - h / 2, d, h));
            } else if (ap.type == GerberAperture::Obround) {
                const qreal h = ap.params.size() > 1 ? ap.params[1] : d;
                const qreal r = std::min(d, h) * 0.5;
                path.addRoundedRect(QRectF(prim.center.x() - d / 2, prim.center.y() - h / 2, d, h), r, r);
            } else {
                path.addEllipse(prim.center, d / 2, d / 2);
            }
        }
        return path;
    };

    const QList<QPolygonF> boardPolys = boardOutline.toFillPolygons();
    painter.setPen(Qt::NoPen);

    for (const QPolygonF& poly : boardPolys) {
        if (poly.size() < 3) {
            continue;
        }

        const QPolygonF topFace = projectPolygon(poly, topZ);
        const QPolygonF bottomFace = projectPolygon(poly, bottomZ);

        painter.setBrush(QColor(20, 34, 18, 235));
        painter.drawPolygon(bottomFace);

        for (int i = 1; i < poly.size(); ++i) {
            QPolygonF side;
            side << topFace[i - 1] << topFace[i] << bottomFace[i] << bottomFace[i - 1];
            painter.setBrush(QColor(60, 78, 46, 240));
            painter.drawPolygon(side);
        }

        painter.setBrush(QColor(42, 96, 48, 248));
        painter.drawPolygon(topFace);
    }

    auto drawLayerGroup = [&](bool bottom) {
        const float z = bottom ? bottomZ - 0.03f : topZ + 0.03f;
        for (auto* layer : m_layers) {
            if (!layer || !layer->isVisible() || isEdgeLayer(layer) || isBottomLayer(layer) != bottom) {
                continue;
            }

            QColor color = layerColor(layer);
            if (!isMaskLayer(layer)) {
                color.setAlpha(255);
            }
            painter.setBrush(color);

            for (const auto& prim : layer->primitives()) {
                const QList<QPolygonF> polys = primitiveFillPath(prim, layer).toFillPolygons();
                for (const QPolygonF& poly : polys) {
                    if (poly.size() >= 3) {
                        painter.drawPolygon(projectPolygon(poly, z));
                    }
                }
            }
        }
    };

    drawLayerGroup(true);
    drawLayerGroup(false);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 214, 120), 1.5));
    for (const QPolygonF& poly : boardPolys) {
        if (poly.size() >= 2) {
            painter.drawPolyline(projectPolygon(poly, topZ + 0.04f));
        }
    }

    // Render Components (Simple 3D Boxes)
    for (const auto& comp : m_components) {
        painter.save();
        const QPointF center = projectPoint(QVector3D(float(comp.pos.x()), float(comp.pos.y()), topZ + 1.8f));
        painter.translate(center);
        painter.rotate(comp.rotation);
        
        const double w = (comp.size.x() > 0 ? comp.size.x() : 5.0) * scale;
        const double h = (comp.size.y() > 0 ? comp.size.y() : 5.0) * scale;
        
        painter.setPen(QPen(Qt::black, 1.0));
        painter.setBrush(QColor(40, 40, 40, 240)); 
        painter.drawRect(QRectF(-w / 2, -h / 2, w, h));
        
        painter.setBrush(QColor(200, 200, 200));
        painter.drawEllipse(QPointF(-w / 2 + scale * 0.5, -h / 2 + scale * 0.5), scale * 0.5, scale * 0.5);
        
        painter.restore();
    }
}

void Gerber3DView::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->pos();
}

void Gerber3DView::mouseMoveEvent(QMouseEvent* event) {
    QPoint delta = event->pos() - m_lastMousePos;
    
    if (event->buttons() & Qt::LeftButton) {
        if (event->modifiers() & Qt::ShiftModifier) {
            // Pan
            float visibleHeight = 2.0f * m_cameraPos.z() * qTan(qDegreesToRadians(m_fov / 2.0f));
            float sensitivity = visibleHeight / height();
            m_cameraPos.setX(m_cameraPos.x() - delta.x() * sensitivity);
            m_cameraPos.setY(m_cameraPos.y() + delta.y() * sensitivity);
        } else {
            // Rotate
            m_rotation.setY(m_rotation.y() + delta.x() * 0.5f);
            m_rotation.setX(qBound(-90.0f, m_rotation.x() - delta.y() * 0.5f, 0.0f));
        }
        updateViewMatrix();
        update();
    }
    m_lastMousePos = event->pos();
}

void Gerber3DView::keyPressEvent(QKeyEvent* event) {
    float moveStep = 5.0f;
    if (event->key() == Qt::Key_Left) m_cameraPos.setX(m_cameraPos.x() - moveStep);
    else if (event->key() == Qt::Key_Right) m_cameraPos.setX(m_cameraPos.x() + moveStep);
    else if (event->key() == Qt::Key_Up) m_cameraPos.setY(m_cameraPos.y() + moveStep);
    else if (event->key() == Qt::Key_Down) m_cameraPos.setY(m_cameraPos.y() - moveStep);
    else if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) zoomIn();
    else if (event->key() == Qt::Key_Minus) zoomOut();
    else {
        QOpenGLWidget::keyPressEvent(event);
        return;
    }
    updateViewMatrix();
    update();
}

void Gerber3DView::wheelEvent(QWheelEvent* event) {
    if (event->angleDelta().y() > 0) zoomIn();
    else zoomOut();
}
