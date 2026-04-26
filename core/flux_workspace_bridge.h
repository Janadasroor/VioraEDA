#ifndef FLUX_WORKSPACE_BRIDGE_H
#define FLUX_WORKSPACE_BRIDGE_H

#include <QString>
#include <QMap>

class SchematicAPI;

namespace Flux {
namespace Core {

class FluxWorkspaceBridge {
public:
    static void setVariable(const QString& name, double value);
    static double getVariable(const QString& name);
    
    static void setComponentProperty(const char* ref, const char* prop, double value);
    static void setComponentPropertyStr(const char* ref, const char* prop, const char* value);
};

void set_active_schematic_api(SchematicAPI* api);

} // namespace Core
} // namespace Flux

extern "C" {
    double flux_get_var(const char* name);
    void flux_set_prop(const char* ref, const char* prop, double value);
    void flux_set_prop_str(const char* ref, const char* prop, const char* value);
}

#endif // FLUX_WORKSPACE_BRIDGE_H
