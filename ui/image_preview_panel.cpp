#include "image_preview_panel.h"
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QScrollBar>
#include <QFileInfo>
#include <QImageReader>
#include <QTimer>

ImagePreviewPanel::ImagePreviewPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_view = new QGraphicsView(this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setStyleSheet("background-color: #1a1a1e; border: none;");

    m_scene = new QGraphicsScene(this);
    m_view->setScene(m_scene);
    
    m_pixmapItem = new QGraphicsPixmapItem();
    m_scene->addItem(m_pixmapItem);

    layout->addWidget(m_view);
}

bool ImagePreviewPanel::loadImage(const QString& filePath) {
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    
    if (image.isNull()) return false;

    m_pixmapItem->setPixmap(QPixmap::fromImage(image));
    m_scene->setSceneRect(image.rect());
    
    QTimer::singleShot(0, this, &ImagePreviewPanel::fitToView);
    return true;
}

void ImagePreviewPanel::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        double angle = event->angleDelta().y();
        double factor = pow(1.2, angle / 240.0);
        zoom(factor, event->position());
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void ImagePreviewPanel::zoom(double factor, QPointF center) {
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->scale(factor, factor);
}

void ImagePreviewPanel::fitToView() {
    if (m_pixmapItem->pixmap().isNull()) return;
    m_view->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
}

void ImagePreviewPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // Optional: auto-fit on first show, but maybe not on every resize to avoid jumping while zooming
}
