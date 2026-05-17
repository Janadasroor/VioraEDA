// flux_runner — standalone FluxScript runner with Qt GUI support.
// Compiles and executes a .flux file, then enters the Qt event loop
// so that Qt widgets created by the script remain interactive.
//
// Usage:
//   flux_runner path/to/script.flux
//   flux_runner path/to/script.flux entryFunctionName
//
// The optional entry function is called after compilation.
// If omitted, only top-level code runs.

#include <QApplication>
#include <QFile>
#include <QDebug>
#include <iostream>

#include "../core/flux/engine/flux_script_engine.h"
#include <flux/jit_engine.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: flux_runner <file.flux> [entryFunction]\n";
        return 1;
    }

    QApplication app(argc, argv);

    // Initialize JIT engine and register all bridge functions
    FluxScriptEngine::instance().initialize();

    // Read the flux file
    QString path = argv[1];
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        std::cerr << "Error: cannot open " << path.toStdString() << "\n";
        return 1;
    }
    QString source = QString::fromUtf8(f.readAll());
    f.close();

    // Compile and execute top-level expressions
    QString error;
    if (!FluxScriptEngine::instance().executeString(source, &error)) {
        std::cerr << "Compile error: " << error.toStdString() << "\n";
        return 1;
    }

    // Call optional entry function
    if (argc >= 3) {
        QString entryFn = QString::fromUtf8(argv[2]);
        std::string fnErr;
        Flux::JITEngine::instance().callFunction(entryFn.toStdString(), {}, &fnErr);
        if (!fnErr.empty()) {
            qDebug() << "[flux_runner] Entry function" << entryFn << "error:" << QString::fromStdString(fnErr);
        }
    }

    // Also try __anon_expr for top-level code
    {
        std::string anonErr;
        Flux::JITEngine::instance().callFunction("__anon_expr", {}, &anonErr);
    }

    // Enter Qt event loop (keeps windows alive)
    return app.exec();
}
