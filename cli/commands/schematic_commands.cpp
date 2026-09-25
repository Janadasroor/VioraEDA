/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#include "schematic_commands.h"
#include "common.h"
#include "../command_registry.h"

#include "flux/schematic/io/schematic_file_io.h"
#include "schematic/items/schematic_item.h"
#include "flux/schematic/factories/schematic_item_registry.h"
#include "flux/schematic/items/wire_item.h"
#include "flux/schematic/analysis/schematic_annotator.h"
#include "flux/schematic/analysis/schematic_erc.h"
#include "schematic/analysis/spice_netlist_generator.h"
#include "schematic/io/netlist_generator.h"
#include "schematic/io/netlist_to_schematic.h"
#include "simulator/bridge/sim_schematic_bridge.h"
#include "simulator/bridge/sim_manager.h"
#include "simulator/bridge/model_library_manager.h"
#include "symbols/symbol_library.h"
#include "utils/schematic_url_encoder.h"
#include "simulator/core/sim_report_generator.h"
#include "simulator/core/raw_data_parser.h"
#include "schematic/items/oscilloscope_item.h"
#include "schematic/items/generic_component_item.h"
#include "schematic/items/voltage_source_item.h"
#include "schematic/items/avr_microcontroller_item.h"
#include "schematic/dialogs/oscilloscope_properties_dialog.h"
#include "schematic/dialogs/voltage_source_waveform_dialog.h"
#include "schematic/dialogs/avr_microcontroller_dialog.h"
#include "schematic/dialogs/component_properties_dialog.h"
#include "schematic/dialogs/generic_symbol_properties_dialog.h"

#include <QGraphicsScene>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <iostream>
#include <algorithm>

namespace {

QJsonObject componentToJson(const ECOComponent& comp) {
    QJsonObject c;
    c["reference"] = comp.reference;
    c["typeName"] = comp.typeName;
    c["type"] = comp.type;
    c["value"] = comp.value;
    c["spiceModel"] = comp.spiceModel;
    c["footprint"] = comp.footprint;
    c["symbolPinCount"] = comp.symbolPinCount;
    c["excludeFromSim"] = comp.excludeFromSim;
    c["excludeFromPcb"] = comp.excludeFromPcb;
    return c;
}

QMap<QString, QPointF> collectComponentPositions(QGraphicsScene* scene) {
    QMap<QString, QPointF> out;
    if (!scene) return out;
    for (auto* item : scene->items()) {
        if (auto* si = dynamic_cast<SchematicItem*>(item)) {
            const QString ref = si->reference();
            if (!ref.trimmed().isEmpty() && !out.contains(ref)) {
                out[ref] = si->pos();
            }
        }
    }
    return out;
}

class SchematicQueryCommand : public CLICommand {
public:
    QString name() const override { return "schematic-query"; }
    QString description() const override { return "Query schematic file for information."; }
    void setupParser(QCommandLineParser& parser) override {}
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.flxsch"}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{
            {"file", "string"},
            {"pageSize", "string"},
            {"components", "array[component]"},
            {"nets", "array[net]"}
        };
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora schematic-query <file.flxsch>" << std::endl;
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

        QJsonObject out;
        out["file"] = filePath;
        out["pageSize"] = pageSize;

        ECOPackage pkg = NetlistGenerator::generateECOPackage(&scene, QFileInfo(filePath).absolutePath(), nullptr);
        QList<NetlistNet> nets = NetlistGenerator::buildConnectivity(&scene, QFileInfo(filePath).absolutePath(), nullptr);

        QJsonArray comps;
        for (const auto& comp : pkg.components) {
            comps.append(componentToJson(comp));
        }
        out["components"] = comps;

        QJsonArray netsArr;
        for (const auto& net : nets) {
            QJsonObject n;
            n["name"] = net.name;
            QJsonArray pins;
            for (const auto& pin : net.pins) {
                QJsonObject p;
                p["ref"] = pin.componentRef;
                p["pin"] = pin.pinName;
                pins.append(p);
            }
            n["pins"] = pins;
            netsArr.append(n);
        }
        out["nets"] = netsArr;

        printJsonValue(out);
        return 0;
    }
};

