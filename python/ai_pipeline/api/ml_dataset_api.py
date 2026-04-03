import argparse
import json
import math
import os
import shutil
import subprocess
import threading
import time
import uuid
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional
from urllib.parse import urlparse


def _utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def resolve_vio_cmd(explicit_path: Optional[str] = None) -> str:
    candidates: List[str] = []
    if explicit_path:
        candidates.append(os.path.abspath(explicit_path))

    repo_root = _repo_root()
    for rel in (
        "build/vio-cmd",
        "build-debug/vio-cmd",
        "build-asan/vio-cmd",
        "build/dev-debug/vio-cmd",
        "build/flux-cmd",
        "build-debug/flux-cmd",
        "build-asan/flux-cmd",
        "build/dev-debug/flux-cmd",
    ):
        candidates.append(str(repo_root / rel))

    candidates.extend(["vio-cmd", "flux-cmd"])

    for candidate in candidates:
        if os.path.isabs(candidate) and os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate
        resolved = shutil.which(candidate)
        if resolved:
            return resolved

    return candidates[0]


@dataclass
class CommandResult:
    args: List[str]
    returncode: int
    stdout: str
    stderr: str


class VioCmdRunner:
    def __init__(self, cli_path: Optional[str] = None):
        self.cli_path = resolve_vio_cmd(cli_path)

    def _run(self, args: List[str], timeout: Optional[float] = None) -> CommandResult:
        completed = subprocess.run(
            args,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
        )
        return CommandResult(
            args=args,
            returncode=completed.returncode,
            stdout=completed.stdout,
            stderr=completed.stderr,
        )

    @staticmethod
    def _parse_json_output(result: CommandResult) -> Dict[str, Any]:
        payload = (result.stdout or "").strip()
        if not payload:
            raise ValueError(f"Command produced no stdout. stderr={result.stderr.strip()}")
        return json.loads(payload)

    def schematic_netlist(
        self,
        schematic_path: str,
        analysis: str,
        stop: Optional[str] = None,
        step: Optional[str] = None,
        timeout_seconds: Optional[float] = None,
    ) -> Dict[str, Any]:
        cmd = [
            self.cli_path,
            "schematic-netlist",
            schematic_path,
            "--format",
            "json",
            "--analysis",
            analysis,
        ]
        if stop:
            cmd.extend(["--stop", stop])
        if step:
            cmd.extend(["--step", step])
        result = self._run(cmd, timeout=timeout_seconds)
        if result.returncode != 0:
            raise RuntimeError(result.stderr.strip() or "schematic-netlist failed")
        return self._parse_json_output(result)

    def simulate(
        self,
        schematic_path: str,
        analysis: str,
        stop: Optional[str] = None,
        step: Optional[str] = None,
        timeout_seconds: Optional[float] = None,
    ) -> Dict[str, Any]:
        cmd = [
            self.cli_path,
            "simulate",
            schematic_path,
            "--analysis",
            analysis,
            "--json",
        ]
        if stop:
            cmd.extend(["--stop", stop])
        if step:
            cmd.extend(["--step", step])
        result = self._run(cmd, timeout=timeout_seconds)
        if result.returncode != 0:
            raise RuntimeError(result.stderr.strip() or "simulate failed")
        return self._parse_json_output(result)


def _parse_spice_number(value: Optional[str]) -> Optional[float]:
    if value is None:
        return None
    text = str(value).strip().lower()
    if not text:
        return None
    suffixes = {
        "t": 1e12,
        "g": 1e9,
        "meg": 1e6,
        "k": 1e3,
        "m": 1e-3,
        "u": 1e-6,
        "n": 1e-9,
        "p": 1e-12,
        "f": 1e-15,
    }
    for suffix in ("meg", "t", "g", "k", "m", "u", "n", "p", "f"):
        if text.endswith(suffix) and text != suffix:
            return float(text[: -len(suffix)]) * suffixes[suffix]
    return float(text)


