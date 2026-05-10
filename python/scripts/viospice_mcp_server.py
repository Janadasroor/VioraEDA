#!/usr/bin/env python3
import json
import os
import sys
import tempfile
from pathlib import Path

# Add project root to sys.path to allow importing from 'vspice' package
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))

try:
    from vspice import mcp as core
except ImportError:
    # Fallback if package structure is not yet installed
    sys.stderr.write("Note: Importing vspice.mcp from source tree.\n")
    import vspice.mcp as core

# --- MCP Response Helpers ---

def text_content(obj):
    """Format an object as a standard MCP text content item."""
    return {
        "type": "text",
        "text": json.dumps(obj, ensure_ascii=True, indent=2)
    }

def ok_response(obj, image_path=None):
    """Create a successful MCP tool call result."""
    content = [text_content(obj)]
    if image_path:
        b64 = core.read_image_base64(image_path)
        if b64:
            content.append({
                "type": "image",
                "data": b64,
                "mimeType": "image/png"
            })
    return {
        "content": content,
        "isError": False
    }

def fail_response(msg, **extra):
    """Create a failed MCP tool call result (Tool Execution Error)."""
    obj = {"ok": False, "error": msg, **extra}
    return {
        "content": [text_content(obj)],
        "isError": True
    }

# --- Tool Definitions ---

