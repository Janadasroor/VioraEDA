"""
VioSpice bpy-like API — the ``v`` module.

Provides Blender-style namespaces for interacting with the application:

    v.simulation    — run simulations, get waveforms, query results
    v.editor        — schematic/PCB editor operations
    v.waveviewer    — waveform viewer utilities
    v.handlers      — event hook decorators
    v.addons        — addon management
    v.ops           — operator pattern (every GUI action)
    v.project       — project information and management
    v.netlist       — netlist building and inspection
    v.help()        — show API documentation
"""

from __future__ import annotations

import sys
import textwrap
from typing import Any, Callable, Dict, List, Optional


# ─── Simulation API ───────────────────────────────────────────────

class _SimulationAPI:
    """Access simulation results and run new analyses."""

    @staticmethod
    def run(netlist: str, analysis: str = "tran", **kwargs) -> dict:
        """Run a SPICE simulation and return results as a dict.

        Parameters
        ----------
        netlist : str
            SPICE netlist text (CIR format).
        analysis : str
            Analysis type: ``"tran"``, ``"ac"``, ``"op"``, ``"noise"``, etc.
        **kwargs
            Additional analysis parameters (t_stop, t_step, etc.)

        Returns
        -------
        dict
            Simulation results with keys: ``"waveforms"``, ``"node_voltages"``,
            ``"branch_currents"``, ``"s_parameter_results"``, ``"success"``,
            ``"error"``.
        """
        from vspice._core import run_simulation
        return run_simulation(netlist, analysis=analysis, **kwargs)

    @staticmethod
    def get_signal(name: str) -> Optional[Any]:
        """Get a waveform signal by name from the last simulation."""
        from vspice._core import SimResults
        # Try to fetch from project context via UI proxy
        try:
            from vspice.ui import get_proxy
            proxy = get_proxy()
            if proxy and proxy.is_connected():
                result = proxy.run_python_code(
                    f"__vspice_last_results__.get('{name}', None)"
                )
                return result.get("output", "")
        except Exception:
            pass
        return None

    @staticmethod
    def list_signals() -> List[str]:
        """List all available signal names from the last simulation."""
        from vspice._core import SimResults
        try:
            from vspice.ui import get_proxy
            proxy = get_proxy()
            if proxy and proxy.is_connected():
                result = proxy.run_python_code(
                    "list(__vspice_last_results__.waveforms.keys()) if hasattr(__vspice_last_results__, 'waveforms') else []"
                )
                import ast
                return ast.literal_eval(result.get("output", "[]"))
        except Exception:
            pass
        return []

    @staticmethod
    def fft(signal_name: str, sample_rate: float = 1.0) -> tuple:
        """Compute FFT of a signal.

        Parameters
        ----------
        signal_name : str
            Name of the waveform (e.g. ``"V(out)"``).
        sample_rate : float
            Sample rate in Hz.

        Returns
        -------
        tuple
            ``(frequencies, magnitudes)`` arrays.
        """
        from vspice._core import fft_r2c, fft_magnitude
        sig = _SimulationAPI.get_signal(signal_name)
        if sig is None:
            raise ValueError(f"Signal '{signal_name}' not found")
        # This would need the actual waveform data
        raise NotImplementedError("FFT requires active simulation session")


# ─── Editor API ───────────────────────────────────────────────────

class _EditorAPI:
    """Schematic and PCB editor operations."""

    @staticmethod
    def open_schematic(path: str = "") -> bool:
        """Open a schematic file."""
        try:
            from vspice.ui import get_proxy
            proxy = get_proxy()
            if proxy and proxy.is_connected():
                result = proxy.run_python_code(f"__v_editor_open('{path}')")
                return result.get("success", False)
        except Exception:
            pass
        return False

    @staticmethod
    def get_selected() -> List[dict]:
        """Get currently selected components in the editor."""
        try:
            from vspice.ui import get_proxy
            proxy = get_proxy()
            if proxy and proxy.is_connected():
                ctx = proxy.get_schematic_context()
                return ctx.get("selected", [])
        except Exception:
            pass
        return []

    @staticmethod
    def place_component(ref: str, value: str, x: float = 0, y: float = 0) -> bool:
        """Place a component in the schematic."""
        try:
            from vspice.ui import get_proxy
            proxy = get_proxy()
            if proxy and proxy.is_connected():
                result = proxy.run_python_code(
                    f"__v_place_component('{ref}', '{value}', {x}, {y})"
                )
                return result.get("success", False)
        except Exception:
            pass
        return False

    @staticmethod
    def zoom_fit() -> bool:
        """Fit the view to show all components."""
        try:
            from vspice.ui import get_proxy
            proxy = get_proxy()
            if proxy and proxy.is_connected():
                proxy.run_python_code("__v_zoom_fit()")
                return True
        except Exception:
            pass
        return False

    @staticmethod
    def export_netlist() -> str:
        """Export the current schematic as a SPICE netlist."""
        try:
            from vspice.ui import get_proxy
            proxy = get_proxy()
            if proxy and proxy.is_connected():
                result = proxy.run_python_code("__v_export_netlist()")
                return result.get("output", "")
        except Exception:
            pass
        return ""


