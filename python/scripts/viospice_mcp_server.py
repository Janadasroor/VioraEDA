#!/usr/bin/env python3
import json
import os
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
LOG = ROOT / ".viospice" / "mcp-ml-api.log"


def text(obj):
    return json.dumps(obj, ensure_ascii=True, indent=2)


def ok(obj):
    return {
        "content": [{"type": "text", "text": text(obj)}],
        "structuredContent": obj,
    }


def fail(msg, **extra):
    obj = {"ok": False, "error": msg, **extra}
    return {
        "content": [{"type": "text", "text": text(obj)}],
        "structuredContent": obj,
        "isError": True,
    }


def cli():
    for item in [
        ROOT / "build" / "viora",
        ROOT / "build-debug" / "viora",
        ROOT / "build-asan" / "viora",
        ROOT / "build" / "dev-debug" / "viora",
        ROOT / "build" / "vio-cmd",
        ROOT / "build-debug" / "vio-cmd",
    ]:
        if item.exists():
            return str(item)
    return "viora"


def py():
    return sys.executable or "python3"


def req(method, path, port, body=None, headers=None, timeout=5):
    data = None if body is None else json.dumps(body).encode()
    url = f"http://127.0.0.1:{port}{path}"
    hdr = {"Content-Type": "application/json", **(headers or {})}
    out = urllib.request.Request(url, data=data, headers=hdr, method=method)
    with urllib.request.urlopen(out, timeout=timeout) as res:
        raw = res.read().decode()
        return json.loads(raw) if raw else {}


def health(port):
    try:
        return req("GET", "/api/ml/health", port, timeout=2)
    except Exception:
        return None


def start_api(port, cli_path):
    live = health(port)
    if live:
        return {"ok": True, "status": "already_running", "port": port, "health": live}
    LOG.parent.mkdir(parents=True, exist_ok=True)
    with LOG.open("ab") as log:
        proc = subprocess.Popen(
            [
                py(),
                str(ROOT / "python" / "scripts" / "fastapi_ml_dataset_api.py"),
                "--host",
                "127.0.0.1",
                "--port",
                str(port),
                "--cli-path",
                cli_path,
            ],
            cwd=ROOT,
            stdout=log,
            stderr=log,
            start_new_session=True,
        )
    for _ in range(40):
        time.sleep(0.5)
        live = health(port)
        if live:
            return {"ok": True, "status": "started", "port": port, "pid": proc.pid, "health": live}
        if proc.poll() is not None:
            break
    return {
        "ok": False,
        "status": "failed",
        "port": port,
        "pid": proc.pid,
        "log": str(LOG),
    }


