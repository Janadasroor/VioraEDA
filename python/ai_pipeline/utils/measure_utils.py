"""Measure evaluation, constraint filtering, derived labels, and result filters."""

from __future__ import annotations

import math
import random
from typing import Any, Dict, List, Optional, Tuple

from ai_pipeline.utils.spice_format import parse_spice_number


# ---------------------------------------------------------------------------
# Measure evaluation
# ---------------------------------------------------------------------------

def compute_measure(expression: str, waveforms: List[Dict[str, Any]]) -> Dict[str, Any]:
    """Evaluate a measure expression like 'avg V(out)' or 'max V(node) at 1m'."""
    from ai_pipeline.utils.waveform_utils import nearest_value_at

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
        at_target = parse_spice_number(at_text.strip())

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
        return {"expr": expression, "value": nearest_value_at(x_values, y_values, at_target)}
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


# ---------------------------------------------------------------------------
# Constraint evaluation
# ---------------------------------------------------------------------------

def coerce_constraint_operand(operand: Any, sweep_values: Dict[str, Any]) -> Any:
    if isinstance(operand, dict):
        if "param" in operand:
            param_name = str(operand["param"])
            if param_name not in sweep_values:
                raise ValueError(f"constraint references unknown parameter '{param_name}'")
            return sweep_values[param_name]
        if "value" in operand:
            return operand["value"]
        raise ValueError("constraint operand object must contain 'param' or 'value'")
    return operand


def to_comparable_value(value: Any) -> Any:
    if isinstance(value, (int, float)):
        return value
    if isinstance(value, str):
        parsed = parse_spice_number(value)
        if parsed is not None:
            return parsed
        return value
    return value


def evaluate_constraint_rule(rule: Dict[str, Any], sweep_values: Dict[str, Any]) -> bool:
    op = str(rule.get("op") or "").strip().lower()
    left = to_comparable_value(coerce_constraint_operand(rule.get("left"), sweep_values))
    right = to_comparable_value(coerce_constraint_operand(rule.get("right"), sweep_values))
    if op in ("==", "eq"):
        return left == right
    if op in ("!=", "ne"):
        return left != right
    if op in (">", "gt"):
        return left > right
    if op in ("<", "lt"):
        return left < right
    if op in (">=", "ge"):
        return left >= right
    if op in ("<=", "le"):
        return left <= right
    if op == "in":
        return left in right
    if op == "not_in":
        return left not in right
    raise ValueError(f"Unsupported constraint operator: {op}")


def normalize_constraint_rule(rule: Dict[str, Any]) -> Dict[str, Any]:
    if not isinstance(rule, dict):
        raise ValueError("each constraint must be an object")
    if "left" in rule and "right" in rule and "op" in rule:
        return dict(rule)
    if "param" in rule and "op" in rule and "value" in rule:
        return {"left": {"param": rule["param"]}, "op": rule["op"], "right": {"value": rule["value"]}}
    if "param" in rule and "op" in rule and "other_param" in rule:
        return {"left": {"param": rule["param"]}, "op": rule["op"], "right": {"param": rule["other_param"]}}
    raise ValueError("constraint must define either left/op/right or param/op/value|other_param")


def filter_combos_by_constraints(
    normalized_params: List[Dict[str, Any]],
    combos: List[tuple],
    constraints: Optional[List[Dict[str, Any]]],
) -> Tuple[List[tuple], Dict[str, Any]]:
    rules = [normalize_constraint_rule(rule) for rule in list(constraints or [])]
    if not rules:
        return list(combos), {"rule_count": 0, "kept_count": len(combos), "filtered_count": 0}

    kept = []
    filtered = 0
    param_names = [param["name"] for param in normalized_params]
    for combo in combos:
        sweep_values = {name: value for name, value in zip(param_names, combo)}
        if all(evaluate_constraint_rule(rule, sweep_values) for rule in rules):
            kept.append(combo)
        else:
            filtered += 1
    return kept, {
        "rule_count": len(rules),
        "kept_count": len(kept),
        "filtered_count": filtered,
    }


# ---------------------------------------------------------------------------
# Derived label evaluation
# ---------------------------------------------------------------------------

