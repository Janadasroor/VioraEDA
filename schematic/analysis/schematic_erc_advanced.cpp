// schematic_erc_advanced.cpp
// Advanced Electrical Rules Check implementation

#include "schematic_erc_advanced.h"
#include "../items/schematic_item.h"
#include "../items/generic_component_item.h"
#include "../items/power_item.h"
#include <QGraphicsScene>
#include <QSet>
#include <QMap>

// ────────────────────────────────────────────────────────────────────────────
// Main Entry Point
// ────────────────────────────────────────────────────────────────────────────

QList<DesignRuleViolation> SchematicERCAdvanced::runAllChecks(
    QGraphicsScene* scene,
    NetManager* netManager,
    const DesignRuleSet* rules
) {
    QList<DesignRuleViolation> allViolations;

    if (!scene || !rules) {
        return allViolations;
    }

    // Run each enabled check
    for (auto* rule : rules->rules()) {
        if (!rule->enabled()) continue;

        QList<DesignRuleViolation> violations;

        // ERC Checks
        if (rule->category() == RuleCategory::ERC) {
            if (rule->name() == "Unconnected Pins") {
                violations = checkUnconnectedPins(scene, netManager, rule);
            }
            else if (rule->name() == "Output Pin Conflicts") {
                violations = checkOutputConflicts(scene, netManager, rule);
            }
            else if (rule->name() == "Missing Power Pins") {
                violations = checkMissingPowerPins(scene, netManager, rule);
            }
            else if (rule->name() == "Floating Inputs") {
                violations = checkFloatingInputs(scene, netManager, rule);
            }
            else if (rule->name() == "Duplicate Designators") {
                violations = checkDuplicateDesignators(scene, rule);
            }
            else if (rule->name() == "Missing Component Values") {
                violations = checkMissingValues(scene, rule);
            }
            else if (rule->name() == "Different Voltage Domains") {
                violations = checkDifferentVoltageDomains(scene, netManager, rule);
            }
            else if (rule->name() == "Missing Decoupling Capacitors") {
                violations = checkMissingDecouplingCaps(scene, netManager, rule);
            }
            else if (rule->name() == "Unconnected Nets") {
                violations = checkUnconnectedNets(scene, netManager, rule);
            }
        }

        // DRC Checks
        else if (rule->category() == RuleCategory::DRC) {
            if (rule->name() == "Missing Footprint") {
                violations = checkMissingFootprints(scene, rule);
            }
            else if (rule->name() == "Bus Width Mismatch") {
                violations = checkBusWidthMismatch(scene, netManager, rule);
            }
        }

        allViolations.append(violations);
    }

    return allViolations;
}

// ────────────────────────────────────────────────────────────────────────────
// Individual Check Implementations
// ────────────────────────────────────────────────────────────────────────────

QList<DesignRuleViolation> SchematicERCAdvanced::checkUnconnectedPins(
    QGraphicsScene* scene,
    NetManager* netManager,
    DesignRule* rule
) {
    QList<DesignRuleViolation> violations;

    if (!scene || !netManager || !rule) return violations;

    // Get all components
    QList<SchematicItem*> components;
    for (auto* item : scene->items()) {
        if (auto* si = dynamic_cast<SchematicItem*>(item)) {
            if (si->itemType() == SchematicItem::WireType ||
                si->itemType() == SchematicItem::BusType ||
                si->itemType() == SchematicItem::JunctionType ||
                si->itemType() == SchematicItem::LabelType) {
                continue;
            }
            components.append(si);
        }
    }

    // Check each component's pins
    for (auto* comp : components) {
        auto connectionPoints = comp->connectionPoints();
        auto pinTypes = comp->pinElectricalTypes();

        for (int i = 0; i < connectionPoints.size() && i < pinTypes.size(); ++i) {
            auto pinType = pinTypes[i];
            
            // Skip power pins and NC pins
            if (pinType == SchematicItem::PowerInputPin ||
                pinType == SchematicItem::PowerOutputPin ||
                pinType == SchematicItem::NotConnectedPin) {
                continue;
            }

            // Check if pin is connected
            QPointF scenePos = comp->mapToScene(connectionPoints[i]);
            bool isConnected = netManager->arePointsConnected(scenePos, scenePos);

            if (!isConnected) {
                // Check if pin name suggests it should be connected
                QString pinName = comp->pinName(i).toUpper();
                if (pinName.contains("NC") || pinName.contains("N/C") || 
                    pinName.contains("DNC") || pinName.contains("NO CONNECT")) {
                    continue; // Intentionally unconnected
                }

                DesignRuleViolation v;
                v.ruleId = rule->id();
                v.ruleName = rule->name();
                v.severity = rule->defaultSeverity();
                v.message = QString("Pin %1 of %2 is unconnected")
                    .arg(comp->pinName(i))
                    .arg(comp->reference());
                v.objectRef = comp->reference();
                v.position = comp->scenePos();
                v.context["pinNumber"] = i + 1;
                v.context["pinName"] = comp->pinName(i);
                violations.append(v);
            }
        }
    }

    return violations;
}

