/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#include "schematic_item_factory.h"
#include <QDebug>
#include <QSet>
#include "generic_component_item.h"
#include "symbol_library.h"
#include "avr_microcontroller_item.h"

using Flux::Model::SymbolDefinition;

SchematicItemFactory& SchematicItemFactory::instance() {
    static SchematicItemFactory instance;
    return instance;
}

void SchematicItemFactory::registerItemType(const QString& typeName, CreatorFunction creator) {
    if (m_creators.contains(typeName)) {
        qWarning() << "Schematic item type" << typeName << "is already registered. Overwriting.";
    }
    m_creators[typeName] = creator;
}

SchematicItem* SchematicItemFactory::createItem(const QString& typeName, QPointF pos,
                                               const QJsonObject& properties,
                                               QGraphicsItem* parent) {
    SchematicItem* item = nullptr;

    const QSet<QString> powerTypes = {
        "Power", "GND", "VCC", "VDD", "VSS", "VBAT", "3.3V", "5V", "12V"
    };
    const bool isPowerItem = powerTypes.contains(typeName);
    const bool isVoltageSource = typeName.startsWith("Voltage_Source", Qt::CaseInsensitive) ||
                                 typeName.compare("voltage", Qt::CaseInsensitive) == 0 ||
                                 typeName.compare("bv", Qt::CaseInsensitive) == 0;
    const bool isCurrentSource = typeName.startsWith("Current_Source", Qt::CaseInsensitive) ||
                                   typeName.compare("current", Qt::CaseInsensitive) == 0 ||
                                   typeName.compare("bi", Qt::CaseInsensitive) == 0 ||
                                   typeName.compare("bi2", Qt::CaseInsensitive) == 0;
    const bool isJfet = typeName.compare("njf", Qt::CaseInsensitive) == 0 ||
                        typeName.compare("pjf", Qt::CaseInsensitive) == 0;
    const bool isBjtAlias = typeName.compare("npn", Qt::CaseInsensitive) == 0 ||
                            typeName.compare("npn2", Qt::CaseInsensitive) == 0 ||
                            typeName.compare("npn3", Qt::CaseInsensitive) == 0 ||
                            typeName.compare("npn4", Qt::CaseInsensitive) == 0 ||
                            typeName.compare("pnp", Qt::CaseInsensitive) == 0 ||
                            typeName.compare("pnp2", Qt::CaseInsensitive) == 0 ||
                            typeName.compare("pnp4", Qt::CaseInsensitive) == 0 ||
                            typeName.compare("lpnp", Qt::CaseInsensitive) == 0;
    const bool isMosAlias = typeName.compare("nmos", Qt::CaseInsensitive) == 0 ||
                            typeName.compare("nmos4", Qt::CaseInsensitive) == 0 ||
                            typeName.compare("pmos", Qt::CaseInsensitive) == 0 ||
                            typeName.compare("pmos4", Qt::CaseInsensitive) == 0;
    const bool isMesfet = typeName.compare("mesfet", Qt::CaseInsensitive) == 0;
    const bool isControlledSource = (typeName == "E" || typeName == "G" || typeName == "F" || typeName == "H" ||
                                     typeName == "VCVS" || typeName == "VCCS" || typeName == "CCCS" || typeName == "CCVS");
    const bool isSpecializedItem = (typeName == "TuningSlider" || 
                                    typeName == "RotaryKnob" ||
                                    typeName == "Joystick" ||
                                    typeName == "Oscilloscope" ||
                                    typeName == "Oscilloscope Instrument" ||
                                    typeName == "OscilloscopeInstrument" ||
                                    typeName == "LogicAnalyzer" ||
                                    typeName == "LogicToggle" ||
                                    typeName == "LogicProbe" ||
                                    typeName == "SignalGenerator" ||
                                    typeName == "7-Segment Display" ||
                                    typeName == "Dual 7-Segment Display" ||
                                    typeName == "14-Segment Display" ||
                                    typeName == "16-Segment Display" ||
                                    typeName == "XspiceBlock" ||
                                    typeName == "SystemVerilogBlock" ||
                                    typeName == "AvrMicrocontroller" ||
                                    typeName == "Voltmeter (DC)" ||
                                    typeName == "Voltmeter (AC)" ||
                                    typeName == "Ammeter (DC)" ||
                                    typeName == "Ammeter (AC)" ||
                                    typeName == "Wattmeter" ||
                                    typeName == "VirtualTerminalInstrument" ||
                                    typeName == "Flux Measurement Probe" ||
                                    isControlledSource);

    if (!isPowerItem && !isVoltageSource && !isCurrentSource && !isJfet && !isBjtAlias && !isMosAlias && !isMesfet && !isSpecializedItem) {
        // Built-in interactive controls (switches, buttons, …) win over a
        // same-named library symbol: a GenericComponentItem is not
        // interactive, so its clicks fall into probing and the control can
        // never be toggled. The interactive set is probed once from the
        // registered creators (a default-constructed instance reports
        // isInteractive()); the count check re-probes if more types register
        // later (there is no unregister path, so it cannot go stale).
        static QSet<QString> s_interactiveTypes;
        static size_t s_creatorCount = 0;
        if (s_creatorCount != (size_t)m_creators.size()) {
            s_interactiveTypes.clear();
            for (auto it = m_creators.constBegin(); it != m_creators.constEnd(); ++it) {
                if (SchematicItem* probe = it.value()(QPointF(), QJsonObject(), nullptr)) {
                    if (probe->isInteractive()) s_interactiveTypes.insert(it.key());
                    delete probe;
                }
            }
            s_creatorCount = (size_t)m_creators.size();
        }
        if (s_interactiveTypes.contains(typeName)) {
            auto it = m_creators.find(typeName);
            if (it != m_creators.end()) {
                item = it.value()(pos, properties, parent);
            }
        }
        if (!item) {
            if (SymbolDefinition* def = SymbolLibraryManager::instance().findSymbol(typeName)) {
                item = new GenericComponentItem(*def, parent);
                item->setPos(pos);
            }
        }
    }

    // Check if typeName matches an MCU name — create AVR block with that MCU pre-selected
    if (!item) {
        const auto& mcuDb = AvrMicrocontrollerItem::mcuDatabase();
        if (mcuDb.contains(typeName)) {
            auto* avr = new AvrMicrocontrollerItem(typeName, parent);
            avr->setPos(pos);
            if (!properties.isEmpty()) avr->fromJson(properties);
            item = avr;
        }
    }

    if (!item) {
        auto it = m_creators.find(typeName);
        if (it != m_creators.end()) {
            item = it.value()(pos, properties, parent);
        } else {
            qWarning() << "SchematicItemFactory Error: Unknown item type:" << typeName;
            return nullptr;
        }
    }
    if (item) {
        // Apply common properties from JSON
        if (properties.contains("name")) {
            item->setName(properties["name"].toString());
        }
        if (properties.contains("value")) {
            item->setValue(properties["value"].toString());
        }
        if (properties.contains("id")) {
            item->setId(QUuid(properties["id"].toString()));
        }
        
        // Auto-assign reference designator if not already set
        if (properties.contains("reference")) {
            item->setReference(properties["reference"].toString());
        } else {
            // Use generic "?" for new items to avoid cross-sheet conflicts
            item->setReference(item->referencePrefix() + "?");
        }
        
        // Force update to rebuild primitives with the new reference
        item->update();
    }
    return item;
}

QStringList SchematicItemFactory::registeredTypes() const {
    return m_creators.keys();
}

bool SchematicItemFactory::isTypeRegistered(const QString& typeName) const {
    return m_creators.contains(typeName);
}

QString SchematicItemFactory::nextReference(const QString& prefix) {
    int& counter = m_referenceCounters[prefix];
    counter++;
    return prefix + QString::number(counter);
}

int SchematicItemFactory::getCounter(const QString& prefix) const {
    return m_referenceCounters.value(prefix, 0);
}

void SchematicItemFactory::resetCounter(const QString& prefix) {
    m_referenceCounters[prefix] = 0;
}

void SchematicItemFactory::resetAllCounters() {
    m_referenceCounters.clear();
}
