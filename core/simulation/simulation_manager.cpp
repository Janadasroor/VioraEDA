/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#include "simulation_manager.h"
#include "netlist_processor.h"
#include "spice_backend.h"
#include "jit_bridge.h"
#include "simulator/core/sim_results.h"
#include "jit_context_manager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryFile>
#include <QTextStream>
#include <QMetaObject>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <utility>
#ifdef Q_OS_WIN
#include "dlfcn_win.h"
#else
#include <dlfcn.h>
#endif

using namespace Flux;

void CommandWorker::execute(const QString& cmd) { 
    if (cmd.startsWith("alter", Qt::CaseInsensitive)) {
        executeSequence(QStringList() << cmd);
    } else {
        SpiceBackend::instance().execute(cmd); 
    }
}
void CommandWorker::executeSequence(const QStringList& cmds) {
    if (cmds.isEmpty() || !m_manager) return;

    bool needsResume = false;

    // Issue 1: In native SmartSignal mode, do not use bg_halt / bg_resume cycles for alter sequences.
    // The native engine applies alter commands directly in-solver.
    // Only in non-native / legacy mode, gate through haltAndWait.
    if (m_manager->isRunning() && !m_manager->isNativeSmartSignalMode()) {
        qDebug() << "[SimWorker] Requesting bg_halt for alteration...";

        // Issue 3: Use dynamic halt budget scaled by circuit complexity
        auto budget = m_manager->dynamicHaltBudget();
        if (m_manager->haltAndWait(budget)) {
            qDebug() << "[SimWorker] Halt confirmed at sync point.";
            needsResume = true;
        } else if (SpiceBackend::instance().isPaused()) {
            // Engine reports paused even though the sync-pause callback never fired.
            qDebug() << "[SimWorker] Engine reports paused; proceeding with alteration.";
            needsResume = true;
        } else {
            // Issue 2: Instead of dropping the update permanently, retry with coalescing.
            qDebug() << "[SimWorker] Halt confirmation timed out and engine is not paused; rescheduling alter retry.";
            m_manager->m_jitUpdateInProgress = false;
            m_manager->m_fluxSyncRequested = false;
            m_manager->m_haltRequested = false;
            SpiceBackend::instance().execute("bg_resume");

            // Re-queue the commands to retry after a brief delay so user actions (e.g. switch toggle) are not lost
            QTimer::singleShot(50, this, [this, cmds]() {
                executeSequence(cmds);
            });
            return;
        }
    }

    // Apply alter commands while engine is safely halted at sync point
    // (ngSpice_Command will execute them since fl_paused=true && !fl_exited)
    for (const QString& cmd : cmds) {
        SpiceBackend::instance().execute(cmd);
    }

    m_manager->m_jitUpdateInProgress = false;

    if (needsResume) {
        qDebug() << "[SimWorker] Resuming simulation...";
        // NOTE: m_haltRequested stays true until handleEngineStateChange confirms actual resume
        // This prevents spurious halt detection race condition
        m_manager->m_autoResumeCounter = 0;
        m_manager->m_streamingCounter = 0; 
        SpiceBackend::instance().execute("bg_resume");
        
        // Wait for ngspice callback indicating actual resume (!finished && !isPaused)
        // or indicating bg thread has exited (finished && !isPaused)
        {
            std::unique_lock<std::mutex> lock(m_manager->m_workerSyncMutex);
            bool gotCallback = m_manager->m_workerSyncCond.wait_for(lock, std::chrono::milliseconds(500), [this] {
                // Wait for any state change from halted
                return !m_manager->m_ngspiceIsHalted.load() || m_manager->m_state == SimulationState::Finished;
            });
            
            if (!gotCallback) {
                // Timeout - check current state
                qDebug() << "[SimWorker] Resume callback timeout, checking state...";
                bool isPaused = SpiceBackend::instance().isPaused();
                qDebug() << "[SimWorker] isPaused=" << isPaused << "ngspiceIsHalted=" << m_manager->m_ngspiceIsHalted;
                
                if (!isPaused && !m_manager->m_ngspiceIsHalted) {
                    // ngspice reports not paused and not halted - bg thread exited
                    // Need to restart with bg_run
                    qDebug() << "[SimWorker] Bg thread exited after alter, restarting with bg_run...";
                    SpiceBackend::instance().execute("bg_run");
                    m_manager->m_haltRequested = false;
                } else {
                    // Still paused, try resume command
                    qDebug() << "[SimWorker] Trying resume command...";
                    SpiceBackend::instance().execute("resume");
                    
                    bool resumed = m_manager->m_workerSyncCond.wait_for(lock, std::chrono::milliseconds(500), [this] {
                        return !m_manager->m_ngspiceIsHalted.load();
                    });
                    
                    if (!resumed) {
                        // Last resort: try bg_run
                        qDebug() << "[SimWorker] Resume failed, trying bg_run...";
                        SpiceBackend::instance().execute("bg_run");
                    }
                    m_manager->m_haltRequested = false;
                }
            } else {
                // Got callback - check if we're running or finished
                if (m_manager->m_state == SimulationState::Running) {
                    qDebug() << "[SimWorker] Resume succeeded - now Running.";
                } else if (m_manager->m_state == SimulationState::Finished) {
                    qDebug() << "[SimWorker] Bg thread finished, restarting with bg_run...";
                    SpiceBackend::instance().execute("bg_run");
                    m_manager->m_haltRequested = false;
                }
            }
        }
    }
}
void CommandWorker::loadCircuit(char** deck) { SpiceBackend::instance().loadCircuit(deck); }

