/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#include "netlist_commands.h"
#include "common.h"
#include "../command_registry.h"

#include <QGraphicsScene>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QDir>
#include <QFile>
#include "python/cpp/core/flux_script_manager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryFile>
#include <QTimer>
#include <QThread>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMainWindow>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <cstdlib>

#include "../simulator/core/raw_data_parser.h"
#include "flux/schematic/io/schematic_file_io.h"
#include "schematic/analysis/spice_netlist_generator.h"
#include "simulation_manager.h"
#include "simulator/bridge/sim_manager.h"
#include "simulator/core/sim_results.h"
#include "simulator/core/sim_value_parser.h"
#include "simulator/bridge/model_library_manager.h"
#include "../ui/waveform_viewer.h"
#include "../schematic/ui/simulation/simulation_panel.h"
#include "schematic/ui/oscilloscope_window.h"
#include "schematic/ui/mini_scope_widget.h"
#include "simulator/bridge/slang_manager.h"
#include "flux/schematic/editor/schematic_api.h"
#include "flux/schematic/factories/schematic_item_registry.h"
#include "symbols/symbol_library.h"
#include "schematic/io/netlist_generator.h"
#include "schematic/items/schematic_item.h"
#include "schematic/io/netlist_to_schematic.h"

#if __has_include("pcb/drc/pcb_drc.h")
#define VIOSPICE_HAS_PCB 1
#include "vioraeda/drc/pcb_drc.h"
#include "vioraeda/factories/pcb_item_registry.h"
#include "vioraeda/io/pcb_file_io.h"
#include "vioraeda/editor/pcb_api.h"
#else
#define VIOSPICE_HAS_PCB 0
#endif

namespace {

QString normalizeNetlistLine(const QString& line) {
    QString s = line;
    const int commentIdx = s.indexOf(';');
    if (commentIdx >= 0) s = s.left(commentIdx);
    s = s.trimmed();
    if (s.isEmpty()) return QString();
    s.replace(QRegularExpression("\\s+"), " ");
    return s.toUpper();
}

QStringList normalizeNetlistText(const QString& text) {
    QStringList lines = text.split('\n');
    QStringList out;
    QString current;
    for (QString line : lines) {
        line.replace('\r', "");
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;
        if (trimmed.startsWith('*') || trimmed.startsWith(';')) continue;
        if (trimmed.startsWith('+')) {
            const QString cont = normalizeNetlistLine(trimmed.mid(1));
            if (!cont.isEmpty() && !current.isEmpty()) {
                current += " " + cont;
            }
            continue;
        }
        if (!current.isEmpty()) {
            out.append(current);
            current.clear();
        }
        current = normalizeNetlistLine(trimmed);
    }
    if (!current.isEmpty()) out.append(current);
    return out;
}

bool resolveBaseSignalIndex(const RawData& data, const QString& name, int* outIndex, QString* error) {
    if (outIndex) *outIndex = -1;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return true;
    int idx = -1;
    for (int i = 0; i < (int)data.varNames.size(); ++i) {
        if (QString::fromStdString(data.varNames[i]).compare(trimmed, Qt::CaseInsensitive) == 0) { idx = i; break; }
    }
    if (idx < 1) {
        if (error) *error = QString("Base signal not found (or invalid): %1").arg(trimmed);
        return false;
    }
    if (outIndex) *outIndex = idx - 1;
    return true;
}

QVector<int> decimatedIndices(const RawData& data, int baseSignalIndex, int maxPoints, double tStart, double tEnd) {
    const int total = data.x.size();
    QVector<int> out;
    if (total <= 0) return out;
    const bool useRange = !(std::isnan(tStart) || std::isnan(tEnd));
    const double rangeStart = useRange ? qMin(tStart, tEnd) : 0.0;
    const double rangeEnd = useRange ? qMax(tStart, tEnd) : 0.0;

    QVector<int> candidates;
    candidates.reserve(total);
    for (int i = 0; i < total; ++i) {
        if (!useRange || (data.x[i] >= rangeStart && data.x[i] <= rangeEnd)) candidates.push_back(i);
    }
    if (candidates.isEmpty()) return out;

    if (maxPoints <= 0 || candidates.size() <= maxPoints) {
        return candidates;
    }

    const int buckets = qMax(1, maxPoints / 2);
    const int bucketSize = qMax(1, (candidates.size() + buckets - 1) / buckets);
    out.reserve(qMin(maxPoints, total));
    const auto& base = data.y[baseSignalIndex];

    for (int start = 0; start < candidates.size(); start += bucketSize) {
        const int end = qMin(candidates.size(), start + bucketSize);
        int minIdx = candidates[start];
        int maxIdx = candidates[start];
        double minVal = base[minIdx];
        double maxVal = base[maxIdx];
        for (int i = start + 1; i < end; ++i) {
            const int idx = candidates[i];
            const double v = base[idx];
            if (v < minVal) { minVal = v; minIdx = idx; }
            if (v > maxVal) { maxVal = v; maxIdx = idx; }
        }
        if (minVal == maxVal) {
            if (out.isEmpty() || out.back() != minIdx) out.push_back(minIdx);
        } else if (minIdx < maxIdx) {
            if (out.isEmpty() || out.back() != minIdx) out.push_back(minIdx);
            if (out.back() != maxIdx) out.push_back(maxIdx);
        } else {
            if (out.isEmpty() || out.back() != maxIdx) out.push_back(maxIdx);
            if (out.back() != minIdx) out.push_back(minIdx);
        }
        if (out.size() >= maxPoints) break;
    }

    while (out.size() > maxPoints) out.pop_back();
    return out;
}

QVector<int> filteredIndices(const RawData& data, double tStart, double tEnd) {
    QVector<int> out;
    const bool useRange = !(std::isnan(tStart) || std::isnan(tEnd));
    const double rangeStart = useRange ? qMin(tStart, tEnd) : 0.0;
    const double rangeEnd = useRange ? qMax(tStart, tEnd) : 0.0;
    out.reserve(data.x.size());
    for (int i = 0; i < data.x.size(); ++i) {
        if (!useRange || (data.x[i] >= rangeStart && data.x[i] <= rangeEnd)) out.push_back(i);
    }
    return out;
}

QJsonObject rawToJson(const RawData& data, const QStringList& signalNames, const QVector<int>& indices, int maxPoints, double tStart, double tEnd, int baseSignalIndex = -1) {
    QJsonObject out;
    QJsonArray xArr;
    int baseSignal = baseSignalIndex;
    if (baseSignal < 0 || baseSignal >= data.y.size()) {
        baseSignal = indices.isEmpty() ? 0 : indices[0];
    }
    const QVector<int> idx = decimatedIndices(data, baseSignal, maxPoints, tStart, tEnd);
    for (int i : idx) xArr.append(data.x[i]);
    out["x"] = xArr;
    QJsonArray sigArr;
    for (int i = 0; i < signalNames.size(); ++i) {
        QJsonObject s;
        s["name"] = signalNames[i];
        QJsonArray vals;
        const auto& vec = data.y[indices[i]];
        for (int k : idx) vals.append(vec[k]);
        s["values"] = vals;
        sigArr.append(s);
    }
    out["signals"] = sigArr;
    return out;
}

QString rawToCsv(const RawData& data, const QStringList& signalNames, const QVector<int>& indices, int maxPoints, double tStart, double tEnd, int baseSignalIndex = -1) {
    QString out;
    QTextStream stream(&out);
    stream << QString::fromStdString(data.varNames[0]);
    for (const auto& sig : signalNames) stream << "," << sig;
    stream << "\n";
    int baseSignal = baseSignalIndex;
    if (baseSignal < 0 || baseSignal >= data.y.size()) {
        baseSignal = indices.isEmpty() ? 0 : indices[0];
    }
    const QVector<int> idx = decimatedIndices(data, baseSignal, maxPoints, tStart, tEnd);
    for (int i : idx) {
        stream << data.x[i];
        for (int j = 0; j < indices.size(); ++j) {
            const auto& vec = data.y[indices[j]];
            if (i < vec.size()) stream << "," << vec[i];
            else stream << ",";
        }
        stream << "\n";
    }
    return out;
}

struct SignalStats {
    QString name;
    double min = 0.0;
    double max = 0.0;
    double avg = 0.0;
    double rms = 0.0;
};

QVector<SignalStats> computeSignalStats(const RawData& data, const QStringList& signalNames, const QVector<int>& indices, const QVector<int>& sampleIndices) {
    QVector<SignalStats> stats;
    stats.reserve(indices.size());
    for (int i = 0; i < indices.size(); ++i) {
        const auto& vec = data.y[indices[i]];
        if (vec.empty()) continue;
        SignalStats s;
        s.name = signalNames[i];
        double sum = 0.0;
        double sumSq = 0.0;
        bool seeded = false;
        double minVal = 0.0;
        double maxVal = 0.0;
        if (sampleIndices.isEmpty()) {
            for (double v : vec) {
                if (!seeded) { minVal = v; maxVal = v; seeded = true; }
                sum += v;
                sumSq += v * v;
                if (v < minVal) minVal = v;
                if (v > maxVal) maxVal = v;
            }
            const int n = vec.size();
            s.min = minVal;
            s.max = maxVal;
            s.avg = sum / n;
            s.rms = std::sqrt(sumSq / n);
        } else {
            for (int idx : sampleIndices) {
                if (idx < 0 || idx >= vec.size()) continue;
                const double v = vec[idx];
                if (!seeded) { minVal = v; maxVal = v; seeded = true; }
                sum += v;
                sumSq += v * v;
                if (v < minVal) minVal = v;
                if (v > maxVal) maxVal = v;
            }
            const int n = sampleIndices.size();
            if (n > 0 && seeded) {
                s.min = minVal;
                s.max = maxVal;
                s.avg = sum / n;
                s.rms = std::sqrt(sumSq / n);
            } else {
                continue;
            }
        }
        stats.push_back(s);
    }
    return stats;
}

enum class MeasureType { Min, Max, Avg, Rms, Pp, At };

struct MeasureRequest {
    QString expr;
    QString signalName;
    MeasureType type;
    double atTime = 0.0;
};

int findVarIndex(const QStringList& vars, const QString& name) {
    for (int i = 0; i < vars.size(); ++i) {
        if (vars[i].compare(name, Qt::CaseInsensitive) == 0) return i;
    }
    for (int i = 0; i < vars.size(); ++i) {
        const QString& v = vars[i];
        if ((v.startsWith("V(", Qt::CaseInsensitive) || v.startsWith("I(", Qt::CaseInsensitive))
            && v.endsWith(")")) {
            const QString inner = v.mid(2, v.size() - 3);
            if (inner.compare(name, Qt::CaseInsensitive) == 0) return i;
        }
    }
    return -1;
}

bool parseMeasure(const QString& expr, MeasureRequest* out, QString* error) {
    if (!out) return false;
    QString s = expr.trimmed();
    if (s.isEmpty()) {
        if (error) *error = "Empty measure expression.";
        return false;
    }
    MeasureRequest req;
    req.expr = s;

    int atPos = s.indexOf("@t=", Qt::CaseInsensitive);
    if (atPos >= 0) {
        const QString namePart = s.left(atPos).trimmed();
        const QString timePart = s.mid(atPos + 3).trimmed();
        double t = 0.0;
        if (!SimValueParser::parseSpiceNumber(timePart, t)) {
            if (error) *error = "Invalid time in measure: " + expr;
            return false;
        }
        req.type = MeasureType::At;
        req.atTime = t;
        s = namePart;
    } else if (s.endsWith("_min", Qt::CaseInsensitive)) {
        req.type = MeasureType::Min;
        s.chop(4);
    } else if (s.endsWith("_max", Qt::CaseInsensitive)) {
        req.type = MeasureType::Max;
        s.chop(4);
    } else if (s.endsWith("_avg", Qt::CaseInsensitive)) {
        req.type = MeasureType::Avg;
        s.chop(4);
    } else if (s.endsWith("_rms", Qt::CaseInsensitive)) {
        req.type = MeasureType::Rms;
        s.chop(4);
    } else if (s.endsWith("_pp", Qt::CaseInsensitive)) {
        req.type = MeasureType::Pp;
        s.chop(3);
    } else {
        req.type = MeasureType::Avg;
    }

    s = s.trimmed();
    req.signalName = s;
    if (req.signalName.isEmpty()) {
        if (error) *error = "Invalid measure signal: " + expr;
        return false;
    }
    *out = req;
    return true;
}

int nearestIndex(const QVector<double>& xs, double t) {
    if (xs.isEmpty()) return -1;
    int lo = 0;
    int hi = xs.size() - 1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (xs[mid] < t) lo = mid + 1;
        else hi = mid;
    }
    if (lo == 0) return 0;
    if (lo >= xs.size()) return xs.size() - 1;
    const double d1 = std::abs(xs[lo] - t);
    const double d0 = std::abs(xs[lo - 1] - t);
    return (d0 <= d1) ? (lo - 1) : lo;
}

