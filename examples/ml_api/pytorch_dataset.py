#!/usr/bin/env python3
import json
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple


try:
    import torch
    from torch.utils.data import Dataset
except ImportError:  # pragma: no cover
    torch = None
    Dataset = object


def load_jsonl(path: str, accepted_only: bool = True) -> List[Dict[str, Any]]:
    records: List[Dict[str, Any]] = []
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            if not line.strip():
                continue
            record = json.loads(line)
            if accepted_only and not record.get("accepted", True):
                continue
            records.append(record)
    return records


def _find_stat(records_stats: List[Dict[str, Any]], signal: str, field: str, default: float = 0.0) -> float:
    for stat in records_stats:
        if str(stat.get("name", "")).strip() == signal and field in stat:
            return float(stat[field])
    return default


def extract_feature_vector(
    record: Dict[str, Any],
    stat_features: Optional[Iterable[Tuple[str, str]]] = None,
    param_features: Optional[Iterable[str]] = None,
) -> List[float]:
    stat_features = list(stat_features or [])
    param_features = list(param_features or [])
    features: List[float] = []

    stats = record.get("artifacts", {}).get("stats", [])
    for signal, field in stat_features:
        features.append(_find_stat(stats, signal, field))

    params = record.get("metadata", {}).get("sweep_values", {})
    for key in param_features:
        value = params.get(key, 0.0)
        try:
            features.append(float(value))
        except Exception:
            features.append(0.0)

    return features


def extract_label_vector(record: Dict[str, Any], label_names: Iterable[str]) -> List[float]:
    labels = record.get("labels", {})
    values: List[float] = []
    for name in label_names:
        value = labels.get(name, 0.0)
        try:
            values.append(float(value))
        except Exception:
            values.append(0.0)
    return values


class VioSpiceJsonlDataset(Dataset):
    def __init__(
        self,
        jsonl_path: str,
        *,
        label_names: Iterable[str],
        stat_features: Optional[Iterable[Tuple[str, str]]] = None,
        param_features: Optional[Iterable[str]] = None,
        accepted_only: bool = True,
    ) -> None:
        if torch is None:
            raise RuntimeError("torch is not installed")
        self.records = load_jsonl(jsonl_path, accepted_only=accepted_only)
        self.label_names = list(label_names)
        self.stat_features = list(stat_features or [])
        self.param_features = list(param_features or [])

    def __len__(self) -> int:
        return len(self.records)

    def __getitem__(self, index: int):
        record = self.records[index]
        x = extract_feature_vector(
            record,
            stat_features=self.stat_features,
            param_features=self.param_features,
        )
        y = extract_label_vector(record, self.label_names)
        return torch.tensor(x, dtype=torch.float32), torch.tensor(y, dtype=torch.float32)


def main() -> None:
    dataset_path = "/tmp/viospice-datasets/amp.jsonl"
    records = load_jsonl(dataset_path, accepted_only=True)
    print(f"loaded records: {len(records)}")
    if records:
        x = extract_feature_vector(
            records[0],
            stat_features=[("ALL", "avg"), ("ALL", "max"), ("ALL", "rms")],
            param_features=["vin_dc"],
        )
        y = extract_label_vector(records[0], ["gain_ratio"])
        print("sample features:", x)
        print("sample labels:", y)


if __name__ == "__main__":
    main()