def _parse_range(range_expr: Optional[str]) -> Optional[tuple]:
    if not range_expr:
        return None
    parts = str(range_expr).split(":")
    if len(parts) != 2:
        raise ValueError("range must use t0:t1 syntax")
    start = _parse_spice_number(parts[0])
    end = _parse_spice_number(parts[1])
    if start is None or end is None:
        raise ValueError("range must contain valid spice numbers")
    return (min(start, end), max(start, end))


def _crop_xy(x_values: List[float], y_values: List[float], range_expr: Optional[str]) -> tuple[List[float], List[float]]:
    parsed = _parse_range(range_expr)
    if not parsed:
        return list(x_values), list(y_values)
    start, end = parsed
    cropped_x: List[float] = []
    cropped_y: List[float] = []
    for x, y in zip(x_values, y_values):
        if start <= x <= end:
            cropped_x.append(x)
            cropped_y.append(y)
    return cropped_x, cropped_y


def _decimate_xy(x_values: List[float], y_values: List[float], max_points: Optional[int]) -> tuple[List[float], List[float]]:
    if not max_points or max_points <= 0 or len(x_values) <= max_points:
        return list(x_values), list(y_values)
    indices = []
    last_index = len(x_values) - 1
    for point_index in range(max_points):
        idx = round(point_index * last_index / max(1, max_points - 1))
        indices.append(idx)
    deduped = sorted(set(indices))
    return [x_values[i] for i in deduped], [y_values[i] for i in deduped]


def _match_waveform(simulation_json: Dict[str, Any], signal_name: str) -> Optional[Dict[str, Any]]:
    target = signal_name.strip().lower()
    for wave in simulation_json.get("waveforms", []):
        if str(wave.get("name", "")).strip().lower() == target:
            return wave
    return None


def _pseudo_waveform_from_scalar(name: str, value: float) -> Dict[str, Any]:
    return {"name": name, "x": [0.0], "y": [value]}


def _collect_waveforms(simulation_json: Dict[str, Any], requested_signals: List[str]) -> List[Dict[str, Any]]:
    if requested_signals:
        collected: List[Dict[str, Any]] = []
        for signal in requested_signals:
            wave = _match_waveform(simulation_json, signal)
            if wave:
                collected.append(
                    {
                        "name": wave.get("name", signal),
                        "x": list(wave.get("x", [])),
                        "y": list(wave.get("y", [])),
                    }
                )
                continue

            node_voltages = simulation_json.get("nodeVoltages", {})
            branch_currents = simulation_json.get("branchCurrents", {})
            normalized = signal.strip()
            node_key = normalized[2:-1] if normalized.lower().startswith("v(") and normalized.endswith(")") else normalized
            branch_key = normalized[2:-1] if normalized.lower().startswith("i(") and normalized.endswith(")") else normalized
            for key, value in node_voltages.items():
                if str(key).strip().lower() == node_key.lower():
                    collected.append(_pseudo_waveform_from_scalar(f"V({key})", value))
                    break
            else:
                for key, value in branch_currents.items():
                    if str(key).strip().lower() == branch_key.lower():
                        collected.append(_pseudo_waveform_from_scalar(f"I({key})", value))
                        break
        return collected

    if simulation_json.get("waveforms"):
        return [
            {
                "name": wave.get("name", ""),
                "x": list(wave.get("x", [])),
                "y": list(wave.get("y", [])),
            }
            for wave in simulation_json.get("waveforms", [])
        ]

    collected = []
    for key, value in simulation_json.get("nodeVoltages", {}).items():
        collected.append(_pseudo_waveform_from_scalar(f"V({key})", value))
    for key, value in simulation_json.get("branchCurrents", {}).items():
        collected.append(_pseudo_waveform_from_scalar(f"I({key})", value))
    return collected


