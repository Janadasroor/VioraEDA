/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SIM_MANAGER_QT_H
#define SIM_MANAGER_QT_H

#include <QObject>
#include <QStringList>
#include <QMap>
#include "../core/sim_results.h"
#include "../core/sim_netlist.h"

class QGraphicsScene;
class NetManager;

// Normalize SmartSignal FluxScript source: replace user-facing pin references
// (e.g. V("In1"), inputs["In1"], inputs[0]) with FluxScript ABI tokens (inputs[inN]).
QString normalizeFluxSmartBlockSource(QString source, const QStringList& inputPins);

/**
 * @brief Thread-safe (future) Manager for the simulation module.
 */
class SimManager : public QObject {
    Q_OBJECT
public:
    virtual ~SimManager();
    static SimManager& instance();

    struct PendingStepRun {
        QString netlist;
        QString label;
    };

    void runDCOP(QGraphicsScene* scene, NetManager* netMgr);
    void runTransient(QGraphicsScene* scene, NetManager* netMgr, double tStop, double tStep);
    void runAC(QGraphicsScene* scene, NetManager* netMgr, double fStart, double fStop, int points);
    void runRFAnalysis(QGraphicsScene* scene, NetManager* netMgr, double fStart, double fStop, int points, const QString& p1Src, const QString& p2Node, double z0 = 50.0);
    
    void runMonteCarlo(QGraphicsScene* scene, NetManager* netMgr, int runs);
    void runParametricSweep(QGraphicsScene* scene, NetManager* netMgr, const QString& component, const QString& param, double start, double stop, int steps);
    void runSensitivity(QGraphicsScene* scene, NetManager* netMgr, const QString& targetSignal);
    void runNetlistText(const QString& netlistContent);
    
    // Async entry point for simulation
    void runNgspiceSimulation(const QString& netlist, const SimAnalysisConfig& config);

    // Helper to generate netlist on the UI thread safely
    static QString generateNetlist(QGraphicsScene* scene, NetManager* netMgr, const SimAnalysisConfig& config, const QString& projectDir = "");

    // Debugger / Pre-flight check
    QStringList preflightCheck(QGraphicsScene* scene, NetManager* netMgr, SimNetlist& outNetlist, const QString& projectDir = QString());
    void runWithNetlist(const SimNetlist& netlist);
    
    void runRealTime(QGraphicsScene* scene, NetManager* netMgr, double maxStep = 1e-3, double maxTime = 0.0, int maxPts = 100000);
    void startNextRealTimeSegment();
    void stopRealTime();
    void updateParameterLive(const QString& name, double value);

    // FluxScript JIT Integration
    void compileFluxScripts(QGraphicsScene* scene);
    QMap<QString, class SmartSignalItem*> m_fluxScriptTargets;
    QMap<QString, QMap<QString, QString>> m_pinToNetMap;
    
    void stopAll();
    void pauseSimulation(bool pause);
    bool isPaused() const { return m_paused; }
    bool isRunning() const;

Q_SIGNALS:
    void simulationStarted();
    void simulationFinished(const SimResults& results);
    void netlistGenerated(const QString& netlist, const SimAnalysisConfig& config);
    void generationFailed(const QString& error);
    void simulationPaused(bool paused);
    void simulationStopped();
    void realTimeDataBatchReceived(const std::vector<double>& times, const std::vector<std::vector<double>>& values, const QStringList& names);
    void errorOccurred(const QString& msg);
    void logMessage(const QString& msg);

private Q_SLOTS:
    void onInteractiveStateChanged();
    void onRealTimeTick();
    void cleanupSimulation();

private:
    explicit SimManager(QObject* parent = nullptr);
    void parseRawResultsFile(const QString& path, const QString& netlistText, SimAnalysisType analysisType);
    bool startSharedSimulation(const QString& netlistContent, const QString& logMessage);
    void startNgspiceWithNetlist(const QString& netlistContent);
    void startNextStepSweepRun();
    void mergeStepSweepResults(const SimResults& runResults, const QString& stepLabel, int runIndex);

    SimControl* m_control = nullptr;
    bool m_paused = false;
    bool m_resultsPending = false;
    SimAnalysisConfig m_lastConfig;
    // Core run-generation captured when the current shared run was started.
    // Core completions from an older generation (a newer core run began since,
    // e.g. a direct run from the netlist editor) are dropped by the
    // rawResultsReady/simulationFinished handlers instead of being parsed
    // against this run's netlist text.
    quint64 m_activeCoreGen{0};

    QList<PendingStepRun> m_pendingStepRuns;
    SimResults m_stepSweepResults;
    QString m_activeStepLabel;
    QString m_activeNetlistText;
    QString m_sharedNetlistPath;
    bool m_stopRequested = false;
    // True once simulationStopped() has been emitted for the current logical
    // run (reset at every run entry). Late duplicate finishes — e.g. a stale
    // core Finished arriving after stopAll() already reported — must not
    // re-fire: consumers clear the active run tab/UI state on this signal and
    // a duplicate landing during the NEXT run would corrupt it.
    bool m_stopNotified{false};
    void emitStoppedOnce();
    int m_completedStepRuns = 0;
    class QProcess* m_ngspiceProcess = nullptr;

    QGraphicsScene* m_rtScene = nullptr;
    NetManager* m_rtNetMgr = nullptr;
    bool m_rtPending = false;
    double m_rtCurrentTime = 0.0;
    class QTimer* m_rtTimer = nullptr;
};

#endif // SIM_MANAGER_QT_H
