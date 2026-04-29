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
    
    // Simulation Data Access
    int flux_sim_get_vector_size(const char* name);
    double flux_sim_get_vector_val(const char* name, int index);
    double flux_sim_get_vector_x(const char* name, int index);
    
    // Simulation Control
    void flux_run_sim(const char* analysisType, double tStop, double tStep);
    
    // Project Info
    const char* flux_get_project_name();
    const char* flux_get_schematic_file();
    const char* flux_get_open_schematics();
    void flux_select_schematic(const char* fileName);
    
    // Standard Output
    void viora_flux_print(const char* msg);
    
    // Plotting
    void flux_plot_point(const char* seriesName, double x, double y);
    
    // SPICE Runtime Registration
    void flux_register_analysis(const char* analysisType);
    void flux_register_measure(const char* name, const char* measureType);
    void flux_register_probe(const char* varName, const char* outputName);
    void flux_register_save(const char* varName);
}

#endif // FLUX_WORKSPACE_BRIDGE_H
