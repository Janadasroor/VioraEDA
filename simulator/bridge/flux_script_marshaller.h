#ifndef FLUX_SCRIPT_MARSHALLER_BRIDGE_H
#define FLUX_SCRIPT_MARSHALLER_BRIDGE_H

#ifdef HAVE_FLUXSCRIPT
#include <flux/marshaller.h>
#endif
#include <map>
#include <string>

/**
 * @brief Legacy bridge to the new standalone FluxScript Marshaller.
 */
class FluxScriptMarshaller {
public:
    static pybind11::dict voltagesToDict(const std::map<std::string, double>& voltages) {
#ifdef HAVE_FLUXSCRIPT
        return Flux::Marshaller::mapToDict(voltages);
#else
        return pybind11::dict();
#endif
    }

    static std::map<std::string, double> pythonToOutputs(const pybind11::object& result) {
#ifdef HAVE_FLUXSCRIPT
        return Flux::Marshaller::dictToMap(result);
#else
        return std::map<std::string, double>();
#endif
    }
};

#endif // FLUX_SCRIPT_MARSHALLER_BRIDGE_H
