#include "flux_workspace_bridge.h"
#include "../schematic/editor/schematic_api.h"
#include <QDebug>

static QMap<QString, double> g_fluxVars;
static SchematicAPI* g_activeApi = nullptr;

namespace Flux {
namespace Core {

void FluxWorkspaceBridge::setVariable(const QString& name, double value) {
    g_fluxVars[name] = value;
}

double FluxWorkspaceBridge::getVariable(const QString& name) {
    return g_fluxVars.value(name, 0.0);
}

void FluxWorkspaceBridge::setComponentProperty(const char* ref, const char* prop, double value) {
    if (!g_activeApi) return;
    g_activeApi->setProperty(QString::fromUtf8(ref), QString::fromUtf8(prop), value);
}

void FluxWorkspaceBridge::setComponentPropertyStr(const char* ref, const char* prop, const char* value) {
    if (!g_activeApi) return;
    g_activeApi->setProperty(QString::fromUtf8(ref), QString::fromUtf8(prop), QString::fromUtf8(value));
}

// Internal helper for VioSpice to hook the API
void set_active_schematic_api(SchematicAPI* api) {
    g_activeApi = api;
}

} // namespace Core
} // namespace Flux

extern "C" {
    double flux_get_var(const char* name) {
        if (!name) return 0.0;
        return Flux::Core::FluxWorkspaceBridge::getVariable(QString::fromUtf8(name));
    }
    
    void flux_set_prop(const char* ref, const char* prop, double value) {
        Flux::Core::FluxWorkspaceBridge::setComponentProperty(ref, prop, value);
    }
    
    void flux_set_prop_str(const char* ref, const char* prop, const char* value) {
        Flux::Core::FluxWorkspaceBridge::setComponentPropertyStr(ref, prop, value);
    }
}
