#include "image_item.h"

#include <QBuffer>
#include <QColor>
#include <QPainter>
#include <QPainterPath>

PCBImageItem::PCBImageItem(QGraphicsItem* parent)
    : PCBItem(parent)
    , m_sizeMm(20.0, 20.0) {
}

PCBImageItem::PCBImageItem(const QImage& image, const QSizeF& sizeMm, QGraphicsItem* parent)
    : PCBItem(parent)
    , m_image(image)
    , m_sizeMm(sizeMm) {
    rebuildFabPreview();
}

QRectF PCBImageItem::boundingRect() const {
    return QRectF(-m_sizeMm.width() * 0.5, -m_sizeMm.height() * 0.5, m_sizeMm.width(), m_sizeMm.height());
}

QPainterPath PCBImageItem::shape() const {
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

void PCBImageItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF rect = boundingRect();
    if (!m_fabPreview.isNull()) {
        painter->drawImage(rect, m_fabPreview);
    } else {
        painter->setPen(QPen(QColor(220, 80, 80), 0));
        painter->setBrush(QColor(220, 80, 80, 35));
        painter->drawRect(rect);
        painter->drawLine(rect.topLeft(), rect.bottomRight());
        painter->drawLine(rect.topRight(), rect.bottomLeft());
    }

    if (isSelected()) {
        drawSelectionGlow(painter);
    }
}

QJsonObject PCBImageItem::toJson() const {
    QJsonObject json = PCBItem::toJson();
    json["widthMm"] = m_sizeMm.width();
    json["heightMm"] = m_sizeMm.height();
    json["rotation"] = rotation();
    json["opacity"] = opacity();

    if (!m_image.isNull()) {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        if (buffer.open(QIODevice::WriteOnly)) {
            m_image.save(&buffer, "PNG");
            json["imagePngBase64"] = QString::fromLatin1(bytes.toBase64());
        }
    }

    return json;
}

bool PCBImageItem::fromJson(const QJsonObject& json) {
    PCBItem::fromJson(json);
    m_sizeMm.setWidth(json["widthMm"].toDouble(m_sizeMm.width()));
    m_sizeMm.setHeight(json["heightMm"].toDouble(m_sizeMm.height()));
    setRotation(json["rotation"].toDouble(rotation()));
    setOpacity(json["opacity"].toDouble(opacity()));

    const QByteArray base64 = json["imagePngBase64"].toString().toLatin1();
    if (!base64.isEmpty()) {
        QImage image;
        image.loadFromData(QByteArray::fromBase64(base64), "PNG");
        m_image = image;
    }

    rebuildFabPreview();
    update();
    return true;
}

PCBItem* PCBImageItem::clone() const {
    PCBImageItem* item = new PCBImageItem(m_image, m_sizeMm);
    item->fromJson(toJson());
    return item;
}

void PCBImageItem::setImage(const QImage& image) {
    prepareGeometryChange();
    m_image = image;
    rebuildFabPreview();
    update();
}

void PCBImageItem::setSizeMm(const QSizeF& sizeMm) {
    prepareGeometryChange();
    m_sizeMm = sizeMm;
    update();
}

void PCBImageItem::rebuildFabPreview() {
    m_fabPreview = QImage();
    if (m_image.isNull()) return;

    const QImage src = m_image.convertToFormat(QImage::Format_ARGB32);
    const int w = src.width();
    const int h = src.height();
    if (w <= 0 || h <= 0) return;

    QVector<uchar> mask(w * h, 0);
    auto idx = [w](int x, int y) { return y * w + x; };

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const QColor c = QColor::fromRgba(src.pixel(x, y));
            if (c.alpha() < 20) continue;
            const int gray = qGray(c.rgb());
            if (gray < 200) mask[idx(x, y)] = 1;
        }
    }

    m_fabPreview = QImage(w, h, QImage::Format_ARGB32);
    m_fabPreview.fill(Qt::transparent);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!mask[idx(x, y)]) continue;

            bool edge = (x == 0 || y == 0 || x == w - 1 || y == h - 1);
            if (!edge) {
                edge = !mask[idx(x - 1, y)] || !mask[idx(x + 1, y)] ||
                       !mask[idx(x, y - 1)] || !mask[idx(x, y + 1)];
            }

            if (edge) {
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        const int nx = x + ox;
                        const int ny = y + oy;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                        m_fabPreview.setPixelColor(nx, ny, QColor(12, 12, 12, 255));
                    }
                }
            }
        }
    }
}