def _waveform_stats(name: str, y_values: List[float]) -> Dict[str, Any]:
    if not y_values:
        return {"name": name, "count": 0}
    avg = sum(y_values) / len(y_values)
    rms = math.sqrt(sum(value * value for value in y_values) / len(y_values))
    return {
        "name": name,
        "count": len(y_values),
        "min": min(y_values),
        "max": max(y_values),
        "avg": avg,
        "rms": rms,
        "pp": max(y_values) - min(y_values),
    }


def _nearest_value_at(x_values: List[float], y_values: List[float], target: float) -> float:
    nearest_index = min(range(len(x_values)), key=lambda idx: abs(x_values[idx] - target))
    return y_values[nearest_index]


def _compute_measure(expression: str, waveforms: List[Dict[str, Any]]) -> Dict[str, Any]:
    text = expression.strip()
    parts = text.split(None, 1)
    if len(parts) != 2:
        return {"expr": expression, "error": "Unsupported measure format"}
    op = parts[0].lower()
    remainder = parts[1].strip()

    at_target = None
    signal_name = remainder
    if " at " in remainder.lower():
        signal_name, at_text = remainder.rsplit(" at ", 1)
        at_target = _parse_spice_number(at_text.strip())

    target_waveform = None
    for waveform in waveforms:
        if str(waveform.get("name", "")).strip().lower() == signal_name.strip().lower():
            target_waveform = waveform
            break
    if not target_waveform:
        return {"expr": expression, "error": "Signal not found"}

    x_values = list(target_waveform.get("x", []))
    y_values = list(target_waveform.get("y", []))
    if not y_values:
        return {"expr": expression, "error": "No samples"}
    if at_target is not None:
        return {"expr": expression, "value": _nearest_value_at(x_values, y_values, at_target)}
    if op == "avg":
        return {"expr": expression, "value": sum(y_values) / len(y_values)}
    if op == "max":
        return {"expr": expression, "value": max(y_values)}
    if op == "min":
        return {"expr": expression, "value": min(y_values)}
    if op == "rms":
        return {"expr": expression, "value": math.sqrt(sum(value * value for value in y_values) / len(y_values))}
    if op == "pp":
        return {"expr": expression, "value": max(y_values) - min(y_values)}
    if op == "value":
        return {"expr": expression, "error": "value measures require 'at <time>'"}
    return {"expr": expression, "error": "Unsupported measure operator"}


