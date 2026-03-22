#include "schematic_zoom_area_tool.h"
#include "schematic_view.h"
#include <QGraphicsScene>
#include <QPixmap>

namespace {
QPixmap tintToBlack(const QPixmap& src) {
    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const int a = qAlpha(line[x]);
            if (a == 0) continue;
            line[x] = qRgba(0, 0, 0, a);
        }
    }
    return QPixmap::fromImage(img);
}

QCursor makeToolCursor(const QString& iconPath, const QCursor& fallback) {
    QPixmap pm(iconPath);
    if (!pm.isNull()) {
        QPixmap scaled = pm.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap black = tintToBlack(scaled);
        return QCursor(black, black.width() / 2, black.height() / 2);
    }
    return fallback;
}
}

SchematicZoomAreaTool::SchematicZoomAreaTool(QObject* parent)
    : SchematicTool("Zoom Area", parent)
    , m_rubberBand(nullptr) {
    m_zoomInCursor = makeToolCursor(":/icons/view_zoom_in.svg", QCursor(Qt::CrossCursor));
    m_zoomOutCursor = makeToolCursor(":/icons/view_zoom_out.svg", QCursor(Qt::CrossCursor));
}

SchematicZoomAreaTool::~SchematicZoomAreaTool() {
    if (m_rubberBand) delete m_rubberBand;
}

void SchematicZoomAreaTool::activate(SchematicView* view) {
    SchematicTool::activate(view);
    if (view) view->setCursor(cursor());
}

void SchematicZoomAreaTool::deactivate() {
    if (m_rubberBand) {
        m_rubberBand->hide();
    }
    if (view()) view()->unsetCursor();
    SchematicTool::deactivate();
}

void SchematicZoomAreaTool::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton) return;

    m_origin = event->pos();
    if (event->button() == Qt::LeftButton && m_defaultMode == ZoomMode::ZoomOut) {
        const QPointF scenePos = view()->mapToScene(event->pos());
        const double scaleFactor = 1.2;
        view()->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        view()->centerOn(scenePos);
        if (view()) view()->setCursor(m_zoomOutCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (!m_rubberBand) {
            m_rubberBand = new QRubberBand(QRubberBand::Rectangle, view());
        }
        m_rubberBand->setGeometry(QRect(m_origin, QSize()));
        m_rubberBand->show();
        if (view()) view()->setCursor(m_zoomInCursor);
    } else if (event->button() == Qt::RightButton) {
        if (view()) view()->setCursor(m_zoomOutCursor);
    }
    event->accept();
}

void SchematicZoomAreaTool::mouseMoveEvent(QMouseEvent* event) {
    if (m_rubberBand) {
        m_rubberBand->setGeometry(QRect(m_origin, event->pos()).normalized());
        event->accept();
    }
}

void SchematicZoomAreaTool::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton) return;

    if (event->button() == Qt::LeftButton && m_defaultMode == ZoomMode::ZoomOut) {
        if (view()) view()->setCursor(m_zoomOutCursor);
        event->accept();
        return;
    }

    const QPoint releasePos = event->pos();
    const int dx = std::abs(releasePos.x() - m_origin.x());
    const int dy = std::abs(releasePos.y() - m_origin.y());

    if (event->button() == Qt::RightButton) {
        // Right click = zoom out at cursor
        const QPointF scenePos = view()->mapToScene(event->pos());
        const double scaleFactor = 1.2;
        view()->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        view()->centerOn(scenePos);
        if (view()) view()->setCursor(m_zoomInCursor);
        event->accept();
        return;
    }

    if (m_rubberBand) {
        QRect rect = m_rubberBand->geometry();
        m_rubberBand->hide();

        if (rect.width() > 5 && rect.height() > 5) {
            QRectF sceneRect = view()->mapToScene(rect).boundingRect();
            view()->fitInView(sceneRect, Qt::KeepAspectRatio);
        } else {
            // Click = zoom in at cursor
            const QPointF scenePos = view()->mapToScene(event->pos());
            const double scaleFactor = 1.2;
            view()->scale(scaleFactor, scaleFactor);
            view()->centerOn(scenePos);
        }
        if (view()) view()->setCursor(m_zoomInCursor);
        event->accept();
    }
}

QCursor SchematicZoomAreaTool::cursor() const {
    return (m_defaultMode == ZoomMode::ZoomOut) ? m_zoomOutCursor : m_zoomInCursor;
}
