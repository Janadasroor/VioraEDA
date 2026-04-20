#ifndef SIMULATION_MANAGER_H
#define SIMULATION_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QTimer>
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

#ifdef HAVE_NGSPICE
#include <ngspice/sharedspice.h>
#endif

class SimControl;

/**
 * @brief Manages interaction with the Ngspice simulation engine
 */
class SimulationManager : public QObject {
    Q_OBJECT

public:
    static SimulationManager& instance();

    bool isAvailable() const;
    bool supportsNativeLogicADevices() const;
    void initialize();
    void runSimulation(const QString& netlist, SimControl* control = nullptr);
    bool validateNetlist(const QString& netlist, QString* errorOut = nullptr);
    QString lastErrorMessage() const;
    void stopSimulation();
    void shutdown();

    // --- Dynamic Interaction API ---
    double getVectorValue(const QString& name);
    void setParameter(const QString& name, double value);
    void sendInternalCommand(const QString& command);

    // --- Real-Time Switch Control ---
    void alterSwitch(const QString& switchRef, bool open, double vt = 0.5, double vh = 0.1);
    void alterSwitchResistance(const QString& resistorName, double resistance);
    void alterSwitchVoltage(const QString& controlSourceName, double voltage);
    
    // Thread-safe update queuing for Flux smart blocks
    void queueFluxSourceUpdate(const QString& sourceName, double value);

    // --- FluxScript JIT Targets ---
    struct FluxScriptTarget {
        QStringList outputVoltageSources;
        QMap<QString, QString> pinToNetMap;
    };
    void setFluxScriptTargets(const QMap<QString, FluxScriptTarget>& targets);
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

    QMap<QString, FluxScriptTarget> m_fluxScriptTargets;
    std::mutex m_fluxTargetsMutex;
    std::vector<VectorMap> m_vectorMap;
    mutable std::mutex m_vectorMutex;
    
    // Protection for ngspice API calls
    std::mutex m_ngspiceMutex;

Q_SIGNALS:
    void outputReceived(const QString& text);
    void simulationFinished();
    void rawResultsReady(const QString& rawPath);
    void simulationStarted();
    void errorOccurred(const QString& error);
    void realTimeDataBatchReceived(const std::vector<double>& times, const std::vector<std::vector<double>>& values, const QStringList& names);

private Q_SLOTS:
    void handleSimulationFinished(const QString& rawPath);
    void processBufferedData();

private:
    explicit SimulationManager(QObject* parent = nullptr);
    ~SimulationManager();

    bool loadNetlistInternal(const QString& netlist, bool keepStorage, QString* errorOut);

    bool m_isInitialized;
    bool m_lastLoadFailed = false;
    std::atomic<bool> m_lastRunFailed{false};
    QString m_lastErrorMessage;
    std::atomic<bool> m_bgRunIssued{false};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_pauseRequested{false};
    std::atomic<bool> m_switchToggleInProgress{false};  // Prevents simulationFinished() during switch toggles
    std::atomic<bool> m_jitUpdateInProgress{false};    // Prevents simulationFinished() during JIT updates
    
    // High-performance JIT update sync
    std::thread m_jitSyncThread;
    std::mutex m_jitSyncMutex;
    std::condition_variable m_jitSyncCond;
    std::atomic<bool> m_jitSyncThreadRunning{false};
    QMap<QString, double> m_pendingHighPriorityUpdates;
    
    QString m_currentNetlist;
    SimControl* m_streamingControl = nullptr;
    
    // Thread-safe buffering for real-time updates
    struct SimDataPoint {
        double time;
        std::vector<double> values;
    };
    std::vector<SimDataPoint> m_simBuffer;
    std::mutex m_bufferMutex;
    QTimer* m_bufferTimer = nullptr;
    int m_streamingCounter = 0;
    int m_skipFactor = 1;

    std::vector<QString> m_logBuffer;
    std::mutex m_logMutex;
    std::mutex m_controlMutex;

    std::vector<QByteArray> m_circStorage;
    std::vector<char*> m_circPtrs;

    // Callbacks from ngspice (static because they are C function pointers)
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
};

#endif // SIMULATION_MANAGER_H