class NetlistCompareCommand : public CLICommand {
public:
    QString name() const override { return "netlist-compare"; }
    QString description() const override { return "Compare schematic generated netlist against an external netlist."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption({"a", "analysis"}, "Analysis type (op, tran, ac)", "type", "op"));
        parser.addOption(QCommandLineOption({"s", "step"}, "Step size for transient", "time", "100u"));
        parser.addOption(QCommandLineOption({"t", "stop"}, "Stop time for transient", "time", "10m"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.flxsch", "external.net"}}, {"options", QJsonObject{{"analysis", "op|tran|ac"}, {"step", "string"}, {"stop", "string"}}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"schematic", "string"}, {"external", "string"}, {"schematicLineCount", "int"}, {"externalLineCount", "int"}, {"differences", "array[{line,schematicCount,externalCount}]"}, {"summary", "object"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.size() < 2) {
            std::cerr << "Usage: viora netlist-compare <file.flxsch> <external.net> [options]" << std::endl;
            return 1;
        }
        const QString schematicPath = args.at(0);
        const QString externalPath = args.at(1);

        if (!QFileInfo::exists(schematicPath)) {
            std::cerr << "Error: Schematic not found: " << schematicPath.toStdString() << std::endl;
            return 1;
        }
        if (!QFileInfo::exists(externalPath)) {
            std::cerr << "Error: Netlist not found: " << externalPath.toStdString() << std::endl;
            return 1;
        }

        QGraphicsScene scene;
        QString pageSize;
        TitleBlockData dummyTB;
        if (!SchematicFileIO::loadSchematic(&scene, schematicPath, pageSize, dummyTB)) {
            std::cerr << "Error loading schematic: " << SchematicFileIO::lastError().toStdString() << std::endl;
            return 1;
        }

        SpiceNetlistGenerator::SimulationParams params;
        QString analysisType = parser.value("analysis").toLower();
        if (analysisType == "tran") {
            params.type = SpiceNetlistGenerator::Transient;
            params.step = parser.value("step").isEmpty() ? "1e-6" : parser.value("step");
            params.stop = parser.value("stop").isEmpty() ? "1e-2" : parser.value("stop");
        } else if (analysisType == "ac") {
            params.type = SpiceNetlistGenerator::AC;
            params.start = "10";
            params.stop = "1e6";
        } else {
            params.type = SpiceNetlistGenerator::OP;
        }

        auto result = SpiceNetlistGenerator::generate(&scene, QFileInfo(schematicPath).absolutePath(), nullptr, params);

        QFile extFile(externalPath);
        if (!extFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            std::cerr << "Error: Cannot read netlist file: " << externalPath.toStdString() << std::endl;
            return 1;
        }
        const QString externalNetlist = QString::fromUtf8(extFile.readAll());
        extFile.close();

        const QStringList schLines = normalizeNetlistText(result.netlist);
        const QStringList extLines = normalizeNetlistText(externalNetlist);

        QMap<QString, int> schCounts;
        for (const QString& l : schLines) schCounts[l]++;
        QMap<QString, int> extCounts;
        for (const QString& l : extLines) extCounts[l]++;

        QSet<QString> allKeys = QSet<QString>(schCounts.keyBegin(), schCounts.keyEnd());
        allKeys.unite(QSet<QString>(extCounts.keyBegin(), extCounts.keyEnd()));

        QJsonArray diffs;
        int onlySch = 0;
        int onlyExt = 0;
        int common = 0;

        for (const QString& key : allKeys) {
            const int a = schCounts.value(key);
            const int b = extCounts.value(key);
            if (a == b && a > 0) {
                common += a;
                continue;
            }
            if (a > b) onlySch += (a - b);
            if (b > a) onlyExt += (b - a);
            QJsonObject d;
            d["line"] = key;
            d["schematicCount"] = a;
            d["externalCount"] = b;
            diffs.append(d);
        }

        QJsonObject out;
        out["schematic"] = schematicPath;
        out["external"] = externalPath;
        out["schematicLineCount"] = schLines.size();
        out["externalLineCount"] = extLines.size();
        out["differences"] = diffs;
        QJsonObject summary;
        summary["common"] = common;
        summary["onlyInSchematic"] = onlySch;
        summary["onlyInExternal"] = onlyExt;
        summary["differentLineCount"] = diffs.size();
        out["summary"] = summary;
        printJsonValue(out);
        return 0;
    }
};

class NetlistRunCommand : public CLICommand {
public:
    QString name() const override { return "netlist-run"; }
    QString description() const override { return "Run simulation on standard SPICE netlist or schematic."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption("timeout", "Netlist run timeout (e.g. 60s, 5000ms)", "time", "60s"));
        parser.addOption(QCommandLineOption("compat", "Apply backward compatibility transformations (for .cir files)"));
        parser.addOption(QCommandLineOption("robust", "Add robust simulation options to solver"));
        parser.addOption(QCommandLineOption("export-raw", "Export raw data after netlist-run (csv|json)", "rawformat"));
        parser.addOption(QCommandLineOption("stats", "Compute statistical summary of signals"));
        parser.addOption(QCommandLineOption("measure", "Evaluate measurement expressions on waveforms", "expr"));
        parser.addOption(QCommandLineOption("assert", "Evaluate pass/fail assertions", "expr"));
        parser.addOption(QCommandLineOption("measure-format", "Output format for measurements (text|json)", "format", ""));
        parser.addOption(QCommandLineOption("range", "Time range t0:t1 for stats/measurements/export", "range"));
        parser.addOption(QCommandLineOption("signal", "Signal to export (repeatable, raw-export)", "signame"));
        parser.addOption(QCommandLineOption("max-points", "Limit exported samples (raw-export, netlist-run --export-raw)", "pointcount"));
        parser.addOption(QCommandLineOption("base-signal", "Base signal for decimation align", "signame"));
        parser.addOption(QCommandLineOption({"a", "analysis"}, "Analysis type (op, tran, ac)", "type", "op"));
        parser.addOption(QCommandLineOption({"s", "step"}, "Step size for transient", "time", "100u"));
        parser.addOption(QCommandLineOption({"t", "stop"}, "Stop time for transient", "time", "10m"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.cir|file.flxsch"}}, {"options", QJsonObject{{"json", "bool"}, {"timeout", "string"}, {"analysis", "op|tran|ac"}, {"step", "string"}, {"stop", "string"}, {"export-raw", "csv|json"}, {"signal", "name (repeatable)"}, {"max-points", "int"}, {"base-signal", "name"}, {"stats", "bool"}, {"range", "t0:t1"}, {"measure", "expr (repeatable)"}, {"assert", "expr (repeatable)"}}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"file", "string"}, {"ok", "bool"}, {"timeout", "bool"}, {"error", "string"}, {"log", "array[string]"}, {"rawPath", "string"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora netlist-run <file.cir|file.flxsch> [options]" << std::endl;
            return 1;
        }
        QString filePath = args.at(0);
        if (!QFileInfo::exists(filePath)) {
            std::cerr << "Error: Netlist not found: " << filePath.toStdString() << std::endl;
            return 1;
        }

        auto& sim = SimulationManager::instance();
        if (!sim.isAvailable()) {
            std::cerr << "Error: Ngspice not available in this build." << std::endl;
            return 1;
        }

        QString timeoutError;
        const auto timeoutMsOpt = parseTimeoutMs(parser.value("timeout"), &timeoutError);
        if (!timeoutMsOpt.has_value()) {
            std::cerr << "Error: " << timeoutError.toStdString() << std::endl;
            return 1;
        }
        const int timeoutMs = timeoutMsOpt.value();

        QString runPath = filePath;
        std::unique_ptr<QTemporaryFile> tempNetlist;
        QGraphicsScene scene;

        const QString suffix = QFileInfo(filePath).suffix().toLower();
        const bool applyCompat = parser.isSet("compat");
        
        sim.initialize();
        
        QObject::connect(&SimManager::instance(), &SimManager::logMessage, &sim, [&](const QString& msg) {
            if (!g_quiet) std::cout << "[Simulator] " << msg.toStdString() << std::endl;
        });
        QObject::connect(&SimManager::instance(), &SimManager::errorOccurred, &sim, [&](const QString& msg) {
            std::cerr << "[Simulator Error] " << msg.toStdString() << std::endl;
        });
        
        if (suffix == "flxsch") {
            QString pageSize;
            TitleBlockData dummyTB;
            if (!SchematicFileIO::loadSchematic(&scene, filePath, pageSize, dummyTB)) {
                std::cerr << "Error loading schematic: " << SchematicFileIO::lastError().toStdString() << std::endl;
                return 1;
            }

            // Standalone-sheet guard: top-level hierarchical ports dangle
            // without a parent sheet, so refuse instead of running a doomed
            // deck that dies with a cryptic singular-matrix error.
            {
                QStringList topPorts;
                for (QGraphicsItem* gi : scene.items()) {
                    if (auto* si = dynamic_cast<SchematicItem*>(gi)) {
                        if (si->itemType() == SchematicItem::HierarchicalPortType) {
                            QString n = si->value().trimmed();
                            if (n.isEmpty()) n = si->reference().trimmed();
                            if (!n.isEmpty()) topPorts.append(n);
                        }
                    }
                }
                if (!topPorts.isEmpty()) {
                    std::cerr << "Error: '" << filePath.toStdString() << "' contains hierarchical port(s) ("
                              << topPorts.join(", ").toStdString()
                              << ") and cannot be simulated standalone. Open the parent schematic that instantiates this sheet and run there." << std::endl;
                    return 1;
                }
            }

            SpiceNetlistGenerator::SimulationParams params;
            QString analysisType = parser.value("analysis").toLower();
            if (analysisType == "tran") {
                params.type = SpiceNetlistGenerator::Transient;
                params.step = parser.value("step").isEmpty() ? "1e-6" : parser.value("step");
                params.stop = parser.value("stop").isEmpty() ? "1e-2" : parser.value("stop");
            } else if (analysisType == "ac") {
                params.type = SpiceNetlistGenerator::AC;
                params.start = "10";
                params.stop = "1e6";
            } else {
                params.type = SpiceNetlistGenerator::OP;
            }

            const auto result = SpiceNetlistGenerator::generate(&scene, QFileInfo(filePath).absolutePath(), nullptr, params);

            if (!g_quiet) std::cout << "FluxScript: Found " << result.componentPins.size() << " component pin mappings." << std::endl;
            for (auto it = result.componentPins.begin(); it != result.componentPins.end(); ++it) {
                if (!g_quiet) std::cout << "  " << it.key().toStdString() << ": " << it.value().size() << " pins." << std::endl;
            }

            SimManager::instance().m_pinToNetMap = result.componentPins;
            SimManager::instance().compileFluxScripts(&scene);

            QString netlistText = result.netlist;
            if (parser.isSet("robust")) {
                netlistText += "\n.options gmin=1e-9 abstol=1e-10 reltol=1e-3 chgtol=1e-13 method=gear\n";
                netlistText += ".options rshunt=10Meg\n";
            }

            const QString baseName = QFileInfo(filePath).completeBaseName();
            const QString tempPattern = QDir::tempPath() + "/viospice_netlist_" + baseName + "_XXXXXX.cir";
            tempNetlist = std::make_unique<QTemporaryFile>(tempPattern);
            if (!tempNetlist->open()) {
                std::cerr << "Error: Failed to create temporary netlist." << std::endl;
                return 1;
            }
            tempNetlist->write(netlistText.toUtf8());
            tempNetlist->flush();
            runPath = tempNetlist->fileName();
        } else if ((applyCompat || parser.isSet("robust")) && suffix == "cir") {
            QFile inFile(filePath);
            if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                std::cerr << "Error: Cannot read netlist file: " << filePath.toStdString() << std::endl;
                return 1;
            }
            QString rawNetlist = QString::fromUtf8(inFile.readAll());
            inFile.close();

            QString finalNetlist = applyCompat ? SpiceNetlistGenerator::generateCompatibilityLayer(rawNetlist) : rawNetlist;
            if (parser.isSet("robust")) {
                finalNetlist += "\n.options gmin=1e-9 abstol=1e-10 reltol=1e-3 chgtol=1e-13 method=gear\n";
                finalNetlist += ".options rshunt=10Meg\n";
            }

            const QString baseName = QFileInfo(filePath).completeBaseName();
            const QString tempPattern = QDir::tempPath() + "/viospice_run_" + baseName + "_XXXXXX.cir";
            tempNetlist = std::make_unique<QTemporaryFile>(tempPattern);
            if (!tempNetlist->open()) {
                std::cerr << "Error: Failed to create temporary netlist." << std::endl;
                return 1;
            }
            tempNetlist->write(finalNetlist.toUtf8());
            tempNetlist->flush();
            runPath = tempNetlist->fileName();
        }

