#ifndef PCB_SHAPE_TOOL_H
#define PCB_SHAPE_TOOL_H

#include "pcb_tool.h"

class QGraphicsPathItem;
class QGraphicsEllipseItem;
class QGraphicsSimpleTextItem;

class PCBShapeTool : public PCBTool {
    Q_OBJECT

public:
    enum class ShapeKind {
        Line,
        Circle,
        Arc
    };

    PCBShapeTool(const QString& toolName, ShapeKind kind, QObject* parent = nullptr);

    QString tooltip() const override;
    QCursor cursor() const override { return QCursor(Qt::CrossCursor); }
    QMap<QString, QVariant> toolProperties() const override;
    void setToolProperty(const QString& name, const QVariant& value) override;

    void activate(PCBView* view) override;
    void deactivate() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void updatePreview(const QPointF& pos);
    void finishShape(const QPointF& pos);
    void updateMeasurementLabel(const QPointF& pos);
    void clearMeasurementLabel();
    bool findMagneticEndpoint(const QPointF& pos, QPointF* snappedPos) const;
    QPointF snapToMagneticEndpoint(const QPointF& pos) const;
    void updateSnapIndicator(const QPointF& pos);
    void clearSnapIndicator();

    ShapeKind m_shapeKind;
    bool m_isDrawing = false;
    QPointF m_origin;
    QGraphicsPathItem* m_previewPath = nullptr;
    QGraphicsEllipseItem* m_snapIndicator = nullptr;
    QGraphicsSimpleTextItem* m_measureLabel = nullptr;
    int m_layerId = 0;
    double m_strokeWidth = 0.25;
    double m_arcStartAngleDeg = 0.0;
    double m_arcSpanAngleDeg = 180.0;
};

#endif // PCB_SHAPE_TOOL_H
