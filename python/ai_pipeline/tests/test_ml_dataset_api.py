import json
import sys
import tempfile
import unittest
from pathlib import Path

PYTHON_ROOT = Path(__file__).resolve().parents[2]
if str(PYTHON_ROOT) not in sys.path:
    sys.path.insert(0, str(PYTHON_ROOT))

from ai_pipeline.api.ml_dataset_api import SimulationDatasetService


class FakeRunner:
    def schematic_netlist(self, schematic_path, analysis, stop=None, step=None, timeout_seconds=None):
        return {
            "kind": "netlist",
            "schematic_path": schematic_path,
            "analysis": analysis,
            "stop": stop,
            "step": step,
        }

    def simulate(self, schematic_path, analysis, stop=None, step=None, timeout_seconds=None):
        return {
            "analysis": analysis,
            "waveforms": [
                {
                    "name": "V(out)",
                    "x": [0.0, 1.0, 2.0, 3.0],
                    "y": [1.0, 2.0, 3.0, 4.0],
                }
            ],
            "nodeVoltages": {},
            "branchCurrents": {},
            "stop": stop,
            "step": step,
        }


class SimulationDatasetServiceTest(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.schematic_path = Path(self.temp_dir.name) / "sample.sch"
        self.schematic_path.write_text("{}", encoding="utf-8")
        self.service = SimulationDatasetService(runner=FakeRunner())

    def tearDown(self):
        self.temp_dir.cleanup()

    def test_run_job_returns_rich_sample(self):
        result = self.service.run_job(
            {
                "schematic_path": str(self.schematic_path),
                "analysis": "tran",
                "signals": ["V(out)"],
                "measures": ["avg V(out)"],
                "labels": {"target_gain": 12.0},
                "tags": {"family": "amplifier"},
                "metadata": {"split": "train"},
                "stop": "10m",
                "step": "10u",
                "max_points": 512,
                "base_signal": "V(out)",
                "range": "0:1m",
                "compat": True,
            }
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["labels"]["target_gain"], 12.0)
        self.assertEqual(result["tags"]["family"], "amplifier")
        self.assertEqual(result["metadata"]["split"], "train")
        self.assertEqual(result["artifacts"]["netlist"]["analysis"], "tran")
        self.assertEqual(len(result["artifacts"]["waveforms"][0]["x"]), 1)
        self.assertEqual(result["artifacts"]["stats"][0]["avg"], 1.0)
        self.assertEqual(result["artifacts"]["measures"][0]["expr"], "avg V(out)")

    def test_run_batch_writes_jsonl(self):
        output_path = Path(self.temp_dir.name) / "dataset.jsonl"
        result = self.service.run_batch(
            jobs=[
                {"schematic_path": str(self.schematic_path), "analysis": "op"},
                {"schematic_path": str(self.schematic_path), "analysis": "tran"},
            ],
            concurrency=2,
            output_path=str(output_path),
            inline_results=False,
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["completed_count"], 2)
        lines = output_path.read_text(encoding="utf-8").strip().splitlines()
        self.assertEqual(len(lines), 2)
        payload = json.loads(lines[0])
        self.assertIn("artifacts", payload)


if __name__ == "__main__":
    unittest.main()
