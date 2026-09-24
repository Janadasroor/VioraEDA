/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SIMULATION_MANAGER_H
#define SIMULATION_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QPair>
#include <QTimer>
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <deque>
#include <chrono>

#ifdef HAVE_NGSPICE
#include <ngspice/sharedspice.h>
#endif

#include "simulation_types.h"
#include <condition_variable>
#include <mutex>

class SimControl;
class SimulationManager;

/**
 * @brief Manages interaction with the Ngspice simulation engine
 */
// Background Command Worker
class CommandWorker : public QObject {
    Q_OBJECT
public:
    explicit CommandWorker(QObject* parent = nullptr) : QObject(parent) {}
    void setManager(SimulationManager* m) { m_manager = m; }
public slots:
    void execute(const QString& cmd);
    void executeSequence(const QStringList& cmds);
    void loadCircuit(char** deck);
private:
    SimulationManager* m_manager = nullptr;
};

/**
 * @brief Represents the current operational state of the simulation engine.
 */
enum class SimulationState {
    Idle,           ///< No simulation loaded or running
    Loading,        ///< Circuit netlist is being loaded/parsed
    Running,        ///< Simulation background thread is active and producing data
    Halted,         ///< Simulation is paused at a sync point for interaction
    Paused,         ///< Simulation is manually paused by the user
    Stopping,       ///< Shutdown sequence in progress
    Finished,       ///< Simulation completed naturally
    Error           ///< Simulation failed or crashed
};

class SimulationManager : public QObject {
    Q_OBJECT
    friend class CommandWorker;
public:
    static SimulationManager& instance();

    bool isAvailable() const;
    bool supportsNativeLogicADevices() const;
    bool isNativeSmartSignalMode() const;
    void initialize();
    void runSimulation(const QString& netlist, SimControl* control = nullptr);
    bool validateNetlist(const QString& netlist, QString* errorOut = nullptr);
    QString lastErrorMessage() const;
    void stopSimulation();
    void shutdown();

    // --- Dynamic Interaction API ---
    double getVectorValue(const QString& name);
    QPair<QVector<double>, QVector<double>> getVectorHistory(const QString& name);
    void setParameter(const QString& name, double value);
    void sendInternalCommand(const QString& command);

    // --- Real-Time Switch Control ---
    void alterSwitch(const QString& switchRef, bool open, double vt = 0.5, double vh = 0.1);
    void alterSwitchResistance(const QString& resistorName, double resistance);
    void alterSwitchVoltage(const QString& controlSourceName, double voltage);
    
    // Thread-safe update queuing for real-time tuning (Flux or Sliders)
    void queueParameterUpdate(const QString& name, double value);
    void queueFluxSourceUpdate(const QString& sourceName, double value);
    void queueInternalCommand(const QString& cmd);

    // --- FluxScript JIT Targets ---
    void setFluxScriptTargets(const QMap<QString, Flux::FluxScriptTarget>& targets);
    void clearFluxScriptTargets();
    void setSkipFactor(int factor);

    struct VectorMap {
        int index;
        QString name;
        bool isVoltage;
        bool isScale = false;
    };
    int getVectorIndex(const QString& name) const;
    bool isRunning() const;
    bool isJitUpdateInProgress() const { return m_jitUpdateInProgress; }

    QMap<QString, Flux::FluxScriptTarget> m_fluxScriptTargets;
    std::mutex m_fluxTargetsMutex;
    std::vector<VectorMap> m_vectorMap;
    mutable std::mutex m_vectorMutex;

Q_SIGNALS:
    void outputReceived(const QString& text);
    void simulationFinished();
    void rawResultsReady(const QString& rawPath);
    void simulationStarted();
    void errorOccurred(const QString& error);
    void realTimeDataBatchReceived(const std::vector<double>& times, const std::vector<std::vector<double>>& values, const QStringList& names);

public Q_SLOTS:
    void handleSimulationFinished(const QString& rawPath, quint64 runGen);
    void processBufferedData();
    void clearCircuits();
    bool loadNetlistInternal(const QString& netlist, bool keepStorage, QString* errorOut);
    void applyPendingFluxSourceUpdates();
    void requestFluxSourceSync();

