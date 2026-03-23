#include "current_source_properties_dialog.h"
#include "../items/current_source_item.h"
#include "../../simulator/core/sim_value_parser.h"
#include "../editor/schematic_commands.h"
#include <QJsonObject>

CurrentSourcePropertiesDialog::CurrentSourcePropertiesDialog(CurrentSourceItem* item, QUndoStack* undoStack, QGraphicsScene* scene, QWidget* parent)
    : SmartPropertiesDialog({item}, undoStack, scene, parent), m_item(item) {
    
    setWindowTitle("Current Source Configuration - " + item->reference());
    
    PropertyTab mainTab;
    mainTab.title = "Source Type";
    
    PropertyField typeField;
    typeField.name = "sourceType";
    typeField.label = "Waveform Type";
    typeField.type = PropertyField::Choice;
    typeField.choices = {"DC", "Sine", "Pulse", "Behavioral"};
    typeField.defaultValue = (int)item->sourceType();
    mainTab.fields.append(typeField);

    mainTab.fields.append({"exclude_simulation", "Exclude from Simulation", PropertyField::Boolean, item->excludeFromSimulation()});
    mainTab.fields.append({"exclude_pcb", "Exclude from PCB Editor", PropertyField::Boolean, item->excludeFromPcb()});
    
    addTab(mainTab);

    PropertyTab dcTab;
    dcTab.title = "DC Value";
    dcTab.fields.append({"dcCurrent", "DC Current", PropertyField::EngineeringValue, item->dcCurrent(), {}, "A"});
    addTab(dcTab);

    PropertyTab sineTab;
    sineTab.title = "Sine";
    sineTab.fields.append({"sineAmplitude", "Amplitude (pk)", PropertyField::EngineeringValue, item->sineAmplitude(), {}, "A"});
    sineTab.fields.append({"sineFrequency", "Frequency", PropertyField::EngineeringValue, item->sineFrequency(), {}, "Hz"});
    sineTab.fields.append({"sineOffset", "DC Offset", PropertyField::EngineeringValue, item->sineOffset(), {}, "A"});
    addTab(sineTab);

    PropertyTab pulseTab;
    pulseTab.title = "Pulse";
    pulseTab.fields.append({"pulseI1", "Initial Value (I1)", PropertyField::EngineeringValue, item->pulseI1(), {}, "A"});
    pulseTab.fields.append({"pulseI2", "Pulsed Value (I2)", PropertyField::EngineeringValue, item->pulseI2(), {}, "A"});
    pulseTab.fields.append({"pulseDelay", "Delay Time", PropertyField::EngineeringValue, item->pulseDelay(), {}, "s"});
    pulseTab.fields.append({"pulseRise", "Rise Time", PropertyField::EngineeringValue, item->pulseRise(), {}, "s"});
    pulseTab.fields.append({"pulseFall", "Fall Time", PropertyField::EngineeringValue, item->pulseFall(), {}, "s"});
    pulseTab.fields.append({"pulseWidth", "On Time (Twidth)", PropertyField::EngineeringValue, item->pulseWidth(), {}, "s"});
    pulseTab.fields.append({"pulsePeriod", "Period (Tperiod)", PropertyField::EngineeringValue, item->pulsePeriod(), {}, "s"});
    addTab(pulseTab);

    PropertyTab behavioralTab;
    behavioralTab.title = "Behavioral (BI)";
    PropertyField exprField;
    exprField.name = "behavioralExpr";
    exprField.label = "Expression (I=...)";
    exprField.type = PropertyField::MultilineText;
    exprField.defaultValue = item->value();
    exprField.tooltip = "Behavioral current: I=<expression>, e.g. I=I(Vsrc)*2 or I=if(V(in)>0,1m,0)";
    exprField.validator = [](const QVariant& value) -> QString {
        const QString v = value.toString().trimmed();
        if (v.isEmpty()) return "Expression is empty.";
        int depth = 0;
        for (const QChar& c : v) {
            if (c == '(') depth++;
            else if (c == ')') depth--;
            if (depth < 0) break;
        }
        if (depth != 0) return "Unbalanced parentheses.";
        return QString();
    };
    behavioralTab.fields.append(exprField);
    addTab(behavioralTab);

    updateTabVisibility();
}

