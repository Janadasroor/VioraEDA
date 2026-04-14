#include "ui/splash_screen.h"
#include "core/theme_manager.h"
#include "core/recent_projects.h"
#include "core/ws_server.h"
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
#include "core/flux_script_engine.h"
#include "pcb/editor/mainwindow.h"
#include "pcb/factories/pcb_item_registry.h"
#include "pcb/tools/pcb_tool_registry_builtin.h"
#include "footprints/footprint_library.h"


#include <QApplication>
#include <QIcon>
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

// ---------------------------------------------------------------------------
// Embedded Python — initialized at startup so `import vspice` works inside
// the GUI process (Blender-style integration).
// ---------------------------------------------------------------------------
#ifdef HAVE_PYTHON
#include <Python.h>

static void initEmbeddedPython() {
    // Prevent signal handlers from interfering with Qt
    PyConfig config;
    PyConfig_InitIsolatedConfig(&config);

    // Use system Python — not isolated, so site-packages (numpy, etc.) work
    config.site_import = 1;
    config.user_site_directory = 1;
    config.write_bytecode = 0;  // Don't write .pyc in app dirs

    PyStatus status = Py_InitializeFromConfig(&config);
    if (PyStatus_Exception(status)) {
        qWarning() << "Embedded Python failed to initialize:"
                   << PyStatus_IsError(status) ? "error" : "exit";
        PyConfig_Clear(&config);
        return;
    }
    PyConfig_Clear(&config);

    // Ensure the vspice package is on the path
    // The .so lives in build/python/vspice/_core.so; the .py files are in
    // python/vspice/ (symlinked to user site-packages).
    PyRun_SimpleString(
        "import sys, os\n"
        "# Add project python/ directory so 'import vspice' works\n"
        "_p = os.path.expanduser('~/.local/lib/python3.12/site-packages')\n"
        "if _p not in sys.path:\n"
        "    sys.path.insert(0, _p)\n"
    );

    qDebug() << "Embedded Python initialized — `import vspice` available from within the app";
}

static void shutdownEmbeddedPython() {
    if (Py_IsInitialized()) {
        Py_Finalize();
    }
}
#else
static void initEmbeddedPython() {}
static void shutdownEmbeddedPython() {}
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Initialize global theme
    ThemeManager::instance();
    
    // Initialize FluxScript Simulation Bridge
    initializeFluxSimBridge();
    FluxScriptEngine::instance().initialize();


    a.setApplicationName("viospice");
    a.setApplicationVersion("0.1.0");
    a.setOrganizationName("VIO");
    a.setWindowIcon(QIcon(":/icons/logo_viospice.png"));

    // --- Single Instance / Command Line Argument Handling ---
    QString serverName = "viospice_instance_server";
    QString fileToOpen;
    bool showHelp = false;
    bool showVersion = false;

    for (int i = 1; i < argc; ++i) {
        QString arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            showHelp = true;
        } else if (arg == "--version" || arg == "-v") {
            showVersion = true;
        } else if (!arg.startsWith("-")) {
            fileToOpen = QFileInfo(arg).absoluteFilePath();
        }
    }

    if (showHelp) {
        printf("viospice - High Performance Circuit Simulator\n");
        printf("Usage: viospice [options] [file]\n\n");
        printf("Options:\n");
        printf("  -h, --help     Show this help message\n");
        printf("  -v, --version  Show version information\n");
        return 0;
    }

    if (showVersion) {
        printf("viospice version 0.1.0\n");
        return 0;
    }

    // Try to connect to an existing instance
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(500)) {
        if (!fileToOpen.isEmpty()) {
            socket.write(fileToOpen.toUtf8());
            socket.waitForBytesWritten();
        }
        return 0;
    }

    // No existing instance, start server for this one
    QLocalServer* server = new QLocalServer(&a);
    QLocalServer::removeServer(serverName);
    server->listen(serverName);

    // Start WebSocket state bridge server
#if VIOSPICE_HAS_QT_WEBSOCKETS
    WsServer* wsServer = new WsServer(18420, &a);
    WsServer::setInstance(wsServer);
    wsServer->start();
