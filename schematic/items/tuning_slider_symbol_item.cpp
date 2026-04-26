#include "tuning_slider_symbol_item.h"
#include "../editor/schematic_editor.h"
#include "../ui/simulation_panel.h"
#include "../core/flux_workspace_bridge.h"
#include "../core/jit_context_manager.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <cmath>

TuningSliderSymbolItem::TuningSliderSymbolItem(QPointF pos, QGraphicsItem* parent)
    : SchematicItem(parent) {
    setPos(pos);
    setReference("A");
    setValue("50");
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
}

TuningSliderSymbolItem::~TuningSliderSymbolItem() {
}

QRectF TuningSliderSymbolItem::boundingRect() const {
    return QRectF(-5, -5, m_width + 10, m_height + 10);
}

void TuningSliderSymbolItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);

    QRectF rect(0, 0, m_width, m_height);
    
    // Instrument Body (Professional Slate Gray)
    painter->setBrush(QColor(45, 45, 55));
    painter->setPen(QPen(isSelected() ? QColor(59, 130, 246) : Qt::white, 2));
    painter->drawRoundedRect(rect.adjusted(1, 1, -1, -1), 5, 5);

    // Top Header Accent
    painter->setBrush(QColor(60, 60, 70));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(QRectF(1, 1, m_width - 2, 16), 4, 4);
    painter->drawRect(QRectF(1, 10, m_width - 2, 7)); // Flatten bottom of curve

    // Title / Parameter Name (Glowing Green)
    painter->setPen(QColor(0, 255, 100));
    QFont f = painter->font(); f.setPointSize(8); f.setBold(true);
    painter->setFont(f);
    painter->drawText(QRectF(8, 2, m_width - 16, 15), Qt::AlignLeft | Qt::AlignVCenter, reference());

    // Digital Readout Box
    painter->setBrush(QColor(10, 20, 10)); // Dark screen color
    painter->setPen(QPen(QColor(100, 100, 110), 1));
    QRectF readout(m_width - 45, 3, 40, 12);
    painter->drawRoundedRect(readout, 2, 2);

    // Value text in Readout
    painter->setPen(QColor(0, 255, 100));
    f.setPointSize(7); f.setBold(false);
    painter->setFont(f);
    QString valStr = QString::number(m_current, 'g', 4);
    painter->drawText(readout.adjusted(0, 0, -3, 0), Qt::AlignRight | Qt::AlignVCenter, valStr);

    // Slider Track Area
    painter->setBrush(QColor(25, 25, 30));
    painter->setPen(QPen(QColor(80, 80, 90), 1));
    painter->drawRoundedRect(QRectF(10, 20, m_width - 20, 10), 3, 3);

    // Track Line
    painter->setPen(QPen(QColor(255, 255, 255, 30), 1));
    painter->drawLine(15, 25, m_width - 15, 25);

    // Handle (Metallic Blue Knob)
    double handleX = valueToPos(m_current);
    QRectF handleRect(handleX - 4, 18, 8, 14);
    
    QLinearGradient knobGrad(handleRect.topLeft(), handleRect.bottomRight());
    knobGrad.setColorAt(0, QColor(100, 180, 255));
    knobGrad.setColorAt(0.5, QColor(59, 130, 246));
    knobGrad.setColorAt(1, QColor(30, 80, 200));
    
    painter->setBrush(knobGrad);
    painter->setPen(QPen(Qt::white, 1));
    painter->drawRoundedRect(handleRect, 2, 2);
    
    // Handle Center Line
    painter->setPen(QPen(Qt::white, 1));
    painter->drawLine(handleX, 20, handleX, 30);
}

void TuningSliderSymbolItem::setCurrentValue(double v) {
    m_current = qBound(m_min, v, m_max);
    setValue(QString::number(m_current));
    
    // Update Flux Variable if set
    if (!m_fluxVarName.isEmpty()) {
        Flux::Core::FluxWorkspaceBridge::setVariable(m_fluxVarName, m_current);
        
        // Execute reactive script if linked
        if (!m_scriptPath.isEmpty()) {
            QFile f(m_scriptPath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString source = QTextStream(&f).readAll();
                QMap<int, QString> errors;
                // Use a unique ID based on pointer to allow multiple active sliders
                QString jitId = QString("slider_%1").arg(reinterpret_cast<quintptr>(this));
                if (Flux::JITContextManager::instance().compileAndLoad("standalone_" + jitId, source, errors)) {
                    void* addr = Flux::JITContextManager::instance().getFunctionAddress("standalone_" + jitId);
                    if (addr) {
                        typedef void (*RunFunc)();
                        reinterpret_cast<RunFunc>(addr)();
                    }
                }
            }
        }
    }

    update();
    triggerRealTimeUpdate();
}

void TuningSliderSymbolItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (QRectF(0, 16, m_width, 19).contains(event->pos())) {
        m_dragging = true;
        setCurrentValue(posToValue(event->pos().x()));
        event->accept();
    } else {
        SchematicItem::mousePressEvent(event);
    }
}

void TuningSliderSymbolItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (m_dragging) {
        setCurrentValue(posToValue(event->pos().x()));
        event->accept();
    } else {
        SchematicItem::mouseMoveEvent(event);
    }
}

void TuningSliderSymbolItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    m_dragging = false;
    SchematicItem::mouseReleaseEvent(event);
}

double TuningSliderSymbolItem::posToValue(double x) const {
    double rel = qBound(0.0, (x - 15.0) / (m_width - 30.0), 1.0);
    return m_min + rel * (m_max - m_min);
}

double TuningSliderSymbolItem::valueToPos(double val) const {
    if (m_max <= m_min) return 15.0;
    double rel = (val - m_min) / (m_max - m_min);
    return 15.0 + rel * (m_width - 30.0);
}

void TuningSliderSymbolItem::triggerRealTimeUpdate() {
    auto* editor = qobject_cast<SchematicEditor*>(QApplication::activeWindow());
    if (editor && editor->getSimulationPanel() && editor->getSimulationPanel()->isRealTimeMode()) {
        editor->getSimulationPanel()->onRunSimulation();
    }
}

QJsonObject TuningSliderSymbolItem::toJson() const {
    QJsonObject j = SchematicItem::toJson();
    j["type"] = "TuningSlider";
    j["min"] = m_min;
    j["max"] = m_max;
    j["current"] = m_current;
    j["fluxVar"] = m_fluxVarName;
    j["scriptPath"] = m_scriptPath;
    return j;
}

bool TuningSliderSymbolItem::fromJson(const QJsonObject& json) {
    if (!SchematicItem::fromJson(json)) return false;
    m_min = json["min"].toDouble(0.0);
    m_max = json["max"].toDouble(100.0);
    m_current = json["current"].toDouble(50.0);
    m_fluxVarName = json["fluxVar"].toString();
    m_scriptPath = json["scriptPath"].toString();
    return true;
}

SchematicItem* TuningSliderSymbolItem::clone() const {
    auto* item = new TuningSliderSymbolItem(pos());
    item->fromJson(toJson());
    return item;
}
