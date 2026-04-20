#include "simulation_manager.h"
#include "simulator/core/sim_results.h"
#include "jit_context_manager.h"
#include "schematic/items/smart_signal_item.h"
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

namespace {
    /**
     * Resolves a file path case-insensitively.
     * This is needed because ngspice lowercases paths, but Linux filesystems are case-sensitive.
     * If the exact path doesn't exist, we scan the directory for a case-insensitive match.
     */
    QString resolveCaseInsensitiveFilePath(const QString& path) {
        QFileInfo fi(path);
        // If the file exists exactly as specified, return it immediately.
        if (fi.exists()) return path;
        
        QDir dir(fi.absolutePath());
        if (!dir.exists()) return path;
        
        QString target = fi.fileName().toLower();
        // Scan directory for a matching filename (case-insensitive)
        const auto entries = dir.entryList(QDir::Files);
        for (const QString& entry : entries) {
            if (entry.toLower() == target) {
                return dir.absoluteFilePath(entry);
            }
        }
        return path; // Return original if not found
    }

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

    // Launch high-priority JIT sync thread
    m_jitSyncThreadRunning = true;
    m_jitSyncThread = std::thread([this]() {
        while (m_jitSyncThreadRunning) {
            QMap<QString, double> updates;
            {
                std::unique_lock<std::mutex> lock(m_jitSyncMutex);
                m_jitSyncCond.wait(lock, [this]() { 
                    return !m_pendingHighPriorityUpdates.isEmpty() || !m_jitSyncThreadRunning; 
                });
                
                if (!m_jitSyncThreadRunning) break;
                
                updates = m_pendingHighPriorityUpdates;
                m_pendingHighPriorityUpdates.clear();
            }

            if (updates.isEmpty()) continue;
            
            // CRITICAL: Wait for initial data stream to stabilize
            // If we alter during the first few steps, spSolve/spFactor will crash
            if (m_streamingCounter < 5) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // 1. Halt simulation
            m_jitUpdateInProgress = true;
            {
                std::lock_guard<std::mutex> apiLock(m_ngspiceMutex);
                if (!m_bgRunIssued) { // Abort if simulation finished while waiting for mutex
                    m_jitUpdateInProgress = false;
                    continue;
                }
                ngSpice_Command((char*)"bg_halt");
            }
            
            // Wait for halt (max 500ms)
            int waitCount = 0;
            while (ngSpice_running() && waitCount < 250) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                waitCount++;
            }

            // Check if we stopped because it finished or because we halted it
            if (!ngSpice_running() && m_bgRunIssued) {
                // Extended safety sleep for sparse matrix stabilization
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                
                std::lock_guard<std::mutex> apiLock(m_ngspiceMutex);
                
                // Double check logical state under lock
                if (!m_bgRunIssued) {
                    m_jitUpdateInProgress = false;
                    continue;
                }

                // 2. Apply updates
                for (auto it = updates.constBegin(); it != updates.constEnd(); ++it) {
                    const QString cmd = QString("alter @%1[dc] = %2")
                                            .arg(it.key(), QString::number(it.value(), 'g', 12));
                    ngSpice_Command(cmd.toLatin1().data());
                }
                
                // 3. Resume
                ngSpice_Command((char*)"bg_resume");
            }
            m_jitUpdateInProgress = false;
        }
    });
}

SimulationManager::~SimulationManager() {
    m_jitSyncThreadRunning = false;
    m_jitSyncCond.notify_all();
    if (m_jitSyncThread.joinable()) {
        m_jitSyncThread.join();
    }
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
    // Guardrail:
    // Native LT-style `A... DLATCH|DFF|AND...` netlists only work with the
    // VioMATRIXC ngspice build because that frontend rewrites them during
    // deck parsing. Plain shared-ngspice does not perform that transform, so
    // the schematic generator must stay on the explicit mixed-mode bridge path
    // unless this probe returns true.
    Dl_info info;
    QString libPath;
    if (dladdr((void*)ngSpice_Init, &info) && info.dli_fname) {
        libPath = QString::fromLocal8Bit(info.dli_fname);
    } else if (dladdr((void*)ngSpice_Command, &info) && info.dli_fname) {
        libPath = QString::fromLocal8Bit(info.dli_fname);
    }

    return libPath.contains("VioMATRIXC", Qt::CaseInsensitive) ||
           libPath.contains("releasesh", Qt::CaseInsensitive);
#else
    return false;
#endif
}

QString SimulationManager::lastErrorMessage() const {
    std::lock_guard<std::mutex> lock(const_cast<SimulationManager*>(this)->m_logMutex);
    return m_lastErrorMessage;
}

