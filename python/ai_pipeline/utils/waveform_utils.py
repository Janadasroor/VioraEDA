"""Waveform collection, cropping, decimation, and statistics."""

from __future__ import annotations

import math
from typing import Any, Dict, List, Optional, Tuple


def parse_range(range_expr: Optional[str]) -> Optional[Tuple[float, float]]:
    """Parse a 't0:t1' range expression. Returns (start, end) or None."""
    if not range_expr:
        return None
    from ai_pipeline.utils.spice_format import parse_spice_number

    parts = str(range_expr).split(":")
    if len(parts) != 2:
        raise ValueError("range must use t0:t1 syntax")
    start = parse_spice_number(parts[0])
    end = parse_spice_number(parts[1])
    if start is None or end is None:
        raise ValueError("range must contain valid spice numbers")
    return (min(start, end), max(start, end))


def crop_xy(
    x_values: List[float], y_values: List[float], range_expr: Optional[str]
) -> Tuple[List[float], List[float]]:
    """Crop waveform data to an optional time range."""
    parsed = parse_range(range_expr)
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


def decimate_xy(
    x_values: List[float], y_values: List[float], max_points: Optional[int]
) -> Tuple[List[float], List[float]]:
    """Evenly-spaced decimation to at most *max_points* samples."""
    if not max_points or max_points <= 0 or len(x_values) <= max_points:
        return list(x_values), list(y_values)
    indices = []
    last_index = len(x_values) - 1
    for point_index in range(max_points):
        idx = round(point_index * last_index / max(1, max_points - 1))
        indices.append(idx)
    deduped = sorted(set(indices))
    return [x_values[i] for i in deduped], [y_values[i] for i in deduped]


def match_waveform(simulation_json: Dict[str, Any], signal_name: str) -> Optional[Dict[str, Any]]:
    """Find a waveform by name (case-insensitive) in simulation JSON."""
    target = signal_name.strip().lower()
    for wave in simulation_json.get("waveforms", []):
        if str(wave.get("name", "")).strip().lower() == target:
            return wave
    return None


def pseudo_waveform_from_scalar(name: str, value: float) -> Dict[str, Any]:
    """Wrap a scalar node voltage / branch current as a pseudo-waveform."""
    return {"name": name, "x": [0.0], "y": [value]}


def collect_waveforms(simulation_json: Dict[str, Any], requested_signals: List[str]) -> List[Dict[str, Any]]:
    """Extract waveforms from simulation output, falling back to scalars."""
    if requested_signals:
        collected: List[Dict[str, Any]] = []
        for signal in requested_signals:
            wave = match_waveform(simulation_json, signal)
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
                    collected.append(pseudo_waveform_from_scalar(f"V({key})", value))
                    break
            else:
                for key, value in branch_currents.items():
                    if str(key).strip().lower() == branch_key.lower():
                        collected.append(pseudo_waveform_from_scalar(f"I({key})", value))
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
        collected.append(pseudo_waveform_from_scalar(f"V({key})", value))
    for key, value in simulation_json.get("branchCurrents", {}).items():
        collected.append(pseudo_waveform_from_scalar(f"I({key})", value))
    return collected


def waveform_stats(name: str, y_values: List[float]) -> Dict[str, Any]:
    """Compute summary statistics for a waveform."""
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


def nearest_value_at(x_values: List[float], y_values: List[float], target: float) -> float:
    """Return the y-value at the x-point closest to *target*."""
    nearest_index = min(range(len(x_values)), key=lambda idx: abs(x_values[idx] - target))
    return y_values[nearest_index]