TOOLS = {
    "viora_status": {
        "description": "Show VioSpice/Viora project repo, CLI engine, and ML API status.",
        "inputSchema": {"type": "object", "properties": {"port": {"type": "integer"}}, "additionalProperties": False},
    },
    "viora_list_files": {
        "description": "List files and directories in the VioSpice/Viora project workspace.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "Relative path from project root (default: '.')"}
            },
            "additionalProperties": False,
        },
    },
    "viora_read_file": {
        "description": "Read the content of a schematic, script, or model file in the project.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "Relative path from project root"}
            },
            "required": ["path"],
            "additionalProperties": False,
        },
    },
    "viora_write_file": {
        "description": "Create or update a design file (schematic, netlist, script) in the project.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "Relative path from project root"},
                "content": {"type": "string", "description": "Text content to write"}
            },
            "required": ["path", "content"],
            "additionalProperties": False,
        },
    },
    "viora_ui_get_current_tab": {
        "description": "Get the currently active editor tab name from the running VioSpice GUI.",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "additionalProperties": False,
        },
    },
    "viospice_raw_info": {
        "description": "Analyze a .raw simulation binary to get signal metadata.",
        "inputSchema": {
            "type": "object",
            "properties": {"file": {"type": "string"}},
            "required": ["file"],
        },
    },
    "viospice_raw_export": {
        "description": "Export binary simulation waveforms to structured JSON, CSV, or Parquet.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {"type": "string"},
                "out": {"type": "string"},
                "format": {"type": "string", "enum": ["json", "csv", "parquet"]},
            },
            "required": ["file", "out"],
        },
    },
    "viora_symbol_list": {
        "description": "Query the Viora Symbol Library for available components.",
        "inputSchema": {
            "type": "object",
            "properties": {"path": {"type": "string"}},
        },
    },
    "viora_schematic_query": {
        "description": "Analyze a Viora .flxsch schematic to extract components and connectivity.",
        "inputSchema": {
            "type": "object",
            "properties": {"file": {"type": "string"}},
            "required": ["file"],
        },
    },
    "viora_flux_run": {
        "description": "Execute VioSpice/Viora automation logic via FluxScript.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {"type": "string", "description": "FluxScript file to run"},
                "code": {"type": "string", "description": "Raw FluxScript code to evaluate"},
                "t": {"type": "number", "description": "Simulation time (default 0.0)"},
                "inputs": {"type": "array", "items": {"type": "number"}, "description": "Array of input values"}
            },
        },
    },
    "viora_flux_help": {
        "description": "Get syntax and usage documentation for the FluxScript language.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    "viospice_ui_open_schematic": {
        "description": "Open a schematic or netlist file in the running VioSpice GUI editor.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "Path to the .flxsch or .cir file"},
                "convert": {"type": "boolean", "description": "If true, convert SPICE netlists to high-level schematics automatically.", "default": False}
            },
            "required": ["path"],
            "additionalProperties": False,
        },
    },
    "viora_pcb_render": {
        "description": "Render a Viora .pcb board to a PNG image.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {"type": "string", "description": "Input .pcb file"},
                "out": {"type": "string", "description": "Output .png file"},
                "timeout": {"type": "integer"}
            },
            "required": ["file", "out"],
            "additionalProperties": False,
        },
    },
    "viora_schematic_render": {
        "description": "Generate a PNG image of a Viora schematic for visual inspection.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {"type": "string", "description": "Input .flxsch file"},
                "out": {"type": "string", "description": "Output .png file"},
                "transparent": {"type": "boolean", "description": "Transparent background"},
                "scale": {"type": "number", "description": "Render scale (default 4.0)"},
                "json": {"type": "boolean"},
                "timeout": {"type": "integer"}
            },
            "required": ["file", "out"],
            "additionalProperties": False,
        },
    },
    "viora_symbol_render": {
        "description": "Generate a PNG image of a Viora component symbol.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {"type": "string", "description": "Input .viosym file"},
                "out": {"type": "string", "description": "Output .png file"},
                "transparent": {"type": "boolean", "description": "Transparent background"},
                "scale": {"type": "number", "description": "Render scale (default 4.0)"},
                "json": {"type": "boolean"},
                "timeout": {"type": "integer"}
            },
            "required": ["file", "out"],
            "additionalProperties": False,
        },
    },
    "viospice_netlist_validate": {
        "description": "Pre-flight validation of a SPICE netlist or Viora schematic.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {"type": "string", "description": "Relative path to .cir or .flxsch"},
                "cir": {"type": "string", "description": "Raw SPICE netlist content"}
            },
            "anyOf": [{"required": ["file"]}, {"required": ["cir"]}],
            "additionalProperties": False,
        },
    },
    "viospice_netlist_run": {
        "description": "Execute a SPICE or VioSpice simulation from a netlist or schematic.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {"type": "string", "description": "Path to .cir or .flxsch"},
                "cir": {"type": "string", "description": "Raw SPICE netlist text"},
                "analysis": {"type": "string", "description": "e.g. 'tran 1n 100u'"},
                "stop": {"type": "string", "description": "Transient stop time"},
                "step": {"type": "string", "description": "Transient step time"},
                "signals": {"type": "array", "items": {"type": "string"}, "description": "Specific nodes to save"},
                "json": {"type": "boolean", "default": True},
                "robust": {"type": "boolean", "description": "Enable robust simulation mode (adds damping)", "default": False},
                "compat": {"type": "boolean", "description": "Enable LTspice compatibility mode", "default": True},
                "smart_signals": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "ref": {"type": "string", "description": "Component reference (e.g. 'SB1')"},
                            "code": {"type": "string", "description": "FluxScript code"},
                            "inputs": {"type": "array", "items": {"type": "string"}},
                            "outputs": {"type": "array", "items": {"type": "string"}}
                        },
                        "required": ["ref", "code", "outputs"]
                    },
                    "description": "Attach FluxScript behavioral logic to specific components."
                }
            },
            "anyOf": [{"required": ["file"]}, {"required": ["cir"]}],
            "additionalProperties": False,
        },
    },
}