def run(cmd, timeout=120, json_out=False):
    try:
        proc = subprocess.run(
            cmd,
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired:
        return {"ok": False, "timeout": timeout, "command": cmd}
    out = proc.stdout.strip()
    err = proc.stderr.strip()
    data = None
    if json_out and out:
      try:
          data = json.loads(out)
      except json.JSONDecodeError:
          data = None
    return {
        "ok": proc.returncode == 0,
        "code": proc.returncode,
        "command": cmd,
        "stdout": out,
        "stderr": err,
        "data": data,
    }


def add(cmd, flag, val):
    if val is None:
        return
    if isinstance(val, bool):
        if val:
            cmd.append(flag)
        return
    if isinstance(val, list):
        for item in val:
            cmd.extend([flag, str(item)])
        return
    cmd.extend([flag, str(val)])


def netlist_run(args):
    file = args.get("file")
    if not file and args.get("cir"):
        fd, file = tempfile.mkstemp(prefix="viocode-", suffix=".cir", dir=ROOT)
        os.close(fd)
        Path(file).write_text(args["cir"], encoding="utf-8")
    if not file:
        return {"ok": False, "error": "missing file or cir"}
    cmd = [cli(), "netlist-run", file]
    add(cmd, "--analysis", args.get("analysis"))
    add(cmd, "--stop", args.get("stop"))
    add(cmd, "--step", args.get("step"))
    add(cmd, "--export-raw", args.get("export_raw"))
    add(cmd, "--signal", args.get("signals") or [])
    add(cmd, "--measure", args.get("measures") or [])
    add(cmd, "--assert", args.get("assertions") or [])
    add(cmd, "--max-points", args.get("max_points"))
    add(cmd, "--base-signal", args.get("base_signal"))
    add(cmd, "--range", args.get("range"))
    add(cmd, "--timeout", args.get("sim_timeout"))
    add(cmd, "--compat", args.get("compat"))
    add(cmd, "--stats", args.get("stats"))
    if args.get("json", True):
        cmd.append("--json")
    if args.get("quiet", True):
        cmd.append("--quiet")
    return run(cmd, timeout=args.get("timeout", 180), json_out=args.get("json", True))


def schematic_netlist(args):
    cmd = [cli(), "schematic-netlist", args["file"]]
    add(cmd, "--analysis", args.get("analysis"))
    add(cmd, "--stop", args.get("stop"))
    add(cmd, "--step", args.get("step"))
    add(cmd, "--format", args.get("format"))
    add(cmd, "--out", args.get("out"))
    if args.get("json", False):
        cmd.append("--json")
        cmd.append("--quiet")
    return run(cmd, timeout=args.get("timeout", 120), json_out=args.get("json", False))


def flux(args):
    cmd = [cli(), "flux", args["subcommand"]]
    if args.get("target"):
        cmd.append(args["target"])
    add(cmd, "--output", args.get("output"))
    add(cmd, "--tran", args.get("tran"))
    add(cmd, "--netlist", args.get("netlist"))
    add(cmd, "--run", args.get("run"))
    add(cmd, "--json", args.get("json"))
    if args.get("quiet", True):
        cmd.append("--quiet")
    return run(cmd, timeout=args.get("timeout", 180), json_out=args.get("json", False))


def raw_info(args):
    cmd = [cli(), "raw-info", args["file"]]
    add(cmd, "--summary", args.get("summary"))
    add(cmd, "--json", args.get("json", True))
    if args.get("json", True):
        cmd.append("--quiet")
    return run(cmd, timeout=args.get("timeout", 60), json_out=args.get("json", True))


def raw_export(args):
    cmd = [cli(), "raw-export", args["file"]]
    add(cmd, "--format", args.get("format"))
    add(cmd, "--out", args.get("out"))
    add(cmd, "--signal", args.get("signals") or [])
    add(cmd, "--signal-regex", args.get("signal_regex"))
    add(cmd, "--max-points", args.get("max_points"))
    add(cmd, "--base-signal", args.get("base_signal"))
    add(cmd, "--range", args.get("range"))
    return run(cmd, timeout=args.get("timeout", 120), json_out=args.get("format") == "json")


def report(args):
    cmd = [cli(), "generate-report", args["file"], args["out"]]
    add(cmd, "--title", args.get("title"))
    add(cmd, "--author", args.get("author"))
    add(cmd, "--raw-file", args.get("raw_file"))
    add(cmd, "--schematic-png", args.get("schematic_png"))
    add(cmd, "--no-schematic", args.get("no_schematic"))
    add(cmd, "--no-waveforms", args.get("no_waveforms"))
    add(cmd, "--no-measurements", args.get("no_measurements"))
    add(cmd, "--no-netlist", args.get("no_netlist"))
    add(cmd, "--max-points", args.get("max_points"))
    add(cmd, "--json", args.get("json"))
    return run(cmd, timeout=args.get("timeout", 180), json_out=args.get("json", False))


def ml_sim(args):
    port = args.get("port", 8790)
    cli_path = cli()
    live = health(port)
    if not live and args.get("auto_start", True):
        started = start_api(port, cli_path)
        if not started.get("ok"):
            return started
        live = started.get("health")
    if not live:
        return {"ok": False, "error": "ML API is not running", "port": port}
    hdr = {}
    if args.get("api_key"):
        hdr["X-API-Key"] = args["api_key"]
    try:
        res = req("POST", "/api/ml/simulate", port, body=args["payload"], headers=hdr, timeout=args.get("timeout", 60))
        return {"ok": True, "port": port, "data": res}
    except urllib.error.HTTPError as err:
        body = err.read().decode()
        return {"ok": False, "port": port, "status": err.code, "body": body}
    except Exception as err:
        return {"ok": False, "port": port, "error": str(err)}


TOOLS = {
    "viospice_status": {
        "description": "Show VioSpice repo, CLI, and ML API status.",
        "inputSchema": {"type": "object", "properties": {"port": {"type": "integer"}}, "additionalProperties": False},
    },
    "viospice_netlist_run": {
        "description": "Run a VioSpice netlist or schematic simulation with viora netlist-run.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {"type": "string"},
                "cir": {"type": "string"},
                "analysis": {"type": "string"},
                "stop": {"type": "string"},
                "step": {"type": "string"},
                "export_raw": {"type": "string"},
                "signals": {"type": "array", "items": {"type": "string"}},
                "measures": {"type": "array", "items": {"type": "string"}},
                "assertions": {"type": "array", "items": {"type": "string"}},
                "max_points": {"type": "integer"},
                "base_signal": {"type": "string"},
                "range": {"type": "string"},
                "sim_timeout": {"type": "string"},
                "compat": {"type": "boolean"},
                "stats": {"type": "boolean"},
                "json": {"type": "boolean"},
                "quiet": {"type": "boolean"},
                "timeout": {"type": "integer"},
            },
            "anyOf": [{"required": ["file"]}, {"required": ["cir"]}],
            "additionalProperties": False,
        },
    },
    "viospice_schematic_netlist": {
        "description": "Generate a SPICE or JSON netlist from a .flxsch schematic.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {"type": "string"},
                "analysis": {"type": "string"},
                "stop": {"type": "string"},
                "step": {"type": "string"},
                "format": {"type": "string"},
                "out": {"type": "string"},
                "json": {"type": "boolean"},
                "timeout": {"type": "integer"},
            },
            "required": ["file"],
            "additionalProperties": False,
        },
    },
    "viospice_flux": {
        "description": "Run VioSpice FluxScript commands such as run, compile, eval, or repl-style evals.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "subcommand": {"type": "string"},
                "target": {"type": "string"},
                "output": {"type": "string"},
                "tran": {"type": "string"},
                "netlist": {"type": "boolean"},
                "run": {"type": "boolean"},
                "json": {"type": "boolean"},
                "quiet": {"type": "boolean"},
                "timeout": {"type": "integer"},
            },
            "required": ["subcommand"],
            "additionalProperties": False,
        },
    },
    "viospice_raw_info": {
        "description": "Inspect a VioSpice .raw file.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {"type": "string"},
                "summary": {"type": "boolean"},
                "json": {"type": "boolean"},
                "timeout": {"type": "integer"},
            },
            "required": ["file"],
            "additionalProperties": False,
        },
    },
    "viospice_raw_export": {
        "description": "Export signals from a VioSpice .raw file.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {"type": "string"},
                "format": {"type": "string"},
                "out": {"type": "string"},
                "signals": {"type": "array", "items": {"type": "string"}},
                "signal_regex": {"type": "string"},
                "max_points": {"type": "integer"},
                "base_signal": {"type": "string"},
                "range": {"type": "string"},
                "timeout": {"type": "integer"},
            },
            "required": ["file"],
            "additionalProperties": False,
        },
    },
    "viospice_generate_report": {
        "description": "Generate a VioSpice HTML report for a schematic.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {"type": "string"},
                "out": {"type": "string"},
                "title": {"type": "string"},
                "author": {"type": "string"},
                "raw_file": {"type": "string"},
                "schematic_png": {"type": "string"},
                "no_schematic": {"type": "boolean"},
                "no_waveforms": {"type": "boolean"},
                "no_measurements": {"type": "boolean"},
                "no_netlist": {"type": "boolean"},
                "max_points": {"type": "integer"},
                "json": {"type": "boolean"},
                "timeout": {"type": "integer"},
            },
            "required": ["file", "out"],
            "additionalProperties": False,
        },
    },
    "viospice_ml_api_start": {
        "description": "Start the local VioSpice FastAPI ML simulation service.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "port": {"type": "integer"},
            },
            "additionalProperties": False,
        },
    },
    "viospice_ml_api_health": {
        "description": "Check the local VioSpice FastAPI ML simulation service.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "port": {"type": "integer"},
            },
            "additionalProperties": False,
        },
    },
    "viospice_ml_api_simulate": {
        "description": "Run one simulation job through the VioSpice ML FastAPI service.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "payload": {"type": "object"},
                "port": {"type": "integer"},
                "api_key": {"type": "string"},
                "auto_start": {"type": "boolean"},
                "timeout": {"type": "integer"},
            },
            "required": ["payload"],
            "additionalProperties": False,
        },
    },
}