def lookup_measure_value(measures: List[Dict[str, Any]], expr: str) -> Any:
    for measure in measures:
        if str(measure.get("expr", "")).strip() == str(expr).strip():
            if "value" in measure:
                return measure["value"]
            raise ValueError(f"measure '{expr}' did not produce a value")
    raise ValueError(f"measure '{expr}' not found")


def lookup_stat_value(stats: List[Dict[str, Any]], signal: str, field: str) -> Any:
    for stat in stats:
        if str(stat.get("name", "")).strip() == str(signal).strip():
            if field in stat:
                return stat[field]
            raise ValueError(f"stat field '{field}' not found for signal '{signal}'")
    raise ValueError(f"stats for signal '{signal}' not found")


def resolve_derived_operand(
    operand: Any,
    *,
    labels: Dict[str, Any],
    metadata: Dict[str, Any],
    measures: List[Dict[str, Any]],
    stats: List[Dict[str, Any]],
) -> Any:
    if isinstance(operand, dict):
        if "value" in operand:
            return operand["value"]
        if "param" in operand:
            params = metadata.get("sweep_values") or {}
            key = str(operand["param"])
            if key not in params:
                raise ValueError(f"derived label references unknown param '{key}'")
            return to_comparable_value(params[key])
        if "label" in operand:
            key = str(operand["label"])
            if key not in labels:
                raise ValueError(f"derived label references unknown label '{key}'")
            return to_comparable_value(labels[key])
        if "measure" in operand:
            return to_comparable_value(lookup_measure_value(measures, str(operand["measure"])))
        if "stat" in operand:
            stat_cfg = dict(operand["stat"] or {})
            signal = str(stat_cfg.get("signal") or "")
            field = str(stat_cfg.get("field") or "")
            if not signal or not field:
                raise ValueError("derived stat operand requires signal and field")
            return to_comparable_value(lookup_stat_value(stats, signal, field))
    return to_comparable_value(operand)


def evaluate_derived_expression(
    expression: Any,
    *,
    labels: Dict[str, Any],
    metadata: Dict[str, Any],
    measures: List[Dict[str, Any]],
    stats: List[Dict[str, Any]],
) -> Any:
    if not isinstance(expression, dict):
        return to_comparable_value(expression)

    if any(key in expression for key in ("value", "param", "label", "measure", "stat")):
        return resolve_derived_operand(
            expression,
            labels=labels,
            metadata=metadata,
            measures=measures,
            stats=stats,
        )

    op = str(expression.get("op") or "").strip().lower()
    if not op:
        raise ValueError("derived expression requires op")

    if op == "abs":
        value = evaluate_derived_expression(
            expression.get("value"),
            labels=labels,
            metadata=metadata,
            measures=measures,
            stats=stats,
        )
        return abs(value)

    if op in {"add", "sub", "mul", "div", "pow", "min", "max"}:
        left = evaluate_derived_expression(
            expression.get("left"),
            labels=labels,
            metadata=metadata,
            measures=measures,
            stats=stats,
        )
        right = evaluate_derived_expression(
            expression.get("right"),
            labels=labels,
            metadata=metadata,
            measures=measures,
            stats=stats,
        )
        if op == "add":
            return left + right
        if op == "sub":
            return left - right
        if op == "mul":
            return left * right
        if op == "div":
            return left / right
        if op == "pow":
            return left ** right
        if op == "min":
            return min(left, right)
        if op == "max":
            return max(left, right)

    raise ValueError(f"Unsupported derived expression op: {op}")


def apply_derived_labels(
    derived_labels: List[Dict[str, Any]],
    *,
    labels: Dict[str, Any],
    metadata: Dict[str, Any],
    measures: List[Dict[str, Any]],
    stats: List[Dict[str, Any]],
) -> Dict[str, Any]:
    computed = dict(labels)
    for rule in derived_labels:
        if not isinstance(rule, dict):
            raise ValueError("each derived label must be an object")
        name = str(rule.get("name") or "").strip()
        if not name:
            raise ValueError("derived label requires a name")
        value = evaluate_derived_expression(
            rule.get("expression"),
            labels=computed,
            metadata=metadata,
            measures=measures,
            stats=stats,
        )
        computed[name] = value
    return computed


# ---------------------------------------------------------------------------
# Result filter evaluation
# ---------------------------------------------------------------------------