void SimulationManager::initialize() {
    if (m_isInitialized) return;

#ifdef HAVE_NGSPICE
    // Set environment variables for ngspice to find spinit and code models
    // This must be done BEFORE ngSpice_Init
    QString scriptsPath = "/home/jnd/.ngspice";
    if (qEnvironmentVariableIsEmpty("SPICE_SCRIPTS")) {
        qputenv("SPICE_SCRIPTS", scriptsPath.toUtf8());
    }
    if (qEnvironmentVariableIsEmpty("SPICE_LIB_DIR")) {
        qputenv("SPICE_LIB_DIR", scriptsPath.toUtf8());
    }

    // Initialize ngspice with our callbacks
    // we pass 'this' as userData to route callbacks back to the instance
    ngSpice_Init(
        cbSendChar,
        cbSendStat,
        cbControlledExit,
        cbSendData,
        cbSendInitData,
        cbBGThreadRunning,
        this
    );
    m_isInitialized = true;
    qDebug() << "Ngspice initialized";
    {
        std::lock_guard<std::mutex> lock(m_ngspiceMutex);
        ngSpice_Command((char*)"set ngbehavior=ltps");
        ngSpice_Command((char*)"set filetype=binary");
    }
#else
    qWarning() << "Ngspice not available (HAVE_NGSPICE not defined)";
#endif
}

bool SimulationManager::isRunning() const {
#ifdef HAVE_NGSPICE
    std::lock_guard<std::mutex> lock(const_cast<SimulationManager*>(this)->m_ngspiceMutex);
    return ngSpice_running();
#else
    return false;
#endif
}

void SimulationManager::runSimulation(const QString& netlist, SimControl* control) {
    if (!isAvailable()) {
        Q_EMIT errorOccurred("Simulation engine not installed.");
        return;
    }
    if (!m_isInitialized) initialize();

#ifdef HAVE_NGSPICE
    m_currentNetlist = netlist;
    {
        std::lock_guard<std::mutex> lock(m_controlMutex);
        m_streamingControl = control;
    }
    {
        std::lock_guard<std::mutex> lock(m_vectorMutex);
        m_vectorMap.clear();
    }
    m_lastLoadFailed = false;
    m_lastRunFailed = false;
    m_bgRunIssued = false;
    m_stopRequested = false;
    m_pauseRequested = false;

    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_simBuffer.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_logMutex);
        m_logBuffer.clear();
    }
    m_streamingCounter = 0;
    m_skipFactor = 1;
    if (control) {
        m_bufferTimer->start();
    } else {
        m_bufferTimer->stop();
    }

    QString error;
    const bool loaded = loadNetlistInternal(netlist, true, &error);
    if (!loaded) {
        m_bufferTimer->stop();
        if (!error.isEmpty()) Q_EMIT errorOccurred(error);
        return;
    }

    Q_EMIT simulationStarted();
    m_streamingCounter = 0; // Reset for stabilization period
    {
        std::lock_guard<std::mutex> lock(m_ngspiceMutex);
        ngSpice_Command(const_cast<char*>("set filetype=binary"));
        const int rc = ngSpice_Command((char*)"bg_run");
        if (rc != 0 || m_lastLoadFailed) {
            m_bufferTimer->stop();
            m_bgRunIssued = false;
            QString finalErr = "Ngspice failed to start simulation.";
            if (!m_lastErrorMessage.isEmpty()) finalErr = m_lastErrorMessage;
            Q_EMIT errorOccurred(finalErr);
            Q_EMIT simulationFinished();
            return;
        }
    }
    m_bgRunIssued = true;
#endif
}

bool SimulationManager::validateNetlist(const QString& netlist, QString* errorOut) {
    if (!isAvailable()) {
        if (errorOut) *errorOut = "Simulation engine not installed.";
        return false;
    }
    if (!m_isInitialized) initialize();
#ifdef HAVE_NGSPICE
    m_currentNetlist = netlist;
    QString error;
    const bool loaded = loadNetlistInternal(netlist, false, &error);
    if (!loaded && errorOut) *errorOut = error;
    return loaded;
#else
    if (errorOut) *errorOut = "Ngspice not available in this build.";
    return false;
#endif
}

