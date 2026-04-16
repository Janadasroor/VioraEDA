/**
 * @file bindings_core.cpp
 * @brief nanobind Python bindings for the headless VioSpice solver.
 *
 * Usage from Python:
 *     import vspice
 *     value, ok = vspice.parse_spice_number("4.7u")
 *     results = vspice.SimResults()
 *     wf = vspice.SimWaveform("V(out)", [0,1,2], [0, 1.5, 3.0])
 *     arr = wf.y  # numpy array (zero-copy view)
 */

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/complex.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/ndarray.h>
#include <nanobind/trampoline.h>

#include "sim_value_parser.h"
#include "sim_results.h"
#include "sim_netlist.h"
#include "sim_math.h"
#include "sim_model_parser.h"
#include "sim_meas_evaluator.h"
#include "raw_data_parser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unistd.h>

namespace nb = nanobind;

// ---------------------------------------------------------------------------
// Helper: convert std::vector<double> to a numpy ndarray (copies data, safe)
// ---------------------------------------------------------------------------
static nb::ndarray<double, nb::numpy> vec_to_ndarray(const std::vector<double>& v) {
    if (v.empty()) {
        return nb::ndarray<double, nb::numpy>(nullptr, {0}, {});
    }
    auto* buf = new std::vector<double>(v);
    return nb::ndarray<double, nb::numpy>(
        buf->data(), {buf->size()}, {},
        nb::capsule(buf, [](void* p) noexcept { delete static_cast<std::vector<double>*>(p); })
    );
}

// ---------------------------------------------------------------------------
// parse_spice_number — standalone function
// ---------------------------------------------------------------------------
static std::tuple<double, bool> parse_spice_number(const std::string& text) {
    double value = 0.0;
    bool ok = SimValueParser::parseSpiceNumber(text, value);
    return {value, ok};
}