        const bool exportRaw = parser.isSet("export-raw");
        const bool exportStats = parser.isSet("stats");
        const QStringList measureExprs = parser.values("measure");
        const QStringList assertExprs = parser.values("assert");
        const bool exportMeasures = !measureExprs.isEmpty();
        const bool runAssertions = !assertExprs.isEmpty();
        const bool exportRequested = exportRaw || exportStats || exportMeasures || runAssertions;
        const QString measureFormat = parser.value("measure-format").trimmed().toLower();
        const bool measureFormatJson = (measureFormat == "json");
        if (!measureFormat.isEmpty() && measureFormat != "text" && measureFormat != "json") {
            std::cerr << "Error: Invalid --measure-format. Use text or json." << std::endl;
            return 1;
        }

        QStringList outputs;
        QStringList warnings;
        QString errorMsg;
        bool finished = false;

        QObject::connect(&sim, &SimulationManager::outputReceived, &sim, [&](const QString& msg) {
            const QString trimmed = msg.trimmed();
            outputs << trimmed;
            if (isWarningLine(trimmed)) warnings << trimmed;
        });
        QObject::connect(&sim, &SimulationManager::errorOccurred, &sim, [&](const QString& msg) {
            errorMsg = msg;
        });
        QObject::connect(&sim, qOverload<>(&SimulationManager::simulationFinished), &sim, [&]() {
            finished = true;
        });

        QObject::connect(&SimManager::instance(), &SimManager::logMessage, &sim, [&](const QString& msg) {
            if (!g_quiet) std::cout << "[Simulator] " << msg.toStdString() << std::endl;
        });
        QObject::connect(&SimManager::instance(), &SimManager::errorOccurred, &sim, [&](const QString& msg) {
            std::cerr << "[Simulator Error] " << msg.toStdString() << std::endl;
        });

