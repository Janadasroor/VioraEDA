#ifndef PATH_MANAGER_H
#define PATH_MANAGER_H

#include <QDir>
#include <QStandardPaths>

class PathManager {
public:
    static QString baseLibraryPath() {
#ifdef Q_OS_WIN
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Viospice";
#else
        return QDir::homePath() + "/.local/share/viospice";
#endif
    }

    static QString symbolLibraryPath() {
        return baseLibraryPath() + "/symbols";
    }

    static QString subcircuitLibraryPath() {
        return baseLibraryPath() + "/subcircuits";
    }
};

#endif // PATH_MANAGER_H