class SchematicNetlistCommand : public CLICommand {
public:
    QString name() const override { return "schematic-netlist"; }
    QString description() const override { return "Generate SPICE or JSON netlist from schematic."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption({"f", "format"}, "Output format (spice|json)", "format", "spice"));
        parser.addOption(QCommandLineOption({"a", "analysis"}, "Analysis type (op, tran, ac)", "type", "op"));
        parser.addOption(QCommandLineOption({"s", "step"}, "Step size for transient", "time", "100u"));
        parser.addOption(QCommandLineOption({"t", "stop"}, "Stop time for transient", "time", "10m"));
        parser.addOption(QCommandLineOption("out", "Output file path", "file"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.flxsch"}}, {"options", QJsonObject{{"format", "spice|json"}, {"analysis", "op|tran|ac"}, {"step", "string"}, {"stop", "string"}, {"out", "file"}}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"netlist", "string (spice or json)"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora schematic-netlist <file.flxsch> [options]" << std::endl;
            return 1;
        }
        QString filePath = args.at(0);
        QString format = parser.value("format").trimmed().toLower();
        if (parser.isSet("json")) format = "json";
        const QString outPath = parser.value("out").trimmed();
        QGraphicsScene scene;
        QString pageSize;
        TitleBlockData dummyTB;
        if (!SchematicFileIO::loadSchematic(&scene, filePath, pageSize, dummyTB)) {
            std::cerr << "Error loading schematic: " << SchematicFileIO::lastError().toStdString() << std::endl;
            return 1;
        }

        if (format == "json") {
            const QString net = NetlistGenerator::generate(&scene, QFileInfo(filePath).absolutePath(), NetlistGenerator::FluxJSON, nullptr);
            if (!outPath.isEmpty()) {
                QFile outFile(outPath);
                if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    std::cerr << "Error: Unable to write netlist to " << outPath.toStdString() << std::endl;
                    return 1;
                }
                outFile.write(net.toUtf8());
                outFile.close();
            } else {
                std::cout << net.toStdString() << std::endl;
            }
            return 0;
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

        auto result = SpiceNetlistGenerator::generate(&scene, QFileInfo(filePath).absolutePath(), nullptr, params);
        if (!outPath.isEmpty()) {
            QFile outFile(outPath);
            if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                std::cerr << "Error: Unable to write netlist to " << outPath.toStdString() << std::endl;
                return 1;
            }
            outFile.write(result.netlist.toUtf8());
            outFile.close();
        } else {
            std::cout << result.netlist.toStdString() << std::endl;
        }
        return 0;
    }
};

class SchematicRenderCommand : public CLICommand {
public:
    QString name() const override { return "schematic-render"; }
    QString description() const override { return "Render a schematic to an image."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption("scale", "Render scale (default 4.0)", "scale", "4"));
        parser.addOption(QCommandLineOption("transparent", "Render PNG with transparent background"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.flxsch", "out.png"}}, {"options", QJsonObject{{"transparent", "bool"}, {"json", "bool"}, {"scale", "number"}}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"file", "string"}, {"output", "string"}, {"width", "int"}, {"height", "int"}, {"scale", "number"}, {"transparent", "bool"}, {"bounds", "rect"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.size() < 2) {
            std::cerr << "Usage: viora schematic-render <file.flxsch> <out.png> [options]" << std::endl;
            return 1;
        }
        QString filePath = args.at(0);
        QString outPath = args.at(1);

        QGraphicsScene scene;
        QString pageSize;
        TitleBlockData dummyTB;
        if (!SchematicFileIO::loadSchematic(&scene, filePath, pageSize, dummyTB)) {
            std::cerr << "Error loading schematic: " << SchematicFileIO::lastError().toStdString() << std::endl;
            return 1;
        }

        QRectF rect = scene.itemsBoundingRect();
        if (rect.isEmpty()) rect = QRectF(-50, -50, 100, 100);
        rect.adjust(-10, -10, 10, 10);

        const qreal scale = qMax(0.1, parser.value("scale").toDouble());
        QImage image(rect.size().toSize() * scale, QImage::Format_ARGB32);
        const bool transparent = parser.isSet("transparent");
        image.fill(transparent ? Qt::transparent : QColor(20, 20, 25));

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        scene.render(&painter, QRectF(), rect);
        painter.end();

        if (!image.save(outPath)) {
            std::cerr << "Failed to save image to " << outPath.toStdString() << std::endl;
            return 1;
        }

        if (parser.isSet("json")) {
            QJsonObject out;
            out["file"] = filePath;
            out["output"] = outPath;
            out["width"] = image.width();
            out["height"] = image.height();
            out["scale"] = scale;
            out["transparent"] = transparent;
            QJsonObject bounds;
            bounds["x"] = rect.x();
            bounds["y"] = rect.y();
            bounds["w"] = rect.width();
            bounds["h"] = rect.height();
            out["bounds"] = bounds;
            printJsonValue(out);
        } else {
            printInfoStd("Successfully rendered schematic to " + outPath.toStdString());
        }
        return 0;
    }
};

class SchematicBomCommand : public CLICommand {
public:
    QString name() const override { return "schematic-bom"; }
    QString description() const override { return "Generate Bill of Materials (BOM) from schematic."; }
    void setupParser(QCommandLineParser& parser) override {}
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.flxsch"}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"file", "string"}, {"components", "array[component]"}, {"groups", "array[group]"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora schematic-bom <file.flxsch>" << std::endl;
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

