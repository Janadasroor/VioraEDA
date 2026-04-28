#ifndef VIOSPICE_SIMULATOR_BRIDGE_SLANG_MANAGER_H
#define VIOSPICE_SIMULATOR_BRIDGE_SLANG_MANAGER_H

#include <QString>
#include <QList>

class SlangManager {
public:
    static SlangManager& instance();

    struct PortInfo {
        QString name;
        int width;
        bool isInput;
    };

    // Parse the SystemVerilog source and extract port information
    QList<PortInfo> extractPorts(const QString& svSource, const QString& moduleName, QString* error = nullptr);

    // Generate C++ JIT code from the SystemVerilog module
    QString translateToCpp(const QString& svSource, const QString& moduleName, QString* error = nullptr);

private:
    SlangManager() = default;
    ~SlangManager() = default;
    SlangManager(const SlangManager&) = delete;
    SlangManager& operator=(const SlangManager&) = delete;
};

#endif // VIOSPICE_SIMULATOR_BRIDGE_SLANG_MANAGER_H
