#ifndef TUNING_SLIDER_ITEM_H
#define TUNING_SLIDER_ITEM_H

#include <QGraphicsItem>
#include <QObject>
#include "schematic_item.h"

/**
 * @brief A visual slider that can be attached to a schematic component to tune its value in real-time.
 */
class TuningSliderItem : public QObject, public QGraphicsItem {
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    explicit TuningSliderItem(SchematicItem* target, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    void setRange(double min, double max);
    void setValue(double val);

    void setFluxVariableName(const QString& name) { m_fluxVarName = name; }
    QString fluxVariableName() const { return m_fluxVarName; }

    void setReactiveScript(const QString& path) { m_scriptPath = path; }
    QString reactiveScript() const { return m_scriptPath; }

Q_SIGNALS:
    void valueChanged(double newVal);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    void updateTargetValue();
    double posToValue(double x) const;
    double valueToPos(double val) const;

    SchematicItem* m_target;
    QString m_fluxVarName;
    QString m_scriptPath;
    double m_min = 0.0;
    double m_max = 100.0;
    double m_currentValue = 50.0;
    bool m_dragging = false;
    
    const double m_width = 100.0;
    const double m_height = 20.0;
};

#endif // TUNING_SLIDER_ITEM_H
