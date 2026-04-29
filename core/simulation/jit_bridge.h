#ifndef JIT_BRIDGE_H
#define JIT_BRIDGE_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <vector>
#include "simulation_types.h"

namespace Flux {

class JitBridge {
public:
    static JitBridge& instance();

    /**
     * Registers JIT function pointers with the ngspice solver for high-speed
     * native code execution.
     */
    void registerTargetsWithEngine(const QMap<QString, FluxScriptTarget>& targets);

    /**
     * Executes the JIT feedback loop for SmartSignals that aren't running
     * in native mode (Mixed-Mode simulation).
     */
    void executeFeedbackLoop(const std::vector<double>& sampleValues, double timeValue, 
                             const QMap<QString, FluxScriptTarget>& targets);

private:
    JitBridge();
    QStringList getRegistrationAliases(const QString& id);
};

} // namespace Flux

#endif // JIT_BRIDGE_H
