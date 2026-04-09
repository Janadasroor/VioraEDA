#ifndef PCB_SHAPE_ITEM_H
#define PCB_SHAPE_ITEM_H

#include "pcb_item.h"

#include <QSizeF>

class PCBShapeItem : public PCBItem {
public:
    enum class ShapeKind {
        Line,
        Circle,
        Arc
    };

    explicit PCBShapeItem(QGraphicsItem* parent = nullptr);
    PCBShapeItem(ShapeKind kind, const QSizeF& sizeMm, QGraphicsItem* parent = nullptr);

    QString itemTypeName() const override { return "Shape"; }
    ItemType itemType() const override { return ShapeType; }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject& json) override;
    PCBItem* clone() const override;

    ShapeKind shapeKind() const { return m_kind; }
    void setShapeKind(ShapeKind kind);
    QString shapeKindName() const;

    QSizeF sizeMm() const { return m_sizeMm; }
    void setSizeMm(const QSizeF& sizeMm);

    double strokeWidth() const { return m_strokeWidth; }
    void setStrokeWidth(double width);

    double startAngleDeg() const { return m_startAngleDeg; }
    void setStartAngleDeg(double degrees);

    double spanAngleDeg() const { return m_spanAngleDeg; }
    void setSpanAngleDeg(double degrees);

private:
    QRectF localRect() const;
    QPainterPath basePath() const;

    ShapeKind m_kind = ShapeKind::Line;
    QSizeF m_sizeMm = QSizeF(10.0, 10.0);
    double m_strokeWidth = 0.25;
    double m_startAngleDeg = 0.0;
    double m_spanAngleDeg = 180.0;
};

#endif // PCB_SHAPE_ITEM_H
