"""
VioSpice Python integration — shared configuration and utilities.

All Python modules should import from here instead of duplicating
path resolution, default ports, API-key env-vars, or sys.path hacks.
"""

from __future__ import annotations

import os
import shutil
from pathlib import Path
from typing import List, Optional

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

def repo_root() -> Path:
    """Return the absolute path of the VioSpice repository root."""
    return Path(__file__).resolve().parents[2]


def python_root() -> Path:
    """Return the absolute path of the python/ directory."""
    return repo_root() / "python"


def _ensure_python_root_in_syspath() -> None:
    """Call once at import-time from entry-point scripts so sibling modules
    can be imported without manual sys.path manipulation."""
    root = str(python_root())
    if root not in __import__("sys").path:
        __import__("sys").path.insert(0, root)


# ---------------------------------------------------------------------------
# Vio-Cmd / Flux-Cmd CLI resolution  (single source of truth)
# ---------------------------------------------------------------------------

_VIO_CMD_CANDIDATES_REL: List[str] = [
    "build/vio-cmd",
    "build-debug/vio-cmd",
    "build-asan/vio-cmd",
    "build/dev-debug/vio-cmd",
    "build/flux-cmd",
    "build-debug/flux-cmd",
    "build-asan/flux-cmd",
    "build/dev-debug/flux-cmd",
]

_VIO_CMD_NAMES: List[str] = ["vio-cmd", "flux-cmd"]


def resolve_vio_cmd(explicit_path: Optional[str] = None) -> str:
    """Find the VioSpice CLI binary.

    Checks *explicit_path*, then relative build-tree candidates, then PATH.
    Returns the first viable candidate (even if not executable) so that the
    caller can produce a useful error message.
    """
    candidates: List[str] = []
    if explicit_path:
        candidates.append(os.path.abspath(explicit_path))

    root = repo_root()
    for rel in _VIO_CMD_CANDIDATES_REL:
        candidates.append(str(root / rel))

    candidates.extend(_VIO_CMD_NAMES)

    for candidate in candidates:
        if os.path.isabs(candidate) and os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate
        resolved = shutil.which(candidate)
        if resolved:
            return resolved

    return candidates[0]


# ---------------------------------------------------------------------------
# Default configuration constants
# ---------------------------------------------------------------------------

DEFAULT_ML_API_PORT: int = 8790
DEFAULT_ML_API_HOST: str = "0.0.0.0"
DEFAULT_SHARE_SERVER_PORT: int = 8765
DEFAULT_RATE_WINDOW_SECONDS: int = 60

ENV_ML_API_KEY: str = "VIOSPICE_ML_API_KEY"
ENV_ML_RATE_LIMIT: str = "VIOSPICE_ML_RATE_LIMIT"
ENV_ML_RATE_WINDOW: str = "VIOSPICE_ML_RATE_WINDOW_SECONDS"
ENV_GEMINI_API_KEY: str = "GEMINI_API_KEY"
ENV_OCTOPART_API_KEY: str = "OCTOPART_API_KEY"

LOG_FILE_PATH: str = "/tmp/viospice_ai.log"
