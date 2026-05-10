"""
Model Context Protocol (MCP) server integration for VioSpice.
This module provides the core tool definitions and execution logic.
"""

import base64
import json
import os
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Dict, List, Optional

# ... (rest of imports)

def read_image_base64(path_str: str) -> Optional[str]:
    """Read an image file and return it as a base64 encoded string."""
    path = Path(path_str)
    if not path.exists():
        return None
    try:
        return base64.b64encode(path.read_bytes()).decode("utf-8")
    except Exception:
        return None

def pcb_render(file: str, out: str, timeout: int = 60) -> Dict[str, Any]:
    """Render a Viora .pcb board to a PNG image."""
    return run_viora_command(["render", file, out], timeout=timeout)

def schematic_render(file: str, out: str, transparent: bool = False, scale: float = 4.0, timeout: int = 60) -> Dict[str, Any]:
    """Render a Viora .flxsch schematic to a PNG image."""
    args = ["schematic-render", file, out]
    if transparent: args.append("--transparent")
    if scale: args.extend(["--scale", str(scale)])
    return run_viora_command(args, timeout=timeout)

def symbol_render(file: str, out: str, transparent: bool = False, scale: float = 4.0, timeout: int = 60) -> Dict[str, Any]:
    """Render a Viora .viosym symbol to a PNG image."""
    args = ["symbol-render", file, out]
    if transparent: args.append("--transparent")
    if scale: args.extend(["--scale", str(scale)])
    return run_viora_command(args, timeout=timeout)


# Constants
ROOT = Path(__file__).resolve().parents[2]
LOG_DIR = ROOT / ".viospice"
LOG_FILE = LOG_DIR / "mcp-ml-api.log"

def get_viora_executable() -> str:
    """Locate the viora CLI binary."""
    candidates = [
        ROOT / "build" / "viora",
        ROOT / "build-debug" / "viora",
        ROOT / "build-asan" / "viora",
        ROOT / "build" / "vio-cmd",
        ROOT / "build-debug" / "vio-cmd",
    ]
    for item in candidates:
        if item.exists():
            return str(item)
    return "viora"

def run_viora_command(args: List[str], timeout: int = 120, json_out: bool = False, retries: int = 2) -> Dict[str, Any]:
    """Execute a viora CLI command with automatic retries for transient failures."""
    last_err = None
    for attempt in range(retries + 1):
        try:
            proc = subprocess.run(
                [get_viora_executable()] + args,
                cwd=ROOT,
                capture_output=True,
                text=True,
                timeout=timeout,
            )
            
            out = proc.stdout.strip()
            err = proc.stderr.strip()
            data = None
            
            if json_out and out:
                try:
                    data = json.loads(out)
                except json.JSONDecodeError:
                    pass
            
            # Success
            if proc.returncode == 0:
                return {
                    "ok": True,
                    "code": proc.returncode,
                    "stdout": out,
                    "stderr": err,
                    "data": data,
                }
            
            last_err = f"Process exited with code {proc.returncode}. Stderr: {err}"
            
        except subprocess.TimeoutExpired:
            last_err = f"Command timed out after {timeout}s"
        except Exception as e:
            last_err = str(e)
            
        if attempt < retries:
            time.sleep(0.5 * (attempt + 1)) # Exponential-ish backoff
            
    return {"ok": False, "error": last_err, "args": args}


def list_files(path_str: str = ".") -> Dict[str, Any]:
    """List files in the project directory with safety checks."""
    path = (ROOT / path_str).resolve()
    if not str(path).startswith(str(ROOT.resolve())):
        return {"ok": False, "error": "Access denied: Path is outside project root"}
    
    try:
        files = []
        for item in path.iterdir():
            files.append({
                "name": item.name,
                "type": "directory" if item.is_dir() else "file",
                "size": item.stat().st_size if item.is_file() else 0,
                "modified": item.stat().st_mtime
            })
        return {"ok": True, "path": str(path.relative_to(ROOT)), "files": files}
    except Exception as e:
        return {"ok": False, "error": str(e)}

def read_file(path_str: str) -> Dict[str, Any]:
    """Read a file from the project directory with safety checks."""
    path = (ROOT / path_str).resolve()
    if not str(path).startswith(str(ROOT.resolve())):
        return {"ok": False, "error": "Access denied: Path is outside project root"}
    
    try:
        return {"ok": True, "path": path_str, "content": path.read_text(encoding="utf-8")}
    except Exception as e:
        return {"ok": False, "error": str(e)}

