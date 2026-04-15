/**
 * @file python_embed.cpp
 * @brief Embedded Python initialization for VioSpice.
 *
 * This file is compiled separately from Qt headers to avoid macro collisions
 * between Python.h and Qt's MOC system (slots, signals, emit keywords).
 */

#ifdef HAVE_PYTHON
#include <Python.h>
#endif

#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#ifdef HAVE_PYTHON

void initEmbeddedPython() {
    PyConfig config;
    PyConfig_InitIsolatedConfig(&config);

    config.site_import = 1;
    config.user_site_directory = 1;
    config.write_bytecode = 0;

    PyStatus status = Py_InitializeFromConfig(&config);
    if (PyStatus_Exception(status)) {
        qWarning() << "Embedded Python failed to initialize:"
                   << (PyStatus_IsError(status) ? "error" : "exit");
        PyConfig_Clear(&config);
        return;
    }
    PyConfig_Clear(&config);

    // Compute the path to the VioSpice python/ directory
    // The executable is at: <project>/build/viospice
    // The python dir is at:  <project>/python/vspice/
    // So we need: <project>/python/
    QString exePath = QCoreApplication::applicationFilePath();
    QString projectDir = QFileInfo(exePath).dir().absolutePath() + "/..";
    QString vspicePythonDir = QDir(projectDir).absoluteFilePath("python");

    // Also check VIOSPICE_PYTHON env var and user site-packages
    PyRun_SimpleString(
        "import sys, os\n"
        "# Add user site-packages\n"
        "_p = os.path.expanduser('~/.local/lib/python3.12/site-packages')\n"
        "if _p not in sys.path:\n"
        "    sys.path.insert(0, _p)\n"
        "del _p\n"
    );

    // Add the computed VioSpice python/ directory to sys.path
    if (QDir(vspicePythonDir + "/vspice").exists()) {
        QString pyPath = QDir(vspicePythonDir).absolutePath();
        QString pyCode = QString(
            "import sys\n"
            "_vsp = '%1'\n"
            "if _vsp not in sys.path:\n"
            "    sys.path.insert(0, _vsp)\n"
            "del _vsp\n"
        ).arg(pyPath);
        PyRun_SimpleString(pyCode.toUtf8().constData());
        qDebug() << "VioSpice Python path added:" << pyPath;
    } else {
        qWarning() << "VioSpice python/ directory not found at:" << vspicePythonDir;
        // Try fallback: CWD-relative
        QString cwdPython = QDir::current().absoluteFilePath("python");
        if (QDir(cwdPython + "/vspice").exists()) {
            QString pyCode = QString(
                "import sys\n"
                "_vsp = '%1'\n"
                "if _vsp not in sys.path:\n"
                "    sys.path.insert(0, _vsp)\n"
                "del _vsp\n"
            ).arg(QDir(cwdPython).absolutePath());
            PyRun_SimpleString(pyCode.toUtf8().constData());
            qDebug() << "VioSpice Python path (fallback) added:" << cwdPython;
        }
    }

    qDebug() << "Embedded Python initialized — `import vspice` available from within the app";
}

void shutdownEmbeddedPython() {
    if (Py_IsInitialized()) {
        Py_Finalize();
    }
}

#else

void initEmbeddedPython() {}
void shutdownEmbeddedPython() {}

#endif