        {
            const bool jsonOut = parser.isSet("json");
            const bool restoreFd = jsonOut || exportRequested;
            const bool silenceFd = g_quiet || jsonOut;
            ScopedFdSilence silence(silenceFd, restoreFd);
            
            SimControl control;
            sim.runSimulation(runPath, &control);

            QEventLoop loop;
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&sim, qOverload<>(&SimulationManager::simulationFinished), &loop, &QEventLoop::quit);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            if (timeoutMs > 0) {
                timer.start(timeoutMs);
            }
            loop.exec();
        }

        const bool timedOut = !finished;
        QThread::msleep(100);
        
        const QFileInfo info(runPath);
        const QString rawPath = info.absolutePath() + "/" + info.completeBaseName() + ".raw";
        QString exportRawFormat = parser.value("export-raw").trimmed().toLower();
        if (exportRaw && exportRawFormat.isEmpty()) exportRawFormat = "json";
        bool maxOk = false;
        const int maxPoints = parser.value("max-points").toInt(&maxOk);
        const int maxPointsValue = maxOk ? maxPoints : 0;
        double tStart = std::numeric_limits<double>::quiet_NaN();
        double tEnd = std::numeric_limits<double>::quiet_NaN();
        QString rangeError;
        if (!parseRangeOption(parser.value("range"), &tStart, &tEnd, &rangeError)) {
            std::cerr << "Error: " << rangeError.toStdString() << std::endl;
            return 1;
        }

        const bool hasWarnings = !warnings.isEmpty();
        const bool okResult = finished && errorMsg.isEmpty() && !timedOut;
        const bool okForExit = okResult && !(g_exitOnWarning && hasWarnings);

        if (parser.isSet("json")) {
            QJsonObject out;
            out["file"] = runPath;
            out["ok"] = okForExit;
            out["timeout"] = timedOut;
            if (!errorMsg.isEmpty()) out["error"] = errorMsg;
            QJsonArray log;
            for (const QString& line : outputs) log.append(line);
            out["log"] = log;
            out["rawPath"] = rawPath;
            if (hasWarnings) {
                QJsonArray warnArr;
                for (const QString& w : warnings) warnArr.append(w);
                out["warnings"] = warnArr;
            }
            out["hasWarnings"] = hasWarnings;

            if (exportRequested && okResult) {
                RawData data;
                if (RawDataParser::loadRawAscii(rawPath.toStdString(), &data)) {
                    QStringList signalNames = parser.values("signal");
                    if (signalNames.isEmpty()) {
                        for (int i = 1; i < (int)data.varNames.size(); ++i) {
                            signalNames << QString::fromStdString(data.varNames[i]);
                        }
                    }
                    QVector<int> indices;
                    bool ok = true;
                    for (const auto& sig : signalNames) {
                        int idx = -1;
                        for (int j = 0; j < (int)data.varNames.size(); ++j) {
                            if (QString::fromStdString(data.varNames[j]) == sig) {
                                idx = j;
                                break;
                            }
                        }
                        if (idx < 1) { ok = false; break; }
                        indices << (idx - 1);
                    }
                    const QVector<int> rangeIndices = filteredIndices(data, tStart, tEnd);
                    int baseSignalIndex = -1;
                    QString baseError;
                    if (!resolveBaseSignalIndex(data, parser.value("base-signal"), &baseSignalIndex, &baseError)) {
                        out["error"] = baseError;
                        printJsonValue(out);
                        commandExit(1);
                    }
                    if (ok && exportRaw) out["raw"] = rawToJson(data, signalNames, indices, maxPointsValue, tStart, tEnd, baseSignalIndex);
                    if (ok && exportStats) {
                        const auto stats = computeSignalStats(data, signalNames, indices, rangeIndices);
                        QJsonArray statArr;
                        for (const auto& s : stats) {
                            QJsonObject st;
                            st["name"] = s.name;
                            st["min"] = s.min;
                            st["max"] = s.max;
                            st["avg"] = s.avg;
                            st["rms"] = s.rms;
                            statArr.append(st);
                        }
                        out["stats"] = statArr;
                    }
                    if (exportMeasures) {
                        QJsonArray measArr;
                        for (const auto& expr : measureExprs) {
                            MeasureRequest req;
                            QString perr;
                            QJsonObject m;
                            m["expr"] = expr;
                            if (!parseMeasure(expr, &req, &perr)) {
                                m["error"] = perr;
                                measArr.append(m);
                                continue;
                            }
                            QStringList varNames;
                            for (const auto& vn : data.varNames) varNames << QString::fromStdString(vn);
                            const int varIndex = findVarIndex(varNames, req.signalName);
                            if (varIndex < 0) {
                                m["error"] = "Signal not found";
                                measArr.append(m);
                                continue;
                            }
                            if (req.type == MeasureType::At) {
                                QVector<double> xData = QVector<double>(data.x.begin(), data.x.end());
                                const int idx = nearestIndex(xData, req.atTime);
                                if (idx < 0) {
                                    m["error"] = "No samples";
                                } else if (varIndex == 0) {
                                    m["value"] = data.x[idx];
                                } else {
                                    m["value"] = data.y[varIndex - 1][idx];
                                }
                            } else {
                                const QVector<int>& samples = rangeIndices;
                                if (samples.isEmpty()) {
                                    m["error"] = "No samples in range";
                                } else {
                                    const auto& vec = (varIndex == 0) ? data.x : data.y[varIndex - 1];
                                    double minVal = vec[samples[0]];
                                    double maxVal = vec[samples[0]];
                                    double sum = 0.0;
                                    double sumSq = 0.0;
                                    for (int idx : samples) {
                                        const double v = vec[idx];
                                        if (v < minVal) minVal = v;
                                        if (v > maxVal) maxVal = v;
                                        sum += v;
                                        sumSq += v * v;
                                    }
                                    if (req.type == MeasureType::Min) m["value"] = minVal;
                                    else if (req.type == MeasureType::Max) m["value"] = maxVal;
                                    else if (req.type == MeasureType::Pp) m["value"] = (maxVal - minVal);
                                    else if (req.type == MeasureType::Rms) m["value"] = std::sqrt(sumSq / samples.size());
                                    else m["value"] = (sum / samples.size());
                                }
                            }
                            measArr.append(m);
                        }
                        out["measures"] = measArr;
                    }
                    
                    bool allAssertsPassed = true;
                    if (runAssertions) {
                        QJsonArray assertArr;
                        for (const auto& expr : assertExprs) {
                            QJsonObject a;
                            a["expr"] = expr;
                            
                            QRegularExpression re(R"(^(.+?)\s*(==|!=|>=|<=|>|<)\s*(.+)$)");
                            auto match = re.match(expr);
                            if (!match.hasMatch()) {
                                a["error"] = "Invalid assertion format. Use 'signal op value'";
                                a["ok"] = false;
                                allAssertsPassed = false;
                                assertArr.append(a);
                                continue;
                            }
                            
                            QString leftExpr = match.captured(1).trimmed();
                            QString op = match.captured(2);
                            QString rightValStr = match.captured(3).trimmed();
                            
                            double rightVal = 0.0;
                            if (!SimValueParser::parseSpiceNumber(rightValStr, rightVal)) {
                                a["error"] = "Invalid value: " + rightValStr;
                                a["ok"] = false;
                                allAssertsPassed = false;
                                assertArr.append(a);
                                continue;
                            }
                            
                            MeasureRequest req;
                            QString perr;
                            if (!parseMeasure(leftExpr, &req, &perr)) {
                                a["error"] = perr;
                                a["ok"] = false;
                                allAssertsPassed = false;
                                assertArr.append(a);
                                continue;
                            }
                            
                            QStringList varNames;
                            for (const auto& vn : data.varNames) varNames << QString::fromStdString(vn);
                            const int varIndex = findVarIndex(varNames, req.signalName);
                            if (varIndex < 0) {
                                a["error"] = "Signal not found: " + req.signalName;
                                a["ok"] = false;
                                allAssertsPassed = false;
                                assertArr.append(a);
                                continue;
                            }
                            
                            double actualVal = 0.0;
                            bool valOk = false;
                            if (req.type == MeasureType::At) {
                                QVector<double> xData = QVector<double>(data.x.begin(), data.x.end());
                                const int idx = nearestIndex(xData, req.atTime);
                                if (idx >= 0) {
                                    actualVal = (varIndex == 0) ? data.x[idx] : data.y[varIndex - 1][idx];
                                    valOk = true;
                                }
                            } else {
                                const QVector<int>& samples = rangeIndices;
                                if (!samples.isEmpty()) {
                                    const auto& vec = (varIndex == 0) ? data.x : data.y[varIndex - 1];
                                    double minV = vec[samples[0]], maxV = vec[samples[0]], sumV = 0.0, sumSqV = 0.0;
                                    for (int idx : samples) {
                                        const double v = vec[idx];
                                        if (v < minV) minV = v;
                                        if (v > maxV) maxV = v;
                                        sumV += v;
                                        sumSqV += v * v;
                                    }
                                    if (req.type == MeasureType::Min) actualVal = minV;
                                    else if (req.type == MeasureType::Max) actualVal = maxV;
                                    else if (req.type == MeasureType::Pp) actualVal = (maxV - minV);
                                    else if (req.type == MeasureType::Rms) actualVal = std::sqrt(sumSqV / samples.size());
                                    else actualVal = (sumV / samples.size());
                                    valOk = true;
                                }
                            }
                            
                            if (!valOk) {
                                a["error"] = "No data for measurement";
                                a["ok"] = false;
                                allAssertsPassed = false;
                            } else {
                                a["value"] = actualVal;
                                bool result = false;
                                if (op == "==") result = (std::abs(actualVal - rightVal) < 1e-12);
                                else if (op == "!=") result = (std::abs(actualVal - rightVal) >= 1e-12);
                                else if (op == ">") result = (actualVal > rightVal);
                                else if (op == "<") result = (actualVal < rightVal);
                                else if (op == ">=") result = (actualVal >= rightVal);
                                else if (op == "<=") result = (actualVal <= rightVal);
                                a["ok"] = result;
                                if (!result) allAssertsPassed = false;
                            }
                            assertArr.append(a);
                        }
                        out["assertions"] = assertArr;
                        out["ok"] = out["ok"].toBool() && allAssertsPassed;
                    }
                }
            }
            printJsonValue(out);
            commandExit(okForExit ? 0 : 1);
        }

        if (!g_quiet) {
            for (const QString& line : outputs) {
                if (!line.isEmpty()) std::cout << line.toStdString() << std::endl;
            }
        }

        bool allAssertsPassed = true;
        if (okResult && runAssertions) {
            RawData data;
            if (RawDataParser::loadRawAscii(rawPath.toStdString(), &data)) {
                const QVector<int> rangeIndices = filteredIndices(data, tStart, tEnd);
                for (const auto& expr : assertExprs) {
                    QRegularExpression re(R"(^(.+?)\s*(==|!=|>=|<=|>|<)\s*(.+)$)");
                    auto match = re.match(expr);
                    if (!match.hasMatch()) {
                        std::cerr << "Assertion Error: Invalid format: " << expr.toStdString() << std::endl;
                        allAssertsPassed = false;
                        continue;
                    }
                    
                    QString leftExpr = match.captured(1).trimmed();
                    QString op = match.captured(2);
                    QString rightValStr = match.captured(3).trimmed();
                    double rightVal = 0.0;
                    SimValueParser::parseSpiceNumber(rightValStr, rightVal);
                    
                    MeasureRequest req;
                    QString perr;
                    if (!parseMeasure(leftExpr, &req, &perr)) {
                        std::cerr << "Assertion Error: " << perr.toStdString() << " in " << expr.toStdString() << std::endl;
                        allAssertsPassed = false;
                        continue;
                    }
                    
                    QStringList varNames;
                    for (const auto& vn : data.varNames) varNames << QString::fromStdString(vn);
                    const int varIndex = findVarIndex(varNames, req.signalName);
                    if (varIndex < 0) {
                        std::cerr << "Assertion Error: Signal not found: " << req.signalName.toStdString() << std::endl;
                        allAssertsPassed = false;
                        continue;
                    }
                    
                    double actualVal = 0.0;
                    bool valOk = false;
                    if (req.type == MeasureType::At) {
                        QVector<double> xData = QVector<double>(data.x.begin(), data.x.end());
                        const int idx = nearestIndex(xData, req.atTime);
                        if (idx >= 0) {
                            actualVal = (varIndex == 0) ? data.x[idx] : data.y[varIndex - 1][idx];
                            valOk = true;
                        }
                    } else {
                        const QVector<int>& samples = rangeIndices;
                        if (!samples.isEmpty()) {
                            const auto& vec = (varIndex == 0) ? data.x : data.y[varIndex - 1];
                            double minV = vec[samples[0]], maxV = vec[samples[0]], sumV = 0.0, sumSqV = 0.0;
                            for (int idx : samples) {
                                const double v = vec[idx];
                                if (v < minV) minV = v;
                                if (v > maxV) maxV = v;
                                sumV += v;
                                sumSqV += v * v;
                            }
                            if (req.type == MeasureType::Min) actualVal = minV;
                            else if (req.type == MeasureType::Max) actualVal = maxV;
                            else if (req.type == MeasureType::Pp) actualVal = (maxV - minV);
                            else if (req.type == MeasureType::Rms) actualVal = std::sqrt(sumSqV / samples.size());
                            else actualVal = (sumV / samples.size());
                            valOk = true;
                        }
                    }
                    
                    if (!valOk) {
                        std::cerr << "Assertion Failed: " << expr.toStdString() << " (No data)" << std::endl;
                        allAssertsPassed = false;
                    } else {
                        bool result = false;
                        if (op == "==") result = (std::abs(actualVal - rightVal) < 1e-12);
                        else if (op == "!=") result = (std::abs(actualVal - rightVal) >= 1e-12);
                        else if (op == ">") result = (actualVal > rightVal);
                        else if (op == "<") result = (actualVal < rightVal);
                        else if (op == ">=") result = (actualVal >= rightVal);
                        else if (op == "<=") result = (actualVal <= rightVal);
                        
                        if (!result) {
                            std::cerr << "Assertion Failed: " << expr.toStdString() << " (Actual: " << actualVal << ")" << std::endl;
                            allAssertsPassed = false;
                        } else if (!g_quiet) {
                            std::cout << "Assertion Passed: " << expr.toStdString() << " (Value: " << actualVal << ")" << std::endl;
                        }
                    }
                }
            }
        }

        if (timedOut) {
            std::cerr << "Error: Simulation timed out." << std::endl;
            if (g_quiet && !parser.isSet("json")) commandExit(1);
            return 1;
        }
        if (!errorMsg.isEmpty()) {
            std::cerr << "Error: " << errorMsg.toStdString() << std::endl;
            if (g_quiet && !parser.isSet("json")) commandExit(1);
            return 1;
        }

        if (exportRequested) {
            RawData data;
            if (!RawDataParser::loadRawAscii(rawPath.toStdString(), &data)) {
                std::cerr << "Error: Failed to load raw data file for export." << std::endl;
                return 1;
            }
            QStringList signalNames = parser.values("signal");
            if (signalNames.isEmpty()) {
                for (int i = 1; i < (int)data.varNames.size(); ++i) {
                    signalNames << QString::fromStdString(data.varNames[i]);
                }
            }
            QVector<int> indices;
            for (const auto& sig : signalNames) {
                int idx = -1;
                for (int j = 0; j < (int)data.varNames.size(); ++j) {
                    if (QString::fromStdString(data.varNames[j]) == sig) {
                        idx = j;
                        break;
                    }
                }
                if (idx < 1) {
                    std::cerr << "Error: Signal not found: " << sig.toStdString() << std::endl;
                    return 1;
                }
                indices << (idx - 1);
            }
            const QVector<int> rangeIndices = filteredIndices(data, tStart, tEnd);
            int baseSignalIndex = -1;
            QString baseError;
            if (!resolveBaseSignalIndex(data, parser.value("base-signal"), &baseSignalIndex, &baseError)) {
                std::cerr << "Error: " << baseError.toStdString() << std::endl;
                return 1;
            }

            if (exportMeasures && !measureFormatJson) {
                for (const auto& expr : measureExprs) {
                    MeasureRequest req;
                    QString perr;
                    if (!parseMeasure(expr, &req, &perr)) {
                        std::cerr << expr.toStdString() << " error=" << perr.toStdString() << std::endl;
                        continue;
                    }
                    QStringList varNames;
                    for (const auto& vn : data.varNames) varNames << QString::fromStdString(vn);
                    const int varIndex = findVarIndex(varNames, req.signalName);
                    if (varIndex < 0) {
                        std::cerr << expr.toStdString() << " error=Signal not found" << std::endl;
                        continue;
                    }
                    if (req.type == MeasureType::At) {
                        QVector<double> xData = QVector<double>(data.x.begin(), data.x.end());
                        const int idx = nearestIndex(xData, req.atTime);
                        if (idx < 0) {
                            std::cerr << expr.toStdString() << " error=No samples" << std::endl;
                        } else if (varIndex == 0) {
                            std::cout << expr.toStdString() << " = " << data.x[idx] << std::endl;
                        } else {
                            std::cout << expr.toStdString() << " = " << data.y[varIndex - 1][idx] << std::endl;
                        }
                    } else {
                        if (rangeIndices.isEmpty()) {
                            std::cerr << expr.toStdString() << " error=No samples in range" << std::endl;
                        } else {
                            const auto& vec = (varIndex == 0) ? data.x : data.y[varIndex - 1];
                            double minVal = vec[rangeIndices[0]];
                            double maxVal = vec[rangeIndices[0]];
                            double sum = 0.0;
                            double sumSq = 0.0;
                            for (int idx : rangeIndices) {
                                const double v = vec[idx];
                                if (v < minVal) minVal = v;
                                if (v > maxVal) maxVal = v;
                                sum += v;
                                sumSq += v * v;
                            }
                            double value = 0.0;
                            if (req.type == MeasureType::Min) value = minVal;
                            else if (req.type == MeasureType::Max) value = maxVal;
                            else if (req.type == MeasureType::Pp) value = (maxVal - minVal);
                            else if (req.type == MeasureType::Rms) value = std::sqrt(sumSq / rangeIndices.size());
                            else value = (sum / rangeIndices.size());
                            std::cout << expr.toStdString() << " = " << value << std::endl;
                        }
                    }
                }
            }
            if (exportMeasures && measureFormatJson && exportRaw && exportRawFormat == "csv") {
                QJsonArray measArr;
                for (const auto& expr : measureExprs) {
                    MeasureRequest req;
                    QString perr;
                    QJsonObject m;
                    m["expr"] = expr;
                    if (!parseMeasure(expr, &req, &perr)) {
                        m["error"] = perr;
                        measArr.append(m);
                        continue;
                    }
                    QStringList varNames;
                    for (const auto& vn : data.varNames) varNames << QString::fromStdString(vn);
                    const int varIndex = findVarIndex(varNames, req.signalName);
                    if (varIndex < 0) {
                        m["error"] = "Signal not found";
                        measArr.append(m);
                        continue;
                    }
                    if (req.type == MeasureType::At) {
                        QVector<double> xData = QVector<double>(data.x.begin(), data.x.end());
                        const int idx = nearestIndex(xData, req.atTime);
                        if (idx < 0) m["error"] = "No samples";
                        else if (varIndex == 0) m["value"] = data.x[idx];
                        else m["value"] = data.y[varIndex - 1][idx];
                    } else {
                        if (rangeIndices.isEmpty()) {
                            m["error"] = "No samples in range";
                        } else {
                            const auto& vec = (varIndex == 0) ? data.x : data.y[varIndex - 1];
                            double minVal = vec[rangeIndices[0]];
                            double maxVal = vec[rangeIndices[0]];
                            double sum = 0.0;
                            double sumSq = 0.0;
                            for (int idx : rangeIndices) {
                                const double v = vec[idx];
                                if (v < minVal) minVal = v;
                                if (v > maxVal) maxVal = v;
                                sum += v;
                                sumSq += v * v;
                            }
                            if (req.type == MeasureType::Min) m["value"] = minVal;
                            else if (req.type == MeasureType::Max) m["value"] = maxVal;
                            else if (req.type == MeasureType::Pp) m["value"] = (maxVal - minVal);
                            else if (req.type == MeasureType::Rms) m["value"] = std::sqrt(sumSq / rangeIndices.size());
                            else m["value"] = (sum / rangeIndices.size());
                        }
                    }
                    measArr.append(m);
                }
                QJsonObject out;
                out["measures"] = measArr;
                printJsonValueTo(out, std::cerr);
            }
            if (exportMeasures && measureFormatJson && !exportRaw) {
                QJsonArray measArr;
                for (const auto& expr : measureExprs) {
                    MeasureRequest req;
                    QString perr;
                    QJsonObject m;
                    m["expr"] = expr;
                    if (!parseMeasure(expr, &req, &perr)) {
                        m["error"] = perr;
                        measArr.append(m);
                        continue;
                    }
                    QStringList varNames;
                    for (const auto& vn : data.varNames) varNames << QString::fromStdString(vn);
                    const int varIndex = findVarIndex(varNames, req.signalName);
                    if (varIndex < 0) {
                        m["error"] = "Signal not found";
                        measArr.append(m);
                        continue;
                    }
                    if (req.type == MeasureType::At) {
                        QVector<double> xData = QVector<double>(data.x.begin(), data.x.end());
                        const int idx = nearestIndex(xData, req.atTime);
                        if (idx < 0) m["error"] = "No samples";
                        else if (varIndex == 0) m["value"] = data.x[idx];
                        else m["value"] = data.y[varIndex - 1][idx];
                    } else {
                        if (rangeIndices.isEmpty()) {
                            m["error"] = "No samples in range";
                        } else {
                            const auto& vec = (varIndex == 0) ? data.x : data.y[varIndex - 1];
                            double minVal = vec[rangeIndices[0]];
                            double maxVal = vec[rangeIndices[0]];
                            double sum = 0.0;
                            double sumSq = 0.0;
                            for (int idx : rangeIndices) {
                                const double v = vec[idx];
                                if (v < minVal) minVal = v;
                                if (v > maxVal) maxVal = v;
                                sum += v;
                                sumSq += v * v;
                            }
                            if (req.type == MeasureType::Min) m["value"] = minVal;
                            else if (req.type == MeasureType::Max) m["value"] = maxVal;
                            else if (req.type == MeasureType::Pp) m["value"] = (maxVal - minVal);
                            else if (req.type == MeasureType::Rms) m["value"] = std::sqrt(sumSq / rangeIndices.size());
                            else m["value"] = (sum / rangeIndices.size());
                        }
                    }
                    measArr.append(m);
                }
                QJsonObject out;
                out["measures"] = measArr;
                printJsonValue(out);
            }
            if (exportStats && !exportRaw) {
                const auto stats = computeSignalStats(data, signalNames, indices, rangeIndices);
                for (const auto& s : stats) {
                    std::cout << s.name.toStdString()
                              << " min=" << s.min
                              << " max=" << s.max
                              << " avg=" << s.avg
                              << " rms=" << s.rms
                              << std::endl;
                }
            } else if (exportRaw) {
                if (exportRawFormat == "json") {
                    QJsonObject out = rawToJson(data, signalNames, indices, maxPointsValue, tStart, tEnd, baseSignalIndex);
                    out["file"] = rawPath;
                    if (exportStats) {
                        const auto stats = computeSignalStats(data, signalNames, indices, rangeIndices);
                        QJsonArray statArr;
                        for (const auto& s : stats) {
                            QJsonObject st;
                            st["name"] = s.name;
                            st["min"] = s.min;
                            st["max"] = s.max;
                            st["avg"] = s.avg;
                            st["rms"] = s.rms;
                            statArr.append(st);
                        }
                        out["stats"] = statArr;
                    }
                    if (exportMeasures) {
                        QJsonArray measArr;
                        for (const auto& expr : measureExprs) {
                            MeasureRequest req;
                            QString perr;
                            QJsonObject m;
                            m["expr"] = expr;
                            if (!parseMeasure(expr, &req, &perr)) {
                                m["error"] = perr;
                                measArr.append(m);
                                continue;
                            }
                            QStringList varNames;
                            for (const auto& vn : data.varNames) varNames << QString::fromStdString(vn);
                            const int varIndex = findVarIndex(varNames, req.signalName);
                            if (varIndex < 0) {
                                m["error"] = "Signal not found";
                                measArr.append(m);
                                continue;
                            }
                            if (req.type == MeasureType::At) {
                                QVector<double> xData = QVector<double>(data.x.begin(), data.x.end());
                                const int idx = nearestIndex(xData, req.atTime);
                                if (idx < 0) m["error"] = "No samples";
                                else if (varIndex == 0) m["value"] = data.x[idx];
                                else m["value"] = data.y[varIndex - 1][idx];
                            } else {
                                if (rangeIndices.isEmpty()) {
                                    m["error"] = "No samples in range";
                                } else {
                                    const auto& vec = (varIndex == 0) ? data.x : data.y[varIndex - 1];
                                    double minVal = vec[rangeIndices[0]];
                                    double maxVal = vec[rangeIndices[0]];
                                    double sum = 0.0;
                                    double sumSq = 0.0;
                                    for (int idx : rangeIndices) {
                                        const double v = vec[idx];
                                        if (v < minVal) minVal = v;
                                        if (v > maxVal) maxVal = v;
                                        sum += v;
                                        sumSq += v * v;
                                    }
                                    if (req.type == MeasureType::Min) m["value"] = minVal;
                                    else if (req.type == MeasureType::Max) m["value"] = maxVal;
                                    else if (req.type == MeasureType::Pp) m["value"] = (maxVal - minVal);
                                    else if (req.type == MeasureType::Rms) m["value"] = std::sqrt(sumSq / rangeIndices.size());
                                    else m["value"] = (sum / rangeIndices.size());
                                }
                            }
                            measArr.append(m);
                        }
                        out["measures"] = measArr;
                    }
                    printJsonValue(out);
                } else {
                    std::cout << rawToCsv(data, signalNames, indices, maxPointsValue, tStart, tEnd, baseSignalIndex).toStdString();
                }
            }
            commandExit(okForExit ? 0 : 1);
        }
        if (g_exitOnWarning && hasWarnings) {
            if (!g_quiet) {
                std::cerr << "Warning: ngspice reported warnings during simulation." << std::endl;
            }
            if (g_quiet && !parser.isSet("json") && !exportRequested) commandExit(1);
            return 1;
        }
        if (g_quiet && !parser.isSet("json") && !exportRequested) { commandExit(okResult ? 0 : 1); }
        std::cout.flush();
        std::cerr.flush();
        commandExit(okResult ? 0 : 1);
        return okResult ? 0 : 1;
    }
};

