"""
VioSpice headless solver — Python bindings via nanobind.

Core features:
  - SPICE number parser (engineering suffixes, micro/omega variants)
  - Waveform data structures with numpy support
  - Simulation results container
  - SPICE model parser and .meas evaluator
  - Netlist builder (SimNetlist, SimComponentInstance, SimModel)
  - Simulation runner (run_simulation via vio-cmd subprocess)
"""

__version__ = "0.1.0"

# Import all symbols from the nanobind extension
from vspice._core import *  # noqa: F401,F403
from vspice._core import __doc__ as _core_doc  # noqa: F401

# UI Proxy (optional — requires websocket-client)
try:
    from vspice.ui import UIProxy, connect, show_message, add_menu_item, run_python_code  # noqa: F401
except ImportError:
    pass
