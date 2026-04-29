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
#include <utility>
#include <dlfcn.h>

using namespace Flux;

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
}

SimulationManager::~SimulationManager() {}

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
    return m_bgRunIssued.load();
}

bool SimulationManager::recoverEngineIfNeeded() {
#ifdef HAVE_NGSPICE
    if (!m_engineRecoveryRequired.exchange(false)) return true;

    qWarning() << "[SimManager] Recovering engine after fatal state.";
    m_bgRunIssued = false;
    m_stopRequested = false;
    m_pauseRequested = false;

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
    m_bgRunIssued = false;
    m_stopRequested = false;
    m_pauseRequested = false;
    m_haltRequested = false;
    m_fluxSyncRequested = false;
    m_jitUpdateInProgress = false;

    { std::lock_guard<std::mutex> lock(m_bufferMutex); m_simBuffer.clear(); }
    { std::lock_guard<std::mutex> lock(m_jitSyncMutex); m_pendingHighPriorityUpdates.clear(); }
    { std::lock_guard<std::mutex> lock(m_logMutex); m_logBuffer.clear(); }
    
    m_streamingCounter = 0;
    m_skipFactor = 1;
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
    SpiceBackend::instance().execute("set filetype=binary");
    SpiceBackend::instance().execute("save all");
    
    int rc = SpiceBackend::instance().execute("bg_run");
    SpiceBackend::instance().execute("save all");

    if (rc != 0 || m_lastLoadFailed) {
        m_bufferTimer->stop();
        m_bgRunIssued = false;
        QString finalErr = m_lastErrorMessage.isEmpty() ? "Ngspice failed to start simulation." : m_lastErrorMessage;
        Q_EMIT errorOccurred(finalErr);
        Q_EMIT simulationFinished();
        return;
    }
    m_bgRunIssued = true;
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
    m_stopRequested = true;
    SpiceBackend::instance().execute("bg_halt");
    QMetaObject::invokeMethod(m_bufferTimer, "stop", Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, "processBufferedData", Qt::QueuedConnection);
#endif
}

void SimulationManager::shutdown() {
#ifdef HAVE_NGSPICE
    m_bgRunIssued = false;
    SpiceBackend::instance().execute("bg_halt");
    SpiceBackend::instance().execute("quit");
    m_bufferTimer->stop();
    m_isInitialized = false;
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
    if (command == "bg_halt") { m_haltRequested = true; m_bufferTimer->stop(); }
    else if (command == "bg_resume") { m_pauseRequested = false; if (m_streamingControl) m_bufferTimer->start(); }
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
    if (m_isInitialized && m_bgRunIssued) {
        { std::lock_guard<std::mutex> lock(m_jitSyncMutex); m_pendingInternalCommands.append(cmd); }
        QMetaObject::invokeMethod(this, [this]() { requestFluxSourceSync(); }, Qt::QueuedConnection);
    }
#endif
}

void SimulationManager::requestFluxSourceSync() {
#ifdef HAVE_NGSPICE
    if (!m_isInitialized || !m_bgRunIssued || m_stopRequested || m_pauseRequested) return;
    if (m_jitUpdateInProgress || m_fluxSyncRequested) return;

    { std::lock_guard<std::mutex> lock(m_jitSyncMutex);
      if (m_pendingHighPriorityUpdates.isEmpty() && m_pendingInternalCommands.isEmpty()) return; }

    m_jitUpdateInProgress = true; m_fluxSyncRequested = true; m_haltRequested = true;
    SpiceBackend::instance().execute("bg_halt");
#endif
}

void SimulationManager::applyPendingFluxSourceUpdates() {
#ifdef HAVE_NGSPICE
    QMap<QString, double> updates; QStringList commands;
    { std::lock_guard<std::mutex> lock(m_jitSyncMutex);
      updates.swap(m_pendingHighPriorityUpdates); commands.swap(m_pendingInternalCommands); }

    if (updates.isEmpty() && commands.isEmpty()) {
        if (m_bgRunIssued && !m_stopRequested && !m_pauseRequested) {
            if (m_streamingControl) m_bufferTimer->start();
            SpiceBackend::instance().execute("bg_resume");
        }
        m_fluxSyncRequested = false; m_haltRequested = false; m_jitUpdateInProgress = false;
        return;
    }

    for (const QString& cmd : commands) SpiceBackend::instance().execute(cmd);
    for (auto it = updates.constBegin(); it != updates.constEnd(); ++it) {
        QString target = it.key(); double val = it.value(); QString valStr = QString::number(val, 'g', 12);
        if (target.startsWith('V', Qt::CaseInsensitive) || target.startsWith('I', Qt::CaseInsensitive)) {
            SpiceBackend::instance().execute(QString("alter @%1[dc] = %2").arg(target, valStr));
            SpiceBackend::instance().execute(QString("alter %1 dc=%2").arg(target, valStr));
        } else if (target.startsWith('R', Qt::CaseInsensitive)) {
            SpiceBackend::instance().execute(QString("alter @%1[resistance] = %2").arg(target, valStr));
            SpiceBackend::instance().execute(QString("alter %1 resistance = %2").arg(target, valStr));
        } else if (target.startsWith('C', Qt::CaseInsensitive)) {
            SpiceBackend::instance().execute(QString("alter @%1[capacitance] = %2").arg(target, valStr));
            SpiceBackend::instance().execute(QString("alter %1 capacitance = %2").arg(target, valStr));
        } else if (target.startsWith('L', Qt::CaseInsensitive)) {
            SpiceBackend::instance().execute(QString("alter @%1[inductance] = %2").arg(target, valStr));
            SpiceBackend::instance().execute(QString("alter %1 inductance = %2").arg(target, valStr));
        } else {
            SpiceBackend::instance().execute(QString("alter %1 %2").arg(target, valStr));
        }
    }

    if (m_bgRunIssued && !m_stopRequested && !m_pauseRequested) {
        if (m_streamingControl) m_bufferTimer->start();
        SpiceBackend::instance().execute("bg_resume");
    }
    m_fluxSyncRequested = false; m_haltRequested = false; m_jitUpdateInProgress = false;

    bool hasMore = false;
    { std::lock_guard<std::mutex> lock(m_jitSyncMutex); hasMore = !m_pendingHighPriorityUpdates.isEmpty(); }
    if (hasMore) QMetaObject::invokeMethod(this, [this]() { requestFluxSourceSync(); }, Qt::QueuedConnection);
#endif
}