class NetlistValidateCommand : public CLICommand {
public:
    QString name() const override { return "netlist-validate"; }
    QString description() const override { return "Validate SPICE netlist syntax."; }
    void setupParser(QCommandLineParser& parser) override {
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.cir"}}, {"options", QJsonObject{{"json", "bool"}}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"file", "string"}, {"ok", "bool"}, {"error", "string"}, {"log", "array[string]"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora netlist-validate <file.cir> [options]" << std::endl;
            return 1;
        }
        QString filePath = args.at(0);
        if (!QFileInfo::exists(filePath)) {
            std::cerr << "Error: Netlist not found: " << filePath.toStdString() << std::endl;
            return 1;
        }

        auto& sim = SimulationManager::instance();
        if (!sim.isAvailable()) {
            std::cerr << "Error: Ngspice not available in this build." << std::endl;
            return 1;
        }

        QStringList outputs;
        QStringList warnings;
        QString errorMsg;
        QObject::connect(&sim, &SimulationManager::outputReceived, &sim, [&](const QString& msg) {
            const QString trimmed = msg.trimmed();
            outputs << trimmed;
            if (isWarningLine(trimmed)) warnings << trimmed;
        });
        QObject::connect(&sim, &SimulationManager::errorOccurred, &sim, [&](const QString& msg) {
            errorMsg = msg;
        });

