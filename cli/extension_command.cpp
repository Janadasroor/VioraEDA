// extension_command.cpp — CLI subcommand for managing FluxScript extensions
// Usage: viora extension init <id> [--name NAME] [--desc DESC] [--author AUTHOR] [--template TYPE]
//        viora extension validate <dir>   — check manifest + compile
//        viora extension install <dir>    — copy to config dir

#include <iostream>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

#include "../core/flux/engine/flux_script_engine.h"

static QString configDir() {
    return QDir::homePath() + "/.config/VioraEDA/extensions";
}

static QString scaffoldManifest(const QString& id, const QString& name,
                                 const QString& author, const QString& version,
                                 const QString& desc) {
    QString displayName = name.isEmpty() ? id : name;
    QString displayAuthor = author.isEmpty() ? "" : author;
    QString displayDesc = desc.isEmpty() ? "Edit this description" : desc;
    QString displayVersion = version.isEmpty() ? "0.1.0" : version;

    return QString(R"({
  "id": "%1",
  "name": "%2",
  "version": "%3",
  "author": "%4",
  "description": "%5",
  "main": "main.flux",
  "hooks": {
    "onActivate": "on_activate"
  },
  "menu": [
    {"path": "%2", "action": "open_panel"}
  ]
}
)").arg(id, displayName, displayVersion, displayAuthor, displayDesc);
}

static QString scaffoldMain(const QString& name, const QString& templateType) {
    QString displayName = name.isEmpty() ? "Hello" : name;

    if (templateType == "empty") {
        return QString(R"(def init() {
    viora_flux_print("%1 loaded")
}

def on_activate() {
    // Called when extension is activated
}

def open_panel() {
    // Called from Extensions menu
}
)").arg(displayName);
    }

    if (templateType == "panel") {
        return QString(R"(def init() {
    viora_flux_print("%1 loaded")
}

def open_panel() {
    win = flux_qt_create_window("%1")
    box = flux_qt_create_layout("vbox")
    flux_qt_set_layout(win, box)

    lbl = flux_qt_create_label("Hello from %1!")
    flux_qt_layout_add_widget(box, lbl)

    btn = flux_qt_create_button("Click Me")
    flux_qt_layout_add_widget(box, btn)

    flux_qt_on_click_by_name(btn, "on_click")
    flux_qt_set_window_size(win, 300.0, 150.0)
}

def on_click() {
    flux_qt_msg_box("%1", "Button clicked!")
}
)").arg(displayName);
    }

    // calculator (default)
    return QString(R"(def init() {
    viora_flux_print("%1 loaded")
}

def on_calculate() {
    val1 = flux_qt_get_property(input1, "value")
    val2 = flux_qt_get_property(input2, "value")
    result = val1 + val2
    flux_qt_set_property(result_display, "value", result)
}

def open_panel() {
    win = flux_qt_create_window("%1")
    box = flux_qt_create_layout("vbox")
    flux_qt_set_layout(win, box)

    lbl1 = flux_qt_create_label("Value 1:")
    flux_qt_layout_add_widget(box, lbl1)
    input1 = flux_qt_create_spinbox()
    flux_qt_set_property(input1, "value", 0.0)
    flux_qt_layout_add_widget(box, input1)

    lbl2 = flux_qt_create_label("Value 2:")
    flux_qt_layout_add_widget(box, lbl2)
    input2 = flux_qt_create_spinbox()
    flux_qt_set_property(input2, "value", 0.0)
    flux_qt_layout_add_widget(box, input2)

    btn = flux_qt_create_button("Calculate")
    flux_qt_layout_add_widget(box, btn)

    result_label = flux_qt_create_label("Result:")
    flux_qt_layout_add_widget(box, result_label)
    result_display = flux_qt_create_spinbox()
    flux_qt_set_property(result_display, "enabled", 0.0)
    flux_qt_set_property(result_display, "readOnly", 1.0)
    flux_qt_layout_add_widget(box, result_display)

    flux_qt_on_click_by_name(btn, "on_calculate")
    flux_qt_set_window_size(win, 280.0, 250.0)
}
)").arg(displayName);
}

