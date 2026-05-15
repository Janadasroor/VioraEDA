// extension_command.cpp — CLI subcommand for managing FluxScript extensions
// Usage: viora extension init <name>     — scaffold a new extension
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

static QString scaffoldManifest(const QString& id) {
    return QString(R"({
  "id": "%1",
  "name": "%1",
  "version": "0.1.0",
  "description": "Edit this description",
  "hooks": {
    "onActivate": "on_activate"
  },
  "menu": [
    {"path": "%1", "action": "open_panel"}
  ]
}
)").arg(id);
}

static QString scaffoldMain() {
    return QStringLiteral(R"(def on_activate() {
}

def open_panel() {
    win = flux_qt_create_window("Hello")
    lbl = flux_qt_create_label("Your extension here")
    flux_qt_add_widget(win, lbl)
}
)");
}

int cmdExtensionInit(const QStringList& args) {
    if (args.size() < 1) {
        std::cerr << "Usage: viora extension init <name>\n";
        return 1;
    }
    QString id = args[0];
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
        manifest.write(scaffoldManifest(id).toUtf8());
    }
    QFile main(dir.filePath("main.flux"));
    if (main.open(QIODevice::WriteOnly)) {
        main.write(scaffoldMain().toUtf8());
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