bool SimulationManager::loadNetlistInternal(const QString& netlist, bool keepStorage, QString* errorOut) {
#ifdef HAVE_NGSPICE
    auto emitNumberedNetlist = [this](const QStringList& numberedLines, const QString& header) {
        Q_EMIT outputReceived(header);
        for (int i = 0; i < numberedLines.size(); ++i) {
            Q_EMIT outputReceived(QString("%1: %2").arg(i + 1, 4).arg(numberedLines.at(i)));
        }
    };

    // Guardrail:
    // The command ordering below is sensitive. `vicompat` must be set before
    // ngSpice_Circ()/source loads the deck because VioMATRIXC performs LT-style
    // A-device rewriting during parse/load, not after. Reordering this block or
    // moving `set vicompat=lt` into the generated netlist will break native
    // digital devices and force probes back onto internal bridge nodes.
    {
        std::lock_guard<std::mutex> lock(m_ngspiceMutex);
        ngSpice_Command((char*)"reset");
    }
    {
        std::lock_guard<std::mutex> lock(m_ngspiceMutex);
        ngSpice_Command((char*)"set ngbehavior=ltps");
        ngSpice_Command((char*)"set filetype=binary");
    }
    if (supportsNativeLogicADevices()) {
        // VioMATRIXC's LT-style A-device rewrite happens during deck load, so
        // compat mode must be set before ngSpice_Circ()/source parses the netlist.
        std::lock_guard<std::mutex> lock(m_ngspiceMutex);
        ngSpice_Command((char*)"set vicompat=lt");
    } else {
        std::lock_guard<std::mutex> lock(m_ngspiceMutex);
        ngSpice_Command((char*)"set vicompat=all");
    }
    {
        std::lock_guard<std::mutex> lock(m_logMutex);
        m_lastLoadFailed = false;
        m_lastRunFailed = false;
        m_lastErrorMessage.clear();
    }

    bool loaded = false;
    QFile file(netlist);
    QStringList lines;
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            lines << in.readLine();
        }
        file.close();

        if (!lines.isEmpty() && lines.first().startsWith(QChar(0xFEFF))) {
            lines.first().remove(0, 1);
        }

        // Fix WAV file paths: ngspice lowercases paths which breaks case-sensitive filesystems (Linux).
        // We find WAVEFILE entries and resolve the correct case from the disk before passing to ngspice.
        QDir baseDir = QFileInfo(netlist).absoluteDir();
        QRegularExpression wavRe(R"REGEX(WAVEFILE\s*=?\s*"([^"]+)")REGEX", QRegularExpression::CaseInsensitiveOption);

        for (int i = 0; i < lines.size(); ++i) {
            auto match = wavRe.match(lines[i]);
            if (match.hasMatch()) {
                QString rawPath = match.captured(1);
                QString fullPath = QFileInfo(rawPath).isAbsolute() ? rawPath : baseDir.absoluteFilePath(rawPath);
                QString resolved = resolveCaseInsensitiveFilePath(fullPath);
                
                // If we found a case-insensitive match that differs from the input, replace it in the line
                if (resolved != fullPath) {
                    lines[i].replace(rawPath, resolved);
                    qInfo() << "[SimulationManager] Auto-corrected WAV path case:" << resolved;
                }
            }
        }

        int firstNonEmpty = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (!lines.at(i).trimmed().isEmpty()) { firstNonEmpty = i; break; }
        }
        if (firstNonEmpty >= 0) {
            const QString head = lines.at(firstNonEmpty).trimmed();
            if (head.startsWith(".") || head.startsWith("*")) {
                lines.insert(firstNonEmpty, "Viospice Netlist");
            }
        } else {
            lines << "Viospice Netlist";
        }

        QStringList filtered;
        bool inControl = false;
        for (const QString& line : lines) {
            const QString trimmed = line.trimmed().toLower();
            if (trimmed.startsWith(".control")) { inControl = true; continue; }
            if (inControl) {
                if (trimmed.startsWith(".endc")) inControl = false;
                continue;
            }
            filtered << line;
        }
        lines = filtered;

        bool hasEnd = false;
        for (int i = lines.size() - 1; i >= 0; --i) {
            const QString trimmed = lines.at(i).trimmed();
            if (trimmed.isEmpty()) continue;
            hasEnd = trimmed.toLower().startsWith(".end");
            break;
        }
        if (!hasEnd) lines << ".end";

        m_circStorage.clear();
        m_circPtrs.clear();
        m_circStorage.reserve(static_cast<size_t>(lines.size() + 1));
        m_circPtrs.reserve(static_cast<size_t>(lines.size() + 1));
        for (const QString& line : lines) {
            m_circStorage.push_back(line.toLatin1());
            m_circPtrs.push_back(m_circStorage.back().data());
        }
        m_circPtrs.push_back(nullptr);

        int rc = 0;
        {
            std::lock_guard<std::mutex> lock(m_ngspiceMutex);
            rc = ngSpice_Circ(m_circPtrs.data());
        }
        loaded = (rc == 0 && !m_lastLoadFailed);
        if (!loaded) {
            const QString reason = m_lastErrorMessage.isEmpty()
                ? QString("rc=%1").arg(rc)
                : QString("rc=%1, last error: %2").arg(rc).arg(m_lastErrorMessage);
            Q_EMIT outputReceived(QString("Ngspice: failed to load circuit via ngSpice_Circ (%1), falling back to source.").arg(reason));
            emitNumberedNetlist(lines, "[SIM_DEBUG] Shared-ngspice numbered netlist:");
        }
    }

    if (!loaded) {
        QTemporaryFile temp(QDir::tempPath() + "/viospice_netlist_XXXXXX.cir");
        temp.setAutoRemove(false);
        QString sourcePath = netlist;
        if (temp.open()) {
            QTextStream out(&temp);
            for (const QString& line : lines) out << line << "\n";
            out.flush();
            temp.close();
            sourcePath = temp.fileName();
        }
        m_lastLoadFailed = false;
        QString cmd = "source \"" + sourcePath + "\"";
        int rc = 0;
        {
            std::lock_guard<std::mutex> lock(m_ngspiceMutex);
            rc = ngSpice_Command(cmd.toLatin1().data());
        }
        QFile::remove(sourcePath);
        if (rc != 0) {
            emitNumberedNetlist(lines, "[SIM_DEBUG] File-source numbered netlist after load failure:");
            if (errorOut) {
                *errorOut = m_lastErrorMessage.isEmpty() ? "Failed to load netlist into ngspice." : m_lastErrorMessage;
            }
            if (!keepStorage) { m_circStorage.clear(); m_circPtrs.clear(); }
            return false;
        }
        loaded = (rc == 0 && !m_lastLoadFailed);
    }

    if (!loaded && !file.exists()) {
        if (errorOut) *errorOut = "Netlist file not found.";
        if (!keepStorage) { m_circStorage.clear(); m_circPtrs.clear(); }
        return false;
    }

    if (!loaded) {
        Q_EMIT outputReceived("Ngspice: no circuits loaded after load attempt.");
        if (errorOut) *errorOut = "No circuits loaded. Check netlist syntax.";
        if (!keepStorage) { m_circStorage.clear(); m_circPtrs.clear(); }
        return false;
    }

    if (!keepStorage) {
        m_circStorage.clear();
        m_circPtrs.clear();
    }
    return true;
