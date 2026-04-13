"""SPICE number formatting and parsing utilities."""

from __future__ import annotations

from typing import Any, Optional


def parse_spice_number(value: Optional[str]) -> Optional[float]:
    """Parse a SPICE-style number string (with engineering suffixes) into a float."""
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


def format_spice_number(number: float, precision: int = 6) -> str:
    """Format a float with SPICE-style engineering suffixes."""
    if number == 0:
        return "0"
    sign = "-" if number < 0 else ""
    abs_value = abs(number)
    suffixes = [
        (1e12, "t"),
        (1e9, "g"),
        (1e6, "meg"),
        (1e3, "k"),
        (1.0, ""),
        (1e-3, "m"),
        (1e-6, "u"),
        (1e-9, "n"),
        (1e-12, "p"),
        (1e-15, "f"),
    ]
    chosen_scale = None
    chosen_suffix = ""
    for scale, suffix in suffixes:
        scaled = abs_value / scale
        if 1 <= scaled < 1000:
            chosen_scale = scale
            chosen_suffix = suffix
            break
    if chosen_scale is None:
        chosen_scale = 1.0
        chosen_suffix = ""
    scaled = abs_value / chosen_scale
    text = f"{scaled:.{precision}g}"
    if "e" in text.lower():
        text = f"{scaled:.{precision}f}".rstrip("0").rstrip(".")
    return f"{sign}{text}{chosen_suffix}"


def format_generated_value(
    number: float,
    fmt: Optional[str],
    integer: bool = False,
    engineering_format: Optional[str] = None,
    engineering_precision: int = 6,
) -> Any:
    """Format a generated parameter value according to the parameter spec."""
    if integer:
        rounded = int(round(number))
        if fmt:
            return fmt.format(value=rounded)
        return rounded
    if engineering_format:
        mode = str(engineering_format).strip().lower()
        if mode == "spice":
            return format_spice_number(number, precision=engineering_precision)
        raise ValueError(f"Unsupported engineering_format: {engineering_format}")
    if fmt:
        return fmt.format(value=number)
    return number