        const bool ok = [&]() {
            const bool jsonOut = parser.isSet("json");
            const bool restoreFd = jsonOut;
            const bool silenceFd = g_quiet || jsonOut;
            ScopedFdSilence silence(silenceFd, restoreFd);
            return sim.validateNetlist(filePath, &errorMsg);
        }();
        const bool hasWarnings = !warnings.isEmpty();
        const bool okForExit = ok && errorMsg.isEmpty() && !(g_exitOnWarning && hasWarnings);

        if (parser.isSet("json")) {
            QJsonObject out;
            out["file"] = filePath;
            out["ok"] = okForExit;
            if (!errorMsg.isEmpty()) out["error"] = errorMsg;
            QJsonArray log;
            for (const QString& line : outputs) log.append(line);
            out["log"] = log;
            if (hasWarnings) {
                QJsonArray warnArr;
                for (const QString& w : warnings) warnArr.append(w);
                out["warnings"] = warnArr;
            }
            out["hasWarnings"] = hasWarnings;
            printJsonValue(out);
            return okForExit ? 0 : 1;
        }

        if (!g_quiet) {
            for (const QString& line : outputs) {
                if (!line.isEmpty()) std::cout << line.toStdString() << std::endl;
            }
        }
        if (!errorMsg.isEmpty()) {
            std::cerr << "Error: " << errorMsg.toStdString() << std::endl;
            if (g_quiet && !parser.isSet("json")) commandExit(1);
            return 1;
        }
        if (g_exitOnWarning && hasWarnings) {
            if (!g_quiet) std::cerr << "Warning: ngspice reported warnings during validation." << std::endl;
            if (g_quiet && !parser.isSet("json")) commandExit(1);
            return 1;
        }
        if (ok) std::cout << "Netlist OK" << std::endl;
        if (g_quiet && !parser.isSet("json")) commandExit(okForExit ? 0 : 1);
        return okForExit ? 0 : 1;
    }
};

class NetlistToSchematicCommand : public CLICommand {
public:
    QString name() const override { return "netlist-to-schematic"; }
    QString description() const override { return "Synthesize a schematic (.flxsch) from a SPICE netlist (.cir)."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption("out", "Output schematic path", "file"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.cir"}}, {"options", QJsonObject{{"json", "bool"}, {"out", "string"}, {"quiet", "bool"}}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"file", "string"}, {"output", "string"}, {"components", "int"}, {"airWires", "int"}, {"ok", "bool"}, {"error", "string"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora netlist-to-schematic <file.cir> [options]" << std::endl;
            return 1;
        }
        QString filePath = args.at(0);
        if (!QFileInfo::exists(filePath)) {
            std::cerr << "Error: Netlist not found: " << filePath.toStdString() << std::endl;
            return 1;
        }

        QString outPath = parser.value("out");
        if (outPath.isEmpty()) {
            outPath = QFileInfo(filePath).absolutePath() + "/" + QFileInfo(filePath).baseName() + ".flxsch";
        }

        const auto result = NetlistToSchematic::convert(filePath, outPath);

        if (parser.isSet("json")) {
            QJsonObject out;
            out["file"] = filePath;
            out["output"] = result.outputPath;
            out["ok"] = result.success;
            out["components"] = result.componentCount;
            out["airWires"] = result.airWireCount;
            if (!result.errorMessage.isEmpty()) out["error"] = result.errorMessage;
            printJsonValue(out);
            return result.success ? 0 : 1;
        }

        if (result.success) {
            if (!g_quiet) {
                std::cout << "Converted " << filePath.toStdString() << " -> " << result.outputPath.toStdString() << std::endl;
                std::cout << "  Components: " << result.componentCount << std::endl;
                std::cout << "  Air wires:  " << result.airWireCount << std::endl;
            }
        } else {
            std::cerr << "Error: " << result.errorMessage.toStdString() << std::endl;
            return 1;
        }
        return 0;
    }
};

class RawInfoCommand : public CLICommand {
public:
    QString name() const override { return "raw-info"; }
    QString description() const override { return "Display information about raw simulation data file."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption("summary", "Show summary count of signals"));
        parser.addOption(QCommandLineOption("base-signal", "Base signal for validation check", "signame"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.raw"}}, {"options", QJsonObject{{"json", "bool"}, {"summary", "bool"}}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"file", "string"}, {"variables", "int"}, {"points", "int"}, {"varNames", "array[string]"}, {"voltages", "int"}, {"currents", "int"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora raw-info <file.raw> [options]" << std::endl;
            return 1;
        }
        QString filePath = args.at(0);
        RawData data;
        if (!RawDataParser::loadRawAscii(filePath.toStdString(), &data)) {
            std::cerr << "Error loading raw file: " << filePath.toStdString() << std::endl;
            return 1;
        }

        if (parser.isSet("summary")) {
            int voltageCount = 0;
            int currentCount = 0;
            for (const auto& vn : data.varNames) {
                QString name = QString::fromStdString(vn);
                if (name.compare("time", Qt::CaseInsensitive) == 0) continue;
                if (name.startsWith("i(", Qt::CaseInsensitive)) currentCount++;
                else voltageCount++;
            }
            if (parser.isSet("json")) {
                QJsonObject out;
                out["file"] = filePath;
                out["variables"] = data.numVariables;
                out["points"] = data.numPoints;
                out["voltages"] = voltageCount;
                out["currents"] = currentCount;
                printJsonValue(out);
                return 0;
            }
            std::cout << "File: " << filePath.toStdString() << std::endl;
            std::cout << "Variables: " << data.numVariables << std::endl;
            std::cout << "Points: " << data.numPoints << std::endl;
            std::cout << "Voltages: " << voltageCount << std::endl;
            std::cout << "Currents: " << currentCount << std::endl;
            return 0;
        }

        if (parser.isSet("json")) {
            QJsonObject out;
            out["file"] = filePath;
            out["variables"] = data.numVariables;
            out["points"] = data.numPoints;
            QJsonArray names;
            for (const auto& vn : data.varNames) names.append(QString::fromStdString(vn));
            out["varNames"] = names;
            printJsonValue(out);
            return 0;
        }

        std::cout << "File: " << filePath.toStdString() << std::endl;
        std::cout << "Variables: " << data.numVariables << std::endl;
        std::cout << "Points: " << data.numPoints << std::endl;
        for (const auto& vn : data.varNames) {
            std::cout << "  " << vn << std::endl;
        }
        return 0;
    }
};