def normalize_result_filter_rule(rule: Dict[str, Any]) -> Dict[str, Any]:
    if not isinstance(rule, dict):
        raise ValueError("each result filter must be an object")
    if "left" in rule and "right" in rule and "op" in rule:
        return dict(rule)
    supported = ("param", "label", "measure", "stat", "value")
    for key in supported:
        if key in rule and "op" in rule and "target" in rule:
            return {"left": {key: rule[key]}, "op": rule["op"], "right": rule["target"]}
    raise ValueError("result filter must define either left/op/right or <source>/op/target")


def evaluate_result_filters(
    result_filters: List[Dict[str, Any]],
    *,
    labels: Dict[str, Any],
    metadata: Dict[str, Any],
    measures: List[Dict[str, Any]],
    stats: List[Dict[str, Any]],
) -> Dict[str, Any]:
    normalized_rules = [normalize_result_filter_rule(rule) for rule in list(result_filters or [])]
    evaluations = []
    all_passed = True
    for rule in normalized_rules:
        left = resolve_derived_operand(
            rule.get("left"),
            labels=labels,
            metadata=metadata,
            measures=measures,
            stats=stats,
        )
        right = resolve_derived_operand(
            rule.get("right"),
            labels=labels,
            metadata=metadata,
            measures=measures,
            stats=stats,
        )
        passed = evaluate_constraint_rule(
            {"left": {"value": left}, "op": rule["op"], "right": {"value": right}},
            {},
        )
        evaluations.append({"rule": rule, "passed": passed, "left": left, "right": right})
        if not passed:
            all_passed = False
    return {"passed": all_passed, "rule_count": len(normalized_rules), "evaluations": evaluations}


# ---------------------------------------------------------------------------
# Sampling and split assignment
# ---------------------------------------------------------------------------

def select_combos(
    combos: List[tuple],
    sampling: Optional[Dict[str, Any]],
) -> Tuple[List[tuple], Dict[str, Any]]:
    if not combos:
        return [], {"mode": "empty", "selected_count": 0, "total_combinations": 0}

    sampling = dict(sampling or {})
    mode = str(sampling.get("mode") or "exhaustive").strip().lower()
    seed = sampling.get("seed")
    rng = random.Random(seed)
    total = len(combos)

    if mode == "exhaustive":
        return list(combos), {
            "mode": "exhaustive",
            "seed": seed,
            "selected_count": total,
            "total_combinations": total,
        }

    if mode == "random":
        sample_count = int(sampling.get("sample_count", total))
        if sample_count <= 0:
            raise ValueError("sampling.sample_count must be positive")
        replace = bool(sampling.get("replace", False))
        if not replace and sample_count > total:
            raise ValueError(
                "sampling.sample_count exceeds total combinations; use replace=true or reduce sample_count"
            )
        selected = [rng.choice(combos) for _ in range(sample_count)] if replace else rng.sample(combos, sample_count)
        return selected, {
            "mode": "random",
            "seed": seed,
            "replace": replace,
            "selected_count": len(selected),
            "sample_count": sample_count,
            "total_combinations": total,
        }

    raise ValueError("sampling.mode must be 'exhaustive' or 'random'")


def assign_split_names(
    job_count: int, split_ratios: Optional[Dict[str, Any]], seed: Any = None
) -> List[str]:
    if job_count <= 0:
        return []
    ratios = dict(split_ratios or {"train": 0.8, "val": 0.1, "test": 0.1})
    allowed = {"train", "val", "test"}
    filtered = {key: float(value) for key, value in ratios.items() if key in allowed and float(value) > 0}
    if not filtered:
        return ["train"] * job_count
    total = sum(filtered.values())
    if total <= 0:
        return ["train"] * job_count

    ordered = [(key, value / total) for key, value in filtered.items()]
    counts: Dict[str, int] = {}
    fractional = []
    allocated = 0
    for key, ratio in ordered:
        exact = ratio * job_count
        count = int(math.floor(exact))
        counts[key] = count
        allocated += count
        fractional.append((exact - count, key))

    remaining = job_count - allocated
    for _, key in sorted(fractional, reverse=True):
        if remaining <= 0:
            break
        counts[key] += 1
        remaining -= 1

    assignments: List[str] = []
    for key, _ in ordered:
        assignments.extend([key] * counts.get(key, 0))
    while len(assignments) < job_count:
        assignments.append(ordered[0][0])

    rng = random.Random(seed)
    rng.shuffle(assignments)
    return assignments[:job_count]
