#ifndef SCHEMATIC_MINIMAP_H
#define SCHEMATIC_MINIMAP_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>

class SchematicView;

class SchematicMiniMap : public QGraphicsView {
    Q_OBJECT
public:
    explicit SchematicMiniMap(SchematicView* parentView, QWidget* parent = nullptr);

    void updateViewportRect();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void syncToMainView(const QPoint& pos);

    SchematicView* m_mainView;
    QGraphicsRectItem* m_viewportRectItem = nullptr;
    bool m_isDragging;
};

#endif // SCHEMATIC_MINIMAP_H
