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
        QRegularExpression reSub("^\\s*\\.subckt\\s+" + QRegularExpression::escape(m_subcktName), QRegularExpression::CaseInsensitiveOption);
        bool found = false;
        for (auto* it : scene->items()) {
            if (auto* directive = dynamic_cast<SchematicSpiceDirectiveItem*>(it)) {
                QString txt = directive->text();
                if (reSub.match(txt).hasMatch()) {
                    found = true;
                    // Extract W/L
                    QRegularExpression reW("W=([^\\s\\n]+)", QRegularExpression::CaseInsensitiveOption);
                    QRegularExpression reL("L=([^\\s\\n]+)", QRegularExpression::CaseInsensitiveOption);
                    auto mW = reW.match(txt);
                    if (mW.hasMatch()) currentParams["W"] = mW.captured(1);
                    auto mL = reL.match(txt);
                    if (mL.hasMatch()) currentParams["L"] = mL.captured(1);
                    
                    // Extract VGATE (handles V(ctrl), V(ctrl_high), V(ctrl_1) etc)
                    QRegularExpression reVgate("V\\(ctrl[^,)]*,? ?[^)]*\\) \\* ([^\\s\\n]+)", QRegularExpression::CaseInsensitiveOption);
                    auto mV = reVgate.match(txt);
                    if (mV.hasMatch()) currentParams["VGATE"] = mV.captured(1);
                    
                    // Extract PERIOD from PWL points (max time)
                    QRegularExpression rePwl("\\+ ([^\\s]+) ([^\\s\\n]+)");
                    auto itPwl = rePwl.globalMatch(txt);
                    double tMax = 0;
                    while (itPwl.hasNext()) {
                        double t = 0;
                        SimValueParser::parseSpiceNumber(itPwl.next().captured(1).toStdString(), t);
                        if (t > tMax) tMax = t;
                    }
                    if (tMax > 0) currentParams["PERIOD"] = QString::number(tMax, 'g', 6);
                }
            }
        }
        if (!found) qDebug() << "[PowerStageDialog] Warning: Subcircuit directive not found for" << m_subcktName;
    }

    paramsTab.fields.append({"reference", "Reference", PropertyField::Text, item->reference()});
    paramsTab.fields.append({"value", "Model/Subckt", PropertyField::Text, item->value()});
    
    // Advanced Power Stage Params
    paramsTab.fields.append({"PERIOD", "Waveform Period", PropertyField::EngineeringValue, currentParams.value("PERIOD", "1.0"), {}, "s"});
    paramsTab.fields.append({"VGATE", "Gate Drive Voltage", PropertyField::EngineeringValue, currentParams.value("VGATE", "15"), {}, "V"});
    paramsTab.fields.append({"W", "MOS Width", PropertyField::EngineeringValue, currentParams.value("W", "10u"), {}, "m"});
    paramsTab.fields.append({"L", "MOS Length", PropertyField::EngineeringValue, currentParams.value("L", "1u"), {}, "m"});
    
    addTab(paramsTab);

    // Initialize values (SmartPropertiesDialog requires manual initialization)
    setPropertyValue("reference", item->reference());
    setPropertyValue("value", item->value());
    setPropertyValue("PERIOD", currentParams.value("PERIOD", "1.0"));
    setPropertyValue("VGATE", currentParams.value("VGATE", "15"));
    setPropertyValue("W", currentParams.value("W", "10u"));
    setPropertyValue("L", currentParams.value("L", "1u"));

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
        QRegularExpression reSub("^\\s*\\.subckt\\s+" + QRegularExpression::escape(m_subcktName), QRegularExpression::CaseInsensitiveOption);
        for (auto* it : m_scene->items()) {
            if (auto* d = dynamic_cast<SchematicSpiceDirectiveItem*>(it)) {
                if (reSub.match(d->text()).hasMatch()) {
                    subckt = d->text();
                    directiveItem = d;
                    break;
                }
            }
        }
    }

    // 2. Extract PWL points
    QVector<QPointF> points;
    
    // Isolate the first PWL block to avoid mixing signals (e.g. high/low side)
    QRegularExpression rePwlBlock("PWL\\s*\\(([^)]+)\\)", QRegularExpression::CaseInsensitiveOption);
    auto mBlock = rePwlBlock.match(subckt);
    if (mBlock.hasMatch()) {
        QString pwlContent = mBlock.captured(1);
        QRegularExpression rePwl("\\+ ([^\\s]+) ([^\\s\\n]+)");
        auto it = rePwl.globalMatch(pwlContent);
        double tMax = 0;
        while (it.hasNext()) {
            auto match = it.next();
            double t = 0, y = 0;
            SimValueParser::parseSpiceNumber(match.captured(1).toStdString(), t);
            SimValueParser::parseSpiceNumber(match.captured(2).toStdString(), y);
            
            // Map SPICE logic 0/1 back to Drawer -1/1
            double mappedY = (y > 0.5) ? 1.0 : -1.0;
            points.append(QPointF(t, mappedY));
            if (t > tMax) tMax = t;
        }
        
        // Normalize time for the architect (0..1)
        if (tMax > 0) {
            for (auto& p : points) p.setX(p.x() / tMax);
        }
    }
    
    // 3. Launch Architect
    auto* arch = new MosCircuitArchitect(static_cast<QWidget*>(parent()));
    arch->setSourceItem(m_genItem); // Set context for replacement
    arch->setPoints(points);        // Set extracted points
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
        QRegularExpression reSub("^\\s*\\.subckt\\s+" + QRegularExpression::escape(m_subcktName), QRegularExpression::CaseInsensitiveOption);
        for (auto* it : m_scene->items()) {
            if (auto* directive = dynamic_cast<SchematicSpiceDirectiveItem*>(it)) {
                if (reSub.match(directive->text()).hasMatch()) {
                    QString txt = directive->text();
                    
                    // Update W, L, VGATE in the SPICE text
                    QString newW = getPropertyValue("W").toString();
                    QString newL = getPropertyValue("L").toString();
                    QString newVgate = getPropertyValue("VGATE").toString();
                    if (newVgate.endsWith("V", Qt::CaseInsensitive)) newVgate.chop(1);

                    txt.replace(QRegularExpression("W=[^\\s\\n]+"), "W=" + newW);
                    txt.replace(QRegularExpression("L=[^\\s\\n]+"), "L=" + newL);
                    
                    // Update B-source gate scaling (more robust regex)
                    txt.replace(QRegularExpression("(V\\(ctrl[^,)]*,? ?[^)]*\\) \\* )([^\\s\\n]+)", QRegularExpression::CaseInsensitiveOption), "\\1" + newVgate);
                    
                    // Also handle E-source for Push-Pull
                    txt.replace(QRegularExpression("(V\\(ctrl[^,)]*\\) \\* )([^\\s\\n\\}]+)", QRegularExpression::CaseInsensitiveOption), "\\1" + newVgate);

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
