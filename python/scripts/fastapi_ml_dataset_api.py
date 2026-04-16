#!/usr/bin/env python3
"""Entry-point shim for the FastAPI ML Dataset API server."""

import argparse

from ai_pipeline.config import (
    DEFAULT_ML_API_HOST,
    DEFAULT_ML_API_PORT,
    ENV_ML_API_KEY,
    ENV_ML_RATE_LIMIT,
    ENV_ML_RATE_WINDOW,
    _ensure_python_root_in_syspath,
)

_ensure_python_root_in_syspath()


def main() -> None:
    import os

    import uvicorn

    from ai_pipeline.api.fastapi_ml_dataset_api import create_app

    parser = argparse.ArgumentParser(description="VioSpice ML dataset FastAPI service")
    parser.add_argument("--host", default=DEFAULT_ML_API_HOST, help="Host to bind")
    parser.add_argument("--port", type=int, default=DEFAULT_ML_API_PORT, help="Port to bind")
    parser.add_argument("--cli-path", help="Explicit path to viora binary")
    parser.add_argument("--job-store", help="Path to persistent async job store JSON file")
    parser.add_argument("--api-key", help="Require this API key via X-API-Key header")
    parser.add_argument(
        "--rate-limit",
        type=int,
        default=int(os.environ.get(ENV_ML_RATE_LIMIT, "0")) or None,
        help="Max requests per window per client/API key",
    )
    parser.add_argument(
        "--rate-window-seconds",
        type=int,
        default=int(os.environ.get(ENV_ML_RATE_WINDOW, "60")),
        help="Rate-limit window size in seconds",
    )
    parser.add_argument("--reload", action="store_true", help="Enable uvicorn reload mode")
    args = parser.parse_args()

    api_key = args.api_key or os.environ.get(ENV_ML_API_KEY)

    uvicorn.run(
        create_app(
            cli_path=args.cli_path,
            job_store_path=args.job_store,
            api_key=api_key,
            rate_limit=args.rate_limit,
            rate_window_seconds=args.rate_window_seconds,
        ),
        host=args.host,
        port=args.port,
        reload=args.reload,
    )


if __name__ == "__main__":
    main()