def call(name, args):
    """Bridge to the vspice.mcp core implementation."""
    if name == "viora_status":
        return core.get_detailed_status()
    
    if name == "viora_list_files":
        return core.list_files(args.get("path", "."))
    
    if name == "viora_read_file":
        return core.read_file(args["path"])
    
    if name == "viora_write_file":
        return core.write_file(args["path"], args["content"])
    
    if name == "viora_ui_get_current_tab":
        return core.get_current_tab()
    
    if name == "viospice_raw_info":
        return core.raw_info(args["file"])

    if name == "viospice_raw_export":
        return core.raw_export(args["file"], args["out"], args.get("format", "json"))

    if name == "viora_symbol_list":
        return core.symbol_list(args.get("path", "."))

    if name == "viora_schematic_query":
        return core.schematic_query(args["file"])

    if name == "viora_flux_run":
        return core.flux_run(args.get("file"), args.get("code"), t=args.get("t"), inputs=args.get("inputs"))
    
    if name == "viora_flux_help":
        help_path = core.ROOT / "FLUXSCRIPT.md"
        if help_path.exists():
            return {"ok": True, "manual": help_path.read_text()}
        return {"ok": False, "error": "FluxScript manual not found."}
    
    if name == "viora_pcb_render":
        res = core.pcb_render(args["file"], args["out"], timeout=args.get("timeout", 60))
        return (res, args["out"]) if res.get("ok") else res

    if name == "viora_schematic_render":
        res = core.schematic_render(args["file"], args["out"], transparent=args.get("transparent"), scale=args.get("scale"), timeout=args.get("timeout", 60))
        return (res, args["out"]) if res.get("ok") else res

    if name == "viora_symbol_render":
        res = core.symbol_render(args["file"], args["out"], transparent=args.get("transparent"), scale=args.get("scale"), timeout=args.get("timeout", 60))
        return (res, args["out"]) if res.get("ok") else res

    if name == "viospice_ui_open_schematic":
        return core.open_schematic(path=args.get("path"), convert=args.get("convert", False))

    if name == "viospice_netlist_validate":
        return core.netlist_validate(file=args.get("file"), cir=args.get("cir"))
    
    if name == "viospice_netlist_run":
        return core.netlist_run(
            file=args.get("file"),
            cir=args.get("cir"),
            analysis=args.get("analysis"),
            stop=args.get("stop"),
            step=args.get("step"),
            signals=args.get("signals"),
            json_out=args.get("json", True),
            robust=args.get("robust", False),
            compat=args.get("compat", True),
            smart_signals=args.get("smart_signals")
        )
    
    return {"ok": False, "error": f"unknown tool: {name}"}

# --- Standard MCP JSON-RPC Server ---

def send_msg(msg):
    """Send a single-line JSON-RPC message to stdout (MCP standard)."""
    sys.stdout.write(json.dumps(msg, ensure_ascii=True) + "\n")
    sys.stdout.flush()

def read_msg():
    """Read a single line from stdin and parse as JSON."""
    line = sys.stdin.readline()
    if not line: return None
    try:
        return json.loads(line)
    except json.JSONDecodeError:
        sys.stderr.write(f"Error: Malformed JSON received: {line[:100]}...\n")
        return None

def main():
    """Main loop for the MCP server."""
    sys.stderr.write("VioSpice MCP Server started (Standard Line-Delimited JSON Mode)\n")
    while True:
        req = read_msg()
        if req is None: break
        
        method = req.get("method")
        msg_id = req.get("id")
        
        if method == "initialize":
            send_msg({
                "jsonrpc": "2.0",
                "id": msg_id,
                "result": {
                    "protocolVersion": "2024-11-05",
                    "capabilities": {"tools": {}},
                    "serverInfo": {"name": "viospice-mcp", "version": "0.1.0"}
                }
            })
            
        elif method == "tools/list":
            send_msg({
                "jsonrpc": "2.0",
                "id": msg_id,
                "result": {
                    "tools": [{"name": k, **v} for k, v in TOOLS.items()]
                }
            })
            
        elif method == "tools/call":
            params = req.get("params", {})
            name = params.get("name")
            args = params.get("arguments") or {}
            
            raw_out = call(name, args)
            
            # Handle tools that return (result_dict, image_path)
            if isinstance(raw_out, tuple):
                out_dict, img_path = raw_out
            else:
                out_dict, img_path = raw_out, None

            if out_dict.get("ok"):
                send_msg({
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "result": ok_response(out_dict, image_path=img_path)
                })
            else:
                # Signal Tool Execution Error via result.isError: true
                send_msg({
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "result": fail_response(
                        out_dict.get("error", "tool failed"),
                        **{k: v for k, v in out_dict.items() if k != "error"}
                    )
                })
        
        elif method == "notifications/initialized":
            # Optional notification from client
            pass
            
        else:
            # Protocol Error (Method not found)
            if msg_id is not None:
                send_msg({
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "error": {"code": -32601, "message": f"Method not found: {method}"}
                })

if __name__ == "__main__":
    main()