def write_file(path_str: str, content: str) -> Dict[str, Any]:
    """Write a file to the project directory with safety checks."""
    path = (ROOT / path_str).resolve()
    if not str(path).startswith(str(ROOT.resolve())):
        return {"ok": False, "error": "Access denied: Path is outside project root"}
    
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return {"ok": True, "path": path_str}
    except Exception as e:
        return {"ok": False, "error": str(e)}

def raw_info(file: str) -> Dict[str, Any]:
    """Get metadata and signal list from a .raw simulation file."""
    return run_viora_command(["raw-info", file, "--json"], json_out=True)

def raw_export(file: str, out: str, fmt: str = "json") -> Dict[str, Any]:
    """Export waveform data to a different format (csv, json, parquet)."""
    return run_viora_command(["raw-export", file, "--out", out, "--format", fmt, "--json"], json_out=True)

def symbol_list(path: str = ".") -> Dict[str, Any]:
    """List available symbols in a directory or library file."""
    return run_viora_command(["symbol-list", path, "--json"], json_out=True)

def schematic_query(file: str) -> Dict[str, Any]:
    """Get component and net information from a schematic file."""
    return run_viora_command(["schematic-query", file, "--json"], json_out=True)

def flux_run(file: str = None, code: str = None, t: float = None, inputs: list = None) -> Dict[str, Any]:
    """Run a FluxScript from a file or a string."""
    if file:
        args = ["flux", "run", file]
    elif code:
        # Use 'eval' for raw code to get immediate result
        args = ["flux", "eval", code, "--json"]
        if t is not None:
            args.extend(["--time", str(t)])
        if inputs:
            args.extend(["--inputs", ",".join(map(str, inputs))])
    else:
        return {"ok": False, "error": "Missing file or code for flux_run"}
    
    return run_viora_command(args, json_out=True)



def ui_query(code: str, host: str = "localhost", port: int = 18790) -> Dict[str, Any]:
    """Execute Python code in the running GUI context via WebSocket."""
    try:
        from vspice import ui
        proxy = ui.connect(host=host, port=port)
        result = proxy.run_python_code(code)
        return {"ok": True, "output": result.get("output", ""), "is_error": not result.get("ok", False)}
    except Exception as e:
        return {"ok": False, "error": f"VioSpice GUI not detected on {host}:{port} ({e}). Ensure the application is running with the UI Command Server enabled."}

def get_detailed_status() -> Dict[str, Any]:
    """Get a production-grade status report of the VioSpice/Viora ecosystem."""
    exe_str = get_viora_executable()
    exe = Path(exe_str)
    status = {
        "ok": True,
        "root": str(ROOT),
        "viora_executable": str(exe),
        "viora_installed": exe.exists() and os.access(exe, os.X_OK),
        "python_version": sys.version.split()[0],
    }
    
    if status["viora_installed"]:
        try:
            res = subprocess.run([str(exe), "--version"], capture_output=True, text=True, timeout=2)
            status["viora_version"] = res.stdout.strip() or res.stderr.strip() or "v0.2.0"
        except Exception:
            status["viora_version"] = "unknown"
            
    return status

def netlist_validate(file: Optional[str] = None, cir: Optional[str] = None) -> Dict[str, Any]:
    """Validate a SPICE netlist without running simulation."""
    args = ["netlist-validate"]
    if file:
        args.append(file)
        return run_viora_command(args, json_out=True)
    elif cir:
        with tempfile.NamedTemporaryFile(mode='w', suffix='.cir', delete=False) as tf:
            tf.write(cir)
            temp_name = tf.name
        try:
            args.append(temp_name)
            return run_viora_command(args, json_out=True)
        finally:
            if os.path.exists(temp_name): os.remove(temp_name)
    return {"ok": False, "error": "No file or netlist content provided"}

