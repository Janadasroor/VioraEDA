#ifndef SPICE_BACKEND_H
#define SPICE_BACKEND_H

#include <QString>
#include <mutex>

#ifdef HAVE_NGSPICE
#include <ngspice/sharedspice.h>
#endif

namespace Flux {

class SpiceBackend {
public:
    static SpiceBackend& instance();

    bool initialize(SendChar* cbChar, SendStat* cbStat, ControlledExit* cbExit,
                    SendData* cbData, SendInitData* cbInitData, BGThreadRunning* cbRunning,
                    void* userData);
    
    int execute(const QString& command);
    int loadCircuit(char** deck);
    
    bool isPaused() const;
    void* resolveSymbol(const char* name);
    bool isInitialized() const { return m_initialized; }
    void setInitialized(bool val) { m_initialized = val; }

private:
    SpiceBackend();
    bool m_initialized;
    mutable std::mutex m_mutex;
};

} // namespace Flux

#endif // SPICE_BACKEND_H
