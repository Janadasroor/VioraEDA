#ifndef IMAGE_PREVIEW_PANEL_H
#define IMAGE_PREVIEW_PANEL_H

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

class ImagePreviewPanel : public QWidget {
    Q_OBJECT
public:
    explicit ImagePreviewPanel(QWidget* parent = nullptr);
    bool loadImage(const QString& filePath);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void zoom(double factor, QPointF center);
    void fitToView();

    QGraphicsView* m_view;
    QGraphicsScene* m_scene;
    QGraphicsPixmapItem* m_pixmapItem;
};

#endif // IMAGE_PREVIEW_PANEL_H