class SimulationDatasetService:
    def __init__(self, runner: Optional[VioCmdRunner] = None):
        self.runner = runner or VioCmdRunner()

    @staticmethod
    def _normalize_job(job: Dict[str, Any]) -> Dict[str, Any]:
        normalized = dict(job)
        normalized["job_id"] = str(normalized.get("job_id") or uuid.uuid4())
        normalized["analysis"] = str(normalized.get("analysis") or "tran").lower()
        normalized["include_netlist"] = bool(normalized.get("include_netlist", True))
        normalized["include_raw"] = bool(normalized.get("include_raw", True))
        normalized["include_stats"] = bool(normalized.get("include_stats", True))
        normalized["signals"] = list(normalized.get("signals") or [])
        normalized["measures"] = list(normalized.get("measures") or [])
        normalized["labels"] = dict(normalized.get("labels") or {})
        normalized["tags"] = dict(normalized.get("tags") or {})
        normalized["metadata"] = dict(normalized.get("metadata") or {})
        normalized["timeout_seconds"] = normalized.get("timeout_seconds")
        normalized["max_points"] = normalized.get("max_points")
        normalized["base_signal"] = normalized.get("base_signal")
        normalized["range"] = normalized.get("range")
        normalized["stop"] = normalized.get("stop")
        normalized["step"] = normalized.get("step")
        normalized["compat"] = bool(normalized.get("compat", False))
        return normalized

    def run_job(self, job: Dict[str, Any]) -> Dict[str, Any]:
        normalized = self._normalize_job(job)
        schematic_path = normalized.get("schematic_path")
        if not schematic_path:
            raise ValueError("schematic_path is required")
        if not os.path.exists(schematic_path):
            raise FileNotFoundError(f"schematic_path not found: {schematic_path}")

        started_at = time.time()
        netlist_json = None
        if normalized["include_netlist"]:
            netlist_json = self.runner.schematic_netlist(
                schematic_path=schematic_path,
                analysis=normalized["analysis"],
                stop=normalized["stop"],
                step=normalized["step"],
                timeout_seconds=normalized["timeout_seconds"],
            )

        simulation_json = self.runner.simulate(
            schematic_path=schematic_path,
            analysis=normalized["analysis"],
            stop=normalized["stop"],
            step=normalized["step"],
            timeout_seconds=normalized["timeout_seconds"],
        )
        selected_waveforms = _collect_waveforms(simulation_json, normalized["signals"])
        processed_waveforms = []
        for waveform in selected_waveforms:
            cropped_x, cropped_y = _crop_xy(waveform["x"], waveform["y"], normalized["range"])
            decimated_x, decimated_y = _decimate_xy(cropped_x, cropped_y, normalized["max_points"])
            processed_waveforms.append({"name": waveform["name"], "x": decimated_x, "y": decimated_y})

        stats = [_waveform_stats(waveform["name"], waveform["y"]) for waveform in processed_waveforms] if normalized["include_stats"] else []
        measures = [_compute_measure(expr, processed_waveforms) for expr in normalized["measures"]]

        completed_at = time.time()
        return {
            "job_id": normalized["job_id"],
            "ok": True,
            "created_at": _utc_now_iso(),
            "duration_seconds": round(completed_at - started_at, 6),
            "source": {
                "schematic_path": os.path.abspath(schematic_path),
                "analysis": normalized["analysis"],
                "stop": normalized["stop"],
                "step": normalized["step"],
                "range": normalized["range"],
                "signals": normalized["signals"],
                "measures": normalized["measures"],
                "max_points": normalized["max_points"],
                "base_signal": normalized["base_signal"],
                "compat": normalized["compat"],
            },
            "labels": normalized["labels"],
            "tags": normalized["tags"],
            "metadata": normalized["metadata"],
            "artifacts": {
                "netlist": netlist_json,
                "simulation": simulation_json,
                "waveforms": processed_waveforms if normalized["include_raw"] else [],
                "stats": stats,
                "measures": measures,
            },
        }

    def run_batch(
        self,
        jobs: List[Dict[str, Any]],
        concurrency: int = 4,
        output_path: Optional[str] = None,
        fail_fast: bool = False,
        inline_results: bool = False,
    ) -> Dict[str, Any]:
        started_at = time.time()
        results: List[Dict[str, Any]] = []
        completed = 0
        failures = 0
        write_lock = threading.Lock()

        if output_path:
            output_file = Path(output_path)
            output_file.parent.mkdir(parents=True, exist_ok=True)
            output_file.write_text("", encoding="utf-8")

        def execute(job: Dict[str, Any]) -> Dict[str, Any]:
            try:
                record = self.run_job(job)
            except Exception as exc:
                record = {
                    "job_id": str(job.get("job_id") or uuid.uuid4()),
                    "ok": False,
                    "created_at": _utc_now_iso(),
                    "duration_seconds": 0.0,
                    "source": {
                        "schematic_path": os.path.abspath(str(job.get("schematic_path", ""))) if job.get("schematic_path") else "",
                        "analysis": str(job.get("analysis") or "tran").lower(),
                    },
                    "labels": dict(job.get("labels") or {}),
                    "tags": dict(job.get("tags") or {}),
                    "metadata": dict(job.get("metadata") or {}),
                    "error": str(exc),
                    "artifacts": {
                        "netlist": None,
                        "simulation": None,
                    },
                }
            if output_path:
                with write_lock:
                    with open(output_path, "a", encoding="utf-8") as handle:
                        handle.write(json.dumps(record, ensure_ascii=True) + "\n")
            return record

        with ThreadPoolExecutor(max_workers=max(1, concurrency)) as executor:
            future_map = {executor.submit(execute, job): job for job in jobs}
            for future in as_completed(future_map):
                record = future.result()
                completed += 1
                if not record.get("ok", False):
                    failures += 1
                    if fail_fast:
                        for pending in future_map:
                            pending.cancel()
                        break
                if inline_results:
                    results.append(record)

        finished_at = time.time()
        response = {
            "ok": failures == 0,
            "created_at": _utc_now_iso(),
            "job_count": len(jobs),
            "completed_count": completed,
            "failure_count": failures,
            "duration_seconds": round(finished_at - started_at, 6),
            "output_path": os.path.abspath(output_path) if output_path else None,
        }
        if inline_results:
            response["results"] = results
        return response