#else
    if (errorOut) *errorOut = "Ngspice not available in this build.";
    return false;
#endif
}

void SimulationManager::stopSimulation() {
#ifdef HAVE_NGSPICE
    {
        std::lock_guard<std::mutex> lock(m_controlMutex);
        m_streamingControl = nullptr;
    }
    m_stopRequested = true;
    m_pauseRequested = false;
    ngSpice_Command((char*)"bg_halt");
    QMetaObject::invokeMethod(m_bufferTimer, "stop", Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, "processBufferedData", Qt::QueuedConnection);
#endif
}

void SimulationManager::shutdown() {
#ifdef HAVE_NGSPICE
    ngSpice_Command((char*)"bg_halt");
    ngSpice_Command((char*)"quit");
    QMetaObject::invokeMethod(m_bufferTimer, "stop", Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, "processBufferedData", Qt::QueuedConnection);
    m_isInitialized = false;
#endif
}

double SimulationManager::getVectorValue(const QString& name) {
#ifdef HAVE_NGSPICE
    if (!m_isInitialized) return 0.0;
    
    // We use ngGet_Vec_Info which is safe to call during simulation 
    // if ngspice is compiled with shared library support.
    pvector_info info = ngGet_Vec_Info(name.toLatin1().data());
    if (info && info->v_realdata) {
        // For transient/DC it's usually 1 point if we are paused, 
        // or we get the "current" point (the last one).
        if (info->v_length > 0) {
            return info->v_realdata[info->v_length - 1];
        }
    }
#endif
    return 0.0;
}

int SimulationManager::getVectorIndex(const QString& name) const {
    std::lock_guard<std::mutex> lock(m_vectorMutex);
    const QString target = name.toLower().trimmed();
    for (const auto& vm : m_vectorMap) {
        if (vm.name.toLower() == target) return vm.index;
        // Also support "V(node)" shorthand matching
        if (vm.name.toLower() == "v(" + target + ")") return vm.index;
    }
    return -1;
}