namespace {
    QString normalizeStreamVectorName(const QString& rawName) {
        QString q = rawName.trimmed();
        if (q.isEmpty()) return rawName;

        // 1. Deep cleanup: Remove @ and preserve terminal if present (e.g., [IB], [B], etc.)
        // Transform I(@Q1[IB]) -> I(Q1[B]) or @D1[id] -> I(D1)
        static const QRegularExpression deepCleanupRe(
            "(?:I|V)?\\s*\\(?\\s*@\\s*([A-Za-z0-9_.$:+-]+)(?:\\[\\s*i?([a-z]+)\\s*\\])?\\s*\\)?",
            QRegularExpression::CaseInsensitiveOption);
        
        if (const auto m = deepCleanupRe.match(q); m.hasMatch()) {
            QString ref = m.captured(1).toUpper();
            QString term = m.captured(2).toUpper();
            if (term.isEmpty() || term == "I" || term == "D" || term == "C") {
                // For simple devices or Collector/Drain (default), just use I(REF)
                return QString("I(%1)").arg(ref);
            } else {
                // For other terminals (Base, Emitter, Gate, etc.), use I(REF[TERM])
                return QString("I(%1[%2])").arg(ref, term);
            }
        }

        // 2. Standard branch normalization: v1#branch -> I(V1)
        static const QRegularExpression branchRe(
            "^\\s*([A-Za-z0-9_.$:+-]+)\\s*#\\s*branch\\s*$",
            QRegularExpression::CaseInsensitiveOption);
        if (const auto m = branchRe.match(q); m.hasMatch()) {
            return QString("I(%1)").arg(m.captured(1).toUpper());
        }

        // 3. General wrapper normalization: v(net1) -> V(NET1)
        static const QRegularExpression wrapperRe(
            "^(v|i)\\s*\\(\\s*(.+)\\s*\\)$",
            QRegularExpression::CaseInsensitiveOption);
        if (const auto m = wrapperRe.match(q); m.hasMatch()) {
            return QString("%1(%2)")
                .arg(m.captured(1).toUpper(), m.captured(2).trimmed().toUpper());
        }

        return q.toUpper();
    }
}

SimulationManager& SimulationManager::instance() {
    static SimulationManager instance;
    return instance;
}

SimulationManager::SimulationManager(QObject* parent)
    : QObject(parent), m_isInitialized(false) {
    m_bufferTimer = new QTimer(this);
    m_bufferTimer->setInterval(33); // ~30 FPS
    connect(m_bufferTimer, &QTimer::timeout, this, &SimulationManager::processBufferedData);

    m_workerThread = new QThread(this);
    m_worker = new CommandWorker();
    m_worker->setManager(this);
    m_worker->moveToThread(m_workerThread);
    m_workerThread->start();
}

SimulationManager::~SimulationManager() {
    m_workerThread->quit();
    m_workerThread->wait();
    delete m_worker;
}

void SimulationManager::setState(SimulationState newState) {
    if (m_state == newState) return;
    QString oldStateStr = stateString();
    m_state = newState;
    qDebug() << "[SimManager] State transition:" << stateString() << "(from" << oldStateStr << ")";
}

QString SimulationManager::stateString() const {
    switch (m_state) {
        case SimulationState::Idle:     return "Idle";
        case SimulationState::Loading:  return "Loading";
        case SimulationState::Running:  return "Running";
        case SimulationState::Halted:   return "Halted";
        case SimulationState::Paused:   return "Paused";
        case SimulationState::Stopping: return "Stopping";
        case SimulationState::Finished: return "Finished";
        case SimulationState::Error:    return "Error";
    }
    return "Unknown";
}

void SimulationManager::sendCommandAsync(const QString& cmd) {
    qDebug() << "[SimManager] Async command queued:" << cmd;
    QMetaObject::invokeMethod(m_worker, "execute", Qt::QueuedConnection, Q_ARG(QString, cmd));
}

void SimulationManager::loadCircuitAsync(char** deck) {
    QMetaObject::invokeMethod(m_worker, "loadCircuit", Qt::QueuedConnection, Q_ARG(char**, deck));
}



bool SimulationManager::isAvailable() const {
#ifdef HAVE_NGSPICE
    return true;
#else
    return false;
#endif
}

bool SimulationManager::supportsNativeLogicADevices() const {
#ifdef HAVE_NGSPICE
    // ngSpice_IsPaused is a custom symbol only present in the VioMATRIXC patched engine.
    void* sym = SpiceBackend::instance().resolveSymbol("ngSpice_IsPaused");
    return sym != nullptr;
#else
    return false;
#endif
}

bool SimulationManager::isNativeSmartSignalMode() const {
    return supportsNativeLogicADevices();
}

QString SimulationManager::lastErrorMessage() const {
    std::lock_guard<std::mutex> lock(const_cast<SimulationManager*>(this)->m_logMutex);
    return m_lastErrorMessage;
}

void SimulationManager::reportError(const QString& error) {
    if (!error.isEmpty()) {
        qWarning() << "[Simulation] ERROR:" << error;
        // Qt's qWarning on Windows may route to the debugger only (OutputDebugString)
        // when stderr is not a console; duplicate to stderr so CI logs always capture it.
        fprintf(stderr, "[Simulation] ERROR: %s\n", error.toUtf8().constData());
        fflush(stderr);
    }
    Q_EMIT errorOccurred(error);
}

// Must stay in sync with the return value of vio_cosim_abi_tag() in
// VioMATRIXC/src/xspice/icm/digital/d_cosim/cfunc.mod.
static const char* kExpectedCosimAbiTag = "viospice-cm-abi-v1";

bool SimulationManager::verifyCosimAbiMatch(const QString& cmPath) {
    void* handle = dlopen(cmPath.toUtf8().constData(), RTLD_NOW | RTLD_NOLOAD);
    if (!handle) {
        reportError(QString("ABI check failed: could not inspect %1 (%2). "
                            "The simulation engine may be mismatched; refusing to run.")
                        .arg(cmPath, QString::fromLocal8Bit(dlerror())));
        return false;
    }
    auto* tag = reinterpret_cast<const char* (*)(void)>(dlsym(handle, "vio_cosim_abi_tag"));
    if (!tag) {
        reportError(QString("%1 is missing the d_cosim ABI tag symbol. It was likely built from a "
                            "different VioMATRIXC tree than the linked libngspice engine, which causes "
                            "a crash during simulation. Rebuild digital.cm from the matching tree.")
                        .arg(cmPath));
        return false;
    }
    const char* actual = tag();
    if (strcmp(actual, kExpectedCosimAbiTag) != 0) {
        reportError(QString("%1 d_cosim ABI tag mismatch: expected \"%2\", found \"%3\". Rebuild digital.cm "
                            "from the same VioMATRIXC tree as the libngspice engine.")
                        .arg(cmPath, QString::fromLatin1(kExpectedCosimAbiTag), QString::fromLatin1(actual)));
        return false;
    }
    qDebug() << "[XSPICE] d_cosim ABI verified:" << actual;
    return true;
}