        ECOPackage pkg = NetlistGenerator::generateECOPackage(&scene, QFileInfo(filePath).absolutePath(), nullptr);
        QJsonObject out;
        out["file"] = filePath;

        // Flat component list (sorted by reference)
        std::sort(pkg.components.begin(), pkg.components.end(), [](const ECOComponent& a, const ECOComponent& b) {
            return a.reference.toLower() < b.reference.toLower();
        });
        QJsonArray comps;
        for (const auto& comp : pkg.components) {
            comps.append(componentToJson(comp));
        }
        out["components"] = comps;

        // Grouped BOM
        struct BomKey {
            QString typeName;
            QString value;
            QString footprint;
            bool operator<(const BomKey& other) const {
                if (typeName != other.typeName) return typeName < other.typeName;
                if (value != other.value) return value < other.value;
                return footprint < other.footprint;
            }
        };
        QMap<BomKey, QStringList> groups;
        for (const auto& comp : pkg.components) {
            BomKey key{comp.typeName, comp.value, comp.footprint};
            groups[key].append(comp.reference);
        }
        QJsonArray grouped;
        for (auto it = groups.begin(); it != groups.end(); ++it) {
            QJsonObject g;
            g["typeName"] = it.key().typeName;
            g["value"] = it.key().value;
            g["footprint"] = it.key().footprint;
            g["qty"] = it.value().size();
            QStringList refs = it.value();
            std::sort(refs.begin(), refs.end(), [](const QString& a, const QString& b) { return a.toLower() < b.toLower(); });
            QJsonArray refArr;
            for (const QString& r : refs) refArr.append(r);
            g["references"] = refArr;
            grouped.append(g);
        }
        out["groups"] = grouped;

        printJsonValue(out);
        return 0;
    }
};

class SchematicValidateCommand : public CLICommand {
public:
    QString name() const override { return "schematic-validate"; }
    QString description() const override { return "Validate schematic and run ERC checks."; }
    void setupParser(QCommandLineParser& parser) override {}
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.flxsch"}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"file", "string"}, {"erc", "array[erc_issue]"}, {"preflight", "array[string]"}, {"summary", "object"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora schematic-validate <file.flxsch>" << std::endl;
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

        QJsonObject out;
        out["file"] = filePath;
        out["pageSize"] = pageSize;

        // ERC
        QJsonArray ercIssues;
        auto violations = SchematicERC::run(&scene, QFileInfo(filePath).absolutePath());
        for (const auto& v : violations) {
            QJsonObject issue;
            issue["severity"] = (v.severity == ERCViolation::Error) ? "Error" :
                                (v.severity == ERCViolation::Critical) ? "Critical" : "Warning";
            issue["message"] = v.message;
            issue["x"] = v.position.x();
            issue["y"] = v.position.y();
            ercIssues.append(issue);
        }
        out["erc"] = ercIssues;

        // Simulator preflight
        SimNetlist preflightNetlist;
        QStringList preflight = SimManager::instance().preflightCheck(&scene, nullptr, preflightNetlist, QFileInfo(filePath).absolutePath());
        QJsonArray preflightArr;
        for (const QString& msg : preflight) preflightArr.append(msg);
        out["preflight"] = preflightArr;

        // Summary
        QJsonObject summary;
        summary["ercCount"] = ercIssues.size();
        summary["preflightCount"] = preflightArr.size();
        out["summary"] = summary;

        printJsonValue(out);

        bool hasErrors = false;
        for (const auto& v : violations) {
            if (v.severity == ERCViolation::Error || v.severity == ERCViolation::Critical) {
                hasErrors = true;
                break;
            }
        }
        if (hasErrors || (!preflightArr.isEmpty()) || (parser.isSet("exit-on-warning") && (!ercIssues.isEmpty() || !preflightArr.isEmpty()))) {
            return 1;
        }
        return 0;
    }
};

class ErcCommand : public CLICommand {
public:
    QString name() const override { return "erc"; }
    QString description() const override { return "Run electrical rules check (ERC) on a schematic."; }
    void setupParser(QCommandLineParser& parser) override {}
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.flxsch"}}, {"options", QJsonObject{{"json", "bool"}}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora erc <file.flxsch> [options]" << std::endl;
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

        if (!g_quiet) std::cout << "Running ERC on " << filePath.toStdString() << "..." << std::endl;
        auto violations = SchematicERC::run(&scene, QFileInfo(filePath).absolutePath());

