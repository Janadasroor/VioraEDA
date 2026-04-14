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

    PyRun_SimpleString(
        "import sys, os\n"
        "_p = os.path.expanduser('~/.local/lib/python3.12/site-packages')\n"
        "if _p not in sys.path:\n"
        "    sys.path.insert(0, _p)\n"
    );

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
