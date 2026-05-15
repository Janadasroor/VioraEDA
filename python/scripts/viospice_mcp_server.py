#!/usr/bin/env python3
"""FastMCP server for VioraEDA — provides AI agents with circuit design tools."""

import importlib.util
import logging
import sys
from pathlib import Path

# Set up logging to stderr (never stdout — stdio transport uses stdout for JSON-RPC)
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    stream=sys.stderr,
)

ROOT = Path(__file__).resolve().parents[2]

# Load vspice.mcp directly (bypasses vspice/__init__.py which requires _core .so)
spec = importlib.util.spec_from_file_location(
    "core", str(ROOT / "python" / "vspice" / "mcp.py")
)
core = importlib.util.module_from_spec(spec)
sys.modules["core"] = core
spec.loader.exec_module(core)

from mcp.server.fastmcp import FastMCP, Image

mcp = FastMCP("VioraEDA")


# ── Viora Design Tools ──────────────────────────────────────────

@mcp.tool()
def viora_status() -> dict:
    """Show VioraEDA project, CLI engine, and ML API status."""
    return core.get_detailed_status()


@mcp.tool()
def viora_get_project_root() -> dict:
    """Return the project root path.  All relative paths in other tools
    are resolved against this directory."""
    return core.get_project_root()


@mcp.tool()
def viora_list_files(path: str = ".") -> dict:
    """List files and directories in the project workspace."""
    return core.list_files(path)


@mcp.tool()
def viora_read_file(path: str) -> dict:
    """Read the content of a schematic, script, or model file."""
    return core.read_file(path)


@mcp.tool()
def viora_write_file(path: str, content: str) -> dict:
    """Create or update a design file (schematic, netlist, script)."""
    return core.write_file(path, content)


@mcp.tool()
def viora_symbol_list(path: str = ".") -> dict:
    """Query the symbol library for available components."""
    return core.symbol_list(path)


@mcp.tool()
def viora_schematic_query(file: str) -> dict:
    """Analyze a .flxsch schematic to extract components and connectivity."""
    return core.schematic_query(file)


@mcp.tool()
def viora_flux_run(
    file: str = None, code: str = None,
    t: float = None, inputs: list = None
) -> dict:
    """Execute automation logic via FluxScript (file or raw code)."""
    return core.flux_run(file=file, code=code, t=t, inputs=inputs)


@mcp.tool()
def viora_flux_help() -> dict:
    """Get FluxScript syntax and API documentation."""
    help_path = core.ROOT / "docs" / "EXTENSION_API.md"
    if help_path.exists():
        return {"ok": True, "manual": help_path.read_text()}
    return {"ok": False, "error": "FluxScript manual not found."}


@mcp.tool()
def viora_pcb_render(file: str, out: str, timeout: int = 60) -> Image:
    """Render a .pcb board to a PNG image for visual inspection."""
    res = core.pcb_render(file, out, timeout=timeout)
    if not res.get("ok"):
        raise RuntimeError(res.get("error", "PCB render failed"))
    return Image(path=out)


@mcp.tool()
def viora_schematic_render(
    file: str, out: str,
    transparent: bool = False, scale: float = 4.0, timeout: int = 60
) -> Image:
    """Render a .flxsch schematic to a PNG image for visual inspection."""
    res = core.schematic_render(file, out, transparent=transparent, scale=scale, timeout=timeout)
    if not res.get("ok"):
        raise RuntimeError(res.get("error", "Schematic render failed"))
    return Image(path=out)


@mcp.tool()
def viora_symbol_render(
    file: str, out: str,
    transparent: bool = False, scale: float = 4.0, timeout: int = 60
) -> Image:
    """Render a .viosym symbol to a PNG image."""
    res = core.symbol_render(file, out, transparent=transparent, scale=scale, timeout=timeout)
    if not res.get("ok"):
        raise RuntimeError(res.get("error", "Symbol render failed"))
    return Image(path=out)


# ── VioSpice Simulation Tools ───────────────────────────────────

