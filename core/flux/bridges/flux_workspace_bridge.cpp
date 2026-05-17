#include "flux_workspace_bridge.h"
#include "../simulation/jit_context_manager.h"
#include "flux_design_rule_bridge.h"
#include "../schematic/editor/schematic_editor.h"
#include "../schematic/editor/schematic_api.h"
#include "../simulator/bridge/sim_manager.h"
#include "../../simulator/core/sim_results.h"
#include "../ui/waveform_viewer.h"
#include <QDebug>
#include <QApplication>
#include <QEventLoop>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

static const char* dbl_to_str(double d) {
    uint64_t raw;
    std::memcpy(&raw, &d, sizeof(raw));
    return reinterpret_cast<const char*>(static_cast<uintptr_t>(raw));
}

static QMap<QString, double> g_fluxVars;
static SchematicAPI* g_activeApi = nullptr;
static std::vector<std::string> g_stringPool;

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

const char* pool_workspace_string(const QString& s) {
    static std::mutex poolMutex;
    std::lock_guard<std::mutex> lock(poolMutex);
    g_stringPool.push_back(s.toStdString());
    return g_stringPool.back().c_str();
}

} // namespace Core
} // namespace Flux

extern "C" {
    double flux_get_var(double name_dbl) {
        const char* name = dbl_to_str(name_dbl);
        if (!name) return 0.0;
        return Flux::Core::FluxWorkspaceBridge::getVariable(QString::fromUtf8(name));
    }

    void flux_set_var(double name_dbl, double value) {
        const char* name = dbl_to_str(name_dbl);
        if (name) Flux::Core::FluxWorkspaceBridge::setVariable(QString::fromUtf8(name), value);
    }
    
    void flux_set_prop(double ref_dbl, double prop_dbl, double value) {
        const char* ref = dbl_to_str(ref_dbl);
        const char* prop = dbl_to_str(prop_dbl);
        Flux::Core::FluxWorkspaceBridge::setComponentProperty(ref, prop, value);
    }
    
    void flux_set_prop_str(double ref_dbl, double prop_dbl, double value_dbl) {
        const char* ref = dbl_to_str(ref_dbl);
        const char* prop = dbl_to_str(prop_dbl);
        const char* value = dbl_to_str(value_dbl);
        Flux::Core::FluxWorkspaceBridge::setComponentPropertyStr(ref, prop, value);
    }

    // --- Simulation Data Hooks ---
    
    int flux_sim_get_vector_size(double name_dbl) {
        const char* name = dbl_to_str(name_dbl);
        auto* res = Flux::JITContextManager::instance().getSimulationResults();
        if (!res || !name) return 0;
        std::string n(name);
        for (const auto& w : res->waveforms) {
            if (w.name == n) return static_cast<int>(w.yData.size());
        }
        return 0;
    }
    
    double flux_sim_get_vector_val(double name_dbl, int index) {
        const char* name = dbl_to_str(name_dbl);
        auto* res = Flux::JITContextManager::instance().getSimulationResults();
        if (!res || !name) return 0.0;
        std::string n(name);
        for (const auto& w : res->waveforms) {
            if (w.name == n && index >= 0 && index < static_cast<int>(w.yData.size())) {
                return w.yData[index];
            }
        }
        return 0.0;
    }
    
    double flux_sim_get_vector_x(double name_dbl, int index) {
        const char* name = dbl_to_str(name_dbl);
        auto* res = Flux::JITContextManager::instance().getSimulationResults();
        if (!res || !name) return 0.0;
        std::string n(name);
        for (const auto& w : res->waveforms) {
            if (w.name == n && index >= 0 && index < static_cast<int>(w.xData.size())) {
                return w.xData[index];
            }
        }
        return 0.0;
    }

    void flux_run_sim(double analysis_dbl, double tStop, double tStep) {
        const char* analysisType = dbl_to_str(analysis_dbl);
        auto* editor = qobject_cast<SchematicEditor*>(QApplication::activeWindow());
        if (!editor) return;

        SimulationSetupDialog::Config cfg;
        QString type = QString::fromUtf8(analysisType).toLower();
        if (type == "tran" || type == "transient") cfg.type = SimAnalysisType::Transient;
        else if (type == "ac") cfg.type = SimAnalysisType::AC;
        else if (type == "op") cfg.type = SimAnalysisType::OP;
        else cfg.type = SimAnalysisType::Transient;

        cfg.stop = tStop;
        cfg.step = tStep;

        // Run the simulation
        QEventLoop loop;
        auto* sim = &SimManager::instance();
        
        QObject::connect(sim, &SimManager::simulationFinished, &loop, &QEventLoop::quit);
        QObject::connect(sim, &SimManager::errorOccurred, &loop, &QEventLoop::quit);

        editor->runSimulationConfig(cfg);

        // Block until finished
        loop.exec();
    }

    double flux_get_project_name() {
        const char* result = g_activeApi ? Flux::Core::pool_workspace_string(g_activeApi->projectName()) : "Untitled Project";
        uint64_t raw = reinterpret_cast<uintptr_t>(result);
        double d;
        std::memcpy(&d, &raw, sizeof(d));
        return d;
    }

    double flux_get_schematic_file() {
        const char* result = g_activeApi ? Flux::Core::pool_workspace_string(g_activeApi->filePath()) : "untitled.viosch";
        uint64_t raw = reinterpret_cast<uintptr_t>(result);
        double d;
        std::memcpy(&d, &raw, sizeof(d));
        return d;
    }

    double flux_get_open_schematics() {
        auto* editor = qobject_cast<SchematicEditor*>(QApplication::activeWindow());
        if (!editor) { double d = 0; uint64_t raw = 0; std::memcpy(&d, &raw, sizeof(d)); return d; }
        
        auto* tabs = editor->findChild<QTabWidget*>();
        if (!tabs) { double d = 0; uint64_t raw = 0; std::memcpy(&d, &raw, sizeof(d)); return d; }
        
        QStringList names;
        for (int i = 0; i < tabs->count(); ++i) {
            names << tabs->tabText(i).remove("*"); // Clean modified markers
        }
        const char* result = Flux::Core::pool_workspace_string(names.join(","));
        uint64_t raw = reinterpret_cast<uintptr_t>(result);
        double d;
        std::memcpy(&d, &raw, sizeof(d));
        return d;
    }

    void flux_select_schematic(double fileName_dbl) {
        const char* fileName = dbl_to_str(fileName_dbl);
        if (!fileName) return;
        auto* editor = qobject_cast<SchematicEditor*>(QApplication::activeWindow());
        if (!editor) return;
        
        auto* tabs = editor->findChild<QTabWidget*>();
        if (!tabs) return;

        QString target = QString::fromUtf8(fileName).toLower();
        for (int i = 0; i < tabs->count(); ++i) {
            QString current = tabs->tabText(i).toLower().remove("*");
            if (current == target) {
                tabs->setCurrentIndex(i);
                // The onTabChanged handler in editor will sync the API
                break;
            }
        }
    }
    
    // --- Standard Output Hook ---
    
    void viora_flux_print(double msg_dbl) {
        const char* msg = dbl_to_str(msg_dbl);
        if (!msg) return;
        printf("[STDOUT] %s\n", msg);
        fflush(stdout);
        Flux::JITContextManager::instance().logMessage(QString::fromUtf8(msg));
    }

    // --- Plotting ---

    void flux_plot_point(double series_dbl, double x, double y) {
        const char* seriesName = dbl_to_str(series_dbl);
        if (!seriesName) return;
        auto* editor = qobject_cast<SchematicEditor*>(QApplication::activeWindow());
        if (!editor) return;

        auto* viewer = editor->findChild<WaveformViewer*>();
        if (viewer) {
            viewer->appendPoint(QString::fromUtf8(seriesName), x, y);
        }
    }

}
