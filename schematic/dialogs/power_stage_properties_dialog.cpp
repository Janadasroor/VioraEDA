#include "power_stage_properties_dialog.h"
#include "../items/generic_component_item.h"
#include "../items/schematic_spice_directive_item.h"
#include "../editor/schematic_commands.h"
#include "../../ui/mos_circuit_architect.h"
#include "../../simulator/core/sim_value_parser.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QGraphicsScene>
#include <QRegularExpression>
#include <QDebug>

PowerStagePropertiesDialog::PowerStagePropertiesDialog(GenericComponentItem* item, QUndoStack* undoStack, QGraphicsScene* scene, QWidget* parent)
    : SmartPropertiesDialog({item}, undoStack, scene, parent), m_genItem(item) {
    
    setWindowTitle("Power Stage Properties - " + item->reference());
    m_subcktName = item->value();

    PropertyTab paramsTab;
    paramsTab.title = "Parameters";
    
    // Find parameters in the subcircuit directive
    QMap<QString, QString> currentParams;
    if (scene) {
        for (auto* it : scene->items()) {
            if (auto* directive = dynamic_cast<SchematicSpiceDirectiveItem*>(it)) {
                QString txt = directive->text();
                if (txt.contains(".subckt " + m_subcktName, Qt::CaseInsensitive)) {
                    // Primitive parameter extraction from PWL lines is hard, 
                    // but we can look for specific B-source or M-instance params
                    QRegularExpression reW("W=([^\\s\\n]+)");
                    QRegularExpression reL("L=([^\\s\\n]+)");
                    auto mW = reW.match(txt);
                    if (mW.hasMatch()) currentParams["W"] = mW.captured(1);
                    auto mL = reL.match(txt);
                    if (mL.hasMatch()) currentParams["L"] = mL.captured(1);
                    
                    QRegularExpression reVgate("V\\(ctrl_high, gnd_node\\) \\* ([^\\s\\n]+)");
                    auto mV = reVgate.match(txt);
                    if (mV.hasMatch()) currentParams["VGATE"] = mV.captured(1);
                }
            }
        }
    }

    paramsTab.fields.append({"reference", "Reference", PropertyField::Text, item->reference()});
    paramsTab.fields.append({"value", "Model/Subckt", PropertyField::Text, item->value()});
    
    // Advanced Power Stage Params (Common to Half-Bridge/Matrix)
    paramsTab.fields.append({"VGATE", "Gate Drive Voltage", PropertyField::EngineeringValue, currentParams.value("VGATE", "15"), {}, "V"});
    paramsTab.fields.append({"W", "MOS Width", PropertyField::EngineeringValue, currentParams.value("W", "10u"), {}, "m"});
    paramsTab.fields.append({"L", "MOS Length", PropertyField::EngineeringValue, currentParams.value("L", "1u"), {}, "m"});
    
    addTab(paramsTab);

    // Waveform Delegation
    QWidget* redrawPage = new QWidget();
    QVBoxLayout* redrawLayout = new QVBoxLayout(redrawPage);
    redrawLayout->addStretch();
    
    auto* redrawBtn = new QPushButton("Redraw Waveform (Launch Architect)");
    redrawBtn->setMinimumHeight(50);
    redrawBtn->setStyleSheet("font-weight: bold; background-color: #1e293b; color: #3b82f6; border: 1px solid #3b82f6;");
    connect(redrawBtn, &QPushButton::clicked, this, &PowerStagePropertiesDialog::onRedrawWaveform);
    
    redrawLayout->addWidget(redrawBtn);
    redrawLayout->addWidget(new QLabel("This will reopen the MOS Circuit Architect with current waveform points."));
    redrawLayout->addStretch();
    
    m_tabWidget->addTab(redrawPage, "Waveform");
}

void PowerStagePropertiesDialog::onRedrawWaveform() {
    // 1. Find the directive containing the PWL points
    QString subckt;
    SchematicSpiceDirectiveItem* directiveItem = nullptr;
    if (m_scene) {
        for (auto* it : m_scene->items()) {
            if (auto* d = dynamic_cast<SchematicSpiceDirectiveItem*>(it)) {
                if (d->text().contains(".subckt " + m_subcktName, Qt::CaseInsensitive)) {
                    subckt = d->text();
                    directiveItem = d;
                    break;
                }
            }
        }
    }

    // 2. Extract PWL points
    QVector<QPointF> points;
    QRegularExpression rePwl("\\+ ([^\\s]+) ([^\\s\\n]+)");
    auto it = rePwl.globalMatch(subckt);
    double tMax = 0;
    while (it.hasNext()) {
        auto match = it.next();
        double t = 0, y = 0;
        SimValueParser::parseSpiceNumber(match.captured(1).toStdString(), t);
        SimValueParser::parseSpiceNumber(match.captured(2).toStdString(), y);
        points.append(QPointF(t, y));
        if (t > tMax) tMax = t;
    }
    
    // Normalize time for the architect (0..1)
    if (tMax > 0) {
        for (auto& p : points) p.setX(p.x() / tMax);
    }

    // 3. Launch Architect
    auto* arch = new MosCircuitArchitect(static_cast<QWidget*>(parent()));
    arch->setPoints(points);
    arch->show();
    
    accept(); // Close this properties dialog
}

void PowerStagePropertiesDialog::onApply() {
    m_undoStack->beginMacro("Update Power Stage");
    
    QJsonObject newState = m_genItem->toJson();
    newState["reference"] = getPropertyValue("reference").toString();
    newState["value"] = getPropertyValue("value").toString();
    
    // Update the subcircuit directive if params changed
    if (m_scene) {
        for (auto* it : m_scene->items()) {
            if (auto* directive = dynamic_cast<SchematicSpiceDirectiveItem*>(it)) {
                if (directive->text().contains(".subckt " + m_subcktName, Qt::CaseInsensitive)) {
                    QString txt = directive->text();
                    
                    // Update W, L, VGATE in the SPICE text
                    QString newW = getPropertyValue("W").toString();
                    QString newL = getPropertyValue("L").toString();
                    QString newVgate = getPropertyValue("VGATE").toString();
                    if (newVgate.endsWith("V", Qt::CaseInsensitive)) newVgate.chop(1);

                    txt.replace(QRegularExpression("W=[^\\s\\n]+"), "W=" + newW);
                    txt.replace(QRegularExpression("L=[^\\s\\n]+"), "L=" + newL);
                    
                    // Update B-source gate scaling
                    txt.replace(QRegularExpression("(V\\(ctrl_high, gnd_node\\) \\* )([^\\s\\n]+)"), "\\1" + newVgate);
                    txt.replace(QRegularExpression("(V\\(ctrl_low, gnd_node\\) \\* )([^\\s\\n]+)"), "\\1" + newVgate);
                    txt.replace(QRegularExpression("(V\\(ctrl, gnd_node\\) \\* )([^\\s\\n]+)"), "\\1" + newVgate);

                    if (m_subcktName != newState["value"].toString()) {
                        txt.replace(m_subcktName, newState["value"].toString());
                    }

                    m_undoStack->push(new BulkChangePropertyCommand(m_scene, directive, {{"value", txt}}));
                }
            }
        }
    }

    m_undoStack->push(new BulkChangePropertyCommand(m_scene, m_genItem, newState));
    m_undoStack->endMacro();
}

void PowerStagePropertiesDialog::applyPreview() {
    // Basic preview
    m_genItem->setReference(getPropertyValue("reference").toString());
    m_genItem->setValue(getPropertyValue("value").toString());
    m_genItem->update();
}
