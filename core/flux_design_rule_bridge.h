#ifndef FLUX_DESIGN_RULE_BRIDGE_H
#define FLUX_DESIGN_RULE_BRIDGE_H

#include <QObject>
#include <QGraphicsScene>
#include <QStringList>
#include <QPointF>
#include <vector>
#include "design_rule.h"

class NetManager;
class SchematicItem;

namespace Flux {
namespace Core {

/**
 * @brief Bridge between VioSpice Schematic data and FluxScript JIT.
 * Provides C-style hooks for rule scripts to query the design.
 */
class FluxDesignRuleBridge {
public:
    FluxDesignRuleBridge(QGraphicsScene* scene, NetManager* netManager);
    ~FluxDesignRuleBridge();

    // Singleton access for the current thread
    static void setCurrent(FluxDesignRuleBridge* bridge);
    static FluxDesignRuleBridge* current();

    // Data Access (Called from JIT)
    int getComponentCount() const;
    const char* getComponentRef(int index) const;
    const char* getComponentValue(int index) const;
    const char* getComponentType(int index) const;
    int getPinCount(int compIndex) const;
    const char* getPinNet(int compIndex, int pinIndex) const;
    
    void reportViolation(int severity, const char* message, int compIndex);
    void setBlockPins(int compIndex, const char* inPins, const char* outPins);

    // Results
    QList<DesignRuleViolation> violations() const { return m_violations; }
    void clearViolations() { m_violations.clear(); }

private:
    QGraphicsScene* m_scene;
    NetManager* m_netManager;
    QList<SchematicItem*> m_items;
    QList<DesignRuleViolation> m_violations;
    
    // String cache for JIT pointers
    mutable std::vector<std::string> m_stringPool;
    const char* poolString(const QString& s) const;
};

} // namespace Core
} // namespace Flux

// C-Linkage hooks for JIT registration
extern "C" {
    int flux_erc_get_component_count();
    const char* flux_erc_get_ref(int index);
    const char* flux_erc_get_value(int index);
    const char* flux_erc_get_type(int index);
    int flux_erc_get_pin_count(int compIndex);
    const char* flux_erc_get_pin_net(int compIndex, int pinIndex);
    void flux_erc_report(int severity, const char* message, int compIndex);
    void flux_erc_set_block_pins(int compIndex, const char* inPins, const char* outPins);
}

#endif // FLUX_DESIGN_RULE_BRIDGE_H