#include <QStandardPaths>

void SimulationManager::initialize() {
    if (m_isInitialized) return;

#ifdef HAVE_NGSPICE
    QString scriptsPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (scriptsPath.isEmpty()) {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
        scriptsPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
        scriptsPath = QDir::homePath() + "/.viospice";
#endif
    }
    QDir().mkpath(scriptsPath);

    qputenv("SPICE_SCRIPTS", scriptsPath.toUtf8());
    qputenv("SPICE_LIB_DIR", scriptsPath.toUtf8());

    m_isInitialized = SpiceBackend::instance().initialize(
        cbSendChar, cbSendStat, cbControlledExit, cbSendData, cbSendInitData, cbBGThreadRunning, this
    );

    if (m_isInitialized) {
        // Resolve the code-model directory across all install layouts:
        //   dev/CLI/tarball: <appDir>/cm
        //   legacy fallback: <appDir>/../cm
        //   macOS bundle:    <appDir>/../Resources/cm  (MacOS -> Contents/Resources)
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList cmCandidates = {appDir + "/cm", appDir + "/../cm",
                                          appDir + "/../Resources/cm"};
        QString cmDir;
        for (const QString& cand : cmCandidates) {
            if (QDir(cand).exists()) {
                cmDir = cand;
                break;
            }
        }
        if (cmDir.isEmpty()) cmDir = appDir + "/cm";
        auto resolveCm = [&](const QString& sub) -> QString {
            for (const QString& cand : cmCandidates) {
                const QString p = QString("%1/%2.cm").arg(cand, sub);
                if (QFile::exists(p)) return p;
            }
            return QString("%1/%2.cm").arg(cmDir, sub);
        };
        qDebug() << "[XSPICE] Loading code models from:" << cmDir;
        QStringList cmSubDirs = {"analog", "digital", "spice2poly", "tlines", "xtradev", "xtraevt", "viospice"};

        for (const QString& sub : cmSubDirs) {
            QString cmPath = resolveCm(sub);
            if (QFile::exists(cmPath)) {
                int rc = SpiceBackend::instance().execute(QString("codemodel %1").arg(cmPath));
                qDebug() << "[XSPICE] Loaded" << sub << "rc=" << rc;
            } else {
                qDebug() << "[XSPICE] MISSING:" << cmPath;
            }
        }

        SpiceBackend::instance().execute("set ngbehavior=ltps");
        SpiceBackend::instance().execute("set filetype=binary");

        // Guard against the crash seen when digital.cm and libngspice are built
        // from different VioMATRIXC trees (dev vs release). Verify the d_cosim
        // codemodel ABI tag before running; report a clear error instead of a
        // segfault inside CKTdump.
        QString digitalCm = resolveCm("digital");
        if (!verifyCosimAbiMatch(digitalCm)) {
            m_abiMismatch = true;
        }
    }
#endif
}

bool SimulationManager::isRunning() const {
    return m_state == SimulationState::Running || m_state == SimulationState::Halted;
}

std::chrono::milliseconds SimulationManager::dynamicHaltBudget() const {
    // Base budget 2000ms. Scale up based on netlist size and vector count for large circuits (Issue 3)
    int netlistLen = 0;
    {
        std::lock_guard<std::mutex> lock(const_cast<SimulationManager*>(this)->m_netlistMutex);
        netlistLen = m_currentNetlist.size();
    }
    int vecCount = 0;
    {
        std::lock_guard<std::mutex> lock(const_cast<SimulationManager*>(this)->m_vectorMutex);
        vecCount = static_cast<int>(m_vectorMap.size());
    }
    int extraMs = (netlistLen / 50) + (vecCount * 20);
    int totalMs = std::clamp(2000 + extraMs, 2000, 10000);
    return std::chrono::milliseconds(totalMs);
}

bool SimulationManager::haltAndWait(std::chrono::milliseconds budget) {
    m_haltRequested = true;

    {
        std::lock_guard<std::mutex> lock(m_workerSyncMutex);
        // Already parked at a sync point (e.g. state==Halted) — no new
        // callback will fire for a redundant bg_halt, so don't reset the flag.
        if (m_ngspiceIsHalted) return true;
    }

    SpiceBackend::instance().execute("bg_halt");

    const auto deadline = std::chrono::steady_clock::now() + budget;
    {
        std::unique_lock<std::mutex> lock(m_workerSyncMutex);
        while (std::chrono::steady_clock::now() < deadline) {
            if (m_ngspiceIsHalted) return true;
            // Issue 12: Engine already terminated — safe for alter/teardown commands,
            // but do NOT set m_ngspiceIsHalted = true (which conflates paused vs terminated).
            if (m_state == SimulationState::Finished || m_state == SimulationState::Error) {
                return true;
            }
            m_workerSyncCond.wait_for(lock, std::chrono::milliseconds(25));
        }
    }
    return m_ngspiceIsHalted.load();
}

bool SimulationManager::recoverEngineIfNeeded() {
#ifdef HAVE_NGSPICE
    if (!m_engineRecoveryRequired.exchange(false)) return true;

    qWarning() << "[SimManager] Recovering engine after fatal state.";
    setState(SimulationState::Stopping);
    m_stopRequested = false;

    if (m_bufferTimer) m_bufferTimer->stop();

    if (m_isInitialized) {
        SpiceBackend::instance().execute("bg_halt");
        SpiceBackend::instance().execute("quit");
        m_isInitialized = false;
    }

    initialize();
    return m_isInitialized;
#else
    return false;
#endif
}