    // --- State Accessors ---
    SimulationState state() const { return m_state; }
    QString stateString() const;

private:
    // Internal Callback Handlers
    void handleIncomingData(void* vecArray, int numStructs);
    void handleAnalysisMetadata(void* initData);
    void handleEngineStateChange(bool finished, int id);
    explicit SimulationManager(QObject* parent = nullptr);
    ~SimulationManager();
    
    CommandWorker* m_worker = nullptr;
    QThread* m_workerThread = nullptr;
    void sendCommandAsync(const QString& cmd);
    void loadCircuitAsync(char** deck);
    
    bool recoverEngineIfNeeded();
    // Computes a dynamic halt budget scaled by circuit size and vector count (Issue 3)
    std::chrono::milliseconds dynamicHaltBudget() const;

    // Sends bg_halt and waits (bounded retry) for a confirmed pause at a sync
    // point, or for the engine to be observed terminated. Returns true when the
    // engine is safe for alter/teardown commands. Callers must NOT hold
    // m_workerSyncMutex when invoking this.
    bool haltAndWait(std::chrono::milliseconds budget);

    // Verifies that the loaded digital.cm d_cosim codemodel was built from the
    // same VioMATRIXC tree as the linked libngspice engine. A mismatched pair
    // (dev vs release tree) crashes in CKTdump during simulation; detecting it
    // up front turns a segfault into a clear error message.
    bool verifyCosimAbiMatch(const QString& cmPath);

    // Logs an error to the terminal via qWarning() (always visible in Release
    // builds where QT_NO_DEBUG_OUTPUT suppresses qDebug()) and emits errorOccurred().
    void reportError(const QString& error);

    void setState(SimulationState newState);
    std::atomic<SimulationState> m_state{SimulationState::Idle};

    bool m_isInitialized;
    std::atomic<bool> m_abiMismatch{false};
    std::atomic<bool> m_lastLoadFailed{false};
    std::atomic<bool> m_lastRunFailed{false};
    QString m_lastErrorMessage;
    
    // Control Flags
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_haltRequested{false};
    std::atomic<bool> m_jitUpdateInProgress{false};
    std::atomic<bool> m_fluxSyncRequested{false};
    std::atomic<bool> m_engineRecoveryRequired{false};

    // Run generation: bumped on every runSimulation(). Async completions
    // (ngspice callbacks, queued finish handlers, worker lambdas) capture
    // the generation at queue time and no-op when a newer run has started.
    // Without this, a previous run's delayed finish/error/raw lands in the
    // new run (e.g. same old error reported after switching tabs).
    std::atomic<quint64> m_runGeneration{0};
    
    // High-performance JIT update sync
    std::thread m_jitSyncThread;
    std::mutex m_jitSyncMutex;
    QMap<QString, double> m_pendingHighPriorityUpdates;
    QStringList m_pendingInternalCommands;
    
    std::atomic<int> m_autoResumeCounter{0};
    QString m_currentNetlist;
    std::mutex m_netlistMutex;
    SimControl* m_streamingControl = nullptr;
    
    struct SimDataPoint {
        double time;
        std::vector<double> values;
    };
    std::deque<SimDataPoint> m_simBuffer;
    std::mutex m_bufferMutex;
    QTimer* m_bufferTimer = nullptr;
    std::atomic<int> m_streamingCounter{0};
    std::atomic<int> m_skipFactor{1};

    std::vector<QString> m_logBuffer;
    std::mutex m_logMutex;
    std::mutex m_controlMutex;

    std::vector<QByteArray> m_circStorage;
    std::vector<char*> m_circPtrs;

    static int cbSendChar(char* output, int id, void* userData);
    static int cbSendStat(char* stat, int id, void* userData);
    static int cbControlledExit(int status, bool immediate, bool quit, int id, void* userData);

#ifdef HAVE_NGSPICE
    static int cbSendData(pvecvaluesall vecArray, int numStructs, int id, void* userData);
    static int cbSendInitData(pvecinfoall initData, int id, void* userData);
    static int cbBGThreadRunning(bool finished, int id, void* userData);
#else
    static int cbSendData(void* vecArray, int numStructs, int id, void* userData);
    static int cbSendInitData(void* initData, int id, void* userData);
    static int cbBGThreadRunning(bool finished, int id, void* userData);
#endif

public:
    // Synchronization for worker thread
    std::mutex m_workerSyncMutex;
    std::condition_variable m_workerSyncCond;
    std::atomic<bool> m_ngspiceIsHalted{false};
};

#endif // SIMULATION_MANAGER_H
