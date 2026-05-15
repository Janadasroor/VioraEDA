#ifndef FLUX_QT_BRIDGE_H
#define FLUX_QT_BRIDGE_H

#include <QObject>
#include <QPointer>
#include <QHash>
#include <QVariant>
#include <mutex>

/**
 * @brief Clean bridge between the JIT-compiled FluxScript and the Qt Object Model.
 * 
 * This class follows the Handle-Registry pattern:
 * 1. FluxScript receives a 'double' handle (bit-casted pointer).
 * 2. FluxQtBridge validates these handles before any operation.
 * 3. Property access is delegated via Qt's MetaObject system.
 */
class FluxQtBridge : public QObject {
    Q_OBJECT
public:
    static FluxQtBridge& instance();

    /**
     * @brief Registers an existing QObject with the bridge.
     * @return A bit-casted double handle for FluxScript.
     */
    double registerObject(QObject* obj);

    /**
     * @brief Safely retrieves a QObject from a handle.
     */
    QObject* resolveHandle(double handle) const;

    /**
     * @brief Bridge functions for the FluxScript runtime hooks.
     */
    static double getProperty(double handle, const char* name);
    static void setProperty(double handle, const char* name, double value);

    /**
     * @brief Connects a Qt signal to a FluxScript function handle.
     */
    void connectSignal(double handle, const char* signal, double functionHandle);

    /**
     * @brief Connects a Qt signal to a FluxScript function by name.
     */
    void connectSignalByName(double handle, const char* signal, const char* functionName);

public Q_SLOTS:
    void onBridgeEvent();

private:
    explicit FluxQtBridge(QObject* parent = nullptr);
    ~FluxQtBridge() = default;

    mutable std::mutex m_mutex;
    QHash<void*, QPointer<QObject>> m_registry;
    QHash<QObject*, QString> m_signalNameMap;
};

#endif // FLUX_QT_BRIDGE_H
