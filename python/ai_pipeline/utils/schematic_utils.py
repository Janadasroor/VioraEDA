"""Schematic JSON manipulation: path traversal, item matching, value application."""

from __future__ import annotations

from typing import Any, Dict, List


def parse_path_token(token: str) -> List[Any]:
    parts: List[Any] = []
    buffer = []
    index_buffer = []
    in_index = False
    for ch in token:
        if ch == "[":
            if buffer:
                parts.append("".join(buffer))
                buffer = []
            in_index = True
            index_buffer = []
            continue
        if ch == "]":
            if in_index:
                parts.append(int("".join(index_buffer)))
                in_index = False
            continue
        if in_index:
            index_buffer.append(ch)
        else:
            buffer.append(ch)
    if buffer:
        parts.append("".join(buffer))
    return [part for part in parts if part != ""]


def parse_json_path(path_expr: str) -> List[Any]:
    parts: List[Any] = []
    for token in str(path_expr).split("."):
        parts.extend(parse_path_token(token))
    return parts


def get_container_value(container: Any, key: Any) -> Any:
    if isinstance(key, int):
        return container[key]
    return container[key]


def set_container_value(container: Any, key: Any, value: Any) -> None:
    if isinstance(key, int):
        container[key] = value
    else:
        container[key] = value


def set_by_path(document: Any, path_expr: str, value: Any) -> None:
    path = parse_json_path(path_expr)
    if not path:
        raise ValueError("json_path cannot be empty")
    current = document
    for key in path[:-1]:
        current = get_container_value(current, key)
    set_container_value(current, path[-1], value)


def find_matching_items(document: Dict[str, Any], selector: Dict[str, Any]) -> List[Dict[str, Any]]:
    items = document.get("items")
    if not isinstance(items, list):
        raise ValueError("schematic does not contain an items array")
    matches = []
    for item in items:
        if not isinstance(item, dict):
            continue
        if all(str(item.get(key, "")).strip() == str(expected).strip() for key, expected in selector.items()):
            matches.append(item)
    return matches


def apply_target_value(document: Dict[str, Any], target: Dict[str, Any], value: Any) -> Dict[str, Any]:
    value_format = str(target.get("format", "{value}"))
    rendered_value = value_format.format(value=value)
    if target.get("json_path"):
        set_by_path(document, str(target["json_path"]), rendered_value)
        return {"json_path": target["json_path"], "applied_value": rendered_value, "match_count": 1}

    field = target.get("field")
    if not field:
        raise ValueError("target requires either json_path or field")
    selector = {key: target[key] for key in ("reference", "type", "name") if key in target}
    if not selector:
        raise ValueError("target with field requires at least one selector: reference/type/name")
    matches = find_matching_items(document, selector)
    if not matches:
        raise ValueError(f"no schematic items matched selector {selector}")
    for item in matches:
        set_by_path(item, str(field), rendered_value)
    return {"selector": selector, "field": field, "applied_value": rendered_value, "match_count": len(matches)}
