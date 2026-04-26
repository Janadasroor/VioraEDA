#include "flux_design_rule_bridge.h"
#include "../schematic/items/schematic_item.h"
#include "../schematic/analysis/net_manager.h"
#include <mutex>

#include "../schematic/items/smart_signal_item.h"

namespace Flux {
namespace Core {

// Thread-local storage for the active bridge
static thread_local FluxDesignRuleBridge* t_currentBridge = nullptr;

FluxDesignRuleBridge::FluxDesignRuleBridge(QGraphicsScene* scene, NetManager* netManager)
    : m_scene(scene), m_netManager(netManager) {
    
    if (m_scene) {
        for (auto* qItem : m_scene->items()) {
            if (auto* item = dynamic_cast<SchematicItem*>(qItem)) {
                // Ignore wires/buses for component iteration
                if (item->itemType() != SchematicItem::WireType && 
                    item->itemType() != SchematicItem::BusType) {
                    m_items.append(item);
                }
            }
        }
    }
}

FluxDesignRuleBridge::~FluxDesignRuleBridge() {
}

void FluxDesignRuleBridge::setCurrent(FluxDesignRuleBridge* bridge) {
    t_currentBridge = bridge;
}

FluxDesignRuleBridge* FluxDesignRuleBridge::current() {
    return t_currentBridge;
}

int FluxDesignRuleBridge::getComponentCount() const {
    return m_items.size();
}

const char* FluxDesignRuleBridge::getComponentRef(int index) const {
    if (index < 0 || index >= m_items.size()) return "";
    return poolString(m_items[index]->reference());
}

const char* FluxDesignRuleBridge::getComponentValue(int index) const {
    if (index < 0 || index >= m_items.size()) return "";
    return poolString(m_items[index]->value());
}

const char* FluxDesignRuleBridge::getComponentType(int index) const {
    if (index < 0 || index >= m_items.size()) return "";
    return poolString(m_items[index]->itemTypeName());
}

int FluxDesignRuleBridge::getPinCount(int compIndex) const {
    if (compIndex < 0 || compIndex >= m_items.size()) return 0;
    return m_items[compIndex]->connectionPoints().size();
}

const char* FluxDesignRuleBridge::getPinNet(int compIndex, int pinIndex) const {
    if (compIndex < 0 || compIndex >= m_items.size()) return "0";
    if (m_netManager) {
        auto* item = m_items[compIndex];
        auto pts = item->connectionPoints();
        if (pinIndex >= 0 && pinIndex < pts.size()) {
            QPointF scenePt = item->mapToScene(pts[pinIndex]);
            QString net = m_netManager->findNetAtPoint(scenePt);
            return poolString(net.isEmpty() ? "0" : net);
        }
    }
    return "0";
}

void FluxDesignRuleBridge::reportViolation(int severity, const char* message, int compIndex) {
    DesignRuleViolation v;
    v.severity = static_cast<RuleSeverity>(severity);
    v.message = QString::fromUtf8(message);
    
    if (compIndex >= 0 && compIndex < m_items.size()) {
        auto* item = m_items[compIndex];
        v.objectRef = item->reference();
        v.position = item->scenePos();
        v.context["itemType"] = item->itemTypeName();
    }
    
    m_violations.append(v);
}

void FluxDesignRuleBridge::setBlockPins(int compIndex, const char* inPins, const char* outPins) {
    if (compIndex < 0 || compIndex >= m_items.size()) return;
    
    if (auto* smart = dynamic_cast<SmartSignalItem*>(m_items[compIndex])) {
        if (inPins) {
            QStringList pins = QString::fromUtf8(inPins).split(",", Qt::SkipEmptyParts);
            smart->setInputPins(pins);
        }
        if (outPins) {
            QStringList pins = QString::fromUtf8(outPins).split(",", Qt::SkipEmptyParts);
            smart->setOutputPins(pins);
        }
    }
}

const char* FluxDesignRuleBridge::poolString(const QString& s) const {
    std::string stdStr = s.toStdString();
    m_stringPool.push_back(stdStr);
    return m_stringPool.back().c_str();
}

} // namespace Core
} // namespace Flux

// --- C Hooks ---

int flux_erc_get_component_count() {
    if (auto* b = Flux::Core::FluxDesignRuleBridge::current()) return b->getComponentCount();
    return 0;
}

const char* flux_erc_get_ref(int index) {
    if (auto* b = Flux::Core::FluxDesignRuleBridge::current()) return b->getComponentRef(index);
    return "";
}

const char* flux_erc_get_value(int index) {
    if (auto* b = Flux::Core::FluxDesignRuleBridge::current()) return b->getComponentValue(index);
    return "";
}

const char* flux_erc_get_type(int index) {
    if (auto* b = Flux::Core::FluxDesignRuleBridge::current()) return b->getComponentType(index);
    return "";
}

int flux_erc_get_pin_count(int compIndex) {
    if (auto* b = Flux::Core::FluxDesignRuleBridge::current()) return b->getPinCount(compIndex);
    return 0;
}

const char* flux_erc_get_pin_net(int compIndex, int pinIndex) {
    if (auto* b = Flux::Core::FluxDesignRuleBridge::current()) return b->getPinNet(compIndex, pinIndex);
    return "0";
}

void flux_erc_report(int severity, const char* message, int compIndex) {
    if (auto* b = Flux::Core::FluxDesignRuleBridge::current()) b->reportViolation(severity, message, compIndex);
}

void flux_erc_set_block_pins(int compIndex, const char* inPins, const char* outPins) {
    if (auto* b = Flux::Core::FluxDesignRuleBridge::current()) b->setBlockPins(compIndex, inPins, outPins);
}
