#ifndef PCB_IMAGE_ITEM_H
#define PCB_IMAGE_ITEM_H

#include "pcb_item.h"
#include <QImage>

class PCBImageItem : public PCBItem {
    Q_OBJECT

public:
    explicit PCBImageItem(QGraphicsItem* parent = nullptr);
    PCBImageItem(const QImage& image, const QSizeF& sizeMm, QGraphicsItem* parent = nullptr);

    QString itemTypeName() const override { return "Image"; }
    ItemType itemType() const override { return ImageType; }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject& json) override;
    PCBItem* clone() const override;

    void setImage(const QImage& image);
    const QImage& image() const { return m_image; }
    const QImage& fabPreview() const { return m_fabPreview; }

    void setSizeMm(const QSizeF& sizeMm);
    QSizeF sizeMm() const { return m_sizeMm; }

private:
    void rebuildFabPreview();

    QImage m_image;
    QSizeF m_sizeMm;
    QImage m_fabPreview;
};

#endif // PCB_IMAGE_ITEM_H