void SimulationManager::setParameter(const QString& name, double value) {
#ifdef HAVE_NGSPICE
    if (!m_isInitialized) return;
    
    // Use 'alter' or 'set' command depending on what it is.
    // For component values (e.g. R1), use 'alter'. 
    // For global params, use 'set'.
    QString cmd;
    if (name.contains('.')) {
        // Component parameter: R1.R = 10k
        QStringList parts = name.split('.');
        cmd = QString("alter %1 %2 = %3").arg(parts[0], parts[1], QString::number(value, 'g', 12));
    } else {
        cmd = QString("set %1=%2").arg(name, QString::number(value, 'g', 12));
    }
    
    ngSpice_Command(cmd.toLatin1().data());
#endif
}

void SimulationManager::sendInternalCommand(const QString& command) {
#ifdef HAVE_NGSPICE
    if (!m_isInitialized) return;
    if (command == "bg_halt") {
        m_pauseRequested = true;
        m_stopRequested = false;
        QMetaObject::invokeMethod(m_bufferTimer, "stop", Qt::QueuedConnection);
    } else if (command == "bg_resume") {
        m_pauseRequested = false;
        m_stopRequested = false;
        if (m_streamingControl) {
            QMetaObject::invokeMethod(m_bufferTimer, "start", Qt::QueuedConnection);
        }
    }
    ngSpice_Command(command.toLatin1().data());
#endif
}

// --- Real-Time Switch Control ---

void SimulationManager::alterSwitch(const QString& switchRef, bool open, double vt, double vh) {
#ifdef HAVE_NGSPICE
    if (!m_isInitialized) return;

    // VioSpice implements switches as resistors, not voltage-controlled switches.
    // Netlist generator creates: R<ref> n1 n2 <value>
    //   Open state:  1e12 ohms (high resistance)
    //   Closed state: 0.001 ohms (low resistance)
    QString rName = switchRef;
    if (!rName.startsWith("R", Qt::CaseInsensitive)) rName = "R" + rName;

    double resistance = open ? 1e12 : 0.001;
    alterSwitchResistance(rName, resistance);
#endif
}

void SimulationManager::alterSwitchResistance(const QString& resistorName, double resistance) {
#ifdef HAVE_NGSPICE
    if (!m_isInitialized) return;

    // Set flag to prevent simulationFinished() from being emitted
    // when the background thread stops due to bg_halt
    m_switchToggleInProgress = true;

    // 1. Halt the simulation (pauses, doesn't fully stop)
    sendInternalCommand("bg_halt");

    // Small delay to ensure simulation has stopped
    QThread::msleep(20);

    // 2. Alter the resistor value
    // ngspice syntax: alter Rname R=value
    QString cmd = QString("alter %1 R=%2").arg(resistorName, QString::number(resistance, 'g', 12));
    ngSpice_Command(cmd.toLatin1().data());

    // 3. Resume simulation (bg_resume continues from paused state, bg_run would restart)
    sendInternalCommand("bg_resume");

    // Clear flag after resume
    m_switchToggleInProgress = false;
#endif
}

void SimulationManager::alterSwitchVoltage(const QString& controlSourceName, double voltage) {
#ifdef HAVE_NGSPICE
    if (!m_isInitialized) return;

    // Use bg_halt -> alter -> bg_run cycle for voltage-controlled switches
    // (for advanced users who use .model SW with VSWITCH)

    // 1. Halt the simulation
    m_switchToggleInProgress = true;
    sendInternalCommand("bg_halt");

    // Small delay to ensure simulation has stopped
    QThread::msleep(10);

    // 2. Alter the control voltage source
    QString cmd = QString("alter %1 DC=%2").arg(controlSourceName, QString::number(voltage, 'g', 12));
    ngSpice_Command(cmd.toLatin1().data());

    // 3. Resume simulation
    sendInternalCommand("bg_resume");
    m_switchToggleInProgress = false;
#endif
}

void SimulationManager::queueFluxSourceUpdate(const QString& sourceName, double value) {
    // Rate limit: If an update is already being processed by the sync thread, skip this one.
    // We only need the freshest value for the next possible update cycle.
    if (m_jitUpdateInProgress) return;

    {
        std::lock_guard<std::mutex> lock(m_jitSyncMutex);
        m_pendingHighPriorityUpdates[sourceName] = value;
    }
    m_jitSyncCond.notify_one();
}

