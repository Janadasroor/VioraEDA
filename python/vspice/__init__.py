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
  - bpy-like API via ``import vspice.v as v`` (or just ``import v`` in console)
"""

__version__ = "0.2.0"

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
        schematic_changed   — fired when the schematic is modified
        project_opened      — fired when a project is opened
        project_saved       — fired when a project is saved
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
    def schematic_changed(func):
        """Register a handler for schematic_changed events."""
        from vspice._core import register_callback
        register_callback("schematic_changed", func)
        return func

    @staticmethod
    def project_opened(func):
        """Register a handler for project_opened events."""
        from vspice._core import register_callback
        register_callback("project_opened", func)
        return func

    @staticmethod
    def project_saved(func):
        """Register a handler for project_saved events."""
        from vspice._core import register_callback
        register_callback("project_saved", func)
        return func

    @staticmethod
    def list():
        """List all events that have handlers registered."""
        from vspice._core import list_callbacks
        return list_callbacks()

handlers = _Handlers()

# ---------------------------------------------------------------------------
# bpy-like API (v module)
# ---------------------------------------------------------------------------

# Import the v API and also expose it at package level so users can do
# ``import vspice as v`` or, in the embedded console, just ``v.help()``.
from vspice import v_api as v  # noqa: F401,E402

# Also add v.handlers as an alias to our handlers above (for consistency)
v.handlers = handlers  # noqa: E402

# UI Proxy (optional — requires websocket-client)
try:
    from vspice.ui import UIProxy, connect, show_message, add_menu_item, run_python_code  # noqa: F401
except ImportError:
    pass