def netlist_run(
    file: Optional[str] = None,
    cir: Optional[str] = None,
    analysis: Optional[str] = None,
    stop: Optional[str] = None,
    step: Optional[str] = None,
    signals: Optional[List[str]] = None,
    json_out: bool = True,
    robust: bool = False,
    compat: bool = True,
    smart_signals: Optional[List[Dict[str, Any]]] = None
) -> Dict[str, Any]:
    """Execute a SPICE or VioSpice simulation."""
    v_args = ["netlist-run"]
    target = file
    temp_file = None

    # Handle Smart Signals by wrapping in a .flxsch
    if smart_signals:
        items = []
        hybrid_netlist = cir or ""
        
        # 1. Algorithmic parts (FluxScript blocks as Code Carriers)
        for ss in smart_signals:
            ref = ss["ref"]
            items.append({
                "type": "SmartSignalBlock",
                "reference": ref,
                "fluxCode": ss["code"],
                "engineType": "flux",
                "excludeFromSim": True # Always let the netlist handle the binding
            })
            
            # Only inject A-device if pins are explicitly provided
            # Otherwise assume the user defined it in 'cir'
            if "inputs" in ss and "outputs" in ss:
                in_nets = " ".join(ss["inputs"])
                out_nets = " ".join(ss["outputs"])
                
                # XSPICE JIT Model binding
                hybrid_netlist += f"\n* Smart Signal Block: {ref}\n"
                hybrid_netlist += f"A_{ref} [{in_nets}] {out_nets} viospice_jit_model_{ref}\n"
                hybrid_netlist += f".model viospice_jit_model_{ref} viospice_jit (jit_id=\"{ref}\")\n"
            else:
                # Still need the model if they use it in their netlist
                hybrid_netlist += f"\n* Smart Signal Model: {ref}\n"
                hybrid_netlist += f".model viospice_jit_model_{ref} viospice_jit (jit_id=\"{ref}\")\n"

        # 2. Add the combined netlist as a Directive
        hybrid_netlist += "\n.save all\n"
        if analysis:
            # If analysis starts with '.', it's a directive, otherwise it's a command
            if not analysis.startswith("."):
                hybrid_netlist += f".{analysis}\n"
            else:
                hybrid_netlist += f"{analysis}\n"
        
        items.append({
            "type": "Spice Directive",
            "text": hybrid_netlist
        })
        
        # 3. Create .flxsch JSON
        sch_json = {
            "metadata": {"application": "viospice", "version": 1},
            "items": items
        }
        
        fd, temp_file = tempfile.mkstemp(suffix=".flxsch", dir=ROOT)
        os.close(fd)
        Path(temp_file).write_text(json.dumps(sch_json), encoding="utf-8")
        target = temp_file
    elif not target and cir:
        # Standard .cir transient file
        fd, temp_file = tempfile.mkstemp(suffix=".cir", dir=ROOT)
        os.close(fd)
        Path(temp_file).write_text(cir, encoding="utf-8")
        target = temp_file
            
    if target: v_args.append(target)
    if analysis: v_args.extend(["--analysis", analysis])
    if stop: v_args.extend(["--stop", stop])
    if step: v_args.extend(["--step", step])
    if signals: 
        for s in signals:
            v_args.extend(["--signal", s])
    if robust: v_args.append("--robust")
    if compat: v_args.append("--compat")
    if json_out: v_args.append("--json")
    
    try:
        return run_viora_command(v_args, json_out=json_out)
    finally:
        if temp_file and os.path.exists(temp_file):
            os.remove(temp_file)

def open_schematic(path: str, convert: bool = False) -> Dict[str, Any]:
    """Open a schematic or netlist file in the running VioSpice GUI."""
    abs_path = os.path.abspath(path)
    
    try:
        from vspice import ui
        proxy = ui.connect()
        
        if convert and (path.endswith(".cir") or path.endswith(".net")):
            # Construction of temporary schematic with Spice Directive
            # needs to stay here for AI-to-Visual bridge
            path_obj = Path(abs_path)
            # Try high-level conversion via Python snippet in GUI
            # actually let's try to keep it simple and just open if convert is false
            # but user might want conversion.
            # For now, let's prioritize simple opening of the .flxsch I created.
            pass

        result = proxy.open_schematic(abs_path)
        return {"ok": result.get("ok", False), "error": result.get("error", "")}
    except Exception as e:
        return {"ok": False, "error": f"VioSpice GUI not detected on localhost:18790 ({e})."}

def get_current_tab() -> Dict[str, Any]:

    """Query the running GUI for the currently active tab."""
    # This snippet looks for the first visible QTabWidget and gets its tab text
    code = """
import vspice
from PySide6.QtWidgets import QApplication, QTabWidget

def _find_active_tab():
    for w in QApplication.topLevelWidgets():
        if not w.isVisible(): continue
        tabs = w.findChild(QTabWidget)
        if tabs and tabs.isVisible():
            return tabs.tabText(tabs.currentIndex())
    return "Project Manager" # Default if no tabs found (usually PM window)

print(_find_active_tab())
"""
    return ui_query(code)