void SimulationManager::processBufferedData() {
    std::vector<SimDataPoint> batch;
    std::vector<QString> logBatch;
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        if (!m_simBuffer.empty()) {
            m_simBuffer.swap(batch);
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_logMutex);
        if (!m_logBuffer.empty()) {
            m_logBuffer.swap(logBatch);
        }
    }

    for (const QString& msg : logBatch) {
        Q_EMIT outputReceived(msg);
    }

    std::vector<double> times;
    std::vector<std::vector<double>> valueRows;
    times.reserve(batch.size());
    valueRows.reserve(batch.size());

    for (const auto& p : batch) {
        times.push_back(p.time);
        valueRows.push_back(p.values);
    }

    QStringList names;
    {
        std::lock_guard<std::mutex> lock(m_vectorMutex);
        for (const auto& vector : m_vectorMap) {
            if (vector.isScale) continue;
            names << vector.name;
        }
    }

    Q_EMIT realTimeDataBatchReceived(times, valueRows, names);
}

// --- Callbacks ---

int SimulationManager::cbSendChar(char* output, int id, void* userData) {
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (self && output) {
        QString msg = QString::fromLatin1(output);
        // Clean up stderr/stdout distinction if needed
        if (msg.startsWith("stderr ")) msg.remove(0, 7);
        else if (msg.startsWith("stdout ")) msg.remove(0, 7);
        const QString trimmed = msg.trimmed();
        const QString lower = trimmed.toLower();

        // `reset` emits this before any deck is loaded. It is expected noise, not
        // a circuit-load failure, and leaving it in the log is misleading.
        // Keep this filter narrow so real parse/load failures still surface.
        if (lower == "warning: there is no circuit loaded.") {
            return 0;
        }

        {
            std::lock_guard<std::mutex> lock(self->m_logMutex);
            const bool isError =
                lower.contains("there is no circuit") ||
                lower.contains("error on line") ||
                lower.contains("unknown model type") ||
                lower.contains("unable to find definition of model") ||
                lower.contains("mif-error") ||
                lower.contains("circuit not parsed") ||
                lower.contains("ngspice.dll cannot recover") ||
                lower.contains("could not find include file");
            const bool isRunFailure =
                lower.contains("singular matrix") ||
                lower.contains("dynamic gmin stepping failed") ||
                lower.contains("true gmin stepping failed") ||
                lower.contains("source stepping failed") ||
                lower.contains("time-step too small") ||
                lower.contains("timestep too small") ||
                lower.contains("tran simulation(s) aborted") ||
                lower.contains("vector ") && lower.contains("zero length");
            const bool isWarning = lower.contains("warning");

            if (isError) {
                self->m_lastLoadFailed = true;
                if (self->m_lastErrorMessage.isEmpty()) {
                    self->m_lastErrorMessage = trimmed;
                }
            }
            if (isRunFailure) {
                self->m_lastRunFailed = true;
                if (self->m_lastErrorMessage.isEmpty()) {
                    self->m_lastErrorMessage = trimmed;
                }
            }
            
            // Capture specific "Error:" prefix even if not in the known failure list
            if (self->m_lastErrorMessage.isEmpty() && (msg.startsWith("Error:", Qt::CaseInsensitive) || msg.startsWith("Fatal error:", Qt::CaseInsensitive))) {
                self->m_lastErrorMessage = trimmed;
            }
            self->m_logBuffer.push_back(msg);

            if (isError || isWarning) {
                qWarning() << "[Ngspice]" << trimmed;
            }
        }
    }
    return 0;
}

int SimulationManager::cbSendStat(char* stat, int id, void* userData) {
    // Progress updates - Throttled
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (self && stat) {
        static int throttleCounter = 0;
        if (++throttleCounter % 20 != 0) return 0; // Skip 95% of stats

        QString msg = QString::fromLatin1(stat);
        // Usually contains "% complete" or time info
        {
            std::lock_guard<std::mutex> lock(self->m_logMutex);
            self->m_logBuffer.push_back(msg);
        }
    }
    return 0;
}

int SimulationManager::cbControlledExit(int status, bool immediate, bool quit, int id, void* userData) {
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (self) {
        qDebug() << "Ngspice exit request:" << status << " Thread:" << QThread::currentThreadId();
    }
    return 0;
}

#ifdef HAVE_NGSPICE
void SimulationManager::setFluxScriptTargets(const QMap<QString, FluxScriptTarget>& targets) {
    std::lock_guard<std::mutex> lock(m_fluxTargetsMutex);
    m_fluxScriptTargets = targets;
}

void SimulationManager::clearFluxScriptTargets() {
    std::lock_guard<std::mutex> lock(m_fluxTargetsMutex);
    m_fluxScriptTargets.clear();
}

