#include "simulation_manager.h"
#include "netlist_processor.h"
#include "spice_backend.h"
#include "jit_bridge.h"
#include "simulator/core/sim_results.h"
#include "jit_context_manager.h"
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
#include <dlfcn.h>

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

    if (m_manager->isRunning()) {
        qDebug() << "[SimWorker] Requesting bg_halt for alteration...";
        
        // Signal that we are intentionally halting — prevents auto-resume in the callback
        m_manager->m_haltRequested = true;
        
        {
            std::lock_guard<std::mutex> lock(m_manager->m_workerSyncMutex);
            m_manager->m_ngspiceIsHalted = false; // Reset so we can wait for the new pause event
        }
        
        SpiceBackend::instance().execute("bg_halt");
        needsResume = true;
        
        // Wait for the sync-pause callback to confirm the halt.
        // cbBGThreadRunning(finished=true, isPaused=true) will set m_ngspiceIsHalted=true.
        {
            std::unique_lock<std::mutex> lock(m_manager->m_workerSyncMutex);
            bool halted = m_manager->m_workerSyncCond.wait_for(lock, std::chrono::milliseconds(500), [this] {
                return m_manager->m_ngspiceIsHalted;
            });
            if (halted) {
                qDebug() << "[SimWorker] Halt confirmed at sync point.";
            } else {
                qDebug() << "[SimWorker] Warning: Halt timed out. Sim may have finished. Proceeding.";
                needsResume = false; // Thread may be dead — don't try to resume
                m_manager->m_haltRequested = false;
            }
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
                return !m_manager->m_ngspiceIsHalted || m_manager->m_state == SimulationState::Finished;
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
                        return !m_manager->m_ngspiceIsHalted;
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
        const QString q = rawName.trimmed();
        if (q.isEmpty()) return rawName;

        static const QRegularExpression branchRe(
            "^\\s*([A-Za-z0-9_.$:+-]+)\\s*#\\s*branch\\s*$",
            QRegularExpression::CaseInsensitiveOption);
        if (const auto m = branchRe.match(q); m.hasMatch()) {
            return QString("I(%1)").arg(m.captured(1).toUpper());
        }

        static const QRegularExpression deviceCurrentRe(
            "^@\\s*([A-Za-z0-9_.$:+-]+)\\s*\\[\\s*i[a-z]*\\s*\\]$",
            QRegularExpression::CaseInsensitiveOption);
        if (const auto m = deviceCurrentRe.match(q); m.hasMatch()) {
            return QString("I(%1)").arg(m.captured(1).toUpper());
        }

        static const QRegularExpression wrapperRe(
            "^(v|i)\\s*\\(\\s*(.+)\\s*\\)$",
            QRegularExpression::CaseInsensitiveOption);
        if (const auto m = wrapperRe.match(q); m.hasMatch()) {
            return QString("%1(%2)")
                .arg(m.captured(1).toUpper(), m.captured(2).trimmed().toUpper());
        }

        return q;
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
    void* sym = SpiceBackend::instance().resolveSymbol("ngSpice_Init");
    if (!sym) return false;

    Dl_info info;
    if (dladdr(sym, &info) && info.dli_fname) {
        QString libPath = QString::fromLocal8Bit(info.dli_fname);
        return libPath.contains("VioMATRIXC", Qt::CaseInsensitive) ||
               libPath.contains("releasesh", Qt::CaseInsensitive);
    }
    return false;
}

bool SimulationManager::isNativeSmartSignalMode() const {
    return supportsNativeLogicADevices();
}

QString SimulationManager::lastErrorMessage() const {
    std::lock_guard<std::mutex> lock(const_cast<SimulationManager*>(this)->m_logMutex);
    return m_lastErrorMessage;
}

void SimulationManager::initialize() {
    if (m_isInitialized) return;

#ifdef HAVE_NGSPICE
    QString scriptsPath = "/home/jnd/.viospice";
    qputenv("SPICE_SCRIPTS", scriptsPath.toUtf8());
    qputenv("SPICE_LIB_DIR", scriptsPath.toUtf8());

    m_isInitialized = SpiceBackend::instance().initialize(
        cbSendChar, cbSendStat, cbControlledExit, cbSendData, cbSendInitData, cbBGThreadRunning, this
    );

    if (m_isInitialized) {
        QString cmDir = "/home/jnd/cpp_projects/VioMATRIXC/releasesh/src/xspice/icm";
        QStringList cmSubDirs = {"analog", "digital", "spice2poly", "table", "tlines", "xtradev", "xtraevt"};
        
        for (const QString& sub : cmSubDirs) {
            QString cmPath = QString("%1/%2/%2.cm").arg(cmDir, sub);
            if (QFile::exists(cmPath)) {
                SpiceBackend::instance().execute(QString("codemodel %1").arg(cmPath));
            }
        }

        SpiceBackend::instance().execute("set ngbehavior=ltps");
        SpiceBackend::instance().execute("set filetype=binary");
    }
#endif
}

bool SimulationManager::isRunning() const {
    return m_state == SimulationState::Running || m_state == SimulationState::Halted;
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
    if (!isAvailable()) { Q_EMIT errorOccurred("Simulation engine not installed."); return; }
    if (!recoverEngineIfNeeded()) { Q_EMIT errorOccurred("Failed to recover simulation engine."); return; }
    if (!m_isInitialized) initialize();

#ifdef HAVE_NGSPICE
    m_currentNetlist = netlist;
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
    m_autoResumeCounter = 0;
    if (control) m_bufferTimer->start(); else m_bufferTimer->stop();

    QString error;
    if (!loadNetlistInternal(netlist, true, &error)) {
        m_bufferTimer->stop();
        if (!error.isEmpty()) Q_EMIT errorOccurred(error);
        return;
    }

    {
        std::lock_guard<std::mutex> targetLock(m_fluxTargetsMutex);
        JitBridge::instance().registerTargetsWithEngine(m_fluxScriptTargets);
    }

    Q_EMIT simulationStarted();
    
    // Apply any GUI parameters (like switch states) before starting
    applyPendingFluxSourceUpdates();
    
    SpiceBackend::instance().execute("set filetype=binary");
    SpiceBackend::instance().execute("save all");
    
    setState(SimulationState::Running);
    int rc = SpiceBackend::instance().execute("bg_run");

    if (rc != 0 || m_lastLoadFailed) {
        m_bufferTimer->stop();
        setState(SimulationState::Error);
        QString finalErr = m_lastErrorMessage.isEmpty() ? "Ngspice failed to start simulation." : m_lastErrorMessage;
        Q_EMIT errorOccurred(finalErr);
        Q_EMIT simulationFinished();
        return;
    }
#endif
}

bool SimulationManager::validateNetlist(const QString& netlist, QString* errorOut) {
    if (!isAvailable()) { if (errorOut) *errorOut = "Simulation engine not installed."; return false; }
    if (!recoverEngineIfNeeded()) { if (errorOut) *errorOut = "Failed to recover engine."; return false; }
    if (!m_isInitialized) initialize();

    m_currentNetlist = netlist;
    return loadNetlistInternal(netlist, false, errorOut);
}

bool SimulationManager::loadNetlistInternal(const QString& netlist, bool keepStorage, QString* errorOut) {
#ifdef HAVE_NGSPICE
    // Ensure background thread is stopped before resetting
    if (m_state == SimulationState::Running || m_state == SimulationState::Halted) {
        m_haltRequested = true;
        {
            std::lock_guard<std::mutex> lock(m_workerSyncMutex);
            m_ngspiceIsHalted = false;
        }
        SpiceBackend::instance().execute("bg_halt");
        
        // Wait for halt confirmation
        std::unique_lock<std::mutex> lock(m_workerSyncMutex);
        m_workerSyncCond.wait_for(lock, std::chrono::milliseconds(500), [this] {
            return m_ngspiceIsHalted;
        });
    }
    
    setState(SimulationState::Loading);
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
        // Fallback to source command
        QTemporaryFile temp(QDir::tempPath() + "/viospice_XXXX.cir");
        if (temp.open()) {
            QTextStream out(&temp);
            for (const auto& l : processResult.lines) out << l << "\n";
            out.flush(); temp.close();
            m_lastLoadFailed = false;
            rc = SpiceBackend::instance().execute("source \"" + temp.fileName() + "\"");
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
    SpiceBackend::instance().execute("bg_halt");
    SpiceBackend::instance().execute("quit");
    m_bufferTimer->stop();
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
        m_bufferTimer->stop(); 
    }
    else if (command == "bg_resume") { 
        // NOTE: Don't set Running state here - wait for handleEngineStateChange callback
        // to confirm ngspice has actually resumed. This prevents race conditions.
        if (m_streamingControl) m_bufferTimer->start(); 
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
        
        if (isErr) self->m_lastLoadFailed = true;
        if (fatal) self->m_engineRecoveryRequired = true;
        if (isRunFail) self->m_lastRunFailed = true;
        if (self->m_lastErrorMessage.isEmpty() && (isErr || isRunFail)) self->m_lastErrorMessage = msg.trimmed();
        self->m_logBuffer.push_back(msg);
        qDebug() << "[Ngspice]" << msg.trimmed();
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
        if (status != 0 || immediate || quit) self->m_engineRecoveryRequired = true;
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
        if (!v) { 
            sampleValues.push_back(0.0); 
            continue; 
        }
        sampleValues.push_back(v->creal);
        if (v->is_scale && !haveScale) { 
            timeValue = v->creal; 
            haveScale = true; 
        }
    }
    
    if (!haveScale && vecArray->veccount > 0) {
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
    bool isPaused = SpiceBackend::instance().isPaused();
    qDebug() << "[SimManager] Analysis started:" << initData->title << "Type:" << initData->type << "isPaused=" << isPaused << "ngspiceIsHalted=" << m_ngspiceIsHalted;
    
    std::lock_guard<std::mutex> lock(m_vectorMutex);
    m_vectorMap.clear();
    for (int i = 0; i < initData->veccount; ++i) {
        pvecinfo v = initData->vecs[i]; if (!v) continue;
        VectorMap vm; vm.index = i;
        vm.name = normalizeStreamVectorName(QString::fromLatin1(v->vecname));
        vm.isVoltage = (v->is_real && !vm.name.toLower().startsWith("i("));
        vm.isScale = (v->pdvec != nullptr && v->pdvec == v->pdvecscale);
        m_vectorMap.push_back(vm);
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
    qDebug() << "[SimManager] handleEngineStateChange: finished=" << finished << "isPaused=" << isPaused 
             << "haltRequested=" << m_haltRequested.load() << "stopRequested=" << stopRequested;

    // Determine raw path if we have a netlist file path
    QString rawPath;
    if (m_currentNetlist.endsWith(".cir", Qt::CaseInsensitive)) {
        rawPath = m_currentNetlist;
        rawPath.replace(".cir", ".raw", Qt::CaseInsensitive);
    } else if (!m_currentNetlist.isEmpty() && QFileInfo::exists(m_currentNetlist)) {
        QFileInfo fi(m_currentNetlist);
        rawPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".raw";
    }

    if (finished && isPaused) {
        // === Halted at Sync Point ===
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
        m_haltRequested = false; 
        {
            std::lock_guard<std::mutex> lock(m_workerSyncMutex);
            m_ngspiceIsHalted = false;
        }
        m_workerSyncCond.notify_all();
    }
}

void SimulationManager::handleSimulationFinished(const QString& rawPath) {
    m_bufferTimer->stop(); 
    processBufferedData();
    m_stopRequested = false; 

#ifdef HAVE_NGSPICE
    if (!m_lastLoadFailed && !m_lastRunFailed && !rawPath.isEmpty()) {
        QTimer::singleShot(200, this, [this, rawPath]() {
            SpiceBackend::instance().execute("set filetype=binary");
            SpiceBackend::instance().execute("write " + rawPath);
            QThread::msleep(200);
            if (QFile::exists(rawPath) && QFileInfo(rawPath).size() > 0) Q_EMIT rawResultsReady(rawPath);
            Q_EMIT simulationFinished();
        });
        return;
    }
    if (m_lastRunFailed && !m_lastErrorMessage.isEmpty()) Q_EMIT errorOccurred(m_lastErrorMessage);
#endif
    Q_EMIT simulationFinished();
}

void SimulationManager::clearCircuits() {
    m_haltRequested = true;
    sendCommandAsync("bg_halt");
    sendCommandAsync("reset");
    m_circStorage.clear(); m_circPtrs.clear();
}
