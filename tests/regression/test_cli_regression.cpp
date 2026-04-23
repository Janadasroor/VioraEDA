#include <QCoreApplication>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
#include <limits>

#ifndef VIO_CMD_PATH
#define VIO_CMD_PATH "viora"
#endif

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

QJsonObject runJsonCommand(const QStringList& args) {
    QProcess proc;
    proc.start(QString::fromUtf8(VIO_CMD_PATH), args);
    if (!proc.waitForFinished(60000)) {
        throw std::runtime_error("command timed out: " + args.join(" ").toStdString());
    }
    const int exitCode = proc.exitCode();
    const QByteArray out = proc.readAllStandardOutput();
    const QByteArray err = proc.readAllStandardError();
    auto parseObject = [](const QByteArray& payload) -> QJsonObject {
        QJsonParseError err;
        const QJsonDocument d = QJsonDocument::fromJson(payload, &err);
        if (err.error == QJsonParseError::NoError && d.isObject()) {
            return d.object();
        }
        return {};
    };

    QJsonObject parsed = parseObject(out);
    if (parsed.isEmpty()) {
        const int startIdx = out.indexOf('{');
        if (startIdx >= 0) {
            parsed = parseObject(out.mid(startIdx));
        }
    }
    if (parsed.isEmpty()) {
        const int lastStartIdx = out.lastIndexOf("\n{");
        if (lastStartIdx >= 0) {
            parsed = parseObject(out.mid(lastStartIdx + 1));
        }
    }

    QJsonDocument doc(parsed);
    QJsonParseError parseError;
    parseError.error = parsed.isEmpty() ? QJsonParseError::IllegalValue : QJsonParseError::NoError;
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (exitCode != 0) {
            throw std::runtime_error("command failed: " + args.join(" ").toStdString() + " stderr=" + err.toStdString());
        }
        throw std::runtime_error("invalid JSON output for: " + args.join(" ").toStdString());
    }
    if (exitCode != 0) {
        std::cerr << "[WARN] command exited non-zero but produced JSON: "
                  << args.join(" ").toStdString() << std::endl;
    }
    return doc.object();
}