void SimulationManager::processBufferedData() {
    std::vector<SimDataPoint> batch; std::vector<QString> logBatch;
    { std::lock_guard<std::mutex> lock(m_bufferMutex); if (!m_simBuffer.empty()) m_simBuffer.swap(batch); }
    { std::lock_guard<std::mutex> lock(m_logMutex); if (!m_logBuffer.empty()) m_logBuffer.swap(logBatch); }

    for (const QString& msg : logBatch) Q_EMIT outputReceived(msg);

    std::vector<double> times; std::vector<std::vector<double>> valueRows;
    times.reserve(batch.size()); valueRows.reserve(batch.size());
    for (const auto& p : batch) { times.push_back(p.time); valueRows.push_back(p.values); }

    QStringList names;
    { std::lock_guard<std::mutex> lock(m_vectorMutex);
      for (const auto& v : m_vectorMap) if (!v.isScale) names << v.name; }

    Q_EMIT realTimeDataBatchReceived(times, valueRows, names);
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
    
    bool stopReq = false;
    { std::lock_guard<std::mutex> lock(self->m_controlMutex); if (self->m_streamingControl) stopReq = self->m_streamingControl->stopRequested; }
    if (stopReq) SpiceBackend::instance().execute("bg_halt");

    if (vecArray->veccount <= 1 || !vecArray->vecsa || ++self->m_streamingCounter % self->m_skipFactor != 0) return 0;

    std::vector<double> sampleValues; sampleValues.reserve(vecArray->veccount);
    double timeValue = 0.0; bool haveScale = false;
    for (int i = 0; i < vecArray->veccount; ++i) {
        pvecvalues v = vecArray->vecsa[i];
        if (!v) { sampleValues.push_back(0.0); continue; }
        sampleValues.push_back(v->creal);
        if (v->is_scale && !haveScale) { timeValue = v->creal; haveScale = true; }
    }
    if (!haveScale) return 0;

    {
        std::lock_guard<std::mutex> targetLock(self->m_fluxTargetsMutex);
        JitBridge::instance().executeFeedbackLoop(sampleValues, timeValue, self->m_fluxScriptTargets);
    }

    { std::lock_guard<std::mutex> lock(self->m_bufferMutex);
      self->m_simBuffer.push_back({timeValue, std::move(sampleValues)});
      if (self->m_simBuffer.size() > 2000) self->m_simBuffer.erase(self->m_simBuffer.begin(), self->m_simBuffer.begin() + (self->m_simBuffer.size() - 2000)); }
    return 0;
}

int SimulationManager::cbSendInitData(pvecinfoall initData, int id, void* userData) {
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (!self || !initData) return 0;
    std::lock_guard<std::mutex> lock(self->m_vectorMutex);
    self->m_vectorMap.clear();
    for (int i = 0; i < initData->veccount; ++i) {
        pvecinfo v = initData->vecs[i]; if (!v) continue;
        VectorMap vm; vm.index = i;
        vm.name = normalizeStreamVectorName(QString::fromLatin1(v->vecname));
        vm.isVoltage = (v->is_real && !vm.name.toLower().startsWith("i("));
        vm.isScale = (v->pdvec != nullptr && v->pdvec == v->pdvecscale);
        self->m_vectorMap.push_back(vm);
    }
    return 0;
}
#endif

int SimulationManager::cbBGThreadRunning(bool finished, int id, void* userData) {
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (!self || !finished) return 0;

    if (!self->m_stopRequested && (SpiceBackend::instance().isPaused() || self->m_haltRequested)) {
        if (self->m_jitUpdateInProgress) QMetaObject::invokeMethod(self, [self]() { self->applyPendingFluxSourceUpdates(); }, Qt::QueuedConnection);
        QMetaObject::invokeMethod(self->m_bufferTimer, "stop", Qt::QueuedConnection);
        QMetaObject::invokeMethod(self, "processBufferedData", Qt::QueuedConnection);
        return 0;
    }

    self->m_bgRunIssued = false; self->m_stopRequested = false;
    QFileInfo info(self->m_currentNetlist);
    QString rawPath = info.absolutePath() + "/" + info.completeBaseName() + ".raw";
    QMetaObject::invokeMethod(self, [self, rawPath]() { self->handleSimulationFinished(rawPath); }, Qt::QueuedConnection);
    return 0;
}

void SimulationManager::handleSimulationFinished(const QString& rawPath) {
    m_bufferTimer->stop(); 
    processBufferedData();
    m_bgRunIssued = false; 
    m_stopRequested = false; 
    m_pauseRequested = false;

#ifdef HAVE_NGSPICE
    if (!m_lastLoadFailed && !m_lastRunFailed && !rawPath.isEmpty()) {
        QTimer::singleShot(200, this, [this, rawPath]() {
            SpiceBackend::instance().execute("set filetype=ascii");
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
    SpiceBackend::instance().execute("reset");
    m_circStorage.clear(); m_circPtrs.clear();
}