void SimulationManager::runSimulation(const QString& netlist, SimControl* control) {
    if (!isAvailable()) { reportError("Simulation engine not installed."); return; }
    if (m_abiMismatch) {
        reportError("Simulation aborted: the d_cosim codemodel does not match the ngspice engine "
                    "(ABI mismatch). Rebuild digital.cm from the same VioMATRIXC tree as libngspice.");
        return;
    }
    if (!recoverEngineIfNeeded()) { reportError("Failed to recover simulation engine."); return; }
    if (!m_isInitialized) initialize();

#ifdef HAVE_NGSPICE
    { std::lock_guard<std::mutex> lock(m_netlistMutex); m_currentNetlist = netlist; }
    { std::lock_guard<std::mutex> lock(m_controlMutex); m_streamingControl = control; }
    { std::lock_guard<std::mutex> lock(m_vectorMutex); m_vectorMap.clear(); }
    
    m_lastLoadFailed = false;
    m_lastRunFailed = false;
    m_stopRequested = false;
    m_haltRequested = false;
    m_fluxSyncRequested = false;
    m_jitUpdateInProgress = false;

    { std::lock_guard<std::mutex> lock(m_bufferMutex); m_simBuffer.clear(); }
    { std::lock_guard<std::mutex> lock(m_jitSyncMutex); m_pendingHighPriorityUpdates.clear(); }
    { std::lock_guard<std::mutex> lock(m_logMutex); m_logBuffer.clear(); }
    
    m_streamingCounter = 0;
    m_skipFactor = 1; // High resolution for real-time interaction
    
    // Cleanup stale raw file (same location used for default temp netlists)
    QFile::remove(QDir::tempPath() + "/viospice.raw");
    // If already running, halt and confirm before tearing down the circuit.
    // stopSimulation() is fire-and-forget (queued bg_halt); using it here means
    // the stale queued bg_halt can land AFTER the new bg_run and strand the new
    // run in Halted. A synchronous confirmed halt avoids that race entirely.
    if (m_state == SimulationState::Running || m_state == SimulationState::Halted) {
        if (!haltAndWait(dynamicHaltBudget())) {
            qWarning() << "[SimManager] Pre-run halt not confirmed; loadNetlistInternal will force recovery.";
        }
    }

    QString error;
    if (!loadNetlistInternal(netlist, true, &error)) {
        QMetaObject::invokeMethod(m_bufferTimer, "stop", Qt::QueuedConnection);
        setState(SimulationState::Error);
        {
            std::lock_guard<std::mutex> lock(m_logMutex);
            if (error.isEmpty()) error = m_lastErrorMessage.isEmpty() ? "Failed to load netlist into ngspice." : m_lastErrorMessage;
        }
        if (!error.isEmpty()) reportError(error);
        clearCircuits();
        Q_EMIT simulationFinished();
        return;
    }

    {
        std::lock_guard<std::mutex> targetLock(m_fluxTargetsMutex);
        JitBridge::instance().registerTargetsWithEngine(m_fluxScriptTargets);
    }

    Q_EMIT simulationStarted();
    
    // Apply any GUI parameters (like switch states) before starting
    applyPendingFluxSourceUpdates();
    
    // Note: ".save all" is already added by SpiceNetlistGenerator to the deck.
    // Explicitly setting filetype to binary for performance.
    SpiceBackend::instance().execute("set filetype=binary");
    
    setState(SimulationState::Running);
    { std::lock_guard<std::mutex> lock(m_controlMutex); if (m_streamingControl) QMetaObject::invokeMethod(m_bufferTimer, "start", Qt::QueuedConnection); }
    int rc = SpiceBackend::instance().execute("bg_run");

    if (rc != 0 || m_lastLoadFailed) {
        m_bufferTimer->stop();
        setState(SimulationState::Error);
        QString finalErr;
        { std::lock_guard<std::mutex> lock(m_logMutex); finalErr = m_lastErrorMessage.isEmpty() ? "Ngspice failed to start simulation." : m_lastErrorMessage; }
        reportError(finalErr);
        Q_EMIT simulationFinished();
        return;
    }
#endif
}

bool SimulationManager::validateNetlist(const QString& netlist, QString* errorOut) {
    if (!isAvailable()) { if (errorOut) *errorOut = "Simulation engine not installed."; return false; }
    
    // Issue 10: If a simulation is running, validate netlist syntax without destroying live circuit
    if (isRunning()) {
        auto processResult = NetlistProcessor::process(netlist);
        if (!processResult.success) {
            if (errorOut) *errorOut = processResult.error;
            return false;
        }
        return true;
    }

    if (!recoverEngineIfNeeded()) { if (errorOut) *errorOut = "Failed to recover engine."; return false; }
    if (!m_isInitialized) initialize();

    { std::lock_guard<std::mutex> lock(m_netlistMutex); m_currentNetlist = netlist; }
    bool ok = loadNetlistInternal(netlist, false, errorOut);
    processBufferedData();
    return ok;
}

bool SimulationManager::loadNetlistInternal(const QString& netlist, bool keepStorage, QString* errorOut) {
#ifdef HAVE_NGSPICE
    // Ensure background thread is stopped before resetting. Do NOT run
    // destroy/reset/loadCircuit until the old run is confirmed parked — issuing
    // them against an active bg thread is what wedges the engine.
    if (m_state == SimulationState::Running || m_state == SimulationState::Halted) {
        if (!haltAndWait(dynamicHaltBudget())) {
            QString haltErr;
            {
                std::lock_guard<std::mutex> lock(m_logMutex);
                haltErr = m_lastErrorMessage;
            }
            qWarning() << "[SimManager] Halt confirmation timed out; forcing engine recovery before reload.";
            SpiceBackend::instance().execute("bg_halt");
            SpiceBackend::instance().execute("quit");
            m_isInitialized = false;
            initialize();
            if (!m_isInitialized) {
                // Issue 9: Preserve underlying diagnostic detail
                if (errorOut) {
                    *errorOut = haltErr.isEmpty()
                        ? "Failed to stop the running simulation."
                        : QString("Failed to stop the running simulation (ngspice error: %1).").arg(haltErr);
                }
                return false;
            }
        }
    }
    
    setState(SimulationState::Loading);
    SpiceBackend::instance().execute("destroy all");
    SpiceBackend::instance().execute("reset");
    SpiceBackend::instance().execute("set ngbehavior=ltps");
    SpiceBackend::instance().execute("set filetype=binary");
    SpiceBackend::instance().execute(isNativeSmartSignalMode() ? "set vicompat=lt" : "set vicompat=all");

    {
        std::lock_guard<std::mutex> lock(m_logMutex);
        m_lastLoadFailed = false; m_lastRunFailed = false; m_lastErrorMessage.clear();
    }
    
    auto processResult = NetlistProcessor::process(netlist);
    if (!processResult.success) { if (errorOut) *errorOut = processResult.error; return false; }

    m_circStorage.clear(); m_circPtrs.clear();
    m_circStorage.reserve(processResult.lines.size() + 1);
    m_circPtrs.reserve(processResult.lines.size() + 1);
    for (const QString& line : processResult.lines) {
        qDebug() << "[SimManager] Netlist line:" << line;
        m_circStorage.push_back(line.toLatin1());
        m_circPtrs.push_back(m_circStorage.back().data());
    }
    m_circPtrs.push_back(nullptr);

    int rc = SpiceBackend::instance().loadCircuit(m_circPtrs.data());
    bool loaded = (rc == 0 && !m_lastLoadFailed);

    if (!loaded) {
        // Fallback to source command.
        // Note: ngspice's cp_lexer keeps double quotes in command words
        // (parser/lexical.c), so `source "path"` looks up a file whose name
        // starts with '"' and fails. Single quotes are stripped, so use them.
        QTemporaryFile temp(QDir::tempPath() + "/viospice_XXXXXX.cir");
        if (temp.open()) {
            QTextStream out(&temp);
            for (const auto& l : processResult.lines) out << l << "\n";
            out.flush(); temp.close();
            m_lastLoadFailed = false;
            rc = SpiceBackend::instance().execute("source '" + temp.fileName() + "'");
            loaded = (rc == 0 && !m_lastLoadFailed);
        }
    }

    if (!keepStorage) { m_circStorage.clear(); m_circPtrs.clear(); }
    return loaded;
#else
    return false;
#endif
}