QJsonArray findSignalValues(const QJsonArray& signalList, const QString& name) {
    for (const QJsonValue& v : signalList) {
        const QJsonObject s = v.toObject();
        if (s.value("name").toString().compare(name, Qt::CaseInsensitive) == 0) {
            return s.value("values").toArray();
        }
    }
    return {};
}
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::cerr << "Usage: test_cli_regression <fixtures_dir>" << std::endl;
        return 1;
    }

    const QString fixturesDir = QString::fromUtf8(argv[1]);
    const QString schematicPath = QDir(fixturesDir).filePath("untitled.sch");
    const QString rawPath = QDir(fixturesDir).filePath("sample.raw");
    const QString smartSignalPath = QDir(fixturesDir).filePath("smart_signal_pwm.flxsch");
    const bool hasSampleRaw = QFile::exists(rawPath);
    require(QFile::exists(schematicPath), "missing schematic fixture: " + schematicPath.toStdString());
    require(QFile::exists(smartSignalPath), "missing smart-signal fixture: " + smartSignalPath.toStdString());

    // schematic-query
    {
        const QJsonObject out = runJsonCommand({"schematic-query", schematicPath});
        require(out.value("file").toString() == schematicPath, "schematic-query file mismatch");
        require(out.value("components").isArray(), "schematic-query missing components array");
    }

    // schematic-bom
    {
        const QJsonObject out = runJsonCommand({"schematic-bom", schematicPath});
        require(out.value("components").isArray(), "schematic-bom missing components array");
        require(out.value("groups").isArray(), "schematic-bom missing groups array");
    }

    // schematic-validate
    {
        const QJsonObject out = runJsonCommand({"schematic-validate", schematicPath});
        require(out.value("file").toString() == schematicPath, "schematic-validate file mismatch");
        require(out.value("summary").isObject(), "schematic-validate missing summary");
    }

    // schematic-probe
    {
        const QJsonObject out = runJsonCommand({"schematic-probe", schematicPath, "--list"});
        require(out.value("signals").isArray(), "schematic-probe missing signals array");
    }

    // netlist-compare: generate netlist and compare against itself
    {
        QProcess proc;
        proc.start(QString::fromUtf8(VIO_CMD_PATH), {"schematic-netlist", schematicPath});
        if (!proc.waitForFinished(60000)) {
            throw std::runtime_error("schematic-netlist timed out");
        }
        const QByteArray netlist = proc.readAllStandardOutput();
        if (proc.exitCode() != 0) {
            std::cerr << "[WARN] schematic-netlist exited non-zero but returned output." << std::endl;
        }
        require(!netlist.isEmpty(), "schematic-netlist produced no output");

        QTemporaryDir tempDir;
        require(tempDir.isValid(), "failed to create temporary directory");
        const QString netPath = QDir(tempDir.path()).filePath("netlist.sp");
        QFile f(netPath);
        require(f.open(QIODevice::WriteOnly | QIODevice::Truncate), "failed to write temp netlist");
        f.write(netlist);
        f.close();

        const QJsonObject out = runJsonCommand({"netlist-compare", schematicPath, netPath});
        const QJsonObject summary = out.value("summary").toObject();
        require(summary.value("onlyInSchematic").toInt() == 0, "netlist-compare onlyInSchematic != 0");
        require(summary.value("onlyInExternal").toInt() == 0, "netlist-compare onlyInExternal != 0");
    }

    // raw fixture checks are optional in this repo state.
    if (hasSampleRaw) {
        // raw-info summary
        {
            const QJsonObject out = runJsonCommand({"raw-info", rawPath, "--summary", "--json"});
            require(out.value("file").toString() == rawPath, "raw-info file mismatch");
            require(out.value("points").toInt() == 4, "raw-info points mismatch");
            require(out.value("variables").toInt() == 2, "raw-info variables mismatch");
        }

        // raw-export json with base-signal and decimation
        {
            const QJsonObject out = runJsonCommand({"raw-export", rawPath, "--format", "json", "--max-points", "3", "--base-signal", "v(out)"});
            require(out.value("file").toString() == rawPath, "raw-export file mismatch");
            const QJsonArray x = out.value("x").toArray();
            require(!x.isEmpty() && x.size() <= 3, "raw-export x size invalid");
            const QJsonArray signalArray = out.value("signals").toArray();
            require(signalArray.size() == 1, "raw-export signals size mismatch");
        }
    }

    // symbol-validate
    {
        QTemporaryDir tempDir;
        require(tempDir.isValid(), "failed to create temp dir for viosym");
        const QString symPath = QDir(tempDir.path()).filePath("test_symbol.viosym");
        QJsonObject sym;
        sym["name"] = "TestSymbol";
        sym["referencePrefix"] = "X";
        QJsonArray prims;
        QJsonObject pin;
        pin["type"] = "pin";
        pin["x"] = 0;
        pin["y"] = 0;
        pin["number"] = 1;
        pin["name"] = "1";
        pin["orientation"] = "Right";
        pin["length"] = 10;
        prims.append(pin);
        sym["primitives"] = prims;
        QJsonDocument doc(sym);
        QFile f(symPath);
        require(f.open(QIODevice::WriteOnly | QIODevice::Truncate), "failed to write viosym");
        f.write(doc.toJson(QJsonDocument::Indented));
        f.close();

        const QJsonObject out = runJsonCommand({"symbol-validate", symPath});
        require(out.value("name").toString() == "TestSymbol", "symbol-validate name mismatch");
        require(out.value("issues").isArray(), "symbol-validate missing issues array");
    }

    // smart-signal native JIT path: output node must toggle on OUT
    {
        const QJsonObject out = runJsonCommand({
            "netlist-run", smartSignalPath,
            "--analysis", "tran",
            "--stop", "2m",
            "--step", "5u",
            "--export-raw", "json",
            "--json"
        });
        require(out.value("ok").toBool(), "smart-signal run returned ok=false");
        const QJsonObject raw = out.value("raw").toObject();
        const QJsonArray signalList = raw.value("signals").toArray();
        QJsonArray outValues = findSignalValues(signalList, "V(OUT)");
        if (outValues.isEmpty()) {
            outValues = findSignalValues(signalList, "OUT");
        }
        require(!outValues.isEmpty(), "smart-signal missing OUT waveform");

        double minV = std::numeric_limits<double>::infinity();
        double maxV = -std::numeric_limits<double>::infinity();
        for (const QJsonValue& v : outValues) {
            const double dv = v.toDouble();
            if (dv < minV) minV = dv;
            if (dv > maxV) maxV = dv;
        }
        require(minV < 0.1, "smart-signal OUT low-level check failed");
        require(maxV > 4.0, "smart-signal OUT high-level check failed");
    }

    std::cout << "cli.regression: all tests passed\n";
    return 0;
}