@mcp.tool()
def viospice_raw_info(file: str) -> dict:
    """Get signal metadata from a .raw simulation binary."""
    return core.raw_info(file)


@mcp.tool()
def viospice_raw_export(file: str, out: str, format: str = "json") -> dict:
    """Export binary simulation waveforms to JSON, CSV, or Parquet."""
    return core.raw_export(file, out, fmt=format)


@mcp.tool()
def viospice_netlist_validate(file: str = None, cir: str = None) -> dict:
    """Pre-flight validation of a SPICE netlist or schematic."""
    return core.netlist_validate(file=file, cir=cir)


@mcp.tool()
def viospice_netlist_run_async(
    file: str = None,
    cir: str = None,
    analysis: str = None,
    stop: str = None,
    step: str = None,
    signals: list = None,
    robust: bool = False,
    compat: bool = True,
    smart_signals: list = None,
    options: str = None,
    temperature: float = None,
):
    """Start a SPICE simulation in the background and return a job ID.
    Poll for completion with `viospice_netlist_job_status`.
    Retrieve results with `viospice_netlist_job_result`.
    """
    return core.netlist_run_async(
        file=file, cir=cir,
        analysis=analysis, stop=stop, step=step,
        signals=signals,
        robust=robust, compat=compat,
        smart_signals=smart_signals,
        options=options, temperature=temperature,
    )


@mcp.tool()
def viospice_netlist_job_status(job_id: str) -> dict:
    """Check progress of a background simulation job.
    Returns status (queued/running/done/error) and progress percentage."""
    return core.netlist_job_status(job_id)


@mcp.tool()
def viospice_netlist_job_result(job_id: str) -> dict:
    """Retrieve the result of a completed background simulation job.
    Only call this when status is 'done' or 'error'."""
    return core.netlist_job_result(job_id)


@mcp.tool()
def viospice_netlist_run(
    file: str = None,
    cir: str = None,
    analysis: str = None,
    stop: str = None,
    step: str = None,
    signals: list = None,
    robust: bool = False,
    compat: bool = True,
    smart_signals: list = None,
    options: str = None,
    temperature: float = None,
):  # fmt: off
    """Execute a SPICE simulation from a netlist or schematic.
    Returns parsed waveform data in JSON format.

    Parameters
    ----------
    file : .cir or .flxsch path (relative to project root, or absolute)
    cir : raw SPICE netlist text (alternative to file)
    analysis : analysis directive, e.g. 'tran 1n 100u'
    stop : transient stop time (alternative to analysis=)
    step : transient time step
    signals : list of nodes to save, e.g. ["v(1)","v(out)","i(R1)"].
              If omitted, all nodes are saved.
    robust : enable damped simulation mode for tough convergence
    compat : enable LTspice compatibility mode (default: True)
    smart_signals : list of dicts with keys:
        ref (str) - component reference, e.g. 'SB1'
        code (str) - FluxScript behavioral code
        inputs (list[str]) - input node names
        outputs (list[str]) - output node names (required)
    options : raw SPICE option string, e.g. 'reltol=1e-4 abstol=1e-9'
    temperature : simulation temperature in Celsius
    """
    v_args = dict(
        file=file, cir=cir,
        analysis=analysis, stop=stop, step=step,
        signals=signals, json_out=True,
        robust=robust, compat=compat,
        smart_signals=smart_signals,
    )
    if options:
        v_args["options"] = options
    if temperature is not None:
        v_args["temperature"] = temperature
    return core.netlist_run(**v_args)


# ── GUI Integration ─────────────────────────────────────────────

@mcp.tool()
def viora_ui_get_current_tab() -> dict:
    """Get the currently active editor tab from the running VioraEDA GUI."""
    return core.get_current_tab()


@mcp.tool()
def viospice_ui_open_schematic(path: str, convert: bool = False) -> dict:
    """Open a schematic or netlist file in the running VioraEDA GUI editor."""
    return core.open_schematic(path=path, convert=convert)


if __name__ == "__main__":
    logging.info("VioraEDA MCP server starting (stdio transport)")
    mcp.run(transport="stdio")
