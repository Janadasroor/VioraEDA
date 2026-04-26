#ifndef FLUX_MEASUREMENT_ITEM_H
#define FLUX_MEASUREMENT_ITEM_H

#include "schematic_item.h"
#include <QFont>

class FluxMeasurementItem : public SchematicItem {
    Q_OBJECT
public:
    FluxMeasurementItem(QPointF pos = QPointF(), QGraphicsItem* parent = nullptr);
    virtual ~FluxMeasurementItem() = default;

    QString itemTypeName() const override { return "Flux Measurement Probe"; }
    ItemType itemType() const override { return LabelType; }
    
    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject& json) override;
    
    SchematicItem* clone() const override;
    
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // Expression to evaluate (e.g. "return max(V('out'));")
    void setExpression(const QString& expr);
    QString expression() const { return m_expression; }

    void setScriptFile(const QString& path);
    QString scriptFile() const { return m_scriptFile; }

    // Formatted result (e.g. "3.3 V")
    void setResult(const QString& res);
    QString result() const { return m_result; }
    
    void setPrefix(const QString& pre);
    QString prefix() const { return m_prefix; }
    
    void setUnit(const QString& unit);
    QString unit() const { return m_unit; }

    void openPropertiesDialog();

protected:
    void mouseDoubleClickEvent(class QGraphicsSceneMouseEvent* event) override;

private:
    void updateSize();

    QString m_expression;
    QString m_scriptFile;
    QString m_result = "N/A";
    QString m_prefix = "Val";
    QString m_unit = "";
    
    QSizeF m_size;
    QFont m_font;
};

#endif // FLUX_MEASUREMENT_ITEM_H