class RawExportCommand : public CLICommand {
public:
    QString name() const override { return "raw-export"; }
    QString description() const override { return "Export raw simulation signals to CSV, JSON, or Parquet."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption("signal", "Signal to export (repeatable, raw-export)", "signame"));
        parser.addOption(QCommandLineOption("signal-regex", "Filter signals by regex", "pattern"));
        parser.addOption(QCommandLineOption("format", "Output format (csv|json|parquet)", "format", "csv"));
        parser.addOption(QCommandLineOption("max-points", "Limit exported samples", "pointcount"));
        parser.addOption(QCommandLineOption("base-signal", "Base signal for decimation", "signame"));
        parser.addOption(QCommandLineOption("range", "Time range t0:t1", "range"));
        parser.addOption(QCommandLineOption("out", "Output path for parquet file", "file"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.raw"}}, {"options", QJsonObject{{"signal", "name (repeatable)"}, {"signal-regex", "pattern"}, {"format", "csv|json|parquet"}, {"max-points", "int"}, {"base-signal", "name"}, {"range", "t0:t1"}, {"out", "file (parquet)"}}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"file", "string"}, {"x", "array[number]"}, {"signals", "array[{name,values}]"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora raw-export <file.raw> [options]" << std::endl;
            return 1;
        }
        QString filePath = args.at(0);
        RawData data;
        if (!RawDataParser::loadRawAscii(filePath.toStdString(), &data)) {
            std::cerr << "Error loading raw file: " << filePath.toStdString() << std::endl;
            return 1;
        }

        double tStart = std::numeric_limits<double>::quiet_NaN();
        double tEnd = std::numeric_limits<double>::quiet_NaN();
        QString rangeError;
        if (!parseRangeOption(parser.value("range"), &tStart, &tEnd, &rangeError)) {
            std::cerr << "Error: " << rangeError.toStdString() << std::endl;
            return 1;
        }

        QStringList signalNames = parser.values("signal");
        const QString signalRegex = parser.value("signal-regex").trimmed();
        if (signalNames.isEmpty()) {
            for (int i = 1; i < (int)data.varNames.size(); ++i) {
                signalNames << QString::fromStdString(data.varNames[i]);
            }
        }
        if (!signalRegex.isEmpty()) {
            QRegularExpression re(signalRegex);
            if (!re.isValid()) {
                std::cerr << "Error: Invalid signal-regex: " << signalRegex.toStdString() << std::endl;
                return 1;
            }
            QStringList filtered;
            for (const auto& name : signalNames) {
                if (re.match(name).hasMatch()) filtered << name;
            }
            signalNames = filtered;
        }
        if (signalNames.isEmpty()) {
            std::cerr << "Error: No signals matched." << std::endl;
            return 1;
        }

        QVector<int> indices;
        for (const auto& sig : signalNames) {
            int idx = -1;
            for (int i = 0; i < (int)data.varNames.size(); ++i) {
                if (QString::fromStdString(data.varNames[i]) == sig) {
                    idx = i;
                    break;
                }
            }
            if (idx < 1) {
                std::cerr << "Error: Signal not found: " << sig.toStdString() << std::endl;
                return 1;
            }
            indices << (idx - 1);
        }
        int baseSignalIndex = -1;
        QString baseError;
        if (!resolveBaseSignalIndex(data, parser.value("base-signal"), &baseSignalIndex, &baseError)) {
            std::cerr << "Error: " << baseError.toStdString() << std::endl;
            return 1;
        }

        const QString format = parser.value("format").trimmed().toLower();
        bool ok = false;
        const int maxPoints = parser.value("max-points").toInt(&ok);
        const int maxPointsValue = ok ? maxPoints : 0;
        if (format == "parquet") {
            const QString outPath = parser.value("out").trimmed();
            if (outPath.isEmpty()) {
                std::cerr << "Error: --out is required for parquet export." << std::endl;
                return 1;
            }
            const QString csvData = rawToCsv(data, signalNames, indices, maxPointsValue, tStart, tEnd, baseSignalIndex);
            QTemporaryFile temp(QDir::tempPath() + "/viospice_raw_XXXXXX.csv");
            if (!temp.open()) {
                std::cerr << "Error: Failed to create temp CSV for parquet export." << std::endl;
                return 1;
            }
            temp.write(csvData.toUtf8());
            temp.flush();

            QProcess proc;
            QStringList procArgs;
            const QString script = QStringLiteral(
                "import sys\n"
                "try:\n"
                "    import pyarrow.csv as csv\n"
                "    import pyarrow.parquet as pq\n"
                "except Exception as e:\n"
                "    sys.stderr.write('pyarrow import failed: %s\\n' % e)\n"
                "    sys.exit(2)\n"
                "table = csv.read_csv(sys.argv[1])\n"
                "pq.write_table(table, sys.argv[2])\n"
            );
            procArgs << "-c" << script << temp.fileName() << outPath;
            
            QString pythonExe = FluxScriptManager::getPythonExecutable();
            proc.start(pythonExe, procArgs);
            if (!proc.waitForFinished(60000)) {
                std::cerr << "Error: parquet export timed out." << std::endl;
                return 1;
            }
            if (proc.exitCode() != 0) {
                const QByteArray err = proc.readAllStandardError();
                std::cerr << "Error: parquet export failed. " << err.toStdString()
                          << "Hint: install pyarrow in a venv: " << pythonExe.toStdString() << " -m venv .venv && . .venv/bin/activate && pip install pyarrow"
                          << std::endl;
                return 1;
            }
            if (parser.isSet("json")) {
                QJsonObject out;
                out["file"] = outPath;
                out["format"] = "parquet";
                printJsonValue(out);
            } else {
                std::cout << outPath.toStdString() << std::endl;
            }
            return 0;
        }
        const QString outPath = parser.value("out").trimmed();
        if (format == "json") {
            QJsonObject out = rawToJson(data, signalNames, indices, maxPointsValue, tStart, tEnd, baseSignalIndex);
            out["file"] = filePath;
            if (!outPath.isEmpty()) {
                QFile f(outPath);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    std::cerr << "Error: Failed to open output file: " << outPath.toStdString() << std::endl;
                    return 1;
                }
                f.write(QJsonDocument(out).toJson(QJsonDocument::Indented));
            } else {
                printJsonValue(out);
            }
            return 0;
        }

        const QString csvStr = rawToCsv(data, signalNames, indices, maxPointsValue, tStart, tEnd, baseSignalIndex);
        if (!outPath.isEmpty()) {
            QFile f(outPath);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                std::cerr << "Error: Failed to open output file: " << outPath.toStdString() << std::endl;
                return 1;
            }
            f.write(csvStr.toUtf8());
        } else {
            std::cout << csvStr.toStdString();
        }
        return 0;
    }
};

class RawStatsCommand : public CLICommand {
public:
    QString name() const override { return "raw-stats"; }
    QString description() const override { return "Compute signal metrics (min, max, average, RMS)."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption("signal", "Signal to export (repeatable, raw-export)", "signame"));
        parser.addOption(QCommandLineOption("range", "Time range t0:t1", "range"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.raw"}}, {"options", QJsonObject{{"signal", "name (repeatable)"}, {"range", "t0:t1"}}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"file", "string"}, {"stats", "array[object]"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora raw-stats <file.raw> [options]" << std::endl;
            return 1;
        }
        QString filePath = args.at(0);
        RawData data;
        if (!RawDataParser::loadRawAscii(filePath.toStdString(), &data)) {
            std::cerr << "Error loading raw file: " << filePath.toStdString() << std::endl;
            return 1;
        }

        double tStart = std::numeric_limits<double>::quiet_NaN();
        double tEnd = std::numeric_limits<double>::quiet_NaN();
        QString rangeError;
        if (!parseRangeOption(parser.value("range"), &tStart, &tEnd, &rangeError)) {
            std::cerr << "Error: " << rangeError.toStdString() << std::endl;
            return 1;
        }

        QStringList signalNames = parser.values("signal");
        if (signalNames.isEmpty()) {
            for (int i = 1; i < (int)data.varNames.size(); ++i) {
                signalNames << QString::fromStdString(data.varNames[i]);
            }
        }

        QVector<int> indices;
        for (const auto& sig : signalNames) {
            int idx = -1;
            for (int i = 0; i < (int)data.varNames.size(); ++i) {
                if (QString::fromStdString(data.varNames[i]) == sig) {
                    idx = i;
                    break;
                }
            }
            if (idx < 1) {
                std::cerr << "Error: Signal not found: " << sig.toStdString() << std::endl;
                return 1;
            }
            indices << (idx - 1);
        }

        const QVector<int> rangeIndices = filteredIndices(data, tStart, tEnd);
        const auto stats = computeSignalStats(data, signalNames, indices, rangeIndices);

        QJsonObject out;
        out["file"] = filePath;
        QJsonArray statArr;
        for (const auto& s : stats) {
            QJsonObject st;
            st["name"] = s.name;
            st["min"] = s.min;
            st["max"] = s.max;
            st["avg"] = s.avg;
            st["rms"] = s.rms;
            statArr.append(st);
        }
        out["stats"] = statArr;
        printJsonValue(out);
        return 0;
    }
};

class ViewCommand : public CLICommand {
public:
    QString name() const override { return "view"; }
    QString description() const override { return "View raw simulation waveforms in GUI or render to image."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption("type", "Viewer type (plot|osc)", "type", "plot"));
        parser.addOption(QCommandLineOption("render-image", "Render oscilloscope or waveform directly to image file", "file"));
        parser.addOption(QCommandLineOption("width", "Render image width (default: 1000)", "px", "1000"));
        parser.addOption(QCommandLineOption("height", "Render image height (default: 600)", "px", "600"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora view <file.raw> [--type plot|osc] [--render-image <output.png>]" << std::endl;
            return 1;
        }
        QString filePath = args.at(0);
        QString viewType = parser.value("type").toLower();
        QString renderImageFile = parser.value("render-image");
        int width = parser.value("width").toInt();
        int height = parser.value("height").toInt();
        if (width <= 0) width = 1000;
        if (height <= 0) height = 600;

        RawData data;
        if (!RawDataParser::loadRawAscii(filePath.toStdString(), &data)) {
            std::cerr << "Error: Failed to load raw file: " << filePath.toStdString() << std::endl;
            return 1;
        }

        if (viewType == "osc" || viewType == "oscilloscope") {
            auto* oscWin = new OscilloscopeWindow(QUuid::createUuid(), QFileInfo(filePath).baseName());
            
            // Build SimResults from RawData
            SimResults simRes;
            for (size_t i = 1; i < data.varNames.size(); ++i) {
                if (i - 1 < data.y.size()) {
                    SimWaveform wave;
                    wave.name = data.varNames[i];
                    wave.xData = data.x;
                    wave.yData = data.y[i - 1];
                    simRes.waveforms.push_back(wave);
                }
            }

            // Map variables into Oscilloscope channel traces
            QMap<QString, QVector<QPointF>> traces;
            for (size_t i = 1; i < data.varNames.size() && i <= 8; ++i) {
                if (i - 1 < data.y.size()) {
                    QVector<QPointF> pts;
                    pts.reserve(data.x.size());
                    for (size_t s = 0; s < data.x.size(); ++s) {
                        pts.append(QPointF(data.x[s], data.y[i - 1][s]));
                    }
                    QString chName = QString("CH%1 (%2)").arg(i).arg(QString::fromStdString(data.varNames[i]));
                    traces[chName] = pts;
                }
            }

            if (!renderImageFile.isEmpty()) {
                MiniScopeWidget scopeWidget;
                scopeWidget.setMultiTraceData(traces);
                QImage img = scopeWidget.renderToImage(QSize(width, height));
                if (img.save(renderImageFile)) {
                    std::cout << "Rendered oscilloscope image to: " << renderImageFile.toStdString() << std::endl;
                    delete oscWin;
                    return 0;
                } else {
                    std::cerr << "Error: Failed to save rendered image to " << renderImageFile.toStdString() << std::endl;
                    delete oscWin;
                    return 1;
                }
            }

            oscWin->resize(width, height);
            oscWin->show();
            return qApp->exec();
        } else {
            QMainWindow* window = new QMainWindow();
            window->setWindowTitle(QString("VioSpice Waveform Viewer - %1").arg(QFileInfo(filePath).fileName()));
            window->resize(width, height);

            WaveformViewer* viewer = new WaveformViewer(window);
            window->setCentralWidget(viewer);

            QVector<double> time(data.x.begin(), data.x.end());

            viewer->beginBatchUpdate();
            for (size_t i = 1; i < data.varNames.size(); ++i) {
                if (i - 1 < data.y.size()) {
                    QVector<double> values(data.y[i - 1].begin(), data.y[i - 1].end());
                    QString name = QString::fromStdString(data.varNames[i]);
                    viewer->addSignal(name, time, values);
                    viewer->setSignalChecked(name, true);
                }
            }
            viewer->endBatchUpdate();
            viewer->zoomFit();

            if (!renderImageFile.isEmpty()) {
                QImage img(QSize(width, height), QImage::Format_ARGB32_Premultiplied);
                img.fill(Qt::black);
                QPainter painter(&img);
                viewer->render(&painter);
                if (img.save(renderImageFile)) {
                    std::cout << "Rendered waveform image to: " << renderImageFile.toStdString() << std::endl;
                    delete window;
                    return 0;
                } else {
                    std::cerr << "Error: Failed to save rendered image to " << renderImageFile.toStdString() << std::endl;
                    delete window;
                    return 1;
                }
            }

            window->show();
            return qApp->exec();
        }
    }
};

