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
    double flux_get_var(double name_dbl);
    void flux_set_var(double name_dbl, double value);
    void flux_set_prop(double ref_dbl, double prop_dbl, double value);
    void flux_set_prop_str(double ref_dbl, double prop_dbl, double value_dbl);
    
    // Simulation Data Access
    int flux_sim_get_vector_size(double name_dbl);
    double flux_sim_get_vector_val(double name_dbl, int index);
    double flux_sim_get_vector_x(double name_dbl, int index);
    
    // Simulation Control
    void flux_run_sim(double analysis_dbl, double tStop, double tStep);
    
    // Project Info
    double flux_get_project_name();
    double flux_get_schematic_file();
    double flux_get_open_schematics();
    void flux_select_schematic(double fileName_dbl);
    
    // Standard Output
    void viora_flux_print(double msg_dbl);
    
    // Plotting
    void flux_plot_point(double series_dbl, double x, double y);
    
    // SPICE Runtime Registration (defined in fluxscript spice_runtime.cpp)
    double flux_register_analysis(double analysis_dbl);
    double flux_register_measure(double name_dbl, double type_dbl);
    double flux_register_probe(double var_dbl, double output_dbl);
    double flux_register_save(double var_dbl);
}

#endif // FLUX_WORKSPACE_BRIDGE_H
