#ifndef FLUX_SIM_BRIDGE_H
#define FLUX_SIM_BRIDGE_H

/**
 * @brief Initializes the FluxScript/VioSpice simulation bridge.
 * This should be called during VioSpice startup to connect the JIT-compiled
 * scripts to the actual simulation engine.
 */
void initializeFluxSimBridge();

#endif // FLUX_SIM_BRIDGE_H