void SimulationManager::stopSimulation() {
#ifdef HAVE_NGSPICE
    { std::lock_guard<std::mutex> lock(m_controlMutex); m_streamingControl = nullptr; }
    m_haltRequested = true;
    m_stopRequested = true;
    sendCommandAsync("bg_halt");
    QMetaObject::invokeMethod(m_bufferTimer, "stop", Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, "processBufferedData", Qt::QueuedConnection);
#endif
}

void SimulationManager::shutdown() {
#ifdef HAVE_NGSPICE
    setState(SimulationState::Stopping);
    if (m_bufferTimer) m_bufferTimer->stop();
    if (isRunning()) {
        // Issue 15: Confirmed-halt gate to avoid racing with worker executeSequence
        haltAndWait(std::chrono::milliseconds(1000));
    }
    SpiceBackend::instance().execute("bg_halt");
    SpiceBackend::instance().execute("quit");
    m_isInitialized = false;
    setState(SimulationState::Idle);
#endif
}

double SimulationManager::getVectorValue(const QString& name) {
#ifdef HAVE_NGSPICE
    if (!m_isInitialized) return 0.0;
    pvector_info info = ngGet_Vec_Info(name.toLatin1().data());
    if (info && info->v_realdata && info->v_length > 0) return info->v_realdata[info->v_length - 1];
#endif
    return 0.0;
}

QPair<QVector<double>, QVector<double>> SimulationManager::getVectorHistory(const QString& name) {
    QVector<double> time, values;

    // Issue 7: Primary path reads from Ngspice's internal vectors which contain
    // the complete untruncated history for long runs.
#ifdef HAVE_NGSPICE
    if (m_isInitialized) {
        ngSpice_LockRealloc();
        QByteArray timeName("time");
        pvector_info timeInfo = ngGet_Vec_Info(timeName.data());
        QByteArray sigName = name.toLatin1();
        pvector_info sigInfo = ngGet_Vec_Info(sigName.data());
        if (sigInfo == nullptr) {
            QByteArray lowerName = name.toLower().toLatin1();
            sigInfo = ngGet_Vec_Info(lowerName.data());
        }
        if (timeInfo && timeInfo->v_realdata && timeInfo->v_length > 0 &&
            sigInfo && sigInfo->v_realdata && sigInfo->v_length > 0) {
            int len = qMin(timeInfo->v_length, sigInfo->v_length);
            time.reserve(len);
            values.reserve(len);
            for (int i = 0; i < len; ++i) {
                time.append(timeInfo->v_realdata[i]);
                values.append(sigInfo->v_realdata[i]);
            }
        }
        ngSpice_UnlockRealloc();
        if (!time.isEmpty()) return {time, values};
    }
#endif

    // Fallback: read from m_simBuffer (e.g. streaming mode when ngspice vectors not indexed)
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        if (!m_simBuffer.empty()) {
            const QString lowerName = name.toLower();
            int vecIndex = -1;
            {
                std::lock_guard<std::mutex> vlock(m_vectorMutex);
                for (const auto& vm : m_vectorMap) {
                    if (vm.name.toLower() == lowerName) {
                        vecIndex = vm.index;
                        break;
                    }
                }
            }
            if (vecIndex >= 0) {
                time.reserve(m_simBuffer.size());
                values.reserve(m_simBuffer.size());
                for (const auto& dp : m_simBuffer) {
                    if (vecIndex < static_cast<int>(dp.values.size())) {
                        time.append(dp.time);
                        values.append(dp.values[vecIndex]);
                    }
                }
                if (!time.isEmpty()) return {time, values};
            }
        }
    }

    return {time, values};
}

void SimulationManager::setParameter(const QString& name, double value) {
#ifdef HAVE_NGSPICE
    if (!m_isInitialized) return;
    QString valStr = QString::number(value, 'g', 12);
    if (name.contains('.')) {
        QStringList parts = name.split('.');
        SpiceBackend::instance().execute(QString("alter %1 %2 = %3").arg(parts[0], parts[1], valStr));
    } else {
        SpiceBackend::instance().execute(QString("set %1=%2").arg(name, valStr));
    }
#endif
}

void SimulationManager::sendInternalCommand(const QString& command) {
#ifdef HAVE_NGSPICE
    if (!m_isInitialized) return;
    if (command == "bg_halt") { 
        m_haltRequested = true; 
        QMetaObject::invokeMethod(m_bufferTimer, "stop", Qt::QueuedConnection);
    }
    else if (command == "bg_resume" || command == "resume") { 
        // Issue 6: Clear stale m_stopRequested flag on resume
        m_stopRequested = false;
        m_haltRequested = false;
        // NOTE: Don't set Running state here - wait for handleEngineStateChange callback
        // to confirm ngspice has actually resumed. This prevents race conditions.
        { std::lock_guard<std::mutex> lock(m_controlMutex); if (m_streamingControl) QMetaObject::invokeMethod(m_bufferTimer, "start", Qt::QueuedConnection); }
    }
    SpiceBackend::instance().execute(command);
#endif
}

