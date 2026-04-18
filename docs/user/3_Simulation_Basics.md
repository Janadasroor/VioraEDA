# Simulation Basics

VioSpice supports standard SPICE analyses to help you verify your circuit behavior.

## Analysis Types
* **Transient (.tran)**: Observes circuit behavior over a specific period of time. Essential for time-domain signals and switching circuits.
* **AC Analysis (.ac)**: Performs frequency-domain sweeps. Useful for filters, amplifiers, and stability analysis.
* **DC Operating Point (.op)**: Calculates the steady-state bias conditions of the circuit.

## Probing
You can place **Voltage Probes** and **Current Probes** directly on your schematic to see live updates in the integrated waveform viewer during or after the simulation.

## Simulation Engine
VioSpice uses the optimized **VioMATRIXC** engine, ensuring high performance and compatibility with industry-standard models.