QList<DesignRuleViolation> SchematicERCAdvanced::checkOutputConflicts(
    QGraphicsScene* scene,
    NetManager* netManager,
    DesignRule* rule
) {
    QList<DesignRuleViolation> violations;

    if (!scene || !netManager || !rule) return violations;

    // Group output pins by net
    QMap<QString, QList<SchematicItem*>> outputPinsByNet;

    for (auto* item : scene->items()) {
        if (auto* comp = dynamic_cast<SchematicItem*>(item)) {
            auto pinTypes = comp->pinElectricalTypes();
            auto connectionPoints = comp->connectionPoints();

            for (int i = 0; i < pinTypes.size() && i < connectionPoints.size(); ++i) {
                auto pinType = pinTypes[i];
                
                // Check for output-type pins
                if (pinType == SchematicItem::OutputPin ||
                    pinType == SchematicItem::PowerOutputPin ||
                    pinType == SchematicItem::OpenCollectorPin ||
                    pinType == SchematicItem::OpenEmitterPin) {
                    
                    // For now, just track all outputs - proper net checking needs connectivity analysis
                    QString netName = comp->reference() + "_pin_" + QString::number(i);
                    outputPinsByNet[netName].append(comp);
                }
            }
        }
    }

    // Check for conflicts (multiple outputs on same net)
    for (auto it = outputPinsByNet.begin(); it != outputPinsByNet.end(); ++it) {
        const QString& netName = it.key();
        const QList<SchematicItem*>& outputs = it.value();

        if (outputs.size() > 1) {
            // Check if any are direct output conflicts
            bool hasDirectConflict = false;
            QStringList conflictingRefs;

            for (auto* out : outputs) {
                auto pinTypes = out->pinElectricalTypes();
                for (int i = 0; i < pinTypes.size(); ++i) {
                    if (pinTypes[i] == SchematicItem::OutputPin ||
                        pinTypes[i] == SchematicItem::PowerOutputPin) {
                        hasDirectConflict = true;
                        conflictingRefs.append(out->reference());
                    }
                }
            }

            if (hasDirectConflict) {
                DesignRuleViolation v;
                v.ruleId = rule->id();
                v.ruleName = rule->name();
                v.severity = rule->defaultSeverity();
                v.message = QString("Output conflict on net '%1': %2")
                    .arg(netName)
                    .arg(conflictingRefs.join(", "));
                v.netName = netName;
                v.involvedRefs = conflictingRefs;
                violations.append(v);
            }
        }
    }

    return violations;
}

QList<DesignRuleViolation> SchematicERCAdvanced::checkMissingPowerPins(
    QGraphicsScene* scene,
    NetManager* netManager,
    DesignRule* rule
) {
    QList<DesignRuleViolation> violations;

    if (!scene || !netManager || !rule) return violations;

    // Check ICs and other components for unconnected power pins
    QList<SchematicItem*> ics = getComponentsByType(scene, "IC");
    
    for (auto* ic : ics) {
        auto pinTypes = ic->pinElectricalTypes();
        auto connectionPoints = ic->connectionPoints();

        for (int i = 0; i < pinTypes.size() && i < connectionPoints.size(); ++i) {
            if (pinTypes[i] == SchematicItem::PowerInputPin) {
                // For now, skip this check - proper implementation needs net tracing
                // TODO: Implement proper power net detection
                continue;
            }
        }
    }

    return violations;
}

QList<DesignRuleViolation> SchematicERCAdvanced::checkFloatingInputs(
    QGraphicsScene* scene,
    NetManager* netManager,
    DesignRule* rule
) {
    QList<DesignRuleViolation> violations;

    if (!scene || !netManager || !rule) return violations;

    // Check logic gate inputs and other digital inputs
    QList<SchematicItem*> logicGates = getComponentsByType(scene, "LogicGate");
    
    for (auto* gate : logicGates) {
        auto pinTypes = gate->pinElectricalTypes();
        auto connectionPoints = gate->connectionPoints();

        for (int i = 0; i < pinTypes.size() && i < connectionPoints.size(); ++i) {
            if (pinTypes[i] == SchematicItem::InputPin) {
                QPointF scenePos = gate->mapToScene(connectionPoints[i]);
                bool isConnected = netManager->arePointsConnected(scenePos, scenePos);

                if (!isConnected) {
                    DesignRuleViolation v;
                    v.ruleId = rule->id();
                    v.ruleName = rule->name();
                    v.severity = rule->defaultSeverity();
                    v.message = QString("Floating input on %1 pin %2")
                        .arg(gate->reference())
                        .arg(gate->pinName(i));
                    v.objectRef = gate->reference();
                    v.position = gate->scenePos();
                    v.context["pinNumber"] = i + 1;
                    violations.append(v);
                }
            }
        }
    }

    return violations;
}