void SimulationManager::alterSwitch(const QString& switchRef, bool open, double vt, double vh) {
    QString rName = switchRef.startsWith("R", Qt::CaseInsensitive) ? switchRef : "R" + switchRef;
    queueParameterUpdate(rName, open ? 1e12 : 0.001);
}

void SimulationManager::alterSwitchResistance(const QString& resistorName, double resistance) {
    queueParameterUpdate(resistorName, resistance);
}

void SimulationManager::alterSwitchVoltage(const QString& controlSourceName, double voltage) {
    queueParameterUpdate(controlSourceName, voltage);
}

void SimulationManager::queueParameterUpdate(const QString& name, double value) {
    { std::lock_guard<std::mutex> lock(m_jitSyncMutex); m_pendingHighPriorityUpdates[name] = value; }
    QMetaObject::invokeMethod(this, [this]() { requestFluxSourceSync(); }, Qt::QueuedConnection);
}

void SimulationManager::queueFluxSourceUpdate(const QString& sourceName, double value) {
    queueParameterUpdate(sourceName, value);
}

void SimulationManager::queueInternalCommand(const QString& cmd) {
#ifdef HAVE_NGSPICE
    if (m_isInitialized && isRunning()) {
        { std::lock_guard<std::mutex> lock(m_jitSyncMutex); m_pendingInternalCommands.append(cmd); }
        QMetaObject::invokeMethod(this, [this]() { requestFluxSourceSync(); }, Qt::QueuedConnection);
    }
#endif
}

void SimulationManager::requestFluxSourceSync() {
#ifdef HAVE_NGSPICE
    if (!m_isInitialized || m_stopRequested) return;
    if (m_jitUpdateInProgress || m_fluxSyncRequested) return;

    { std::lock_guard<std::mutex> lock(m_jitSyncMutex);
      if (m_pendingHighPriorityUpdates.isEmpty() && m_pendingInternalCommands.isEmpty()) return; }

    m_jitUpdateInProgress = true; m_fluxSyncRequested = true; 
    QMetaObject::invokeMethod(this, "applyPendingFluxSourceUpdates", Qt::QueuedConnection);
#endif
}

