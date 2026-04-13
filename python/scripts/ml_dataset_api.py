#!/usr/bin/env python3
"""Entry-point shim for the stdlib ML Dataset API server."""

from ai_pipeline.config import _ensure_python_root_in_syspath
_ensure_python_root_in_syspath()

from ai_pipeline.api.ml_dataset_api import main


if __name__ == "__main__":
    main()
