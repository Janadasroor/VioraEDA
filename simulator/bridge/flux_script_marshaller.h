#ifndef FLUX_SCRIPT_MARSHALLER_BRIDGE_H
#define FLUX_SCRIPT_MARSHALLER_BRIDGE_H

#include <flux/marshaller.h>
#include <map>
#include <string>

/**
 * @brief Legacy bridge to the new standalone FluxScript Marshaller.
 */
class FluxScriptMarshaller {
public:
    static pybind11::dict voltagesToDict(const std::map<std::string, double>& voltages) {
        return Flux::Marshaller::mapToDict(voltages);
    }

    static std::map<std::string, double> pythonToOutputs(const pybind11::object& result) {
        return Flux::Marshaller::dictToMap(result);
    }
};

#endif // FLUX_SCRIPT_MARSHALLER_BRIDGE_H
