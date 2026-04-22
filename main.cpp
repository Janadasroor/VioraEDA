#include "ui/splash_screen.h"
#include "theme_manager.h"
#include "recent_projects.h"
#include "ws_server.h"
#include "schematic/editor/schematic_editor.h"
#include "schematic/ui/netlist_editor.h"
#include "ui/csv_viewer.h"
#include "ui/project_manager.h"
#include "ui/ui_command_server.h"
#include "symbols/symbol_editor.h"
#include "schematic/factories/schematic_item_registry.h"
#include "schematic/tools/schematic_tool_registry_builtin.h"
#include "symbols/symbol_library.h"
#include "simulator/bridge/model_library_manager.h"
#include "simulator/bridge/flux_sim_bridge.h"
#include "flux_script_engine.h"
#include "pcb/editor/mainwindow.h"
#include "pcb/factories/pcb_item_registry.h"
#include "pcb/tools/pcb_tool_registry_builtin.h"
#include "footprints/footprint_library.h"
#include "simulator/bridge/sim_manager.h"

#include <QApplication>
#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QtConcurrent/QtConcurrent>
#include <QTimer>
#include <QFuture>
#include <QMessageBox>

extern void initEmbeddedPython();
extern void shutdownEmbeddedPython();

extern "C" {
#include "ui/python_hooks.h"
}

int main(int argc, char *argv[])
{
    // MANDATORY: Setup custom simulation engine environment BEFORE any engine code is loaded
    qputenv("SPICE_SCRIPTS", "/home/jnd/.viospice");
    qputenv("SPICE_LIB_DIR", "/home/jnd/.viospice");

    // Enable ASan-friendly exit behavior
    qputenv("ASAN_OPTIONS", "detect_leaks=1");

    QApplication a(argc, argv);
    initEmbeddedPython();
    ThemeManager::instance();
    initializeFluxSimBridge();
    FluxScriptEngine::instance().initialize();

    a.setApplicationName("viospice");
    a.setOrganizationName("VIO");

    QString serverName = "viospice_instance_server";
    QString fileToOpen;
    for (int i = 1; i < argc; ++i) {
        QString arg = argv[i];
        if (!arg.startsWith("-")) fileToOpen = QFileInfo(arg).absoluteFilePath();
    }

    QLocalServer* server = new QLocalServer(&a);
    QLocalServer::removeServer(serverName);
    server->listen(serverName);

    SplashScreen* splash = new SplashScreen();
    splash->show();
    a.processEvents();

    SchematicItemRegistry::registerBuiltInItems();
    SchematicToolRegistryBuiltIn::registerBuiltInTools();
    PCBItemRegistry::registerBuiltInItems();
    PCBToolRegistryBuiltIn::registerBuiltInTools();

    SymbolLibraryManager::instance();
    FootprintLibraryManager::instance();

    QMetaObject::invokeMethod(qApp, [splash, fileToOpen]() {
        if (!fileToOpen.isEmpty()) {
            SchematicEditor* sch = new SchematicEditor();
            sch->setAttribute(Qt::WA_DeleteOnClose);
            sch->openFile(fileToOpen);
            sch->show();
        } else {
            ProjectManager* pm = new ProjectManager;
            pm->setAttribute(Qt::WA_DeleteOnClose);
            pm->show();
        }
        splash->deleteLater();
    }, Qt::QueuedConnection);

    int exitCode = a.exec();

    qDebug() << "Closing all windows...";
    for (auto w : QApplication::topLevelWidgets()) { 
        w->close(); 
    }
    
    // Process events to run CloseEvents and deleteLater
    for(int i=0; i<5; ++i) QApplication::processEvents(QEventLoop::AllEvents, 100);

    // Final forced deletion of any widget orphans that didn't close
    for (auto w : QApplication::topLevelWidgets()) {
        delete w;
    }

    SchematicToolRegistryBuiltIn::cleanup();
    shutdownEmbeddedPython();
    return exitCode;
}
