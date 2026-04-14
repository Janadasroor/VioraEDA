#!/usr/bin/env python3
"""
vspice-cli — Headless VioSpice simulation CLI for ML pipelines.

Usage:
    vspice-cli netlist-run <file.cir> [--analysis tran] [--stop 10m] [--step 100u]
    vspice-cli netlist-run <file.cir> --json
    vspice-cli build --netlist netlist.cir --r1 1k --r2 2k --vin 5
    vspice-cli sweep --template netlist.cir --param R1 linspace:100:10k:10
    vspice-cli meas-eval --results results.json --meas ".meas tran VOUT_MAX MAX V(out)"
    vspice-cli parse-model ".model 2N3904 NPN (IS=6.734f BF=416)"
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


def resolve_vio_cmd():
    """Find the vio-cmd binary."""
    candidates = []
    # Build tree paths
    for rel in [
        "build/vio-cmd", "build-debug/vio-cmd", "build-asan/vio-cmd",
        "build/dev-debug/vio-cmd",
    ]:
        candidates.append(Path(__file__).resolve().parent / rel)
        candidates.append(Path(__file__).resolve().parent.parent / rel)
    candidates.extend(["vio-cmd"])

    for c in candidates:
        c = str(c)
        if os.path.isabs(c) and os.path.isfile(c) and os.access(c, os.X_OK):
            return c
        # Check PATH
        for p in os.environ.get("PATH", "").split(":"):
            full = os.path.join(p, c if os.path.isabs(c) else os.path.basename(c))
            if os.path.isfile(full) and os.access(full, os.X_OK):
                return full
    return None


def cmd_netlist_run(args):
    """Run a SPICE netlist through vio-cmd."""
    vio_cmd = args.vio_cmd or resolve_vio_cmd()
    if not vio_cmd:
        print("Error: vio-cmd not found. Build it first or pass --vio-cmd.", file=sys.stderr)
        sys.exit(1)

    cmd = [vio_cmd, "netlist-run", args.file]
    if args.analysis:
        cmd += ["--analysis", args.analysis]
    if args.stop:
        cmd += ["--stop", args.stop]
    if args.step:
        cmd += ["--step", args.step]
    if args.signal:
        for s in args.signal:
            cmd += ["--signal", s]
    if args.measure:
        for m in args.measure:
            cmd += ["--measure", m]
    if args.json:
        cmd += ["--json"]
    if args.stats:
        cmd += ["--stats"]
    if args.quiet:
        cmd += ["--quiet"]
    if args.compat:
        cmd += ["--compat"]

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0 and not args.json:
        print(result.stderr, file=sys.stderr)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr and not args.quiet:
        print(result.stderr, file=sys.stderr)
    sys.exit(result.returncode)


def cmd_parse_model(args):
    """Parse a SPICE .model line using vspice Python bindings."""
    import vspice

    ok, models, diags = vspice.SimModelParser.parse_model_line(args.line)
    if not ok:
        print(json.dumps({"ok": False, "diagnostics": [
            {"severity": d.severity, "line": d.line, "message": d.message}
            for d in diags
        ]}, indent=2))
        sys.exit(1)

    result = {
        "ok": True,
        "models": {
            name: {
                "name": m.name,
                "type": str(m.type),
                "params": dict(m.params),
            }
            for name, m in models.items()
        }
    }
    print(json.dumps(result, indent=2))


def cmd_parse_meas(args):
    """Parse a .meas statement using vspice."""
    import vspice

    stmt, ok = vspice.meas_parse(args.line)
    if not ok:
        print(json.dumps({"ok": False, "error": "Failed to parse measurement"}), file=sys.stderr)
        sys.exit(1)

    result = {
        "ok": True,
        "statement": {
            "name": stmt.name,
            "analysis_type": stmt.analysis_type,
            "function": str(stmt.function),
            "signal": stmt.signal,
            "expr": stmt.expr,
            "has_at": stmt.has_at,
            "at": stmt.at,
            "has_trig_targ": stmt.has_trig_targ,
            "has_from": stmt.has_from,
            "from_val": getattr(stmt, "from"),
            "has_to": stmt.has_to,
            "to_val": stmt.to,
        }
    }
    print(json.dumps(result, indent=2))


def cmd_netlist_info(args):
    """Show info about a netlist file."""
    vio_cmd = args.vio_cmd or resolve_vio_cmd()
    if not vio_cmd:
        print("Error: vio-cmd not found.", file=sys.stderr)
        sys.exit(1)

    cmd = [vio_cmd, "schematic-netlist", args.file, "--format", "json"]
    if args.analysis:
        cmd += ["--analysis", args.analysis]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stderr, file=sys.stderr)
        sys.exit(result.returncode)
    if result.stdout:
        print(result.stdout, end="")


def cmd_raw_info(args):
    """Inspect a .raw waveform file."""
    vio_cmd = args.vio_cmd or resolve_vio_cmd()
    if not vio_cmd:
        print("Error: vio-cmd not found.", file=sys.stderr)
        sys.exit(1)

    cmd = [vio_cmd, "raw-info", args.file, "--json"]
    if args.summary:
        cmd += ["--summary"]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stderr, file=sys.stderr)
        sys.exit(result.returncode)
    if result.stdout:
        print(result.stdout, end="")


def cmd_raw_export(args):
    """Export signals from a .raw file."""
    vio_cmd = args.vio_cmd or resolve_vio_cmd()
    if not vio_cmd:
        print("Error: vio-cmd not found.", file=sys.stderr)
        sys.exit(1)

    cmd = [vio_cmd, "raw-export", args.file]
    if args.format:
        cmd += ["--format", args.format]
    if args.out:
        cmd += ["--out", args.out]
    if args.signal:
        for s in args.signal:
            cmd += ["--signal", s]
    if args.max_points:
        cmd += ["--max-points", str(args.max_points)]
    if args.range:
        cmd += ["--range", args.range]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stderr, file=sys.stderr)
        sys.exit(result.returncode)
    if result.stdout:
        print(result.stdout, end="")


def cmd_python_api(args):
    """Show vspice Python API info and test it."""
    try:
        import vspice
    except ImportError:
        print("Error: vspice module not installed.", file=sys.stderr)
        print("Run: ln -s build/python/vspice*.so ~/.local/lib/python3.12/site-packages/", file=sys.stderr)
        sys.exit(1)

    print("vspice module loaded successfully!")
    print(f"Module doc: {vspice.__doc__.strip()[:120]}...")
    print()

    # Quick smoke test
    val, ok = vspice.parse_spice_number("4.7u")
    print(f"  parse_spice_number('4.7u') = {val} (ok={ok})")

    wf = vspice.SimWaveform("test", [0, 1, 2], [0, 1.5, 3.0])
    print(f"  SimWaveform stats: {wf.stats()}")

    nl = vspice.SimNetlist()
    nl.add_node("IN")
    print(f"  SimNetlist: {nl.to_dict()}")


def main():
    parser = argparse.ArgumentParser(
        prog="vspice-cli",
        description="VioSpice headless simulation CLI for ML pipelines.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s netlist-run circuit.cir --analysis tran --stop 10m --json
  %(prog)s parse-model ".model 2N3904 NPN (IS=6.734f BF=416.4)"
  %(prog)s parse-meas ".meas tran VOUT_MAX MAX V(out)"
  %(prog)s raw-info output.raw --summary --json
  %(prog)s python-api
        """
    )
    parser.add_argument("--vio-cmd", help="Path to vio-cmd binary")
    parser.add_argument("--quiet", "-q", action="store_true", help="Suppress non-essential output")

    sub = parser.add_subparsers(dest="command", required=True)

    # netlist-run
    p_run = sub.add_parser("netlist-run", help="Run a netlist simulation")
    p_run.add_argument("file", help="Netlist file (.cir)")
    p_run.add_argument("--analysis", "-a", choices=["tran", "ac", "op", "dc"], default="op")
    p_run.add_argument("--stop", help="Stop time (e.g. 10m)")
    p_run.add_argument("--step", help="Step size (e.g. 100u)")
    p_run.add_argument("--signal", "-s", action="append", help="Signal to probe")
    p_run.add_argument("--measure", "-m", action="append", help="Measurement expression")
    p_run.add_argument("--json", "-j", action="store_true", help="Output as JSON")
    p_run.add_argument("--stats", action="store_true", help="Include waveform statistics")
    p_run.add_argument("--compat", action="store_true", help="Compatibility mode")
    p_run.set_defaults(func=cmd_netlist_run)

    # parse-model
    p_pm = sub.add_parser("parse-model", help="Parse a SPICE .model line")
    p_pm.add_argument("line", help=".model line text")
    p_pm.set_defaults(func=cmd_parse_model)

    # parse-meas
    p_pmeas = sub.add_parser("parse-meas", help="Parse a .meas statement")
    p_pmeas.add_argument("line", help=".meas line text")
    p_pmeas.set_defaults(func=cmd_parse_meas)

    # raw-info
    p_ri = sub.add_parser("raw-info", help="Inspect a .raw waveform file")
    p_ri.add_argument("file", help=".raw file path")
    p_ri.add_argument("--summary", action="store_true", help="Show summary only")
    p_ri.set_defaults(func=cmd_raw_info)

    # raw-export
    p_re = sub.add_parser("raw-export", help="Export signals from .raw file")
    p_re.add_argument("file", help=".raw file path")
    p_re.add_argument("--format", "-f", choices=["csv", "json", "parquet"], default="json")
    p_re.add_argument("--out", "-o", help="Output file path")
    p_re.add_argument("--signal", "-s", action="append", help="Signal names to export")
    p_re.add_argument("--max-points", type=int, help="Max points per signal")
    p_re.add_argument("--range", help="Time/frequency range (e.g. 0:1m)")
    p_re.set_defaults(func=cmd_raw_export)

    # netlist-info
    p_ni = sub.add_parser("netlist-info", help="Show netlist file info")
    p_ni.add_argument("file", help="Netlist or schematic file")
    p_ni.add_argument("--analysis", "-a", choices=["tran", "ac", "op"])
    p_ni.set_defaults(func=cmd_netlist_info)

    # python-api
    p_py = sub.add_parser("python-api", help="Test vspice Python API")
    p_py.set_defaults(func=cmd_python_api)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