# ─── Waveform Viewer API ─────────────────────────────────────────

class _WaveViewerAPI:
    """Waveform viewer utilities."""

    @staticmethod
    def add_signal(signal_name: str) -> bool:
        """Add a signal to the waveform viewer."""
        try:
            from vspice.ui import get_proxy
            proxy = get_proxy()
            if proxy and proxy.is_connected():
                proxy.run_python_code(f"__v_add_signal('{signal_name}')")
                return True
        except Exception:
            pass
        return False

    @staticmethod
    def remove_signal(signal_name: str) -> bool:
        """Remove a signal from the waveform viewer."""
        try:
            from vspice.ui import get_proxy
            proxy = get_proxy()
            if proxy and proxy.is_connected():
                proxy.run_python_code(f"__v_remove_signal('{signal_name}')")
                return True
        except Exception:
            pass
        return False

    @staticmethod
    def clear_signals() -> bool:
        """Clear all signals from the viewer."""
        try:
            from vspice.ui import get_proxy
            proxy = get_proxy()
            if proxy and proxy.is_connected():
                proxy.run_python_code("__v_clear_signals()")
                return True
        except Exception:
            pass
        return False

    @staticmethod
    def set_y_range(signal_name: str, y_min: float, y_max: float) -> bool:
        """Set the Y-axis range for a signal."""
        try:
            from vspice.ui import get_proxy
            proxy = get_proxy()
            if proxy and proxy.is_connected():
                proxy.run_python_code(
                    f"__v_set_y_range('{signal_name}', {y_min}, {y_max})"
                )
                return True
        except Exception:
            pass
        return False


# ─── Project API ──────────────────────────────────────────────────

class _ProjectAPI:
    """Project information and management."""

    @staticmethod
    def get_path() -> Optional[str]:
        """Get the current project file path."""
        try:
            from vspice.ui import get_proxy
            proxy = get_proxy()
            if proxy and proxy.is_connected():
                result = proxy.run_python_code("__v_project_path()")
                return result.get("output", "").strip()
        except Exception:
            pass
        return None

    @staticmethod
    def get_name() -> Optional[str]:
        """Get the current project name."""
        path = _ProjectAPI.get_path()
        if path:
            import os
            return os.path.basename(path)
        return None

    @staticmethod
    def save() -> bool:
        """Save the current project."""
        try:
            from vspice.ui import get_proxy
            proxy = get_proxy()
            if proxy and proxy.is_connected():
                proxy.run_python_code("__v_project_save()")
                return True
        except Exception:
            pass
        return False


# ─── Netlist API ─────────────────────────────────────────────────

class _NetlistAPI:
    """Netlist building and inspection."""

    @staticmethod
    def build() -> str:
        """Build a SPICE netlist from the current schematic."""
        return _EditorAPI.export_netlist()

    @staticmethod
    def parse(netlist_text: str) -> Any:
        """Parse a SPICE netlist and return a SimNetlist object."""
        from vspice._core import SimNetlist
        nl = SimNetlist()
        for line in netlist_text.strip().split('\n'):
            line = line.strip()
            if not line or line.startswith('*'):
                continue
            # Simple parsing — the full parser is in C++
            nl._add_raw_line(line)
        return nl

    @staticmethod
    def check(netlist_text: str) -> List[dict]:
        """Check a netlist for errors and warnings."""
        from vspice._core import SimModelParser, SimModelParseOptions
        opts = SimModelParseOptions()
        results = []
        # Diagnostics would come from C++ parser
        return results


# ─── Addon Manager ───────────────────────────────────────────────