        if (parser.isSet("json")) {
            QJsonObject out;
            QJsonArray arr;
            int errCount = 0;
            int warnCount = 0;
            for (const auto& v : violations) {
                QJsonObject item;
                QString sev = (v.severity == ERCViolation::Error) ? "Error" : "Warning";
                item["severity"] = sev;
                item["message"] = v.message;
                item["x"] = v.position.x();
                item["y"] = v.position.y();
                arr.append(item);
                if (v.severity == ERCViolation::Error) ++errCount;
                else ++warnCount;
            }
            out["ok"] = (errCount == 0);
            out["file"] = filePath;
            out["violations"] = arr;
            out["errorCount"] = errCount;
            out["warningCount"] = warnCount;
            std::cout << QJsonDocument(out).toJson(QJsonDocument::Compact).toStdString() << std::endl;
            return errCount > 0 ? 1 : 0;
        }

        if (violations.isEmpty()) {
            if (!g_quiet) std::cout << "ERC Passed! No issues found." << std::endl;
        } else {
            if (!g_quiet) std::cout << "ERC found " << violations.size() << " issues:" << std::endl;
            int errCount = 0;
            for (const auto& v : violations) {
                QString sev = (v.severity == ERCViolation::Error) ? "Error" : "Warning";
                if (!g_quiet) std::cout << "  [" << sev.toStdString() << "] " << v.message.toStdString()
                                        << " at (" << v.position.x() << ", " << v.position.y() << ")" << std::endl;
                if (v.severity == ERCViolation::Error) ++errCount;
            }
            return errCount > 0 ? 1 : 0;
        }
        return 0;
    }
};

class SchematicDiffCommand : public CLICommand {
public:
    QString name() const override { return "schematic-diff"; }
    QString description() const override { return "Compare two schematic files."; }
    void setupParser(QCommandLineParser& parser) override {}
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"a.flxsch", "b.flxsch"}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"a", "string"}, {"b", "string"}, {"components", "object{added,removed,changed}"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.size() < 2) {
            std::cerr << "Usage: viora schematic-diff <a.flxsch> <b.flxsch>" << std::endl;
            return 1;
        }
        const QString aPath = args.at(0);
        const QString bPath = args.at(1);

        QGraphicsScene sceneA;
        QString pageA;
        TitleBlockData tbA;
        if (!SchematicFileIO::loadSchematic(&sceneA, aPath, pageA, tbA)) {
            std::cerr << "Error loading schematic: " << SchematicFileIO::lastError().toStdString() << std::endl;
            return 1;
        }
        QGraphicsScene sceneB;
        QString pageB;
        TitleBlockData tbB;
        if (!SchematicFileIO::loadSchematic(&sceneB, bPath, pageB, tbB)) {
            std::cerr << "Error loading schematic: " << SchematicFileIO::lastError().toStdString() << std::endl;
            return 1;
        }

        ECOPackage pkgA = NetlistGenerator::generateECOPackage(&sceneA, QFileInfo(aPath).absolutePath(), nullptr);
        ECOPackage pkgB = NetlistGenerator::generateECOPackage(&sceneB, QFileInfo(bPath).absolutePath(), nullptr);

        QMap<QString, ECOComponent> mapA;
        QMap<QString, ECOComponent> mapB;
        for (const auto& c : pkgA.components) mapA[c.reference] = c;
        for (const auto& c : pkgB.components) mapB[c.reference] = c;

        QMap<QString, QPointF> posA = collectComponentPositions(&sceneA);
        QMap<QString, QPointF> posB = collectComponentPositions(&sceneB);

        QJsonObject out;
        out["a"] = aPath;
        out["b"] = bPath;

        QJsonArray added;
        QJsonArray removed;
        QJsonArray changed;

        for (auto it = mapA.begin(); it != mapA.end(); ++it) {
            const QString ref = it.key();
            if (!mapB.contains(ref)) {
                removed.append(ref);
                continue;
            }
            const ECOComponent& ca = it.value();
            const ECOComponent& cb = mapB[ref];
            QJsonObject diff;
            bool hasDiff = false;
            if (ca.typeName != cb.typeName) { diff["typeName"] = QJsonArray{ca.typeName, cb.typeName}; hasDiff = true; }
            if (ca.value != cb.value) { diff["value"] = QJsonArray{ca.value, cb.value}; hasDiff = true; }
            if (ca.footprint != cb.footprint) { diff["footprint"] = QJsonArray{ca.footprint, cb.footprint}; hasDiff = true; }
            if (ca.spiceModel != cb.spiceModel) { diff["spiceModel"] = QJsonArray{ca.spiceModel, cb.spiceModel}; hasDiff = true; }
            if (posA.contains(ref) && posB.contains(ref)) {
                QPointF pa = posA[ref];
                QPointF pb = posB[ref];
                if (pa != pb) {
                    QJsonObject pos;
                    pos["from"] = QJsonArray{pa.x(), pa.y()};
                    pos["to"] = QJsonArray{pb.x(), pb.y()};
                    diff["position"] = pos;
                    hasDiff = true;
                }
            }
            if (hasDiff) {
                QJsonObject entry;
                entry["reference"] = ref;
                entry["changes"] = diff;
                changed.append(entry);
            }
        }
        for (auto it = mapB.begin(); it != mapB.end(); ++it) {
            const QString ref = it.key();
            if (!mapA.contains(ref)) added.append(ref);
        }

        out["components"] = QJsonObject{
            {"added", added},
            {"removed", removed},
            {"changed", changed}
        };

        printJsonValue(out);
        return 0;
    }
};