class MlDatasetApiHandler(BaseHTTPRequestHandler):
    service: SimulationDatasetService = SimulationDatasetService()
    server_version = "VioSpiceMlApi/0.1"

    def log_message(self, format: str, *args: Any) -> None:
        print(f"[{self.address_string()}] {format % args}")

    def _send_json(self, status: int, payload: Dict[str, Any]) -> None:
        body = json.dumps(payload, ensure_ascii=True).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS")
        self.end_headers()
        self.wfile.write(body)

    def _read_json_body(self) -> Dict[str, Any]:
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length) if length > 0 else b"{}"
        try:
            parsed = json.loads(raw.decode("utf-8"))
        except json.JSONDecodeError as exc:
            raise ValueError(f"Invalid JSON: {exc}") from exc
        if not isinstance(parsed, dict):
            raise ValueError("Request body must be a JSON object")
        return parsed

    def do_OPTIONS(self) -> None:
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS")
        self.end_headers()

    def do_GET(self) -> None:
        path = urlparse(self.path).path
        if path == "/api/ml/health":
            self._send_json(
                200,
                {
                    "ok": True,
                    "service": "viospice-ml-dataset-api",
                    "created_at": _utc_now_iso(),
                    "cli_path": self.service.runner.cli_path,
                },
            )
            return
        self._send_json(404, {"ok": False, "error": "Not found"})

    def do_POST(self) -> None:
        path = urlparse(self.path).path
        try:
            payload = self._read_json_body()
            if path == "/api/ml/simulate":
                sample = self.service.run_job(payload)
                self._send_json(200, {"ok": sample.get("ok", False), "sample": sample})
                return
            if path == "/api/ml/batch":
                jobs = payload.get("jobs")
                if not isinstance(jobs, list) or not jobs:
                    raise ValueError("jobs must be a non-empty array")
                result = self.service.run_batch(
                    jobs=jobs,
                    concurrency=int(payload.get("concurrency", 4)),
                    output_path=payload.get("output_path"),
                    fail_fast=bool(payload.get("fail_fast", False)),
                    inline_results=bool(payload.get("inline_results", False)),
                )
                self._send_json(200, result)
                return
            self._send_json(404, {"ok": False, "error": "Not found"})
        except Exception as exc:
            self._send_json(400, {"ok": False, "error": str(exc)})


def run_server(host: str, port: int, cli_path: Optional[str] = None) -> None:
    MlDatasetApiHandler.service = SimulationDatasetService(VioCmdRunner(cli_path))
    server = ThreadingHTTPServer((host, port), MlDatasetApiHandler)
    print(f"VioSpice ML Dataset API listening on http://{host}:{port}")
    print("Endpoints:")
    print("  GET  /api/ml/health")
    print("  POST /api/ml/simulate")
    print("  POST /api/ml/batch")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down ML Dataset API...")
    finally:
        server.server_close()


def main() -> None:
    parser = argparse.ArgumentParser(description="VioSpice ML dataset API")
    parser.add_argument("--host", default="0.0.0.0", help="Host to bind")
    parser.add_argument("--port", type=int, default=8787, help="Port to bind")
    parser.add_argument("--cli-path", help="Explicit path to vio-cmd or flux-cmd")
    args = parser.parse_args()
    run_server(args.host, args.port, args.cli_path)


if __name__ == "__main__":
    main()
