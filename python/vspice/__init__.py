"""
VioSpice headless solver — Python bindings via nanobind.

Core features:
  - SPICE number parser (engineering suffixes, micro/omega variants)
  - Waveform data structures with numpy support
  - Simulation results container
  - SPICE model parser and .meas evaluator
  - Netlist builder (SimNetlist, SimComponentInstance, SimModel)
  - Simulation runner (run_simulation via vio-cmd subprocess)
  - Event hooks (@handlers.simulation_finished, etc.)
"""

__version__ = "0.1.0"

# Import all symbols from the nanobind extension
from vspice._core import *  # noqa: F401,F403
from vspice._core import __doc__ as _core_doc  # noqa: F401

# ---------------------------------------------------------------------------
# Event hooks — Python callbacks for C++ events
# ---------------------------------------------------------------------------

class _Handlers:
    """
    Event hooks for responding to simulator and editor events.

    Usage:
        import vspice

        @vspice.handlers.simulation_finished
        def on_sim_done(data):
            import json
            info = json.loads(data)
            print(f"Simulation done: {info['waveform_count']} waveforms")

        @vspice.handlers.simulation_error
        def on_error(data):
            import json
            info = json.loads(data)
            print(f"Simulation error: {info['message']}")

    Available hooks:
        simulation_finished — fired when a simulation completes successfully
        simulation_error    — fired when a simulation fails
    """

    @staticmethod
    def simulation_finished(func):
        """Register a handler for simulation_finished events."""
        from vspice._core import register_callback
        register_callback("simulation_finished", func)
        return func

    @staticmethod
    def simulation_error(func):
        """Register a handler for simulation_error events."""
        from vspice._core import register_callback
        register_callback("simulation_error", func)
        return func

    @staticmethod
    def list():
        """List all events that have handlers registered."""
        from vspice._core import list_callbacks
        return list_callbacks()

handlers = _Handlers()

# UI Proxy (optional — requires websocket-client)
try:
    from vspice.ui import UIProxy, connect, show_message, add_menu_item, run_python_code  # noqa: F401
except ImportError:
    pass
