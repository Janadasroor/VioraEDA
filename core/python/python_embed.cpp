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
    // Compute the path to the VioSpice python/ directory
    // AppImage layout: /usr/bin/viospice -> /usr/python/
    // Dev layout: /build/viospice -> /python/
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        QDir(appDir).absoluteFilePath("../python"),  // AppImage
        QDir(appDir).absoluteFilePath("../../python"), // Dev build/
        QDir::current().absoluteFilePath("python")   // Fallback
    };

    QString vspicePythonDir;
    for (const QString& cand : candidates) {
        if (QDir(cand + "/vspice").exists()) {
            vspicePythonDir = cand;
            break;
        }
    }

    // Add user site-packages dynamically using the site module
    PyRun_SimpleString(
        "import sys, os, site\n"
        "_p = site.getusersitepackages()\n"
        "if _p not in sys.path:\n"
        "    sys.path.insert(0, _p)\n"
        "del _p\n"
    );

    // If we are in an AppImage, also add bundled site-packages
    QString bundledSitePackages = QDir(vspicePythonDir).absoluteFilePath("site-packages");
    if (QDir(bundledSitePackages).exists()) {
        QString pyCode = QString(
            "import sys\n"
            "_sp = '%1'\n"
            "if _sp not in sys.path:\n"
            "    sys.path.append(_sp)\n"
            "del _sp\n"
        ).arg(bundledSitePackages);
        PyRun_SimpleString(pyCode.toUtf8().constData());
        qDebug() << "Added bundled site-packages:" << bundledSitePackages;
    }

    // Add the computed VioSpice python/ directory to sys.path
    if (!vspicePythonDir.isEmpty()) {
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
        qWarning() << "[EmbeddedPython] Could not find VioSpice python/ directory!";
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