QList<DesignRuleViolation> SchematicERCAdvanced::checkDuplicateDesignators(
    QGraphicsScene* scene,
    DesignRule* rule
) {
    QList<DesignRuleViolation> violations;

    if (!scene || !rule) return violations;

    QMap<QString, QList<SchematicItem*>> designators;

    // Collect all designators
    for (auto* item : scene->items()) {
        if (auto* comp = dynamic_cast<SchematicItem*>(item)) {
            if (comp->itemType() == SchematicItem::WireType ||
                comp->itemType() == SchematicItem::BusType ||
                comp->itemType() == SchematicItem::JunctionType ||
                comp->itemType() == SchematicItem::LabelType) {
                continue;
            }

            QString ref = comp->reference();
            if (!ref.isEmpty() && !ref.contains("?")) {
                designators[ref].append(comp);
            }
        }
    }

    // Find duplicates
    for (auto it = designators.begin(); it != designators.end(); ++it) {
        if (it.value().size() > 1) {
            QStringList locations;
            for (auto* comp : it.value()) {
                locations.append(QString("%1 at (%2, %3)")
                    .arg(comp->reference())
                    .arg(comp->scenePos().x())
                    .arg(comp->scenePos().y()));
            }

            DesignRuleViolation v;
            v.ruleId = rule->id();
            v.ruleName = rule->name();
            v.severity = rule->defaultSeverity();
            v.message = QString("Duplicate designator '%1' found: %2")
                .arg(it.key())
                .arg(locations.join(", "));
            v.objectRef = it.key();
            v.involvedRefs = QStringList() << it.key();
            violations.append(v);
        }
    }

    return violations;
}

QList<DesignRuleViolation> SchematicERCAdvanced::checkMissingValues(
    QGraphicsScene* scene,
    DesignRule* rule
) {
    QList<DesignRuleViolation> violations;

    if (!scene || !rule) return violations;

    for (auto* item : scene->items()) {
        if (auto* comp = dynamic_cast<SchematicItem*>(item)) {
            if (comp->itemType() == SchematicItem::WireType ||
                comp->itemType() == SchematicItem::BusType ||
                comp->itemType() == SchematicItem::JunctionType ||
                comp->itemType() == SchematicItem::LabelType ||
                comp->itemType() == SchematicItem::PowerType) {
                continue;
            }

            QString value = comp->value();
            if (value.isEmpty() && !comp->reference().isEmpty()) {
                DesignRuleViolation v;
                v.ruleId = rule->id();
                v.ruleName = rule->name();
                v.severity = rule->defaultSeverity();
                v.message = QString("Component %1 has no value specified")
                    .arg(comp->reference());
                v.objectRef = comp->reference();
                v.position = comp->scenePos();
                v.context["itemType"] = comp->itemTypeName();
                violations.append(v);
            }
        }
    }

    return violations;
}

QList<DesignRuleViolation> SchematicERCAdvanced::checkMissingFootprints(
    QGraphicsScene* scene,
    DesignRule* rule
) {
    QList<DesignRuleViolation> violations;

    if (!scene || !rule) return violations;

    for (auto* item : scene->items()) {
        if (auto* comp = dynamic_cast<SchematicItem*>(item)) {
            if (comp->itemType() == SchematicItem::WireType ||
                comp->itemType() == SchematicItem::BusType ||
                comp->itemType() == SchematicItem::JunctionType ||
                comp->itemType() == SchematicItem::LabelType ||
                comp->itemType() == SchematicItem::PowerType) {
                continue;
            }

            QString footprint = comp->property("footprint").toString();
            if (footprint.isEmpty() && !comp->reference().isEmpty()) {
                DesignRuleViolation v;
                v.ruleId = rule->id();
                v.ruleName = rule->name();
                v.severity = rule->defaultSeverity();
                v.message = QString("Component %1 has no footprint assigned")
                    .arg(comp->reference());
                v.objectRef = comp->reference();
                v.position = comp->scenePos();
                violations.append(v);
            }
        }
    }

    return violations;
}