void CurrentSourcePropertiesDialog::onFieldChanged() {
    SmartPropertiesDialog::onFieldChanged();
    updateTabVisibility();
}

void CurrentSourcePropertiesDialog::updateTabVisibility() {
    QString type = getPropertyValue("sourceType").toString();
    
    setTabVisible(1, type == "DC");
    setTabVisible(2, type == "Sine");
    setTabVisible(3, type == "Pulse");
    setTabVisible(4, type == "Behavioral");
}

void CurrentSourcePropertiesDialog::onApply() {
    m_undoStack->beginMacro("Update Current Source");
    
    QJsonObject newState = m_item->toJson();
    newState["sourceType"] = getPropertyValue("sourceType").toInt();
    newState["dcCurrent"] = getPropertyValue("dcCurrent").toString();
    
    newState["excludeFromSim"] = getPropertyValue("exclude_simulation").toBool();
    newState["excludeFromPcb"] = getPropertyValue("exclude_pcb").toBool();

    newState["sineAmplitude"] = getPropertyValue("sineAmplitude").toString();
    newState["sineFrequency"] = getPropertyValue("sineFrequency").toString();
    newState["sineOffset"] = getPropertyValue("sineOffset").toString();
    
    newState["pulseI1"] = getPropertyValue("pulseI1").toString();
    newState["pulseI2"] = getPropertyValue("pulseI2").toString();
    newState["pulseDelay"] = getPropertyValue("pulseDelay").toString();
    newState["pulseRise"] = getPropertyValue("pulseRise").toString();
    newState["pulseFall"] = getPropertyValue("pulseFall").toString();
    newState["pulseWidth"] = getPropertyValue("pulseWidth").toString();
    newState["pulsePeriod"] = getPropertyValue("pulsePeriod").toString();

    if (getPropertyValue("sourceType").toString() == "Behavioral") {
        QString expr = getPropertyValue("behavioralExpr").toString().trimmed();
        if (expr.isEmpty()) expr = "I=0";
        if (!expr.startsWith("I=", Qt::CaseInsensitive)) expr = "I=" + expr;
        newState["value"] = expr;
    }

    m_undoStack->push(new BulkChangePropertyCommand(m_scene, m_item, newState));
    m_undoStack->endMacro();
}

void CurrentSourcePropertiesDialog::applyPreview() {
    auto type = static_cast<CurrentSourceItem::SourceType>(getPropertyValue("sourceType").toInt());
    m_item->setSourceType(type);
    
    m_item->setExcludeFromSimulation(getPropertyValue("exclude_simulation").toBool());
    m_item->setExcludeFromPcb(getPropertyValue("exclude_pcb").toBool());

    if (type == CurrentSourceItem::DC) {
        m_item->setDcCurrent(getPropertyValue("dcCurrent").toString());
    } else if (type == CurrentSourceItem::Sine) {
        m_item->setSineAmplitude(getPropertyValue("sineAmplitude").toString());
        m_item->setSineFrequency(getPropertyValue("sineFrequency").toString());
        m_item->setSineOffset(getPropertyValue("sineOffset").toString());
    } else if (type == CurrentSourceItem::Pulse) {
        m_item->setPulseI1(getPropertyValue("pulseI1").toString());
        m_item->setPulseI2(getPropertyValue("pulseI2").toString());
        m_item->setPulseDelay(getPropertyValue("pulseDelay").toString());
        m_item->setPulseRise(getPropertyValue("pulseRise").toString());
        m_item->setPulseFall(getPropertyValue("pulseFall").toString());
        m_item->setPulseWidth(getPropertyValue("pulseWidth").toString());
        m_item->setPulsePeriod(getPropertyValue("pulsePeriod").toString());
    } else if (type == CurrentSourceItem::Behavioral) {
        QString expr = getPropertyValue("behavioralExpr").toString().trimmed();
        if (expr.isEmpty()) expr = "I=0";
        if (!expr.startsWith("I=", Qt::CaseInsensitive)) expr = "I=" + expr;
        m_item->setValue(expr);
    }
    
    m_item->update();
}