void SimulationManager::setSkipFactor(int factor) {
    m_skipFactor = std::max(1, factor);
}

int SimulationManager::cbSendData(pvecvaluesall vecArray, int numStructs, int id, void* userData) {
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (!self || !vecArray) return 0;
    Q_UNUSED(numStructs);
    Q_UNUSED(id);

    // Check for user abort
    bool hasControl = false;
    bool stopRequested = false;
    {
        std::lock_guard<std::mutex> lock(self->m_controlMutex);
        if (self->m_streamingControl) {
            hasControl = true;
            stopRequested = self->m_streamingControl->stopRequested;
        }
    }

    if (stopRequested) {
        ngSpice_Command((char*)"bg_halt");
        return 0;
    }

    if (!hasControl || vecArray->veccount <= 1 || !vecArray->vecsa) {
        return 0;
    }

    if (++self->m_streamingCounter % self->m_skipFactor != 0) {
        return 0;
    }

    std::vector<double> sampleValues;
    sampleValues.reserve(static_cast<size_t>(vecArray->veccount));
    double timeValue = 0.0;
    bool haveScale = false;

    for (int i = 0; i < vecArray->veccount; ++i) {
        pvecvalues value = vecArray->vecsa[i];
        if (!value) {
            sampleValues.push_back(0.0);
            continue;
        }
        sampleValues.push_back(value->creal);
        if (value->is_scale && !haveScale) {
            timeValue = value->creal;
            haveScale = true;
        }
    }

    if (!haveScale) {
        return 0;
    }

    // --- FluxScript JIT Feedback Loop ---
    // If we have active FluxScript targets, we execute them NOW and feed back to ngspice
    // This creates a "Mixed-Mode" real-time simulation link.
    {
        std::lock_guard<std::mutex> targetLock(self->m_fluxTargetsMutex);
        if (!self->m_fluxScriptTargets.isEmpty()) {
            // Synchronize simulation data with JIT so V() and I() helpers can read it
            Flux::JITContextManager::instance().setSimulationData(sampleValues);

            for (auto it = self->m_fluxScriptTargets.begin(); it != self->m_fluxScriptTargets.end(); ++it) {
                const QString scriptId = it.key();
                const FluxScriptTarget& target = it.value();

                // 1. Set pin-to-net mapping for this execution context
                Flux::JITContextManager::instance().setPinMapping(target.pinToNetMap);

                // 2. Gather inputs for this block
                std::vector<double> inputs = sampleValues; 

                // 3. Run JIT logic
                double vOut = Flux::JITContextManager::instance().runUpdate(scriptId, timeValue, inputs);

                if (!std::isfinite(vOut)) vOut = 0.0;

                // 4. Queue feedback for the high-priority sync thread.
                for (const QString& vSrc : target.outputVoltageSources) {
                    self->queueFluxSourceUpdate(vSrc, vOut);
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(self->m_bufferMutex);
        self->m_simBuffer.push_back({timeValue, std::move(sampleValues)});
        if (self->m_simBuffer.size() > 2000) {
            self->m_simBuffer.erase(self->m_simBuffer.begin(),
                                    self->m_simBuffer.begin() + static_cast<std::ptrdiff_t>(self->m_simBuffer.size() - 2000));
        }
    }

    return 0;
}

int SimulationManager::cbSendInitData(pvecinfoall initData, int id, void* userData) {
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (!self || !initData) return 0;

    {
        std::lock_guard<std::mutex> lock(self->m_vectorMutex);
        self->m_vectorMap.clear();
        for (int i = 0; i < initData->veccount; ++i) {
            pvecinfo v = initData->vecs[i];
            if (!v) continue;

            VectorMap vm;
            vm.index = i;
            vm.name = normalizeStreamVectorName(QString::fromLatin1(v->vecname));
            vm.isVoltage = (v->is_real && !vm.name.toLower().startsWith("i("));
            vm.isScale = (v->pdvec != nullptr && v->pdvec == v->pdvecscale);
            self->m_vectorMap.push_back(vm);
        }
    }

    qDebug() << "Ngspice: streaming initialized with" << initData->veccount << "vectors.";
    return 0;
}
#else
int SimulationManager::cbSendData(void* vecArray, int numStructs, int id, void* userData) {
    return 0;
}

int SimulationManager::cbSendInitData(void* initData, int id, void* userData) {
    return 0;
}
#endif

int SimulationManager::cbBGThreadRunning(bool finished, int id, void* userData) {
    SimulationManager* self = static_cast<SimulationManager*>(userData);
    if (!self) return 0;

    if (finished) {
        // --- STOPPED ---
        // Was this a manual halt for a JIT update or switch toggle?
        if (self->m_jitUpdateInProgress || self->m_switchToggleInProgress) {
            // It's just a pause. Process data and return.
            QMetaObject::invokeMethod(self->m_bufferTimer, "stop", Qt::QueuedConnection);
            QMetaObject::invokeMethod(self, "processBufferedData", Qt::QueuedConnection);
            return 0;
        }

        // --- NATURAL COMPLETION ---
        // Mark as finished immediately to stop the high-priority JIT thread from restarting it
        self->m_bgRunIssued = false;

        QFileInfo info(self->m_currentNetlist);
        const QString rawPath = info.absolutePath() + "/" + info.completeBaseName() + ".raw";
        QMetaObject::invokeMethod(self, "handleSimulationFinished", Qt::QueuedConnection, Q_ARG(QString, rawPath));
    }
    return 0;
}

void SimulationManager::handleSimulationFinished(const QString& rawPath) {
    m_bufferTimer->stop();
    processBufferedData(); // Flush remaining

#ifdef HAVE_NGSPICE
    // m_bgRunIssued was already cleared in the callback to stop the JIT thread,
    // so we check other status flags here.
    if (!m_lastLoadFailed && !m_lastRunFailed && !rawPath.isEmpty()) {
        qDebug() << "[SimulationManager] handleSimulationFinished: rawPath=" << rawPath;

        // IMPORTANT: Do NOT emit simulationFinished() yet!
        // We need to write the raw file first, otherwise the UI clears the viewer
        // before data is available for probing.
        
        // ngspice needs time to finalize internal data structures after bg_halt.
        QTimer::singleShot(200, this, [this, rawPath]() {
            if (rawPath.isEmpty()) {
                qWarning() << "[SimulationManager] rawPath is empty!";
                Q_EMIT simulationFinished(); // Still emit so UI knows we're done
                return;
            }

            qDebug() << "[SimulationManager] Attempting to write raw file:" << rawPath;
            QFile::remove(rawPath);

            ngSpice_Command((char*)"set filetype=binary");
            const QString writeCmd = "write " + rawPath;
            ngSpice_Command(writeCmd.toLatin1().data());

            // Give ngspice time to write
            QThread::msleep(200);

            // Verify the raw file was actually written
            if (QFile::exists(rawPath)) {
                qint64 size = QFileInfo(rawPath).size();
                qDebug() << "[SimulationManager] Raw file written:" << rawPath << "size=" << size;
                if (size > 0) {
                    Q_EMIT rawResultsReady(rawPath);
                } else {
                    qWarning() << "[SimulationManager] Raw file is empty!";
                }
            } else {
                qWarning() << "[SimulationManager] Raw file NOT created:" << rawPath;
                
                // Try to list what files exist in the directory
                QFileInfo fi(rawPath);
                QDir dir(fi.absolutePath());
                qDebug() << "[SimulationManager] Directory contents:" << dir.entryList();
                
                // Try alternative: write to temp location
                const QString tempPath = QDir::tempPath() + "/" + fi.completeBaseName() + ".raw";
                qDebug() << "[SimulationManager] Trying alternate write to:" << tempPath;
                
                ngSpice_Command((QString("write " + tempPath)).toLatin1().data());
                QThread::msleep(200);
                
                if (QFile::exists(tempPath) && QFileInfo(tempPath).size() > 0) {
                    QFile::remove(rawPath);
                    QFile::copy(tempPath, rawPath);
                    QFile::remove(tempPath);
                    qDebug() << "[SimulationManager] Alternate write succeeded, copied to:" << rawPath;
                    Q_EMIT rawResultsReady(rawPath);
                } else {
                    qWarning() << "[SimulationManager] Alternate write also failed";
                }
            }
            
            // NOW emit simulationFinished - raw file is ready for probing
            Q_EMIT simulationFinished();
        });
        return; // Don't emit simulationFinished() here - it's in the timer callback
    } else {
        qDebug() << "[SimulationManager] Skipping raw file write:"
                 << "bgRunIssued=" << m_bgRunIssued
                 << "lastLoadFailed=" << m_lastLoadFailed
                 << "lastRunFailed=" << m_lastRunFailed
                 << "rawPath=" << rawPath;
        if (m_lastRunFailed && !m_lastErrorMessage.isEmpty()) {
            Q_EMIT errorOccurred(m_lastErrorMessage);
        }
    }
#endif
    m_bgRunIssued = false;
    m_stopRequested = false;
    m_pauseRequested = false;
    Q_EMIT simulationFinished();
}