class _AddonManager:
    """Manage VioSpice addons.

    Addons are Python packages that register additional functionality
    when loaded.

    Example::

        import v

        class MyAddon:
            bl_info = {
                "name": "My Custom Tool",
                "version": (1, 0, 0),
                "blender": (3, 0, 0),  # min compatible
            }

            def register(self):
                print("Addon registered!")

            def unregister(self):
                print("Addon unregistered!")

        v.addons.register(MyAddon)
    """

    def __init__(self):
        self._addons: Dict[str, Any] = {}

    def register(self, addon_class: type) -> bool:
        """Register an addon class."""
        name = getattr(addon_class, "bl_info", {}).get("name", addon_class.__name__)
        if name in self._addons:
            print(f"Addon '{name}' is already registered")
            return False

        try:
            addon = addon_class()
            addon.register()
            self._addons[name] = addon
            print(f"Addon '{name}' registered successfully")
            return True
        except Exception as e:
            print(f"Failed to register addon '{name}': {e}")
            return False

    def unregister(self, name: str) -> bool:
        """Unregister an addon by name."""
        if name not in self._addons:
            print(f"Addon '{name}' is not registered")
            return False

        try:
            addon = self._addons.pop(name)
            addon.unregister()
            print(f"Addon '{name}' unregistered")
            return True
        except Exception as e:
            print(f"Failed to unregister addon '{name}': {e}")
            return False

    def list(self) -> List[str]:
        """List all registered addons."""
        return list(self._addons.keys())

    def get(self, name: str) -> Optional[Any]:
        """Get a registered addon instance."""
        return self._addons.get(name)


# ─── Operator System ──────────────────────────────────────────────

class _OperatorRegistry:
    """Registry of operators (like Blender's bpy.ops).

    Each operator is a callable that performs an action.
    Operators are namespaced: ``v.ops.editor.zoom_fit()``, etc.
    """

    def __init__(self):
        self._operators: Dict[str, Callable] = {}

    def register(self, id: str, func: Callable) -> None:
        """Register an operator.

        Parameters
        ----------
        id : str
            Operator ID, e.g. ``"editor.zoom_fit"``.
        func : callable
            The function to call.
        """
        self._operators[id] = func

    def unregister(self, id: str) -> None:
        """Unregister an operator."""
        self._operators.pop(id, None)

    def __getattr__(self, name: str) -> Any:
        # Support dotted access: v.ops.editor.zoom_fit
        parts = name.split(".")
        if len(parts) == 1:
            # Return a namespace with all operators under this prefix
            ns = _OperatorNamespace(f"{name}")
            return ns

        # Direct operator call
        if name in self._operators:
            return self._operators[name]
        raise AttributeError(f"Operator '{name}' not found")

    def __call__(self, id: str, **kwargs) -> Any:
        """Call an operator by ID."""
        if id not in self._operators:
            raise KeyError(f"Operator '{id}' not found")
        return self._operators[id](**kwargs)

    def list(self) -> List[str]:
        """List all registered operators."""
        return sorted(self._operators.keys())


class _OperatorNamespace:
    """Lazy namespace for operator access."""

    def __init__(self, prefix: str):
        self._prefix = prefix
        # Capture reference to the module-level registry at creation time
        import vspice.v_api as _mod
        self._registry = _mod._op_registry

    def __getattr__(self, name: str) -> Any:
        full_id = f"{self._prefix}.{name}" if self._prefix else name
        if full_id in self._registry:
            return self._registry[full_id]
        # Try deeper nesting
        return _OperatorNamespace(full_id)


# ─── Internal operator registry ───────────────────────────────────

_op_registry: Dict[str, Callable] = {}


# ─── Public API Singletons ────────────────────────────────────────

simulation = _SimulationAPI()
"""Simulation API — run simulations, get waveforms, FFT."""

editor = _EditorAPI()
"""Editor API — schematic/PCB operations."""

waveviewer = _WaveViewerAPI()
"""Waveform viewer API — manage displayed signals."""

project = _ProjectAPI()
"""Project API — project info and management."""

netlist = _NetlistAPI()
"""Netlist API — build, parse, and check netlists."""

addons = _AddonManager()
"""Addon manager — register/unregister addons."""

ops = _OperatorRegistry()
"""Operator registry — programmatic GUI actions."""

# Re-export handlers from vspice (if _core is available)
try:
    from vspice._core import handlers  # noqa: E402
except ImportError:
    handlers = None  # Will be set by __init__.py after loading


# ─── Help ─────────────────────────────────────────────────────────

