# ML Dataset API

This API exposes VioSpice simulation runs as training-ready dataset records so ML engineers can run thousands of circuit jobs and collect rich artifacts per sample.

## What each sample contains

- source schematic path and simulation configuration
- generated schematic netlist in JSON form
- simulation result JSON from `vio-cmd netlist-run`
- simulator-native result JSON from `vio-cmd simulate --json`
- optional decimated raw waveforms extracted from the returned signals
- optional signal statistics computed by the API
- optional measurement expressions computed by the API
- user-provided labels, tags, and metadata

## Start the server

```bash
python3 python/scripts/ml_dataset_api.py --port 8787
```

Optional:

```bash
python3 python/scripts/ml_dataset_api.py --port 8787 --cli-path ./build/vio-cmd
```

## Endpoints

### `GET /api/ml/health`

Returns service status and the resolved `vio-cmd` path.

### `POST /api/ml/simulate`

Runs one job and returns one rich sample.

Example request:

```json
{
  "schematic_path": "/home/jnd/qt_projects/viospice/templates/circuits/basics/voltage_divider.sch",
  "analysis": "tran",
  "stop": "10m",
  "step": "10u",
  "signals": ["V(out)", "V(in)"],
  "measures": ["avg V(out)", "max V(out)"],
  "max_points": 2000,
  "labels": {
    "target_gain": 0.5
  },
  "tags": {
    "family": "divider",
    "split": "train"
  }
}
```

### `POST /api/ml/batch`

Runs many jobs concurrently. For large dataset generation, write the results directly to JSONL.

Example request:

```json
{
  "concurrency": 8,
  "output_path": "/tmp/viospice-datasets/dividers.jsonl",
  "jobs": [
    {
      "schematic_path": "/home/jnd/qt_projects/viospice/templates/circuits/basics/voltage_divider.sch",
      "analysis": "tran",
      "stop": "5m",
      "step": "5u",
      "signals": ["V(out)"],
      "measures": ["avg V(out)", "rms V(out)"],
      "labels": {
        "ratio": 0.5
      }
    },
    {
      "schematic_path": "/home/jnd/qt_projects/viospice/templates/circuits/filters/rc_low_pass.sch",
      "analysis": "ac",
      "signals": ["V(out)"],
      "labels": {
        "family": "low_pass"
      }
    }
  ]
}
```

Each line in the output JSONL file is a self-contained training record.

## Notes

- `analysis` supports `op`, `tran`, and `ac`
- `include_netlist`, `include_raw`, and `include_stats` default to `true`
- `measures` currently supports `avg`, `max`, `min`, `rms`, `pp`, and `value ... at <time>`
- `max_points` lets you decimate waveforms before exporting them into the dataset
- `range` lets you crop the exported waveform window
- `compat` enables the CLI compatibility layer for imported SPICE decks when needed