#endif

    // Show splash screen
    SplashScreen* splash = new SplashScreen();
    splash->show();
    a.processEvents();

    // Initialize systems
    SchematicItemRegistry::registerBuiltInItems();
    SchematicToolRegistryBuiltIn::registerBuiltInTools();
    PCBItemRegistry::registerBuiltInItems();
    PCBToolRegistryBuiltIn::registerBuiltInTools();

    // Initialize symbol, footprint, and model libraries
    SymbolLibraryManager::instance();
    FootprintLibraryManager::instance();

    // Set up loading logic
    auto startMainApp = [&a, &fileToOpen, server](SplashScreen* s) {
        auto openFile = [](const QString& path) {
            QFileInfo fi(path);
            QString ext = fi.suffix().toLower();

            if (ext == "cir" || ext == "sp" || ext == "spice" || ext == "txt") {
                NetlistEditor* netEditor = new NetlistEditor();
                netEditor->setAttribute(Qt::WA_DeleteOnClose);
                QFile file(path);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    netEditor->setNetlist(in.readAll());
                    file.close();
                }
                netEditor->show();
                return (QWidget*)netEditor;
            } else if (ext == "sclib" || ext == "kicad_sym" || ext == "asy") {
                SymbolEditor* symbolEditor = new SymbolEditor();
                symbolEditor->setAttribute(Qt::WA_DeleteOnClose);
                if (ext == "kicad_sym") {
                    symbolEditor->importKicadSymbol(path);
                } else if (ext == "asy") {
                    symbolEditor->importLtspiceSymbol(path);
                } else if (ext == "sclib") {
                    symbolEditor->loadLibrary(path);
                }
                symbolEditor->show();
                return (QWidget*)symbolEditor;
            } else if (ext == "csv") {
                CsvViewer* viewer = new CsvViewer();
                viewer->setAttribute(Qt::WA_DeleteOnClose);
                viewer->loadFile(path);
                viewer->show();
                return (QWidget*)viewer;
            } else if (ext == "pcb" || ext == "kicad_pcb" || ext == "pcbdoc") {
                MainWindow* pcbEditor = new MainWindow();
                pcbEditor->setAttribute(Qt::WA_DeleteOnClose);
                pcbEditor->openFile(path);
                pcbEditor->show();
                return (QWidget*)pcbEditor;
            } else if (ext == "fp" || ext == "mod" || ext == "kicad_mod") {
                // Open footprint editor - would need to add loadFootprint method
                SymbolEditor* fpEditor = new SymbolEditor(); // TODO: Create FootprintEditor
                fpEditor->setAttribute(Qt::WA_DeleteOnClose);
                fpEditor->show();
                return (QWidget*)fpEditor;
            } else {
                SchematicEditor* schEditor = new SchematicEditor();
                schEditor->setAttribute(Qt::WA_DeleteOnClose);
                schEditor->openFile(path);
                schEditor->show();
                return (QWidget*)schEditor;
            }
        };

        // Connect server to open files in this instance
        QObject::connect(server, &QLocalServer::newConnection, [server, openFile]() {
            QLocalSocket* clientSocket = server->nextPendingConnection();
            QObject::connect(clientSocket, &QLocalSocket::readyRead, [clientSocket, openFile]() {
                QString path = QString::fromUtf8(clientSocket->readAll());
                if (!path.isEmpty()) {
                    QWidget* w = openFile(path);
                    if (w) {
                        w->raise();
                        w->activateWindow();
                    }
                }
                clientSocket->disconnectFromServer();
            });
        });

        if (!fileToOpen.isEmpty()) {
            openFile(fileToOpen);
        } else {
            ProjectManager* projectManager = new ProjectManager;
            projectManager->setAttribute(Qt::WA_DeleteOnClose);
            projectManager->show();
        }
        
        s->deleteLater();
    };

    // Background loading
    auto updateSplash = [splash](const QString& status, int progress, int total) {
        splash->setStatus(status);
        splash->setProgress(progress, total);
    };

    QObject::connect(&SymbolLibraryManager::instance(), &SymbolLibraryManager::progressUpdated, splash, updateSplash);
    QObject::connect(&ModelLibraryManager::instance(), &ModelLibraryManager::progressUpdated, splash, updateSplash);
    QObject::connect(&FootprintLibraryManager::instance(), &FootprintLibraryManager::progressUpdated, splash, updateSplash);

    QtConcurrent::run([splash, startMainApp]() {
        // Load symbols first because startup UI depends on them. Model and footprint
        // scans can be much larger, so do not block the splash on those.
        SymbolLibraryManager::instance().loadUserLibraries(QDir::homePath() + "/ViospiceLib/sym", true);

        QMetaObject::invokeMethod(qApp, [startMainApp, splash]() {
            startMainApp(splash);

            // Start UI Command Server for Python integration
            auto& uiServer = UICommandServer::instance();
            if (uiServer.start(18790)) {
                qDebug() << "UI Command Server started on port 18790 — Python scripts can connect via vspice.ui";

                // Connect UI hooks
                uiServer.setShowMessageFn([](const QString& title, const QString& text) {
                    QMessageBox::information(nullptr, title, text);
                });

                uiServer.setGetSchematicContextFn([]() -> QVariantMap {
                    QVariantMap ctx;
                    ctx["has_schematic"] = false;
                    ctx["message"] = "Schematic context integration pending";
                    return ctx;
                });

                uiServer.setRunPythonCodeFn([](const QString& code) -> QVariantMap {
                    QVariantMap result;
                    result["ok"] = true;
                    result["message"] = "Python code execution via external client (code not executed in GUI process)";
                    result["code_length"] = code.length();
                    return result;
                });
            }

            QtConcurrent::run([]() {
                ModelLibraryManager::instance().reload();
                FootprintLibraryManager::instance().loadUserLibraries(QDir::homePath() + "/ViospiceLib/footprints");
            });
        }, Qt::QueuedConnection);
    });

    return a.exec();
}
