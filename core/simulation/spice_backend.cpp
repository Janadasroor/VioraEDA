#include "spice_backend.h"
#include <QLibrary>
#include <QDebug>
#include <dlfcn.h>

#ifdef HAVE_NGSPICE
extern "C" bool ngSpice_IsPaused(void);
#endif

namespace Flux {

SpiceBackend::SpiceBackend() : m_initialized(false) {}

SpiceBackend& SpiceBackend::instance() {
    static SpiceBackend instance;
    return instance;
}

bool SpiceBackend::initialize(SendChar* cbChar, SendStat* cbStat, ControlledExit* cbExit,
                             SendData* cbData, SendInitData* cbInitData, BGThreadRunning* cbRunning,
                             void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;

#ifdef HAVE_NGSPICE
    int rc = ngSpice_Init(cbChar, cbStat, cbExit, cbData, cbInitData, cbRunning, userData);
    m_initialized = (rc == 0);
    return m_initialized;
#else
    return false;
#endif
}

int SpiceBackend::execute(const QString& command) {
    qDebug() << "[SpiceBackend] Executing:" << command;
    if (command == "bg_resume" || command == "bg_halt") {
        // Special case: these are thread-safe and MUST NOT take the mutex to avoid deadlocks in callbacks
#ifdef HAVE_NGSPICE
        return ngSpice_Command(const_cast<char*>(command.toLatin1().constData()));
#else
        return -1;
#endif
    }

    std::lock_guard<std::mutex> lock(m_mutex);
#ifdef HAVE_NGSPICE
    return ngSpice_Command(const_cast<char*>(command.toLatin1().constData()));
#else
    return -1;
#endif
}

int SpiceBackend::loadCircuit(char** deck) {
    std::lock_guard<std::mutex> lock(m_mutex);
#ifdef HAVE_NGSPICE
    return ngSpice_Circ(deck);
#else
    return -1;
#endif
}

bool SpiceBackend::isPaused() const {
#ifdef HAVE_NGSPICE
    return ngSpice_IsPaused();
#else
    return false;
#endif
}

void* SpiceBackend::resolveSymbol(const char* symbolName) {
    if (!symbolName || !*symbolName) return nullptr;

    if (void* sym = dlsym(RTLD_DEFAULT, symbolName)) {
        return sym;
    }

    Dl_info info;
    if (dladdr((void*)ngSpice_Init, &info) && info.dli_fname) {
        QLibrary loadedNgspice(QString::fromLocal8Bit(info.dli_fname));
        if (QFunctionPointer sym = loadedNgspice.resolve(symbolName)) {
            return reinterpret_cast<void*>(sym);
        }
    }

    if (QFunctionPointer sym = QLibrary::resolve("libngspice", symbolName)) {
        return reinterpret_cast<void*>(sym);
    }
    return nullptr;
}

} // namespace Flux