void SimulationManager::applyPendingFluxSourceUpdates() {
#ifdef HAVE_NGSPICE
    QMap<QString, double> updates; QStringList commands;
    { std::lock_guard<std::mutex> lock(m_jitSyncMutex);
      updates.swap(m_pendingHighPriorityUpdates); commands.swap(m_pendingInternalCommands); }

    QStringList spiceCmds;
    for (const QString& cmd : commands) spiceCmds << cmd;
    
    for (auto it = updates.constBegin(); it != updates.constEnd(); ++it) {
        QString target = it.key();
        double val = it.value();
        QString valStr = QString::number(val, 'g', 12);
        
        if (target.startsWith('r', Qt::CaseInsensitive)) {
            // For resistors, use the @device[param] syntax which is more stable for live updates
            spiceCmds << QString("alter @%1[resistance] %2").arg(target, valStr);
        } else if (target.startsWith('v', Qt::CaseInsensitive) || target.startsWith('i', Qt::CaseInsensitive)) {
            spiceCmds << QString("alter %1 dc %2").arg(target, valStr);
        } else {
            spiceCmds << QString("alter %1 %2").arg(target, valStr);
        }
    }

    QMetaObject::invokeMethod(m_worker, [this, spiceCmds]() {
        m_worker->executeSequence(spiceCmds);
        
        // Notify manager that sequence is done
        QMetaObject::invokeMethod(this, [this]() {
            m_fluxSyncRequested = false;
            
            bool hasMore = false;
            { std::lock_guard<std::mutex> lock(m_jitSyncMutex); hasMore = !m_pendingHighPriorityUpdates.isEmpty(); }
            if (hasMore) requestFluxSourceSync();
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
#endif
}

void SimulationManager::processBufferedData() {
    std::deque<SimDataPoint> batch; std::vector<QString> logBatch;
    { std::lock_guard<std::mutex> lock(m_bufferMutex); if (!m_simBuffer.empty()) m_simBuffer.swap(batch); }
    { std::lock_guard<std::mutex> lock(m_logMutex); if (!m_logBuffer.empty()) m_logBuffer.swap(logBatch); }

    for (const QString& msg : logBatch) Q_EMIT outputReceived(msg);

    std::vector<double> times; std::vector<std::vector<double>> valueRows;
    times.reserve(batch.size()); valueRows.reserve(batch.size());
    for (const auto& p : batch) { times.push_back(p.time); valueRows.push_back(p.values); }

    QStringList names;
    { std::lock_guard<std::mutex> lock(m_vectorMutex);
      for (const auto& v : m_vectorMap) if (!v.isScale) names << v.name; }

    if (!times.empty()) {
        Q_EMIT realTimeDataBatchReceived(times, valueRows, names);
    }
}

int SimulationManager::cbSendChar(char* output, int id, void* userData) {
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (!self || !output) return 0;
    QString msg = QString::fromLatin1(output);
    if (msg.startsWith("stderr ")) msg.remove(0, 7); else if (msg.startsWith("stdout ")) msg.remove(0, 7);
    QString lower = msg.trimmed().toLower();
    if (lower == "warning: there is no circuit loaded.") return 0;

    {
        std::lock_guard<std::mutex> lock(self->m_logMutex);
        bool fatal = lower.contains("ngspice.dll cannot recover") || lower.contains("awaits to be reset");
        bool isErr = lower.contains("error") || lower.contains("unknown model") || fatal;
        bool isRunFail = lower.contains("singular matrix") || lower.contains("stepping failed") || lower.contains("step too small");
        bool isWarn = lower.startsWith("warning");

        if (isErr) self->m_lastLoadFailed = true;
        if (fatal) self->m_engineRecoveryRequired = true;
        if (isRunFail) self->m_lastRunFailed = true;
        if (fatal) self->m_lastErrorMessage = msg.trimmed();
        else if (self->m_lastErrorMessage.isEmpty() && (isErr || isRunFail)) self->m_lastErrorMessage = msg.trimmed();

        // Skip verbose per-step noise in real-time/interactive mode
        bool isNoise = lower.startsWith("reference value") || lower.startsWith("referencevalue");
        if (!isNoise) {
            self->m_logBuffer.push_back(msg);
            const QString trimmed = msg.trimmed();
            // Route errors and warnings through qWarning() so they remain visible
            // in the terminal even in Release builds (QT_NO_DEBUG_OUTPUT only
            // silences qDebug()/qInfo()).
            if (isErr || isRunFail || fatal) {
                qWarning() << "[Ngspice] ERROR:" << trimmed;
            } else if (isWarn) {
                qWarning() << "[Ngspice] WARNING:" << trimmed;
            } else {
                qDebug() << "[Ngspice]" << trimmed;
            }
        }
    }
    return 0;
}

int SimulationManager::cbSendStat(char* stat, int id, void* userData) {
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (self && stat) {
        static int throttle = 0; if (++throttle % 20 != 0) return 0;
        std::lock_guard<std::mutex> lock(self->m_logMutex);
        self->m_logBuffer.push_back(QString::fromLatin1(stat));
    }
    return 0;
}

int SimulationManager::cbControlledExit(int status, bool immediate, bool quit, int id, void* userData) {
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (self) {
        // Issue 8: Do not flag recovery on benign halt/quit exits initiated during normal stopping/shutdown
        bool isIntentional = (self->m_state == SimulationState::Stopping || self->m_state == SimulationState::Idle);
        if (status != 0 || (immediate && !isIntentional) || (quit && !isIntentional)) {
            self->m_engineRecoveryRequired = true;
        }
        QMetaObject::invokeMethod(self, [self]() { self->handleSimulationFinished(""); }, Qt::QueuedConnection);
    }
    return 0;
}

#ifdef HAVE_NGSPICE
void SimulationManager::setFluxScriptTargets(const QMap<QString, Flux::FluxScriptTarget>& targets) {
    std::lock_guard<std::mutex> lock(m_fluxTargetsMutex);
    m_fluxScriptTargets = targets;
    if (isNativeSmartSignalMode()) {
        for (auto it = m_fluxScriptTargets.begin(); it != m_fluxScriptTargets.end(); ++it)
            it->outputVoltageSources.clear();
    }
}

void SimulationManager::clearFluxScriptTargets() {
    std::lock_guard<std::mutex> lock(m_fluxTargetsMutex);
    m_fluxScriptTargets.clear();
}

void SimulationManager::setSkipFactor(int factor) { m_skipFactor = std::max(1, factor); }

int SimulationManager::cbSendData(pvecvaluesall vecArray, int numStructs, int id, void* userData) {
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (!self || !vecArray) return 0;
    self->handleIncomingData(vecArray, numStructs);
    return 0;
}

void SimulationManager::handleIncomingData(void* vecArrayPtr, int numStructs) {
    pvecvaluesall vecArray = static_cast<pvecvaluesall>(vecArrayPtr);
    if (!vecArray || vecArray->veccount < 1 || !vecArray->vecsa) return;
    
    // Safety check for stop request
    if (m_stopRequested.load()) return;

    int count = ++m_streamingCounter;
    if (count % m_skipFactor != 0) return;

    std::vector<double> sampleValues; 
    sampleValues.reserve(vecArray->veccount);
    double timeValue = 0.0; 
    bool haveScale = false;
    
    for (int i = 0; i < vecArray->veccount; ++i) {
        pvecvalues v = vecArray->vecsa[i];
        if (!v) continue; 

        if (v->is_scale) {
            if (!haveScale) {
                timeValue = v->creal;
                haveScale = true;
            }
            // Skip adding scale vector (Time) to the sampleValues vector
            // so it aligns perfectly with the 'names' list which filters out isScale.
            continue; 
        }
        
        if (v->is_complex) {
            // Calculate magnitude for AC/Complex results
            double mag = std::sqrt(v->creal * v->creal + v->cimag * v->cimag);
            sampleValues.push_back(mag);
        } else {
            sampleValues.push_back(v->creal);
        }
    }
    
    // Fallback if no scale vector was explicitly marked
    if (!haveScale && vecArray->veccount > 0 && vecArray->vecsa[0]) {
        timeValue = vecArray->vecsa[0]->creal;
        haveScale = true;
    }
    
    if (!haveScale) return;

    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_simBuffer.push_back({timeValue, std::move(sampleValues)});
        if (m_simBuffer.size() > 10000) m_simBuffer.pop_front();
    }
}

int SimulationManager::cbSendInitData(pvecinfoall initData, int id, void* userData) {
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (!self || !initData) return 0;
    self->handleAnalysisMetadata(initData);
    return 0;
}

void SimulationManager::handleAnalysisMetadata(void* initDataPtr) {
    pvecinfoall initData = static_cast<pvecinfoall>(initDataPtr);
    if (!initData) return;

    bool isPaused = SpiceBackend::instance().isPaused();
    qDebug() << "[SimManager] Analysis metadata received. Title:" << (initData->title ? initData->title : "N/A") 
             << "Type:" << (initData->type ? initData->type : "N/A") 
             << "Vectors:" << initData->veccount;
    
    std::lock_guard<std::mutex> lock(m_vectorMutex);
    m_vectorMap.clear();
    if (!initData->vecs) return;

    for (int i = 0; i < initData->veccount; ++i) {
        pvecinfo v = initData->vecs[i]; 
        if (!v || !v->vecname) continue;

        VectorMap vm; 
        vm.index = i;
        vm.name = normalizeStreamVectorName(QString::fromLatin1(v->vecname));
        vm.isVoltage = (v->is_real && !vm.name.toLower().startsWith("i("));
        vm.isScale = (v->pdvec != nullptr && v->pdvec == v->pdvecscale);
        
        if (vm.isScale) continue;

        m_vectorMap.push_back(vm);
    }
    
    qDebug() << "[SimManager] Analysis started with" << m_vectorMap.size() << "plottable vectors.";
    for (const auto& vm : m_vectorMap) {
        qDebug() << "  - Vector[" << vm.index << "]:" << vm.name << (vm.isVoltage ? "(V)" : "(I)");
    }
}
#endif

int SimulationManager::cbBGThreadRunning(bool finished, int id, void* userData) {
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (!self) return 0;
    qDebug() << "[SimManager] cbBGThreadRunning: finished=" << finished << "id=" << id;
    self->handleEngineStateChange(finished, id);
    return 0;
}

void SimulationManager::handleEngineStateChange(bool finished, int id) {
    bool isPaused = SpiceBackend::instance().isPaused();
    bool stopRequested = m_stopRequested.load();
    // Determine raw path if we have a netlist file path
    QString rawPath;
    {
        std::lock_guard<std::mutex> lock(m_netlistMutex);
        if (m_currentNetlist.endsWith(".cir", Qt::CaseInsensitive)) {
            rawPath = m_currentNetlist;
            rawPath.replace(".cir", ".raw", Qt::CaseInsensitive);
        } else if (!m_currentNetlist.isEmpty() && QFileInfo::exists(m_currentNetlist)) {
            QFileInfo fi(m_currentNetlist);
            rawPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".raw";
        } else {
            // Default for temporary netlists
            rawPath = QDir::tempPath() + "/viospice.raw";
        }
    }

    if (finished && (isPaused || m_haltRequested.load())) {
        // === Halted at Sync Point ===
        if (m_haltRequested.load() && !isPaused) {
            qDebug() << "[SimManager] Engine stopped but halt was requested. Forcing Halted state.";
        }
        if (stopRequested) {
            // User requested stop, treat as finished
            setState(SimulationState::Finished);
            {
                std::lock_guard<std::mutex> lock(m_workerSyncMutex);
                m_ngspiceIsHalted = true; // Unlock worker
            }
            m_workerSyncCond.notify_all();
            QMetaObject::invokeMethod(this, "handleSimulationFinished", Qt::QueuedConnection, Q_ARG(QString, rawPath));
        } else {
            setState(SimulationState::Halted);
            {
                std::lock_guard<std::mutex> lock(m_workerSyncMutex);
                m_ngspiceIsHalted = true;
            }
            m_workerSyncCond.notify_all();
            
            if (!m_haltRequested.load()) {
                static QElapsedTimer lastAutoResume;
                if (!lastAutoResume.isValid() || lastAutoResume.elapsed() > 200) {
                    lastAutoResume.restart();
                    qDebug() << "[SimManager] Spurious halt. Auto-resuming...";
                    SpiceBackend::instance().execute("bg_resume");
                }
            }
        }
    } else if (finished && !isPaused) {
        // === Engine Terminated ===
        setState(SimulationState::Finished);
        m_stopRequested = false;
        m_haltRequested = false;
        
        {
            std::lock_guard<std::mutex> lock(m_workerSyncMutex);
            m_ngspiceIsHalted = false; // Reset for next run
        }
        m_workerSyncCond.notify_all();
        
        QMetaObject::invokeMethod(this, "handleSimulationFinished", Qt::QueuedConnection, Q_ARG(QString, rawPath));
    } else if (!finished && !isPaused) {
        // === Engine Running/Resumed ===
        setState(SimulationState::Running);
        m_stopRequested = false; // Issue 6: clear stale stop flag on resume
        { std::lock_guard<std::mutex> lock(m_controlMutex); if (m_streamingControl) QMetaObject::invokeMethod(m_bufferTimer, "start", Qt::QueuedConnection); }
        m_haltRequested = false; 
        {
            std::lock_guard<std::mutex> lock(m_workerSyncMutex);
            m_ngspiceIsHalted = false;
        }
        m_workerSyncCond.notify_all();
    }
}

void SimulationManager::handleSimulationFinished(const QString& rawPath) {
    QMetaObject::invokeMethod(m_bufferTimer, "stop", Qt::QueuedConnection); 
    processBufferedData();
    m_stopRequested = false; 

#ifdef HAVE_NGSPICE
    if (!m_lastLoadFailed && !m_lastRunFailed && !rawPath.isEmpty()) {
        // Issue 5: Execute raw-file export on worker thread without blocking GUI thread,
        // and verify actual vector presence.
        QMetaObject::invokeMethod(m_worker, [this, rawPath]() {
            pvector_info vecInfo = ngGet_Vec_Info(const_cast<char*>("all"));
            if (!vecInfo) vecInfo = ngGet_Vec_Info(const_cast<char*>("time"));
            if (!vecInfo) vecInfo = ngGet_Vec_Info(const_cast<char*>("frequency"));
            if (!vecInfo) vecInfo = ngGet_Vec_Info(const_cast<char*>("v-sweep"));
            if (!vecInfo) vecInfo = ngGet_Vec_Info(const_cast<char*>("i-sweep"));
            if (vecInfo && vecInfo->v_length > 0) {
                SpiceBackend::instance().execute("write " + rawPath);
                bool exists = QFile::exists(rawPath);
                qint64 size = exists ? QFileInfo(rawPath).size() : 0;
                if (exists && size > 0) {
                    QMetaObject::invokeMethod(this, [this, rawPath]() {
                        Q_EMIT rawResultsReady(rawPath);
                    }, Qt::QueuedConnection);
                }
            }
            QMetaObject::invokeMethod(this, [this]() {
                Q_EMIT simulationFinished();
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
        return;
    }
    if (m_lastRunFailed || m_lastLoadFailed) {
        std::lock_guard<std::mutex> lock(m_logMutex);
        if (!m_lastErrorMessage.isEmpty()) reportError(m_lastErrorMessage);
    }
#endif
    Q_EMIT simulationFinished();
}

void SimulationManager::clearCircuits() {
#ifdef HAVE_NGSPICE
    if (isRunning()) {
        // Issue 15: Confirmed-halt gate to avoid racing with worker executeSequence
        haltAndWait(std::chrono::milliseconds(1000));
    }
    sendCommandAsync("bg_halt");
    sendCommandAsync("reset");
    m_circStorage.clear(); m_circPtrs.clear();
#endif
}