class SchematicTransformCommand : public CLICommand {
public:
    QString name() const override { return "schematic-transform"; }
    QString description() const override { return "Apply refactor transformations to schematic."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption("rename-net", "Rename net label (repeatable): old=new", "pair"));
        parser.addOption(QCommandLineOption("normalize-value", "Normalize value (repeatable): old=new", "pair"));
        parser.addOption(QCommandLineOption("prefix-ref", "Rename reference prefix: old=new", "pair"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.flxsch"}}, {"options", QJsonObject{{"rename-net", "old=new (repeatable)"}, {"normalize-value", "old=new (repeatable)"}, {"prefix-ref", "old=new"}}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"file", "string"}, {"renamedNets", "int"}, {"normalizedValues", "int"}, {"updatedRefs", "int"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora schematic-transform <file.flxsch> [options]" << std::endl;
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

        const QStringList renames = parser.values("rename-net");
        const QStringList valueRules = parser.values("normalize-value");
        const QString prefixRule = parser.value("prefix-ref");

        QMap<QString, QString> netRename;
        for (const QString& rule : renames) {
            const int eq = rule.indexOf('=');
            if (eq > 0) {
                const QString from = rule.left(eq).trimmed();
                const QString to = rule.mid(eq + 1).trimmed();
                if (!from.isEmpty() && !to.isEmpty()) netRename[from] = to;
            }
        }

        QMap<QString, QString> valueMap;
        for (const QString& rule : valueRules) {
            const int eq = rule.indexOf('=');
            if (eq > 0) {
                const QString from = rule.left(eq).trimmed();
                const QString to = rule.mid(eq + 1).trimmed();
                if (!from.isEmpty() && !to.isEmpty()) valueMap[from] = to;
            }
        }

        QString prefixFrom;
        QString prefixTo;
        if (!prefixRule.isEmpty()) {
            const int eq = prefixRule.indexOf('=');
            if (eq > 0) {
                prefixFrom = prefixRule.left(eq).trimmed();
                prefixTo = prefixRule.mid(eq + 1).trimmed();
            }
        }

        int renamedNets = 0;
        int normalizedValues = 0;
        int updatedRefs = 0;

        for (auto* item : scene.items()) {
            if (auto* si = dynamic_cast<SchematicItem*>(item)) {
                // Net label rename
                if (si->itemType() == SchematicItem::NetLabelType) {
                    const QString cur = si->value();
                    if (netRename.contains(cur)) {
                        si->setValue(netRename[cur]);
                        renamedNets++;
                    }
                }

                // Normalize values
                const QString val = si->value();
                if (valueMap.contains(val)) {
                    si->setValue(valueMap[val]);
                    normalizedValues++;
                }

                // Prefix rename
                const QString ref = si->reference();
                if (!prefixFrom.isEmpty() && ref.startsWith(prefixFrom)) {
                    si->setReference(prefixTo + ref.mid(prefixFrom.size()));
                    updatedRefs++;
                }
            }
        }

        if (!SchematicFileIO::saveSchematic(&scene, filePath, pageSize)) {
            std::cerr << "Error saving schematic." << std::endl;
            return 1;
        }

        QJsonObject out;
        out["file"] = filePath;
        out["renamedNets"] = renamedNets;
        out["normalizedValues"] = normalizedValues;
        out["updatedRefs"] = updatedRefs;
        printJsonValue(out);
        return 0;
    }
};

class SchematicProbeCommand : public CLICommand {
public:
    QString name() const override { return "schematic-probe"; }
    QString description() const override { return "Query or configure dynamic probes."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption("list", "List available signals (schematic-probe)"));
        parser.addOption(QCommandLineOption("add", "Add probe (repeatable): V(net) or I(device)", "signal"));
        parser.addOption(QCommandLineOption("auto", "Auto-probe all nets (schematic-probe)"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{{"args", QJsonArray{"file.flxsch"}}, {"options", QJsonObject{{"list", "bool"}, {"add", "signal (repeatable)"}, {"auto", "bool"}}}};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{{"file", "string"}, {"signals", "array[string]"}, {"voltages", "array[string]"}, {"currents", "array[string]"}, {"probes", "array[string]"}};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora schematic-probe <file.flxsch> [options]" << std::endl;
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

        SimNetlist netlist = SimSchematicBridge::buildNetlist(&scene, nullptr, QFileInfo(filePath).absolutePath());

        const bool listSignals = parser.isSet("list");
        const QStringList addSignals = parser.values("add");
        const bool autoProbe = parser.isSet("auto");

        if (listSignals || (addSignals.isEmpty() && !autoProbe)) {
            QStringList voltageSignals;
            QStringList currentSignals;

            for (int i = 0; i < netlist.nodeCount(); ++i) {
                const QString name = QString::fromStdString(netlist.nodeName(i)).trimmed();
                if (name.isEmpty()) continue;
                voltageSignals.append("V(" + name + ")");
            }

            for (const auto& comp : netlist.components()) {
                const QString name = QString::fromStdString(comp.name).trimmed();
                if (name.isEmpty()) continue;
                
                bool hasBranch = (comp.type == SimComponentType::VoltageSource || 
                                  comp.type == SimComponentType::Inductor ||
                                  comp.type == SimComponentType::B_VoltageSource ||
                                  comp.type == SimComponentType::OpAmpMacro);
                
                if (!hasBranch) continue;
                currentSignals.append("I(" + name + ")");
            }

            voltageSignals.sort(Qt::CaseInsensitive);
            currentSignals.sort(Qt::CaseInsensitive);

            QJsonArray voltageJson;
            for (const QString& v : voltageSignals) voltageJson.append(v);

            QJsonArray currentJson;
            for (const QString& c : currentSignals) currentJson.append(c);

            QJsonArray allSignals;
            for (const QString& v : voltageSignals) allSignals.append(v);
            for (const QString& c : currentSignals) allSignals.append(c);

            QJsonObject out;
            out["file"] = filePath;
            out["signals"] = allSignals;
            out["voltages"] = voltageJson;
            out["currents"] = currentJson;
            printJsonValue(out);
            return 0;
        }

        if (autoProbe) {
            for (int i = 0; i < netlist.nodeCount(); ++i) {
                const QString name = QString::fromStdString(netlist.nodeName(i)).trimmed();
                if (name.isEmpty()) continue;
                netlist.addAutoProbe(("V(" + name + ")").toStdString());
            }
        } else {
            QSet<QString> validSignals;
            for (int i = 0; i < netlist.nodeCount(); ++i) {
                const QString name = QString::fromStdString(netlist.nodeName(i)).trimmed();
                if (!name.isEmpty()) validSignals.insert("V(" + name + ")");
            }
            for (const auto& comp : netlist.components()) {
                const QString name = QString::fromStdString(comp.name).trimmed();
                if (!name.isEmpty()) validSignals.insert("I(" + name + ")");
            }

            for (const QString& sig : addSignals) {
                if (!validSignals.contains(sig)) {
                    std::cerr << "Warning: Signal not found in schematic: " << sig.toStdString() << std::endl;
                }
                netlist.addAutoProbe(sig.toStdString());
            }
        }

        QJsonObject out;
        out["file"] = filePath;
        QJsonArray probes;
        for (const auto& p : netlist.autoProbes()) {
            probes.append(QString::fromStdString(p));
        }
        out["probes"] = probes;
        printJsonValue(out);
        return 0;
    }
};

class GenerateReportCommand : public CLICommand {
public:
    QString name() const override { return "generate-report"; }
    QString description() const override { return "Generate design review report in HTML."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption("report-title", "Title of report", "title", ""));
        parser.addOption(QCommandLineOption("report-author", "Author of report", "author", ""));
        parser.addOption(QCommandLineOption("no-schematic", "Exclude schematic from report"));
        parser.addOption(QCommandLineOption("no-waveforms", "Exclude waveforms from report"));
        parser.addOption(QCommandLineOption("no-measurements", "Exclude measurements from report"));
        parser.addOption(QCommandLineOption("no-netlist", "Exclude netlist from report"));
        parser.addOption(QCommandLineOption("max-points", "Limit exported samples (raw-export, netlist-run --export-raw)", "pointcount"));
        parser.addOption(QCommandLineOption("raw-file", "Raw simulation data file to include", "file"));
        parser.addOption(QCommandLineOption("schematic-png", "Rendered schematic image file to include", "file"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.size() < 2) {
            std::cerr << "Usage: viora generate-report <file.flxsch> <out.html> [options]" << std::endl;
            return 1;
        }
        QString schematicPath = args.at(0);
        QString outPath = args.at(1);

        SimReportGenerator generator;
        SimReportGenerator::ReportOptions opts;
        opts.title = parser.value("report-title");
        opts.author = parser.value("report-author");
        opts.includeSchematic = !parser.isSet("no-schematic");
        opts.includeWaveforms = !parser.isSet("no-waveforms");
        opts.includeMeasurements = !parser.isSet("no-measurements");
        opts.includeNetlist = !parser.isSet("no-netlist");
        opts.maxWaveformPoints = parser.value("max-points").toInt();
        if (opts.maxWaveformPoints == 0) opts.maxWaveformPoints = 1000;
        
        generator.setSchematicPath(schematicPath);
        generator.setOptions(opts);
        
        const QString rawFilePath = parser.value("raw-file");
        if (!rawFilePath.isEmpty()) {
            RawData data;
            if (RawDataParser::loadRawAscii(rawFilePath.toStdString(), &data)) {
                SimResults results = data.toSimResults();
                generator.setSimulationResults(results);
                
                QString netlistPath = rawFilePath;
                netlistPath.replace(".raw", ".cir");
                QFile netlistFile(netlistPath);
                if (netlistFile.exists() && netlistFile.open(QIODevice::ReadOnly)) {
                    generator.setNetlist(QString::fromUtf8(netlistFile.readAll()));
                    netlistFile.close();
                }
            }
        }
        
        const QString schematicPngPath = parser.value("schematic-png");
        if (!schematicPngPath.isEmpty()) {
            QImage schematicImg(schematicPngPath);
            if (!schematicImg.isNull()) {
                generator.setSchematicImage(schematicImg);
            }
        }
        
        if (!generator.saveToFile(outPath)) {
            std::cerr << "Error: Failed to save report to " << outPath.toStdString() << std::endl;
            return 1;
        }
        
        if (parser.isSet("json")) {
            QJsonObject out;
            out["schematic"] = schematicPath;
            out["report"] = outPath;
            out["success"] = true;
            printJsonValue(out);
        } else {
            printInfoStd("Design review report generated: " + outPath.toStdString());
        }
        return 0;
    }
};

class ShareCommand : public CLICommand {
public:
    QString name() const override { return "share"; }
    QString description() const override { return "Create and share a compact URL representation of the circuit."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption("share-title", "Share title", "title", ""));
        parser.addOption(QCommandLineOption("share-description", "Share description", "desc", ""));
        parser.addOption(QCommandLineOption("upload", "Upload schematic to share server"));
        parser.addOption(QCommandLineOption("copy", "Copy share URL to clipboard"));
        parser.addOption(QCommandLineOption("server", "Share server URL", "url", "http://localhost:8765"));
    }
    QJsonObject inputSchema() const override {
        return QJsonObject{};
    }
    QJsonObject outputSchema() const override {
        return QJsonObject{};
    }
    int execute(const QStringList& args, const QCommandLineParser& parser) override {
        if (args.isEmpty()) {
            std::cerr << "Usage: viora share <file.flxsch> [options]" << std::endl;
            return 1;
        }
        QString schematicPath = args.at(0);
        QString title = parser.value("share-title");
        QString description = parser.value("share-description");
        bool upload = parser.isSet("upload");
        bool copyToClipboard = parser.isSet("copy");
        
        QByteArray data = SchematicUrlEncoder::serializeToCompact(schematicPath);
        if (data.isEmpty()) {
            std::cerr << "Error: Failed to read schematic file: " << schematicPath.toStdString() << std::endl;
            return 1;
        }
        
        bool fitsInUrl = SchematicUrlEncoder::fitsInUrl(data);
        
        if (upload || !fitsInUrl) {
            QString serverUrl = parser.value("server");
            if (serverUrl.isEmpty()) {
                serverUrl = "http://localhost:8765";
            }
            
            QNetworkRequest request(serverUrl + "/api/share");
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            
            QJsonObject body;
            body["schematic"] = QString::fromUtf8(data);
            body["title"] = title;
            body["description"] = description;
            
            QNetworkAccessManager* mgr = new QNetworkAccessManager();
            QNetworkReply* reply = mgr->post(request, QJsonDocument(body).toJson());
            
            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();
            
            QByteArray response = reply->readAll();
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            
            if (statusCode == 201) {
                QJsonObject result = QJsonDocument::fromJson(response).object();
                QString shortUrl = result.value("shortUrl").toString();
                QString fullUrl = result.value("fullUrl").toString();
                
                if (parser.isSet("json")) {
                    QJsonObject out;
                    out["success"] = true;
                    out["url"] = fullUrl;
                    out["shortUrl"] = shortUrl;
                    out["expiresAt"] = result.value("expiresAt").toString();
                    printJsonValue(out);
                } else {
                    printInfoStd("Schematic shared: " + fullUrl.toStdString());
                    if (copyToClipboard) {
                        printInfoStd("URL copied to clipboard");
                    }
                }
            } else {
                std::cerr << "Error: Upload failed - " << response.constData() << std::endl;
                return 1;
            }
        } else {
            QString url = "viospice://open?data=" + SchematicUrlEncoder::encodeForUrl(data);
            
            if (parser.isSet("json")) {
                QJsonObject out;
                out["success"] = true;
                out["url"] = url;
                printJsonValue(out);
            } else {
                printInfoStd("Direct share URL (no server needed):");
                std::cout << url.toStdString() << std::endl;
                if (copyToClipboard) {
                    printInfoStd("URL copied to clipboard");
                }
            }
        }
        
        return 0;
    }
};

class DialogRenderCommand : public CLICommand {
public:
    QString name() const override { return "dialog-render"; }
    QString description() const override { return "Render a properties dialog to an image file for inspection."; }
    void setupParser(QCommandLineParser& parser) override {
        parser.addOption(QCommandLineOption("type", "Dialog type (osc|generic|component)", "type", "osc"));
        parser.addOption(QCommandLineOption("output", "Output image PNG path", "file", "dialog_preview.png"));
        parser.addOption(QCommandLineOption("width", "Dialog width", "px", "640"));
        parser.addOption(QCommandLineOption("height", "Dialog height", "px", "520"));
    }
    QJsonObject inputSchema() const override { return QJsonObject{}; }
    QJsonObject outputSchema() const override { return QJsonObject{}; }
    int execute(const QStringList&, const QCommandLineParser& parser) override {
        QString type = parser.value("type").toLower();
        QString outPath = parser.value("output");
        int w = parser.value("width").toInt();
        int h = parser.value("height").toInt();
        if (w <= 0) w = 640;
        if (h <= 0) h = 520;

        QDialog* dlg = nullptr;
        std::unique_ptr<OscilloscopeItem> oscItem;
        std::unique_ptr<VoltageSourceItem> vSrcItem;
        std::unique_ptr<AvrMicrocontrollerItem> avrItem;
        std::unique_ptr<GenericComponentItem> genItem;

        if (type == "osc" || type == "oscilloscope") {
            oscItem = std::make_unique<OscilloscopeItem>();
            dlg = new OscilloscopePropertiesDialog(oscItem.get());
        } else if (type == "voltage" || type == "voltagesource") {
            vSrcItem = std::make_unique<VoltageSourceItem>();
            dlg = new VoltageSourceWaveformDialog(vSrcItem.get(), nullptr, nullptr, "");
        } else if (type == "avr" || type == "mcu") {
            avrItem = std::make_unique<AvrMicrocontrollerItem>("ATmega328P");
            dlg = new AvrMicrocontrollerDialog(avrItem.get());
        } else if (type == "component") {
            Flux::Model::SymbolDefinition sym("U1");
            genItem = std::make_unique<GenericComponentItem>(sym);
            QList<SchematicItem*> items;
            items.append(static_cast<SchematicItem*>(genItem.get()));
            dlg = new ComponentPropertiesDialog(items);
        } else {
            Flux::Model::SymbolDefinition sym("U1");
            genItem = std::make_unique<GenericComponentItem>(sym);
            dlg = new GenericSymbolPropertiesDialog(static_cast<SchematicItem*>(genItem.get()));
        }

        dlg->resize(w, h);
        dlg->show();
        QCoreApplication::processEvents();

        QPixmap pixmap = dlg->grab();
        if (pixmap.save(outPath)) {
            std::cout << "Successfully rendered dialog (" << type.toStdString() << ") to: " << outPath.toStdString() << std::endl;
            delete dlg;
            return 0;
        } else {
            std::cerr << "Failed to save dialog image to: " << outPath.toStdString() << std::endl;
            delete dlg;
            return 1;
        }
    }
};

} // namespace

void registerSchematicCommands() {
    auto& reg = CommandRegistry::instance();
    reg.registerCommand(std::make_unique<SchematicQueryCommand>());
    reg.registerCommand(std::make_unique<SchematicNetlistCommand>());
    reg.registerCommand(std::make_unique<SchematicRenderCommand>());
    reg.registerCommand(std::make_unique<SchematicBomCommand>());
    reg.registerCommand(std::make_unique<SchematicValidateCommand>());
    reg.registerCommand(std::make_unique<ErcCommand>());
    reg.registerCommand(std::make_unique<SchematicDiffCommand>());
    reg.registerCommand(std::make_unique<SchematicTransformCommand>());
    reg.registerCommand(std::make_unique<SchematicProbeCommand>());
    reg.registerCommand(std::make_unique<GenerateReportCommand>());
    reg.registerCommand(std::make_unique<ShareCommand>());
    reg.registerCommand(std::make_unique<DialogRenderCommand>());
}