QList<DesignRuleViolation> SchematicERCAdvanced::checkBusWidthMismatch(
    QGraphicsScene* scene,
    NetManager* netManager,
    DesignRule* rule
) {
    QList<DesignRuleViolation> violations;
    // TODO: Implement bus width checking
    return violations;
}

QList<DesignRuleViolation> SchematicERCAdvanced::checkDifferentVoltageDomains(
    QGraphicsScene* scene,
    NetManager* netManager,
    DesignRule* rule
) {
    QList<DesignRuleViolation> violations;
    // TODO: Implement voltage domain checking
    return violations;
}

QList<DesignRuleViolation> SchematicERCAdvanced::checkMissingDecouplingCaps(
    QGraphicsScene* scene,
    NetManager* netManager,
    DesignRule* rule
) {
    QList<DesignRuleViolation> violations;

    if (!scene || !netManager || !rule) return violations;

    double minCapacitance = rule->parameters()["minCapacitance"].toDouble();
    if (minCapacitance <= 0) minCapacitance = 0.1e-6; // Default 100nF

    // Check all ICs
    QList<SchematicItem*> ics = getComponentsByType(scene, "IC");
    
    for (auto* ic : ics) {
        if (!hasDecouplingCapacitor(netManager, ic, minCapacitance)) {
            DesignRuleViolation v;
            v.ruleId = rule->id();
            v.ruleName = rule->name();
            v.severity = rule->defaultSeverity();
            v.message = QString("IC %1 may be missing decoupling capacitor")
                .arg(ic->reference());
            v.objectRef = ic->reference();
            v.position = ic->scenePos();
            v.suggestedFix = "Add 100nF capacitor between VCC and GND pins";
            violations.append(v);
        }
    }

    return violations;
}

QList<DesignRuleViolation> SchematicERCAdvanced::checkUnconnectedNets(
    QGraphicsScene* scene,
    NetManager* netManager,
    DesignRule* rule
) {
    QList<DesignRuleViolation> violations;

    if (!scene || !netManager || !rule) return violations;

    // Get all net labels
    QStringList netLabels = netManager->netNames();
    
    for (const QString& netName : netLabels) {
        // Skip power nets
        if (netName.contains("GND", Qt::CaseInsensitive) ||
            netName.contains("VCC", Qt::CaseInsensitive) ||
            netName.contains("VDD", Qt::CaseInsensitive)) {
            continue;
        }

        // Skip NC nets
        if (netName.contains("NC", Qt::CaseInsensitive) ||
            netName.contains("N/C", Qt::CaseInsensitive)) {
            continue;
        }

        // Check if net has any connections
        auto components = getComponentsConnectedToNet(netManager, netName);
        if (components.isEmpty()) {
            DesignRuleViolation v;
            v.ruleId = rule->id();
            v.ruleName = rule->name();
            v.severity = rule->defaultSeverity();
            v.message = QString("Net '%1' has no connections").arg(netName);
            v.netName = netName;
            violations.append(v);
        }
    }

    return violations;
}

// ────────────────────────────────────────────────────────────────────────────
// Helper Functions
// ────────────────────────────────────────────────────────────────────────────

QList<SchematicItem*> SchematicERCAdvanced::getComponentsByType(
    QGraphicsScene* scene, 
    const QString& typeName
) {
    QList<SchematicItem*> result;
    
    for (auto* item : scene->items()) {
        if (auto* comp = dynamic_cast<SchematicItem*>(item)) {
            if (comp->itemTypeName().contains(typeName, Qt::CaseInsensitive)) {
                result.append(comp);
            }
        }
    }
    
    return result;
}

QList<SchematicItem*> SchematicERCAdvanced::getComponentsConnectedToNet(
    NetManager* netManager,
    const QString& netName
) {
    QList<SchematicItem*> result;
    // TODO: Implement using netManager connectivity info
    return result;
}

bool SchematicERCAdvanced::hasDecouplingCapacitor(
    NetManager* netManager,
    SchematicItem* ic,
    double minCapacitance
) {
    // Get VCC and GND nets for this IC
    // TODO: Implement proper net tracing
    // For now, return true to avoid false positives
    return true;
}

QString SchematicERCAdvanced::getNetVoltageDomain(const QString& netName) {
    if (netName.contains("5V", Qt::CaseInsensitive) ||
        netName.contains("VCC5", Qt::CaseInsensitive)) {
        return "5V";
    }
    if (netName.contains("3.3V", Qt::CaseInsensitive) ||
        netName.contains("V3P3", Qt::CaseInsensitive)) {
        return "3.3V";
    }
    if (netName.contains("12V", Qt::CaseInsensitive)) {
        return "12V";
    }
    if (netName.contains("GND", Qt::CaseInsensitive)) {
        return "GND";
    }
    return "Unknown";
}