def call(name, args):
    if name == "viospice_status":
        port = args.get("port", 8790)
        return {
            "ok": True,
            "root": str(ROOT),
            "cli": cli(),
            "ml_api": health(port),
            "log": str(LOG),
        }
    if name == "viospice_netlist_run":
        return netlist_run(args)
    if name == "viospice_schematic_netlist":
        return schematic_netlist(args)
    if name == "viospice_flux":
        return flux(args)
    if name == "viospice_raw_info":
        return raw_info(args)
    if name == "viospice_raw_export":
        return raw_export(args)
    if name == "viospice_generate_report":
        return report(args)
    if name == "viospice_ml_api_start":
        return start_api(args.get("port", 8790), cli())
    if name == "viospice_ml_api_health":
        out = health(args.get("port", 8790))
        return {"ok": bool(out), "data": out}
    if name == "viospice_ml_api_simulate":
        return ml_sim(args)
    return {"ok": False, "error": f"unknown tool: {name}"}


def send(msg):
    data = json.dumps(msg, ensure_ascii=True).encode()
    sys.stdout.buffer.write(f"Content-Length: {len(data)}\r\n\r\n".encode())
    sys.stdout.buffer.write(data)
    sys.stdout.buffer.flush()


def read():
    hdr = {}
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        if line in (b"\r\n", b"\n"):
            break
        key, val = line.decode().split(":", 1)
        hdr[key.strip().lower()] = val.strip()
    size = int(hdr.get("content-length", "0"))
    if size <= 0:
        return None
    body = sys.stdin.buffer.read(size)
    return json.loads(body.decode())