int cmdExtensionInit(const QStringList& args) {
    if (args.size() < 1) {
        std::cerr << "Usage: viora extension init <id> [--name NAME] [--desc DESC] [--author AUTHOR] [--version VER] [--template TYPE]\n";
        std::cerr << "  Templates: empty, panel, calculator (default: calculator)\n";
        return 1;
    }

    QString id = args[0];
    QString name, desc, author, version, templateType;

    // Parse optional flags
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--name" && i + 1 < args.size()) name = args[++i];
        else if (args[i] == "--desc" && i + 1 < args.size()) desc = args[++i];
        else if (args[i] == "--author" && i + 1 < args.size()) author = args[++i];
        else if (args[i] == "--version" && i + 1 < args.size()) version = args[++i];
        else if (args[i] == "--template" && i + 1 < args.size()) templateType = args[++i];
    }

    if (templateType.isEmpty()) templateType = "calculator";

    QDir dir(configDir() + "/" + id);
    if (dir.exists()) {
        std::cerr << "Error: extension '" << id.toStdString() << "' already exists\n";
        return 1;
    }
    if (!dir.mkpath(".")) {
        std::cerr << "Error: cannot create directory\n";
        return 1;
    }

    QFile manifest(dir.filePath("manifest.json"));
    if (manifest.open(QIODevice::WriteOnly)) {
        manifest.write(scaffoldManifest(id, name, author, version, desc).toUtf8());
    }
    QFile main(dir.filePath("main.flux"));
    if (main.open(QIODevice::WriteOnly)) {
        main.write(scaffoldMain(name.isEmpty() ? id : name, templateType).toUtf8());
    }

    std::cout << "Extension '" << id.toStdString() << "' created at "
              << dir.absolutePath().toStdString() << "\n";
    return 0;
}

int cmdExtensionValidate(const QStringList& args) {
    if (args.size() < 1) {
        std::cerr << "Usage: viora extension validate <dir>\n";
        return 1;
    }
    QDir dir(args[0]);
    if (!dir.exists()) {
        std::cerr << "Error: directory not found\n";
        return 1;
    }

    // Check manifest.json
    QFile mf(dir.filePath("manifest.json"));
    if (!mf.exists()) {
        std::cerr << "Error: manifest.json not found\n";
        return 1;
    }
    if (!mf.open(QIODevice::ReadOnly)) {
        std::cerr << "Error: cannot read manifest.json\n";
        return 1;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(mf.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        std::cerr << "Error: manifest.json parse error: "
                  << err.errorString().toStdString() << "\n";
        return 1;
    }
    QJsonObject obj = doc.object();
    QString id = obj["id"].toString();
    if (id.isEmpty()) {
        std::cerr << "Error: manifest.json missing 'id'\n";
        return 1;
    }
    std::cout << "  manifest.json  ✓  (" << id.toStdString() << ")\n";

    // Check main.flux
    QFile sf(dir.filePath("main.flux"));
    if (!sf.exists()) {
        std::cout << "  main.flux      ✗  not found (optional)\n";
    } else {
        if (!sf.open(QIODevice::ReadOnly)) {
            std::cout << "  main.flux      ✗  cannot read\n";
            return 1;
        }
        QString source = QString::fromUtf8(sf.readAll());
        sf.close();

        // Compile-check using FluxScript JIT
        FluxScriptEngine::instance().initialize();
        QString error;
        if (!FluxScriptEngine::instance().executeString(source, &error)) {
            std::cout << "  main.flux      ✗  compile error\n"
                      << "    " << error.toStdString() << "\n";
            return 1;
        }
        std::cout << "  main.flux      ✓  compiles OK\n";
    }

    std::cout << "Extension '" << id.toStdString() << "' is valid\n";
    return 0;
}

int cmdExtensionInstall(const QStringList& args) {
    if (args.size() < 1) {
        std::cerr << "Usage: viora extension install <dir>\n";
        return 1;
    }
    QDir src(args[0]);
    if (!src.exists()) {
        std::cerr << "Error: source directory not found\n";
        return 1;
    }

    // Read manifest to get the extension ID
    QFile mf(src.filePath("manifest.json"));
    if (!mf.open(QIODevice::ReadOnly)) {
        std::cerr << "Error: manifest.json not found\n";
        return 1;
    }
    QString id = QJsonDocument::fromJson(mf.readAll()).object()["id"].toString();
    mf.close();
    if (id.isEmpty()) {
        std::cerr << "Error: manifest.json missing 'id'\n";
        return 1;
    }

    QDir dst(configDir() + "/" + id);
    if (dst.exists()) {
        std::cerr << "Error: extension '" << id.toStdString()
                  << "' already installed at " << dst.absolutePath().toStdString() << "\n";
        return 1;
    }
    if (!dst.mkpath(".")) {
        std::cerr << "Error: cannot create destination\n";
        return 1;
    }

    // Copy files
    for (const auto& f : src.entryList(QDir::Files | QDir::NoDotAndDotDot)) {
        QFile::copy(src.filePath(f), dst.filePath(f));
    }

    std::cout << "Installed '" << id.toStdString() << "' to "
              << dst.absolutePath().toStdString() << "\n";
    return 0;
}