class SimulateCommand : public CLICommand {
public:
    QString name() const override { return "simulate"; }
    QString description() const override { return "Simulate schematic (backward compatibility)."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption("analysis", "Analysis type (op, tran, ac, live)", "type", "op"));
        parser.addOption(QCommandLineOption("step", "Step size", "time", "100u"));
        parser.addOption(QCommandLineOption("stop", "Stop time", "time", "10m"));
        parser.addOption(QCommandLineOption("max-step", "Maximum timestep for live mode", "time", "1e-3"));
        parser.addOption(QCommandLineOption("max-time", "Maximum simulation time for live mode", "time", "0"));
        parser.addOption(QCommandLineOption("max-pts", "Maximum data points for live mode", "count", "100000"));
        parser.addOption(QCommandLineOption("json", "Output results in JSON format"));
        parser.addOption(QCommandLineOption("robust", "Enable robust convergence options"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{
            {"args", QJsonArray{"file.flxsch"}},
            {"options", QJsonObject{
                {"analysis", "string (op, tran, ac, live)"},
                {"step", "string"},
                {"stop", "string"},
                {"json", "bool"},
                {"robust", "bool"}
            }}
        };
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora simulate <file.flxsch> [options]" << std::endl;
            return 1;
        }
        QString filePath = args.at(0);
        QGraphicsScene scene;
        QString pageSize;
        TitleBlockData dummyTB;
        if (!SchematicFileIO::loadSchematic(&scene, filePath, pageSize, dummyTB)) {
            std::cerr << "Error loading schematic: " << SchematicFileIO::lastError().toStdString() << std::endl;
            return 1;
        }

        if (!g_quiet) printInfo("Simulating circuit " + filePath + " (Ngspice backend)...");
        
        QString analysisType = parser.value("analysis").toLower();
        SpiceNetlistGenerator::SimulationParams spiceParams;
        SimAnalysisType t = SimAnalysisType::OP;

        if (analysisType == "live") {
            t = SimAnalysisType::RealTime;

            if (!g_quiet) printInfo(QString("  - Type: Interactive Live (MaxStep=%1s, MaxTime=%2s, MaxPts=%3)")
                      .arg(parser.value("max-step")).arg(parser.value("max-time")).arg(parser.value("max-pts")));

            double maxStep = 1e-3;
            SimValueParser::parseSpiceNumber(parser.value("max-step"), maxStep);
            double maxTime = 0.0;
            SimValueParser::parseSpiceNumber(parser.value("max-time"), maxTime);
            int maxPts = parser.value("max-pts").toInt();
            if (maxStep <= 0) maxStep = 1e-3;
            if (maxPts < 1000) maxPts = 100000;

            auto &sm = SimManager::instance();
            bool finished = false;
            QString lastError;

            QObject::connect(&sm, &SimManager::simulationStopped, [&]() { finished = true; });
            QObject::connect(&sm, &SimManager::errorOccurred, [&](const QString &err) {
                lastError = err;
                finished = true;
            });

            sm.runRealTime(&scene, nullptr, maxStep, maxTime, maxPts);

            QElapsedTimer wallClock;
            wallClock.start();
            const qint64 maxWallMs = (maxTime > 0) ? static_cast<qint64>(maxTime * 1000 * 1.5 + 60000) : 24LL * 3600 * 1000;

            while (!finished && wallClock.elapsed() < maxWallMs) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
                QThread::msleep(10);
                if (!sm.isRunning() && !finished) {
                    QCoreApplication::processEvents();
                    finished = true;
                }
            }

            sm.stopRealTime();
            if (!lastError.isEmpty()) {
                std::cerr << "Interactive simulation failed: " << lastError.toStdString() << std::endl;
                return 1;
            }
            if (!g_quiet) printInfo("Interactive simulation completed.");
            std::_Exit(0);

        } else if (analysisType == "tran") {
            t = SimAnalysisType::Transient;
            spiceParams.type = SpiceNetlistGenerator::Transient;
            spiceParams.stop = parser.value("stop");
            spiceParams.step = parser.value("step");
            if (spiceParams.stop.isEmpty()) spiceParams.stop = "10m";
            if (spiceParams.step.isEmpty()) spiceParams.step = "100u";
            if (!g_quiet) printInfo(QString("  - Type: Transient (Stop=%1, Step=%2)").arg(spiceParams.stop).arg(spiceParams.step));
        } else if (analysisType == "ac") {
            t = SimAnalysisType::AC;
            spiceParams.type = SpiceNetlistGenerator::AC;
            spiceParams.start = "10";
            spiceParams.stop = "1meg";
            spiceParams.step = "100";
            if (!g_quiet) printInfo("  - Type: AC Sweep (10Hz to 1MHz)");
        } else {
            t = SimAnalysisType::OP;
            spiceParams.type = SpiceNetlistGenerator::OP;
            if (!g_quiet) printInfo("  - Type: DC Operating Point");
        }

        auto result = SpiceNetlistGenerator::generate(&scene, QFileInfo(filePath).absolutePath(), nullptr, spiceParams);
        SimManager::instance().m_pinToNetMap = result.componentPins;
        SimManager::instance().compileFluxScripts(&scene);

        QString netlistText = result.netlist;
        if (parser.isSet("robust")) {
            netlistText += "\n.options gmin=1e-9 abstol=1e-10 reltol=1e-3 chgtol=1e-13 method=gear\n";
            netlistText += ".options rshunt=10Meg\n";
        }
        
        QTemporaryFile tempNetlist(QDir::tempPath() + "/viospice_cli_XXXXXX.cir");
        tempNetlist.setAutoRemove(false);
        if (!tempNetlist.open()) {
            std::cerr << "Error creating temporary netlist file." << std::endl;
            return 1;
        }
        {
            QTextStream out(&tempNetlist);
            out << netlistText;
        }
        tempNetlist.close();

        QEventLoop loop;
        SimResults results;
        bool success = false;
        QString lastError;

        auto& sm = SimulationManager::instance();
        QObject::connect(&sm, &SimulationManager::simulationFinished, &loop, &QEventLoop::quit);
        QObject::connect(&sm, &SimulationManager::errorOccurred, [&](const QString& err) {
            lastError = err;
            loop.quit();
        });
        
        QObject::connect(&sm, &SimulationManager::rawResultsReady, [&](const QString& path) {
            RawData rd;
            if (RawDataParser::loadRawAscii(path.toStdString(), &rd)) {
                results.analysisType = t;
                double maxVoltage = 0.0;
                for (int i = 0; i < (int)rd.varNames.size(); ++i) {
                    if (i == 0) continue;
                    SimWaveform wave;
                    wave.name = rd.varNames[i];
                    wave.xData = std::vector<double>(rd.x.begin(), rd.x.end());
                    wave.yData = std::vector<double>(rd.y[i-1].begin(), rd.y[i-1].end());
                    results.waveforms.push_back(wave);
                    
                    if (!rd.y[i-1].empty()) {
                        double lastVal = rd.y[i-1].back();
                        maxVoltage = std::max(maxVoltage, std::abs(lastVal));
                        QString qName = QString::fromStdString(wave.name);
                        if (qName.startsWith("V(", Qt::CaseInsensitive)) {
                             results.nodeVoltages[qName.mid(2, qName.size() - 3).toStdString()] = lastVal;
                        } else if (qName.startsWith("I(", Qt::CaseInsensitive)) {
                             results.branchCurrents[qName.mid(2, qName.size() - 3).toStdString()] = lastVal;
                        }
                    }
                }
                if (maxVoltage > 10000.0) {
                    printInfo("Warning: Extreme voltage detected (>10kV). Circuit may be unstable or unphysical.");
                }
                success = true;
            }
        });

        sm.runSimulation(tempNetlist.fileName(), nullptr);
        
        QElapsedTimer activeTimer;
        activeTimer.start();
        while (!success && !lastError.length() && activeTimer.elapsed() < 30000) {
            QCoreApplication::processEvents();
            SimulationManager::instance().applyPendingFluxSourceUpdates();
            QThread::msleep(5);
            if (!sm.isRunning() && !success) {
                QCoreApplication::processEvents();
                if (!success) break;
            }
        }

        sm.shutdown();
        QThread::msleep(50);
        
        QFile::remove(tempNetlist.fileName());
        QFile::remove(tempNetlist.fileName() + ".raw");

        if (!success) {
            std::cerr << "Simulation failed: " << lastError.toStdString() << std::endl;
            SimulationManager::instance().shutdown();
            return 1;
        }

        if (parser.isSet("json")) {
            printJsonValue(resultsToJson(results));
            std::cout.flush();
            std::cerr.flush();
            std::_Exit(0);
        }

        if (results.waveforms.empty() && results.nodeVoltages.empty()) {
            std::cerr << "Simulation failed to produce results." << std::endl;
            SimulationManager::instance().shutdown();
            return 1;
        }

        if (t == SimAnalysisType::OP) {
            printInfo("\n--- DC Operating Point Results ---");
            for (const auto& [node, v] : results.nodeVoltages) {
                printInfo(QString("V(%1) = %2 V").arg(QString::fromStdString(node)).arg(v));
            }
            for (const auto& [branch, i] : results.branchCurrents) {
                printInfo(QString("I(%1) = %2 mA").arg(QString::fromStdString(branch)).arg(i * 1000.0));
            }
        } else {
            printInfo(QString("\nGenerated %1 waveforms.").arg(results.waveforms.size()));
            for (const auto& wave : results.waveforms) {
                printInfo(QString("  - %1 (%2 points)").arg(QString::fromStdString(wave.name)).arg(wave.yData.size()));
                if (!wave.yData.empty()) {
                    double minE = *std::min_element(wave.yData.begin(), wave.yData.end());
                    double maxE = *std::max_element(wave.yData.begin(), wave.yData.end());
                    printInfo(QString("    Range: [%1 V, %2 V]").arg(minE).arg(maxE));
                }
            }
        }

        printInfo("\nSimulation successful.");
        std::cout.flush();
        std::cerr.flush();
        std::_Exit(0);
        return 0;
    }
};

} // namespace

void registerNetlistCommands() {
    auto& reg = CommandRegistry::instance();
    reg.registerCommand(std::make_unique<NetlistCompareCommand>());
    reg.registerCommand(std::make_unique<NetlistRunCommand>());
    reg.registerCommand(std::make_unique<NetlistValidateCommand>());
    reg.registerCommand(std::make_unique<NetlistToSchematicCommand>());
    reg.registerCommand(std::make_unique<RawInfoCommand>());
    reg.registerCommand(std::make_unique<RawExportCommand>());
    reg.registerCommand(std::make_unique<RawStatsCommand>());
    reg.registerCommand(std::make_unique<ViewCommand>());
    reg.registerCommand(std::make_unique<SimulateCommand>());
}