def resp(req, result=None, error=None):
    msg = {"jsonrpc": "2.0", "id": req["id"]}
    if error is not None:
        msg["error"] = error
    else:
        msg["result"] = result
    send(msg)


def main():
    while True:
        req = read()
        if req is None:
            break
        method = req.get("method")
        if method == "initialize":
            resp(
                req,
                {
                    "protocolVersion": req.get("params", {}).get("protocolVersion", "2024-11-05"),
                    "capabilities": {"tools": {}},
                    "serverInfo": {"name": "viospice-mcp", "version": "0.1.0"},
                },
            )
            continue
        if method == "notifications/initialized":
            continue
        if method == "ping":
            resp(req, {})
            continue
        if method == "tools/list":
            resp(req, {"tools": [{"name": key, **val} for key, val in TOOLS.items()]})
            continue
        if method == "tools/call":
            params = req.get("params", {})
            name = params.get("name")
            args = params.get("arguments") or {}
            out = call(name, args)
            if out.get("ok"):
                resp(req, ok(out))
            else:
                resp(req, fail(out.get("error", "tool failed"), **{k: v for k, v in out.items() if k != "error"}))
            continue
        if "id" in req:
            resp(req, error={"code": -32601, "message": f"Method not found: {method}"})


if __name__ == "__main__":
    main()
