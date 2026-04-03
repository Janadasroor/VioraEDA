import threading
import traceback
import uuid
from typing import Any, Dict, List, Optional

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

from .ml_dataset_api import SimulationDatasetService, VioCmdRunner, _utc_now_iso


class _BasePayloadModel(BaseModel):
    class Config:
        extra = "allow"


class SimulateRequest(_BasePayloadModel):
    schematic_path: str
    analysis: Optional[str] = "tran"
    stop: Optional[str] = None
    step: Optional[str] = None
    signals: List[str] = Field(default_factory=list)
    measures: List[str] = Field(default_factory=list)
    labels: Dict[str, Any] = Field(default_factory=dict)
    tags: Dict[str, Any] = Field(default_factory=dict)
    metadata: Dict[str, Any] = Field(default_factory=dict)
    derived_labels: List[Dict[str, Any]] = Field(default_factory=list)
    result_filters: List[Dict[str, Any]] = Field(default_factory=list)
    discard_filtered: bool = False


class BatchRequest(_BasePayloadModel):
    jobs: List[Dict[str, Any]]
    concurrency: int = 4
    output_path: Optional[str] = None
    fail_fast: bool = False
    inline_results: bool = False


class SweepRequest(_BasePayloadModel):
    template_schematic_path: str
    parameters: List[Dict[str, Any]]
    job_template: Dict[str, Any] = Field(default_factory=dict)
    variant_dir: Optional[str] = None
    sampling: Dict[str, Any] = Field(default_factory=dict)
    split_ratios: Dict[str, Any] = Field(default_factory=dict)
    constraints: List[Dict[str, Any]] = Field(default_factory=list)
    concurrency: int = 4
    output_path: Optional[str] = None
    fail_fast: bool = False
    inline_results: bool = False


class AsyncJobAccepted(BaseModel):
    ok: bool = True
    job_id: str
    status: str
    created_at: str
    kind: str


class InMemoryJobStore:
    def __init__(self) -> None:
        self._jobs: Dict[str, Dict[str, Any]] = {}
        self._lock = threading.Lock()

    def create_job(self, kind: str, payload: Dict[str, Any]) -> Dict[str, Any]:
        job_id = str(uuid.uuid4())
        record = {
            "job_id": job_id,
            "kind": kind,
            "status": "queued",
            "created_at": _utc_now_iso(),
            "started_at": None,
            "finished_at": None,
            "payload": payload,
            "result": None,
            "error": None,
        }
        with self._lock:
            self._jobs[job_id] = record
        return dict(record)

    def get_job(self, job_id: str) -> Optional[Dict[str, Any]]:
        with self._lock:
            record = self._jobs.get(job_id)
            return dict(record) if record else None

    def update_job(self, job_id: str, **fields: Any) -> Dict[str, Any]:
        with self._lock:
            if job_id not in self._jobs:
                raise KeyError(job_id)
            self._jobs[job_id].update(fields)
            return dict(self._jobs[job_id])


def _start_background_job(
    store: InMemoryJobStore,
    kind: str,
    payload: Dict[str, Any],
    runner,
) -> Dict[str, Any]:
    record = store.create_job(kind=kind, payload=payload)
    job_id = record["job_id"]

    def target() -> None:
        store.update_job(job_id, status="running", started_at=_utc_now_iso())
        try:
            result = runner(payload)
            store.update_job(job_id, status="completed", finished_at=_utc_now_iso(), result=result)
        except Exception as exc:
            store.update_job(
                job_id,
                status="failed",
                finished_at=_utc_now_iso(),
                error={"message": str(exc), "traceback": traceback.format_exc()},
            )

    thread = threading.Thread(target=target, daemon=True, name=f"viospice-ml-job-{job_id[:8]}")
    thread.start()
    return record


def create_app(cli_path: Optional[str] = None) -> FastAPI:
    service = SimulationDatasetService(VioCmdRunner(cli_path))
    job_store = InMemoryJobStore()
    app = FastAPI(
        title="VioSpice ML Dataset API",
        version="0.3.0",
        description="ASGI/OpenAPI wrapper for VioSpice ML-oriented simulation, batch generation, and sweep dataset workflows.",
    )

    @app.get("/api/ml/health")
    def health() -> Dict[str, Any]:
        return {
            "ok": True,
            "service": "viospice-ml-dataset-api-fastapi",
            "created_at": _utc_now_iso(),
            "cli_path": service.runner.cli_path,
            "async_jobs": True,
        }

    @app.post("/api/ml/simulate")
    def simulate(request: SimulateRequest) -> Dict[str, Any]:
        try:
            sample = service.run_job(request.dict())
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"ok": sample.get("ok", False), "sample": sample}

    @app.post("/api/ml/batch")
    def batch(request: BatchRequest) -> Dict[str, Any]:
        try:
            return service.run_batch(
                jobs=request.jobs,
                concurrency=request.concurrency,
                output_path=request.output_path,
                fail_fast=request.fail_fast,
                inline_results=request.inline_results,
            )
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc

    @app.post("/api/ml/sweep")
    def sweep(request: SweepRequest) -> Dict[str, Any]:
        try:
            return service.run_sweep(request.dict())
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc

    @app.post("/api/ml/jobs/simulate", response_model=AsyncJobAccepted)
    def simulate_async(request: SimulateRequest) -> Dict[str, Any]:
        record = _start_background_job(job_store, "simulate", request.dict(), service.run_job)
        return {
            "ok": True,
            "job_id": record["job_id"],
            "status": record["status"],
            "created_at": record["created_at"],
            "kind": record["kind"],
        }

    @app.post("/api/ml/jobs/batch", response_model=AsyncJobAccepted)
    def batch_async(request: BatchRequest) -> Dict[str, Any]:
        payload = request.dict()
        record = _start_background_job(
            job_store,
            "batch",
            payload,
            lambda data: service.run_batch(
                jobs=data["jobs"],
                concurrency=data.get("concurrency", 4),
                output_path=data.get("output_path"),
                fail_fast=bool(data.get("fail_fast", False)),
                inline_results=bool(data.get("inline_results", False)),
            ),
        )
        return {
            "ok": True,
            "job_id": record["job_id"],
            "status": record["status"],
            "created_at": record["created_at"],
            "kind": record["kind"],
        }

    @app.post("/api/ml/jobs/sweep", response_model=AsyncJobAccepted)
    def sweep_async(request: SweepRequest) -> Dict[str, Any]:
        record = _start_background_job(job_store, "sweep", request.dict(), service.run_sweep)
        return {
            "ok": True,
            "job_id": record["job_id"],
            "status": record["status"],
            "created_at": record["created_at"],
            "kind": record["kind"],
        }

    @app.get("/api/ml/jobs/{job_id}")
    def get_job(job_id: str) -> Dict[str, Any]:
        record = job_store.get_job(job_id)
        if not record:
            raise HTTPException(status_code=404, detail=f"job '{job_id}' not found")
        return record

    return app


app = create_app()