NB_MODULE(_core, m) {
    // -----------------------------------------------------------------------
    // Module docstring
    // -----------------------------------------------------------------------
    m.doc() = R"(
VioSpice headless solver — Python bindings via nanobind.

Core features:
  - SPICE number parser (engineering suffixes, micro/omega variants)
  - Waveform data structures with zero-copy numpy views
  - Simulation results container
  - Measurement evaluators (.meas statements)
  - RF / S-parameter utilities
    )";

    // -----------------------------------------------------------------------
    // Standalone functions
    // -----------------------------------------------------------------------
    m.def("parse_spice_number", &parse_spice_number,
          nb::arg("text"),
          R"(Parse a SPICE-style number string (e.g. '4.7u', '1k', '2meg').

Returns (value: float, ok: bool).
)");

    m.def("format_spice_number",
          [](double value, int precision) {
              // Re-implement here since the original is in the Qt-heavy file
              if (value == 0.0) return std::string("0");
              std::string sign = value < 0 ? "-" : "";
              double abs_val = std::abs(value);
              struct Suffix { double scale; const char* s; };
              static const Suffix suffixes[] = {
                  {1e12, "t"}, {1e9, "g"}, {1e6, "meg"}, {1e3, "k"},
                  {1.0, ""}, {1e-3, "m"}, {1e-6, "u"},
                  {1e-9, "n"}, {1e-12, "p"}, {1e-15, "f"},
              };
              double chosen_scale = 1.0;
              const char* chosen_suffix = "";
              for (const auto& s : suffixes) {
                  double scaled = abs_val / s.scale;
                  if (scaled >= 1.0 && scaled < 1000.0) {
                      chosen_scale = s.scale;
                      chosen_suffix = s.s;
                      break;
                  }
              }
              char buf[64];
              snprintf(buf, sizeof(buf), "%.6g", abs_val / chosen_scale);
              // Strip trailing zeros
              std::string out(buf);
              if (out.find('.') != std::string::npos) {
                  while (out.back() == '0') out.pop_back();
                  if (out.back() == '.') out.pop_back();
              }
              return sign + out + chosen_suffix;
          },
          nb::arg("value"), nb::arg("precision") = 6,
          "Format a float with SPICE engineering suffixes.");

    // -----------------------------------------------------------------------
    // Enums
    // -----------------------------------------------------------------------
    nb::enum_<SimAnalysisType>(m, "SimAnalysisType")
        .value("OP", SimAnalysisType::OP)
        .value("Transient", SimAnalysisType::Transient)
        .value("AC", SimAnalysisType::AC)
        .value("DC", SimAnalysisType::DC)
        .value("MonteCarlo", SimAnalysisType::MonteCarlo)
        .value("Sensitivity", SimAnalysisType::Sensitivity)
        .value("ParametricSweep", SimAnalysisType::ParametricSweep)
        .value("Noise", SimAnalysisType::Noise)
        .value("Distortion", SimAnalysisType::Distortion)
        .value("Optimization", SimAnalysisType::Optimization)
        .value("FFT", SimAnalysisType::FFT)
        .value("RealTime", SimAnalysisType::RealTime)
        .value("SParameter", SimAnalysisType::SParameter);

    nb::enum_<SimIntegrationMethod>(m, "SimIntegrationMethod")
        .value("BackwardEuler", SimIntegrationMethod::BackwardEuler)
        .value("Trapezoidal", SimIntegrationMethod::Trapezoidal)
        .value("Gear2", SimIntegrationMethod::Gear2);

    nb::enum_<SimTransientStorageMode>(m, "SimTransientStorageMode")
        .value("Full", SimTransientStorageMode::Full)
        .value("Strided", SimTransientStorageMode::Strided)
        .value("AutoDecimate", SimTransientStorageMode::AutoDecimate);

    // -----------------------------------------------------------------------
    // SParameterPoint
    // -----------------------------------------------------------------------
    nb::class_<SParameterPoint>(m, "SParameterPoint")
        .def_rw("frequency", &SParameterPoint::frequency)
        .def_rw("s11", &SParameterPoint::s11)
        .def_rw("s12", &SParameterPoint::s12)
        .def_rw("s21", &SParameterPoint::s21)
        .def_rw("s22", &SParameterPoint::s22);

    // -----------------------------------------------------------------------
    // SimWaveform — with zero-copy numpy arrays
    // -----------------------------------------------------------------------
    nb::class_<SimWaveform>(m, "SimWaveform")
        .def(nb::init<>(),
             "Default constructor (empty waveform).")
        .def(nb::init<std::string, std::vector<double>, std::vector<double>>(),
             nb::arg("name"), nb::arg("x_data"), nb::arg("y_data"),
             "Construct a waveform with name, x-data, and y-data.")
        .def_rw("name", &SimWaveform::name)
        .def_prop_rw("x",
            [](const SimWaveform& w) -> nb::object { return nb::cast(w.xData); },
            [](SimWaveform& w, const std::vector<double>& v) { w.xData = v; },
            "X-axis data as list (time or frequency).")
        .def_prop_rw("y",
            [](const SimWaveform& w) -> nb::object { return nb::cast(w.yData); },
            [](SimWaveform& w, const std::vector<double>& v) { w.yData = v; },
            "Y-axis data as list (voltage/current magnitude).")
        .def_prop_rw("phase",
            [](const SimWaveform& w) -> nb::object { return nb::cast(w.yPhase); },
            [](SimWaveform& w, const std::vector<double>& v) { w.yPhase = v; },
            "Phase data (degrees) for AC analysis.")
        .def("stats",
            [](const SimWaveform& w) -> std::map<std::string, double> {
                if (w.yData.empty()) return {{"count", 0}};
                double sum = 0, sq_sum = 0, mn = w.yData[0], mx = w.yData[0];
                for (double v : w.yData) {
                    sum += v; sq_sum += v * v;
                    if (v < mn) mn = v; if (v > mx) mx = v;
                }
                double n = static_cast<double>(w.yData.size());
                return {
                    {"count", n},
                    {"min", mn},
                    {"max", mx},
                    {"avg", sum / n},
                    {"rms", std::sqrt(sq_sum / n)},
                    {"pp", mx - mn},
                };
            },
            "Compute summary statistics (min, max, avg, rms, pp).")
        .def("__repr__",
            [](const SimWaveform& w) {
                char buf[128];
                snprintf(buf, sizeof(buf), "<SimWaveform '%s' points=%zu>",
                         w.name.c_str(), w.yData.size());
                return std::string(buf);
            });

    // -----------------------------------------------------------------------
    // SimResults
    // -----------------------------------------------------------------------
    nb::class_<SimResults>(m, "SimResults")
        .def(nb::init<>())
        .def_rw("schema_version", &SimResults::schemaVersion)
        .def_rw("analysis_type", &SimResults::analysisType)
        .def_rw("x_axis_name", &SimResults::xAxisName)
        .def_rw("y_axis_name", &SimResults::yAxisName)
        .def_prop_rw("waveforms",
            [](const SimResults& r) -> nb::object { return nb::cast(r.waveforms); },
            [](SimResults& r, const std::vector<SimWaveform>& v) { r.waveforms = v; },
            "List of SimWaveform objects.")
        .def_prop_rw("node_voltages",
            [](const SimResults& r) -> nb::object { return nb::cast(r.nodeVoltages); },
            [](SimResults& r, const std::map<std::string, double>& v) { r.nodeVoltages = v; },
            "Node voltages map.")
        .def_prop_rw("branch_currents",
            [](const SimResults& r) -> nb::object { return nb::cast(r.branchCurrents); },
            [](SimResults& r, const std::map<std::string, double>& v) { r.branchCurrents = v; },
            "Branch currents map.")
        .def_prop_rw("diagnostics",
            [](const SimResults& r) -> nb::object { return nb::cast(r.diagnostics); },
            [](SimResults& r, const std::vector<std::string>& v) { r.diagnostics = v; },
            "Diagnostic messages.")
        .def_prop_rw("s_parameter_results",
            [](const SimResults& r) -> nb::object { return nb::cast(r.sParameterResults); },
            [](SimResults& r, const std::vector<SParameterPoint>& v) { r.sParameterResults = v; },
            "S-parameter results.")
        .def_prop_rw("rf_z0",
            [](const SimResults& r) -> double { return r.rfZ0; },
            [](SimResults& r, double v) { r.rfZ0 = v; },
            "Reference impedance.")
        .def("is_schema_compatible", &SimResults::isSchemaCompatible)
        .def("to_dict",
            [](const SimResults& r) -> nb::object {
                nb::dict result;
                result["analysis_type"] = nb::cast(static_cast<int>(r.analysisType));
                result["waveform_count"] = (int)r.waveforms.size();
                result["node_count"] = (int)r.nodeVoltages.size();
                result["branch_count"] = (int)r.branchCurrents.size();
                result["diagnostics"] = nb::cast(r.diagnostics);
                return nb::cast(result);
            },
            "Return a compact Python dict with summary info.");

    // -----------------------------------------------------------------------
    // SimAnalysisConfig (minimal binding for key fields)
    // -----------------------------------------------------------------------
    nb::class_<SimAnalysisConfig>(m, "SimAnalysisConfig")
        .def(nb::init<>())
        .def_prop_rw("type",
            [](const SimAnalysisConfig& c) { return c.type; },
            [](SimAnalysisConfig& c, SimAnalysisType v) { c.type = v; })
        .def_prop_rw("t_stop",
            [](const SimAnalysisConfig& c) { return c.tStop; },
            [](SimAnalysisConfig& c, double v) { c.tStop = v; })
        .def_prop_rw("t_start",
            [](const SimAnalysisConfig& c) { return c.tStart; },
            [](SimAnalysisConfig& c, double v) { c.tStart = v; })
        .def_prop_rw("t_step",
            [](const SimAnalysisConfig& c) { return c.tStep; },
            [](SimAnalysisConfig& c, double v) { c.tStep = v; })
        .def_prop_rw("f_start",
            [](const SimAnalysisConfig& c) { return c.fStart; },
            [](SimAnalysisConfig& c, double v) { c.fStart = v; })
        .def_prop_rw("f_stop",
            [](const SimAnalysisConfig& c) { return c.fStop; },
            [](SimAnalysisConfig& c, double v) { c.fStop = v; })
        .def_prop_rw("f_points",
            [](const SimAnalysisConfig& c) { return c.fPoints; },
            [](SimAnalysisConfig& c, int v) { c.fPoints = v; })
        .def_prop_rw("rf_z0",
            [](const SimAnalysisConfig& c) { return c.rfZ0; },
            [](SimAnalysisConfig& c, double v) { c.rfZ0 = v; })
        .def_prop_rw("integration_method",
            [](const SimAnalysisConfig& c) { return c.integrationMethod; },
            [](SimAnalysisConfig& c, SimIntegrationMethod v) { c.integrationMethod = v; })
        .def_prop_rw("rel_tol",
            [](const SimAnalysisConfig& c) { return c.relTol; },
            [](SimAnalysisConfig& c, double v) { c.relTol = v; })
        .def_prop_rw("abs_tol",
            [](const SimAnalysisConfig& c) { return c.absTol; },
            [](SimAnalysisConfig& c, double v) { c.absTol = v; });

    // -----------------------------------------------------------------------
    // SimComponentType enum
    // -----------------------------------------------------------------------
    nb::enum_<SimComponentType>(m, "SimComponentType")
        .value("Resistor", SimComponentType::Resistor)
        .value("Capacitor", SimComponentType::Capacitor)
        .value("Inductor", SimComponentType::Inductor)
        .value("VoltageSource", SimComponentType::VoltageSource)
        .value("CurrentSource", SimComponentType::CurrentSource)
        .value("Diode", SimComponentType::Diode)
        .value("BJT_NPN", SimComponentType::BJT_NPN)
        .value("BJT_PNP", SimComponentType::BJT_PNP)
        .value("MOSFET_NMOS", SimComponentType::MOSFET_NMOS)
        .value("MOSFET_PMOS", SimComponentType::MOSFET_PMOS)
        .value("JFET_NJF", SimComponentType::JFET_NJF)
        .value("JFET_PJF", SimComponentType::JFET_PJF)
        .value("OpAmpMacro", SimComponentType::OpAmpMacro)
        .value("Switch", SimComponentType::Switch)
        .value("TransmissionLine", SimComponentType::TransmissionLine)
        .value("SubcircuitInstance", SimComponentType::SubcircuitInstance)
        .value("VCVS", SimComponentType::VCVS)
        .value("VCCS", SimComponentType::VCCS)
        .value("CCVS", SimComponentType::CCVS)
        .value("CCCS", SimComponentType::CCCS)
        .value("B_VoltageSource", SimComponentType::B_VoltageSource)
        .value("B_CurrentSource", SimComponentType::B_CurrentSource);

    // -----------------------------------------------------------------------
    // ToleranceDistribution enum
    // -----------------------------------------------------------------------
    nb::enum_<ToleranceDistribution>(m, "ToleranceDistribution")
        .value("Uniform", ToleranceDistribution::Uniform)
        .value("Gaussian", ToleranceDistribution::Gaussian)
        .value("WorstCase", ToleranceDistribution::WorstCase);

    // -----------------------------------------------------------------------
    // SimNode
    // -----------------------------------------------------------------------
    nb::class_<SimNode>(m, "SimNode")
        .def(nb::init<>())
        .def(nb::init<int, std::string>(), nb::arg("id"), nb::arg("name"))
        .def_rw("id", &SimNode::id)
        .def_rw("name", &SimNode::name)
        .def("__repr__", [](const SimNode& n) {
            char buf[64];
            snprintf(buf, sizeof(buf), "<SimNode id=%d name='%s'>", n.id, n.name.c_str());
            return std::string(buf);
        });

    // -----------------------------------------------------------------------
    // SimModel
    // -----------------------------------------------------------------------
    nb::class_<SimModel>(m, "SimModel")
        .def(nb::init<>())
        .def(nb::init<std::string, SimComponentType>(),
             nb::arg("name"), nb::arg("type"))
        .def_rw("name", &SimModel::name)
        .def_rw("type", &SimModel::type)
        .def_prop_rw("params",
            [](const SimModel& m) -> nb::object { return nb::cast(m.params); },
            [](SimModel& m, const std::map<std::string, double>& v) { m.params = v; },
            "Model parameters (e.g. IS, BF, VTO for transistors).")
        .def("__repr__", [](const SimModel& m) {
            return "<SimModel '" + m.name + "' type=" + std::to_string(static_cast<int>(m.type)) +
                   " params=" + std::to_string(m.params.size()) + ">";
        });

    // -----------------------------------------------------------------------
    // SimTolerance
    // -----------------------------------------------------------------------
    nb::class_<SimTolerance>(m, "SimTolerance")
        .def(nb::init<>())
        .def(nb::init<double, ToleranceDistribution>(),
             nb::arg("value"), nb::arg("distribution"))
        .def_rw("value", &SimTolerance::value)
        .def_rw("distribution", &SimTolerance::distribution)
        .def_rw("lot_id", &SimTolerance::lotId)
        .def("__repr__", [](const SimTolerance& t) {
            return "<SimTolerance " + std::to_string(t.value * 100) + "%>";
        });

    // -----------------------------------------------------------------------
    // SimComponentInstance
    // -----------------------------------------------------------------------
    nb::class_<SimComponentInstance>(m, "SimComponentInstance")
        .def(nb::init<>())
        .def(nb::init<std::string, SimComponentType, std::vector<int>>(),
             nb::arg("name"), nb::arg("type"), nb::arg("nodes"),
             "Create a component instance (e.g. R1, Q1, V1).")
        .def_rw("name", &SimComponentInstance::name)
        .def_rw("type", &SimComponentInstance::type)
        .def_rw("nodes", &SimComponentInstance::nodes)
        .def_prop_rw("params",
            [](const SimComponentInstance& c) -> nb::object { return nb::cast(c.params); },
            [](SimComponentInstance& c, const std::map<std::string, double>& v) { c.params = v; },
            "Numeric parameters (e.g. resistance, capacitance).")
        .def_prop_rw("param_expressions",
            [](const SimComponentInstance& c) -> nb::object { return nb::cast(c.paramExpressions); },
            [](SimComponentInstance& c, const std::map<std::string, std::string>& v) { c.paramExpressions = v; },
            "Symbolic parameter expressions (e.g. '{RVAL*1.2}').")
        .def_prop_rw("tolerances",
            [](const SimComponentInstance& c) -> nb::object { return nb::cast(c.tolerances); },
            [](SimComponentInstance& c, const std::map<std::string, SimTolerance>& v) { c.tolerances = v; },
            "Parameter tolerances for Monte Carlo / sensitivity analysis.")
        .def_rw("model_name", &SimComponentInstance::modelName)
        .def_rw("subcircuit_name", &SimComponentInstance::subcircuitName)
        .def("__repr__", [](const SimComponentInstance& c) {
            return "<SimComponent '" + c.name + "' type=" + std::to_string(static_cast<int>(c.type)) +
                   " nodes=" + std::to_string(c.nodes.size()) + ">";
        })
        .def("to_spice", [](const SimComponentInstance& c) -> std::string {
            // Generate a simple SPICE netlist line
            std::string line = c.name;
            for (int n : c.nodes) line += " " + std::to_string(n);
            // Add key parameters
            for (const auto& [k, v] : c.params) {
                char buf[64];
                snprintf(buf, sizeof(buf), " %s=%.6g", k.c_str(), v);
                line += buf;
            }
            return line;
        }, "Generate a SPICE netlist line for this component.");

    // -----------------------------------------------------------------------
    // SimSubcircuit
    // -----------------------------------------------------------------------
    nb::class_<SimSubcircuit>(m, "SimSubcircuit")
        .def(nb::init<>())
        .def(nb::init<std::string, std::vector<std::string>>(),
             nb::arg("name"), nb::arg("pin_names"),
             "Create a subcircuit definition.")
        .def_rw("name", &SimSubcircuit::name)
        .def_rw("pin_names", &SimSubcircuit::pinNames)
        .def_prop_rw("components",
            [](const SimSubcircuit& s) -> nb::object { return nb::cast(s.components); },
            [](SimSubcircuit& s, const std::vector<SimComponentInstance>& v) { s.components = v; },
            "Components inside the subcircuit.")
        .def_prop_rw("models",
            [](const SimSubcircuit& s) -> nb::object { return nb::cast(s.models); },
            [](SimSubcircuit& s, const std::map<std::string, SimModel>& v) { s.models = v; },
            "Models used inside the subcircuit.")
        .def_prop_rw("parameters",
            [](const SimSubcircuit& s) -> nb::object { return nb::cast(s.parameters); },
            [](SimSubcircuit& s, const std::map<std::string, double>& v) { s.parameters = v; },
            "Subcircuit parameters.")
        .def("__repr__", [](const SimSubcircuit& s) {
            return "<SimSubcircuit '" + s.name + "' pins=" + std::to_string(s.pinNames.size()) +
                   " comps=" + std::to_string(s.components.size()) + ">";
        });

    // -----------------------------------------------------------------------
    // SimNetlist
    // -----------------------------------------------------------------------
    nb::class_<SimNetlist>(m, "SimNetlist")
        .def(nb::init<>(), "Create an empty netlist.")
        .def("add_node", &SimNetlist::addNode,
             nb::arg("name"),
             "Add a named node. Returns the node ID (0 = ground).")
        .def("ground_node", &SimNetlist::groundNode,
             "Return the ground node ID (always 0).")
        .def("add_component", &SimNetlist::addComponent,
             nb::arg("comp"),
             "Add a component instance to the netlist.")
        .def("add_model", &SimNetlist::addModel,
             nb::arg("model"),
             "Add a SPICE model definition.")
        .def("add_subcircuit", &SimNetlist::addSubcircuit,
             nb::arg("sub"),
             "Add a subcircuit definition.")
        .def("set_parameter", &SimNetlist::setParameter,
             nb::arg("name"), nb::arg("value"),
             "Set a global parameter (e.g. TEMP, RVAL).")
        .def("get_parameter", &SimNetlist::getParameter,
             nb::arg("name"), nb::arg("default_val") = 0.0,
             "Get a global parameter value.")
        .def("add_auto_probe", &SimNetlist::addAutoProbe,
             nb::arg("signal_name"),
             "Mark a signal for automatic probing during simulation.")
        .def("flatten", &SimNetlist::flatten,
             "Expand all subcircuits into primitive components.")
        .def("evaluate_expressions", &SimNetlist::evaluateExpressions,
             "Resolve all parameter expressions into numeric values.")
        .def_prop_rw("analysis",
            [](SimNetlist& n) -> const SimAnalysisConfig& { return n.analysis(); },
            [](SimNetlist& n, const SimAnalysisConfig& v) { n.setAnalysis(v); },
            "Simulation analysis configuration.")
        .def_prop_rw("components",
            [](const SimNetlist& n) -> nb::object { return nb::cast(n.components()); },
            [](SimNetlist& n, const std::vector<SimComponentInstance>& v) {
                n.mutableComponents() = v;
            },
            "All component instances in the netlist.")
        .def_prop_rw("models",
            [](const SimNetlist& n) -> nb::object { return nb::cast(n.models()); },
            [](SimNetlist& n, const std::map<std::string, SimModel>& v) {
                n.mutableModels() = v;
            },
            "All model definitions.")
        .def_prop_rw("subcircuits",
            [](const SimNetlist& n) -> nb::object { return nb::cast(n.subcircuits()); },
            [](SimNetlist& n, const std::map<std::string, SimSubcircuit>& v) {
                n.mutableSubcircuits() = v;
            },
            "All subcircuit definitions.")
        .def("auto_probes",
            [](const SimNetlist& n) -> nb::object { return nb::cast(n.autoProbes()); },
            "Signals to probe during simulation (read-only).")
        .def("diagnostics",
            [](const SimNetlist& n) -> nb::object { return nb::cast(n.diagnostics()); },
            "Diagnostic messages from netlist parsing (read-only).")
        .def("node_count", &SimNetlist::nodeCount,
             "Total number of nodes in the netlist.")
        .def("find_node", &SimNetlist::findNode,
             nb::arg("name"),
             "Find a node ID by name. Returns -1 if not found.")
        .def("node_name", &SimNetlist::nodeName,
             nb::arg("id"),
             "Get a node name by ID.")
        .def("find_model",
            [](const SimNetlist& n, const std::string& name) -> nb::object {
                const SimModel* m = n.findModel(name);
                if (!m) return nb::none();
                return nb::cast(*m);  // Return a copy
            },
            nb::arg("name"),
            "Find a model by name, or None.")
        .def("find_subcircuit",
            [](const SimNetlist& n, const std::string& name) -> nb::object {
                const SimSubcircuit* s = n.findSubcircuit(name);
                if (!s) return nb::none();
                return nb::cast(*s);  // Return a copy
            },
            nb::arg("name"),
            "Find a subcircuit by name, or None.")
        .def("to_dict",
            [](const SimNetlist& n) -> nb::object {
                nb::dict result;
                result["nodes"] = n.nodeCount();
                result["components"] = (int)n.components().size();
                result["models"] = (int)n.models().size();
                result["subcircuits"] = (int)n.subcircuits().size();
                result["diagnostics"] = nb::cast(n.diagnostics());
                return nb::cast(result);
            },
            "Return a compact Python dict with netlist summary.")
        .def("__repr__", [](const SimNetlist& n) {
            return "<SimNetlist nodes=" + std::to_string(n.nodeCount()) +
                   " comps=" + std::to_string(n.components().size()) +
                   " models=" + std::to_string(n.models().size()) + ">";
        });

    // -----------------------------------------------------------------------
    // MeasFunction enum
    // -----------------------------------------------------------------------
    nb::enum_<MeasFunction>(m, "MeasFunction")
        .value("FIND", MeasFunction::FIND)
        .value("DERIV", MeasFunction::DERIV)
        .value("PARAM", MeasFunction::PARAM)
        .value("MAX", MeasFunction::MAX)
        .value("MIN", MeasFunction::MIN)
        .value("PP", MeasFunction::PP)
        .value("AVG", MeasFunction::AVG)
        .value("RMS", MeasFunction::RMS)
        .value("N", MeasFunction::N)
        .value("INTEG", MeasFunction::INTEG)
        .value("MIN_AT", MeasFunction::MIN_AT)
        .value("MAX_AT", MeasFunction::MAX_AT)
        .value("FIRST", MeasFunction::FIRST)
        .value("LAST", MeasFunction::LAST)
        .value("DUTY", MeasFunction::DUTY)
        .value("SLEWRATE", MeasFunction::SLEWRATE)
        .value("SLEWRATE_FALL", MeasFunction::SLEWRATE_FALL)
        .value("SLEWRATE_RISE", MeasFunction::SLEWRATE_RISE)
        .value("FREQ", MeasFunction::FREQ)
        .value("PERIOD", MeasFunction::PERIOD)
        .value("TRIG_TARG", MeasFunction::TRIG_TARG)
        .value("Unknown", MeasFunction::Unknown);

    // -----------------------------------------------------------------------
    // MeasTrigger
    // -----------------------------------------------------------------------
    nb::class_<MeasTrigger>(m, "MeasTrigger")
        .def(nb::init<>())
        .def_rw("signal", &MeasTrigger::signal)
        .def_rw("lhs_expr", &MeasTrigger::lhsExpr)
        .def_rw("rhs_expr", &MeasTrigger::rhsExpr)
        .def_rw("value", &MeasTrigger::value)
        .def_rw("index", &MeasTrigger::index)
        .def_rw("rising", &MeasTrigger::rising)
        .def_rw("use_rise_fall", &MeasTrigger::useRiseFall)
        .def_rw("use_cross", &MeasTrigger::useCross)
        .def_rw("use_last", &MeasTrigger::useLast)
        .def_rw("td", &MeasTrigger::td)
        .def_rw("cross", &MeasTrigger::cross);

    // -----------------------------------------------------------------------
    // MeasStatement
    // -----------------------------------------------------------------------
    nb::class_<MeasStatement>(m, "MeasStatement")
        .def(nb::init<>())
        .def_rw("analysis_type", &MeasStatement::analysisType)
        .def_rw("name", &MeasStatement::name)
        .def_rw("function", &MeasStatement::function)
        .def_rw("signal", &MeasStatement::signal)
        .def_rw("expr", &MeasStatement::expr)
        .def_rw("has_at", &MeasStatement::hasAt)
        .def_rw("at", &MeasStatement::at)
        .def_rw("has_when", &MeasStatement::hasWhen)
        .def_rw("when", &MeasStatement::when)
        .def_rw("trig", &MeasStatement::trig)
        .def_rw("targ", &MeasStatement::targ)
        .def_rw("has_trig_targ", &MeasStatement::hasTrigTarg)
        .def_rw("has_from", &MeasStatement::hasFrom)
        .def_rw("has_to", &MeasStatement::hasTo)
        .def_rw("from", &MeasStatement::from)
        .def_rw("to", &MeasStatement::to)
        .def_rw("line_number", &MeasStatement::lineNumber)
        .def_rw("source_name", &MeasStatement::sourceName)
        .def("__repr__", [](const MeasStatement& s) {
            return "<MeasStatement '" + s.name + "' func=" + std::to_string(static_cast<int>(s.function)) + ">";
        });

    // -----------------------------------------------------------------------
    // MeasResult
    // -----------------------------------------------------------------------
    nb::class_<MeasResult>(m, "MeasResult")
        .def(nb::init<>())
        .def_rw("name", &MeasResult::name)
        .def_rw("value", &MeasResult::value)
        .def_rw("valid", &MeasResult::valid)
        .def_rw("quantity_label", &MeasResult::quantityLabel)
        .def_rw("display_unit", &MeasResult::displayUnit)
        .def_rw("error", &MeasResult::error)
        .def("__repr__", [](const MeasResult& r) {
            std::string tag = r.valid ? std::to_string(r.value) : "INVALID";
            return "<MeasResult '" + r.name + "' " + tag +
                   (r.valid ? "" : " error='" + r.error + "'") + ">";
        });

    // -----------------------------------------------------------------------
    // SimMeasEvaluator
    // -----------------------------------------------------------------------
    m.def("meas_parse",
          [](const std::string& line, int line_number, const std::string& source_name) -> std::tuple<MeasStatement, bool> {
              MeasStatement stmt;
              bool ok = SimMeasEvaluator::parse(line, line_number, source_name, stmt);
              return {stmt, ok};
          },
          nb::arg("line"), nb::arg("line_number") = 0, nb::arg("source_name") = "",
          R"(Parse a .meas statement line.

Returns (MeasStatement, ok: bool).
)");

    m.def("meas_evaluate",
          [](const std::vector<MeasStatement>& statements,
             const SimResults& results,
             const std::string& analysis_type) -> std::vector<MeasResult> {
              return SimMeasEvaluator::evaluate(statements, results, analysis_type);
          },
          nb::arg("statements"), nb::arg("results"), nb::arg("analysis_type"),
          R"(Evaluate .meas statements against simulation results.

Only evaluates statements matching the given analysis type (e.g. 'tran', 'ac').
Returns a list of MeasResult.
)");

    // -----------------------------------------------------------------------
    // SimParseDiagnosticSeverity enum
    // -----------------------------------------------------------------------
    nb::enum_<SimParseDiagnosticSeverity>(m, "SimParseDiagnosticSeverity")
        .value("Info", SimParseDiagnosticSeverity::Info)
        .value("Warning", SimParseDiagnosticSeverity::Warning)
        .value("Error", SimParseDiagnosticSeverity::Error);

    // -----------------------------------------------------------------------
    // SimParseDiagnostic
    // -----------------------------------------------------------------------
    nb::class_<SimParseDiagnostic>(m, "SimParseDiagnostic")
        .def(nb::init<>())
        .def_rw("severity", &SimParseDiagnostic::severity)
        .def_rw("line", &SimParseDiagnostic::line)
        .def_rw("source", &SimParseDiagnostic::source)
        .def_rw("message", &SimParseDiagnostic::message)
        .def_rw("text", &SimParseDiagnostic::text)
        .def("__repr__", [](const SimParseDiagnostic& d) {
            return "<Diagnostic L" + std::to_string(d.line) + " " + d.message + ">";
        });

    // -----------------------------------------------------------------------
    // SimModelParseOptions
    // -----------------------------------------------------------------------
    nb::class_<SimModelParseOptions>(m, "SimModelParseOptions")
        .def(nb::init<>())
        .def_rw("source_name", &SimModelParseOptions::sourceName)
        .def_rw("active_lib_section", &SimModelParseOptions::activeLibSection)
        .def_rw("strict", &SimModelParseOptions::strict);

    // -----------------------------------------------------------------------
    // SimModelParser
    // -----------------------------------------------------------------------
    nb::class_<SimModelParser>(m, "SimModelParser")
        .def_static("parse_model_line",
            [](const std::string& line,
               int line_number,
               const std::string& source_name) -> std::tuple<bool, std::map<std::string, SimModel>, std::vector<SimParseDiagnostic>> {
                SimNetlist tmp;
                std::map<std::string, SimModel> models;
                std::vector<SimParseDiagnostic> diags;
                bool ok = SimModelParser::parseModelLine(tmp, models, line, line_number, source_name, &diags);
                return {ok, models, diags};
            },
            nb::arg("line"),
            nb::arg("line_number") = 0, nb::arg("source_name") = "",
            "Parse a single .model line. Returns (ok, models_dict, diagnostics).")
        .def_static("parse_library",
            [](SimNetlist& netlist,
               const std::string& content,
               const SimModelParseOptions& options) -> std::tuple<bool, std::vector<SimParseDiagnostic>> {
                std::vector<SimParseDiagnostic> diags;
                bool ok = SimModelParser::parseLibrary(netlist, content, options, &diags);
                return {ok, diags};
            },
            nb::arg("netlist"), nb::arg("content"), nb::arg("options") = SimModelParseOptions(),
            R"(Parse a SPICE library string containing .model, .subckt, and other definitions.

Returns (ok: bool, diagnostics: list[SimParseDiagnostic]).
)");

    // -----------------------------------------------------------------------
    // Simulation Runner — shells out to viora netlist-run + parses .raw
    // -----------------------------------------------------------------------
    m.def("run_simulation",
          [](const std::string& netlist_text,
             const std::string& analysis,
             const std::string& stop_time,
             const std::string& step_time,
             const std::string& viora_path,
             int timeout_seconds) -> nb::dict {
              nb::dict result;

              // Write netlist to temp file
              char tmpfile[] = "/tmp/vspice_XXXXXX.cir";
              int fd = mkstemps(tmpfile, 4);
              if (fd < 0) { result["ok"] = false; result["error"] = "Failed to create temp file"; return result; }
              ssize_t written = write(fd, netlist_text.data(), netlist_text.size());
              if (written < 0) { close(fd); unlink(tmpfile); result["ok"] = false; result["error"] = "Failed to write temp file"; return result; }
              close(fd);

              // Derive raw file path (viora creates same_basename.raw)
              // tmpfile is "/tmp/vspice_XXXXXX.cir" -> raw is "/tmp/vspice_XXXXXX.raw"
              std::string raw_path = tmpfile;
              size_t dotPos = raw_path.rfind('.');
              if (dotPos != std::string::npos) raw_path.resize(dotPos);
              raw_path += ".raw";

              // Find viora
              std::string cmd = viora_path;
              if (cmd.empty()) {
                  FILE* which = popen("which viora 2>/dev/null", "r");
                  if (which) {
                      char buf[1024];
                      if (fgets(buf, sizeof(buf), which)) {
                          cmd = buf;
                          size_t nl = cmd.find('\n');
                          if (nl != std::string::npos) cmd.resize(nl);
                      }
                      pclose(which);
                  }
                  if (cmd.empty()) {
                      const char* paths[] = { "build/viora", "build-debug/viora", "build-asan/viora", nullptr };
                      for (int i = 0; paths[i]; ++i) {
                          FILE* test = fopen(paths[i], "r");
                          if (test) { fclose(test); cmd = paths[i]; break; }
                      }
                  }
                  if (cmd.empty()) {
                      unlink(tmpfile);
                      result["ok"] = false;
                      result["error"] = "viora not found. Install it or pass viora_path.";
                      return result;
                  }
              }

              // Build command
              std::ostringstream oss;
              oss << cmd << " netlist-run " << tmpfile << " --json";
              if (!analysis.empty()) oss << " --analysis " << analysis;
              if (!stop_time.empty()) oss << " --stop " << stop_time;
              if (!step_time.empty()) oss << " --step " << step_time;
              if (timeout_seconds > 0) oss << " --timeout " << timeout_seconds;
              oss << " 2>/dev/null";

              // Execute
              FILE* pipe = popen(oss.str().c_str(), "r");
              if (!pipe) {
                  unlink(tmpfile); unlink(raw_path.c_str());
                  result["ok"] = false; result["error"] = "Failed to execute viora";
                  return result;
              }
              std::string vio_out;
              char buf[4096];
              while (fgets(buf, sizeof(buf), pipe)) vio_out += buf;
              int rc = pclose(pipe);

              unlink(tmpfile);

              if (rc != 0) {
                  result["ok"] = false;
                  result["error"] = "viora returned non-zero exit code";
                  result["viora_output"] = vio_out;
                  return result;
              }

              // Parse the .raw file if it exists
              RawData raw;
              std::string raw_error;
              bool has_raw = RawDataParser::loadRawAscii(raw_path, &raw, &raw_error);
              unlink(raw_path.c_str());

              if (!has_raw) {
                  result["ok"] = false;
                  result["error"] = "Simulation ran but no results: " + raw_error;
                  return result;
              }

              // Convert to structured result
              result["ok"] = true;
              result["analysis"] = analysis.empty() ? "op" : analysis;

              // Time/frequency axis
              std::vector<double> axis;
              if (!raw.x.empty()) axis = raw.x;

              // Waveforms
              nb::list waveforms;
              for (size_t i = 1; i < raw.varNames.size(); ++i) {
                  nb::dict wf;
                  wf["name"] = raw.varNames[i];
                  wf["x"] = nb::cast(axis);
                  if (i - 1 < raw.y.size()) {
                      wf["y"] = nb::cast(std::vector<double>(raw.y[i-1].begin(), raw.y[i-1].end()));
                  }
                  if (i - 1 < raw.yPhase.size() && raw.hasPhase.size() > i-1 && raw.hasPhase[i-1]) {
                      wf["phase"] = nb::cast(std::vector<double>(raw.yPhase[i-1].begin(), raw.yPhase[i-1].end()));
                  }
                  waveforms.append(wf);
              }
              result["waveforms"] = waveforms;

              // OP results (first point only)
              if (raw.numPoints == 1) {
                  nb::dict node_voltages;
                  nb::dict branch_currents;
                  for (size_t i = 1; i < raw.varNames.size(); ++i) {
                      std::string name = raw.varNames[i];
                      double val = (i - 1 < raw.y.size() && !raw.y[i-1].empty()) ? raw.y[i-1][0] : 0.0;
                      // Extract node/branch from V(name) or I(name)
                      if (name.size() > 2 && name[1] == '(' && name.back() == ')') {
                          std::string inner = name.substr(2, name.size() - 3);
                          if (name[0] == 'V' || name[0] == 'v') node_voltages[inner.c_str()] = val;
                          else if (name[0] == 'I' || name[0] == 'i') branch_currents[inner.c_str()] = val;
                      }
                  }
                  result["node_voltages"] = node_voltages;
                  result["branch_currents"] = branch_currents;
              }

              return result;
          },
          nb::arg("netlist_text"),
          nb::arg("analysis") = "op",
          nb::arg("stop_time") = "",
          nb::arg("step_time") = "",
          nb::arg("viora_path") = "",
          nb::arg("timeout_seconds") = 60,
          R"(Run a SPICE netlist simulation via viora and parse results.

Args:
    netlist_text: SPICE netlist as a string
    analysis: 'op', 'tran', 'ac', or 'dc'
    stop_time: Stop time for transient (e.g. '10m')
    step_time: Step size for transient (e.g. '100u')
    viora_path: Path to viora binary (auto-detected if empty)
    timeout_seconds: Max simulation time

Returns:
    dict with keys:
        ok: bool — whether simulation succeeded
        analysis: str
        waveforms: list[dict] — each with name, x, y, [phase]
        node_voltages: dict[str, float] — for OP analysis
        branch_currents: dict[str, float] — for OP analysis
        error: str — on failure
)");

    // -----------------------------------------------------------------------
    // Callback Bridge — C++ can invoke Python functions
    // -----------------------------------------------------------------------

    // Global callback registry
    static std::map<std::string, nb::callable> g_callbacks;
    static std::mutex g_cb_mutex;

    m.def("register_callback",
          [](const std::string& name, nb::callable callback) {
              std::lock_guard<std::mutex> lock(g_cb_mutex);
              g_callbacks[name] = std::move(callback);
          },
          nb::arg("name"), nb::arg("callback"),
          R"(Register a Python callable by name.

The callback can later be invoked by C++ code (e.g., when a menu item
is clicked or a simulation finishes).

Example:
    vspice.register_callback("on_sim_done", lambda: print("Done!"))
)");

    m.def("unregister_callback",
          [](const std::string& name) {
              std::lock_guard<std::mutex> lock(g_cb_mutex);
              g_callbacks.erase(name);
          },
          nb::arg("name"),
          "Remove a previously registered callback.");

    m.def("invoke_callback",
          [](const std::string& name, nb::tuple args, nb::dict kwargs) -> nb::object {
              std::lock_guard<std::mutex> lock(g_cb_mutex);
              auto it = g_callbacks.find(name);
              if (it == g_callbacks.end()) {
                  throw std::runtime_error("Callback not found: " + name);
              }
              return it->second(*args, **kwargs);
          },
          nb::arg("name"), nb::arg("args") = nb::make_tuple(), nb::arg("kwargs") = nb::dict(),
          R"(Invoke a registered Python callback by name.

This is primarily used internally by C++ code (e.g., when a menu item
triggers). But you can also call it manually.
)");

    m.def("list_callbacks",
          []() -> std::vector<std::string> {
              std::lock_guard<std::mutex> lock(g_cb_mutex);
              std::vector<std::string> names;
              for (const auto& [k, v] : g_callbacks) names.push_back(k);
              return names;
          },
          "Return a list of registered callback names.");

    // -----------------------------------------------------------------------
    // CallbackHandle — a lightweight wrapper that C++ code can store
    // -----------------------------------------------------------------------
    struct CallbackHandle {
        std::string name;
        bool exists() const {
            std::lock_guard<std::mutex> lock(g_cb_mutex);
            return g_callbacks.find(name) != g_callbacks.end();
        }
        nb::object operator()(nb::tuple args, nb::dict kwargs) const {
            std::lock_guard<std::mutex> lock(g_cb_mutex);
            auto it = g_callbacks.find(name);
            if (it == g_callbacks.end()) {
                throw std::runtime_error("Callback not found: " + name);
            }
            return it->second(*args, **kwargs);
        }
    };

    nb::class_<CallbackHandle>(m, "CallbackHandle")
        .def_rw("name", &CallbackHandle::name)
        .def("exists", &CallbackHandle::exists,
             "Check if the callback still exists.")
        .def("invoke",
             [](CallbackHandle& self, nb::tuple args, nb::dict kwargs) {
                 return self(args, kwargs);
             },
             nb::arg("args") = nb::make_tuple(), nb::arg("kwargs") = nb::dict(),
             "Invoke the callback with given arguments.")
        .def("__call__",
             [](CallbackHandle& self, nb::tuple args, nb::dict kwargs) {
                 return self(args, kwargs);
             },
             nb::arg("args") = nb::make_tuple(), nb::arg("kwargs") = nb::dict(),
             "Allow calling the handle directly: handle(arg1, arg2)")
        .def("__repr__",
             [](const CallbackHandle& h) {
                 return "<CallbackHandle '" + h.name + "'>";
             });

    m.def("get_callback_handle",
          [](const std::string& name) -> std::optional<CallbackHandle> {
              std::lock_guard<std::mutex> lock(g_cb_mutex);
              if (g_callbacks.find(name) == g_callbacks.end()) {
                  return std::nullopt;
              }
              return CallbackHandle{name};
          },
          nb::arg("name"),
          "Get a CallbackHandle for a registered callback (or None if not found).");
}