def help(topic: str = "") -> None:
    """Print VioSpice API documentation.

    Parameters
    ----------
    topic : str, optional
        Specific topic: ``"simulation"``, ``"editor"``, ``"waveviewer"``,
        ``"project"``, ``"netlist"``, ``"addons"``, ``"ops"``, ``"handlers"``.
        If empty, shows the overview.
    """
    if topic == "simulation" or topic.startswith("sim"):
        print(textwrap.dedent("""\
            v.simulation — Simulation API
            =============================
            v.simulation.run(netlist, analysis="tran", ...)  — Run a SPICE simulation
            v.simulation.list_signals()                       — List available signals
            v.simulation.get_signal(name)                     — Get a waveform
            v.simulation.fft(signal_name, sample_rate)        — Compute FFT
        """))
    elif topic == "editor":
        print(textwrap.dedent("""\
            v.editor — Editor API
            =====================
            v.editor.open_schematic(path)    — Open a schematic file
            v.editor.get_selected()           — Get selected components
            v.editor.place_component(...)     — Place a component
            v.editor.zoom_fit()               — Fit view to content
            v.editor.export_netlist()         — Export as SPICE netlist
        """))
    elif topic == "waveviewer" or topic.startswith("wave"):
        print(textwrap.dedent("""\
            v.waveviewer — Waveform Viewer API
            ==================================
            v.waveviewer.add_signal(name)       — Add a signal
            v.waveviewer.remove_signal(name)    — Remove a signal
            v.waveviewer.clear_signals()        — Clear all signals
            v.waveviewer.set_y_range(...)       — Set Y-axis range
        """))
    elif topic == "project":
        print(textwrap.dedent("""\
            v.project — Project API
            =======================
            v.project.get_path()   — Get current project file path
            v.project.get_name()   — Get project name
            v.project.save()       — Save the project
        """))
    elif topic == "netlist":
        print(textwrap.dedent("""\
            v.netlist — Netlist API
            =======================
            v.netlist.build()        — Build netlist from schematic
            v.netlist.parse(text)    — Parse a SPICE netlist
            v.netlist.check(text)    — Check for errors/warnings
        """))
    elif topic == "addons" or topic.startswith("addon"):
        print(textwrap.dedent("""\
            v.addons — Addon Manager
            ========================
            v.addons.register(AddonClass)   — Register an addon
            v.addons.unregister(name)       — Unregister an addon
            v.addons.list()                 — List registered addons
            v.addons.get(name)              — Get addon instance

            Define an addon class with bl_info and register()/unregister() methods.
        """))
    elif topic == "ops" or topic.startswith("op"):
        print(textwrap.dedent("""\
            v.ops — Operator Registry
            =========================
            v.ops.register(id, func)   — Register an operator
            v.ops.unregister(id)       — Unregister an operator
            v.ops.list()               — List all operators
            v.ops(id, **kwargs)        — Call an operator

            Operators are namespaced: v.ops.editor.zoom_fit()
        """))
    elif topic == "handlers" or topic.startswith("handler"):
        print(textwrap.dedent("""\
            v.handlers — Event Hooks
            ========================
            @v.handlers.simulation_finished   — Called after simulation completes
            @v.handlers.simulation_error       — Called on simulation error
            v.handlers.list()                   — List registered handlers

            Example:
                @v.handlers.simulation_finished
                def on_done(results):
                    print(f"Simulation finished: {len(results.waveforms)} signals")
        """))
    else:
        # Overview
        print(textwrap.dedent("""\
            VioSpice Python API (v module)
            ===============================

            Namespaces:
              v.simulation     — Run simulations, get waveforms, FFT
              v.editor         — Schematic/PCB editor operations
              v.waveviewer     — Waveform viewer utilities
              v.project        — Project info and management
              v.netlist        — Netlist building and inspection
              v.addons         — Addon management
              v.ops            — Operator pattern (GUI actions)
              v.handlers       — Event hook decorators

            Functions:
              v.help([topic])  — Show this documentation

            Examples:
              >>> import v
              >>> v.help()
              >>> v.help("simulation")
              >>> result = v.simulation.run(netlist, analysis="tran", t_stop="10ms")
              >>> signals = v.simulation.list_signals()
              >>> v.editor.zoom_fit()
              >>> v.addons.register(MyAddon)

            Type v.help("topic") for detailed docs on each namespace.
        """))


# ─── Console auto-import helper ───────────────────────────────────

def _setup_console() -> None:
    """Register default operators and prepare the console environment."""
    # Register built-in operators that proxy to the UI
    ops.register("editor.zoom_fit", lambda: editor.zoom_fit())
    ops.register("editor.export_netlist", lambda: editor.export_netlist())
    ops.register("viewer.clear_signals", lambda: waveviewer.clear_signals())
    ops.register("project.save", lambda: project.save())

    # Auto-create Gemini terminal instance
    global gemini
    try:
        from vspice.gemini_terminal import GeminiTerminal
        gemini = GeminiTerminal()
    except Exception:
        gemini = None


# Auto-setup when module is imported
_setup_console()
