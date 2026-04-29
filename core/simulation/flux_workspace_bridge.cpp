#include "flux_workspace_bridge.h"
#include "jit_context_manager.h"
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

    // --- Simulation Data Hooks ---
    
    int flux_sim_get_vector_size(const char* name) {
        auto* res = Flux::JITContextManager::instance().getSimulationResults();
        if (!res || !name) return 0;
        std::string n(name);
        for (const auto& w : res->waveforms) {
            if (w.name == n) return static_cast<int>(w.yData.size());
        }
        return 0;
    }
    
    double flux_sim_get_vector_val(const char* name, int index) {
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
    
    double flux_sim_get_vector_x(const char* name, int index) {
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

    void flux_run_sim(const char* analysisType, double tStop, double tStep) {
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

    const char* flux_get_project_name() {
        if (!g_activeApi) return "Untitled Project";
        return Flux::Core::pool_workspace_string(g_activeApi->projectName());
    }

    const char* flux_get_schematic_file() {
        if (!g_activeApi) return "untitled.viosch";
        return Flux::Core::pool_workspace_string(g_activeApi->filePath());
    }

    const char* flux_get_open_schematics() {
        auto* editor = qobject_cast<SchematicEditor*>(QApplication::activeWindow());
        if (!editor) return "";
        
        auto* tabs = editor->findChild<QTabWidget*>();
        if (!tabs) return "";
        
        QStringList names;
        for (int i = 0; i < tabs->count(); ++i) {
            names << tabs->tabText(i).remove("*"); // Clean modified markers
        }
        return Flux::Core::pool_workspace_string(names.join(","));
    }

    void flux_select_schematic(const char* fileName) {
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
    
    void viora_flux_print(const char* msg) {
        if (!msg) return;
        printf("[STDOUT] %s\n", msg);
        fflush(stdout);
        Flux::JITContextManager::instance().logMessage(QString::fromUtf8(msg));
    }

    // --- Plotting ---

    void flux_plot_point(const char* seriesName, double x, double y) {
        if (!seriesName) return;
        auto* editor = qobject_cast<SchematicEditor*>(QApplication::activeWindow());
        if (!editor) return;

        auto* viewer = editor->findChild<WaveformViewer*>();
        if (viewer) {
            viewer->appendPoint(QString::fromUtf8(seriesName), x, y);
        }
    }

    void flux_register_analysis(const char* analysisType) {
        if (!analysisType) return;
        Flux::JITContextManager::instance().logMessage(QString("[JIT] Requested Analysis: %1").arg(analysisType));
    }

    void flux_register_measure(const char* name, const char* measureType) {
        if (!name || !measureType) return;
        Flux::JITContextManager::instance().logMessage(QString("[JIT] Requested Measurement: %1 %2").arg(name, measureType));
    }

    void flux_register_probe(const char* varName, const char* outputName) {
        if (!varName) return;
        Flux::JITContextManager::instance().logMessage(QString("[JIT] Requested Probe: %1").arg(varName));
    }

    void flux_register_save(const char* varName) {
        if (!varName) return;
        Flux::JITContextManager::instance().logMessage(QString("[JIT] Requested Save: %1").arg(varName));
    }
}
