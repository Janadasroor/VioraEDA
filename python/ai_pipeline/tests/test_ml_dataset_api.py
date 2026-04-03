import json
import sys
import tempfile
import unittest
from pathlib import Path

PYTHON_ROOT = Path(__file__).resolve().parents[2]
if str(PYTHON_ROOT) not in sys.path:
    sys.path.insert(0, str(PYTHON_ROOT))

from ai_pipeline.api.ml_dataset_api import SimulationDatasetService, _expand_parameter_values, _format_spice_number


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
        self.schematic_path.write_text(
            json.dumps(
                {
                    "items": [
                        {"type": "Resistor", "reference": "R1", "value": "1k"},
                        {"type": "Resistor", "reference": "R2", "value": "2k"},
                        {"type": "VoltageSource", "reference": "V1", "dcVoltage": "5", "sineFrequency": "1k"},
                    ]
                }
            ),
            encoding="utf-8",
        )
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

    def test_expand_sweep_jobs_materializes_variants(self):
        expanded = self.service.expand_sweep_jobs(
            template_schematic_path=str(self.schematic_path),
            job_template={
                "analysis": "tran",
                "signals": ["V(out)"],
            },
            parameters=[
                {
                    "name": "r1_value",
                    "target": {"reference": "R1", "field": "value"},
                    "values": ["1k", "10k"],
                },
                {
                    "name": "vin_dc",
                    "target": {"reference": "V1", "field": "dcVoltage"},
                    "values": ["3.3", "5.0"],
                },
            ],
        )
        self.assertEqual(expanded["job_count"], 4)
        first_variant_path = Path(expanded["jobs"][0]["schematic_path"])
        self.assertTrue(first_variant_path.exists())
        variant_doc = json.loads(first_variant_path.read_text(encoding="utf-8"))
        values = {item["reference"]: item for item in variant_doc["items"]}
        self.assertIn(values["R1"]["value"], {"1k", "10k"})
        self.assertIn(values["V1"]["dcVoltage"], {"3.3", "5.0"})
        self.assertIn("sweep_values", expanded["jobs"][0]["metadata"])

    def test_run_sweep_executes_generated_jobs(self):
        output_path = Path(self.temp_dir.name) / "sweep.jsonl"
        result = self.service.run_sweep(
            {
                "template_schematic_path": str(self.schematic_path),
                "job_template": {
                    "analysis": "tran",
                    "signals": ["V(out)"],
                    "measures": ["avg V(out)"],
                },
                "parameters": [
                    {
                        "name": "source_freq",
                        "target": {"reference": "V1", "field": "sineFrequency"},
                        "values": ["1k", "10k", "100k"],
                    }
                ],
                "output_path": str(output_path),
                "concurrency": 2,
            }
        )
        self.assertTrue(result["ok"])
        self.assertEqual(result["generated_job_count"], 3)
        lines = output_path.read_text(encoding="utf-8").strip().splitlines()
        self.assertEqual(len(lines), 3)
        record = json.loads(lines[0])
        self.assertIn("sweep_values", record["metadata"])
        self.assertEqual(record["tags"]["sweep"], "true")

    def test_expand_sweep_jobs_supports_seeded_random_sampling(self):
        expanded_a = self.service.expand_sweep_jobs(
            template_schematic_path=str(self.schematic_path),
            job_template={"analysis": "tran"},
            parameters=[
                {
                    "name": "r1_value",
                    "target": {"reference": "R1", "field": "value"},
                    "values": ["1k", "2k", "3k"],
                },
                {
                    "name": "vin_dc",
                    "target": {"reference": "V1", "field": "dcVoltage"},
                    "values": ["1.8", "3.3", "5.0"],
                },
            ],
            sampling={"mode": "random", "sample_count": 4, "seed": 42},
        )
        expanded_b = self.service.expand_sweep_jobs(
            template_schematic_path=str(self.schematic_path),
            job_template={"analysis": "tran"},
            parameters=[
                {
                    "name": "r1_value",
                    "target": {"reference": "R1", "field": "value"},
                    "values": ["1k", "2k", "3k"],
                },
                {
                    "name": "vin_dc",
                    "target": {"reference": "V1", "field": "dcVoltage"},
                    "values": ["1.8", "3.3", "5.0"],
                },
            ],
            sampling={"mode": "random", "sample_count": 4, "seed": 42},
        )
        values_a = [item["sweep_values"] for item in expanded_a["manifests"]]
        values_b = [item["sweep_values"] for item in expanded_b["manifests"]]
        self.assertEqual(expanded_a["sampling"]["selected_count"], 4)
        self.assertEqual(values_a, values_b)

    def test_run_sweep_assigns_splits(self):
        output_path = Path(self.temp_dir.name) / "split_sweep.jsonl"
        result = self.service.run_sweep(
            {
                "template_schematic_path": str(self.schematic_path),
                "job_template": {
                    "analysis": "tran",
                    "signals": ["V(out)"],
                },
                "parameters": [
                    {
                        "name": "r1_value",
                        "target": {"reference": "R1", "field": "value"},
                        "values": ["1k", "2k", "3k", "4k", "5k"],
                    }
                ],
                "split_ratios": {"train": 0.6, "val": 0.2, "test": 0.2},
                "sampling": {"mode": "random", "sample_count": 5, "seed": 7},
                "output_path": str(output_path),
                "concurrency": 2,
            }
        )
        self.assertTrue(result["ok"])
        records = [json.loads(line) for line in output_path.read_text(encoding="utf-8").strip().splitlines()]
        splits = [record["metadata"]["split"] for record in records]
        self.assertEqual(len(records), 5)
        self.assertIn("train", splits)
        self.assertIn("val", splits)
        self.assertIn("test", splits)
        self.assertTrue(all(record["tags"]["split"] in {"train", "val", "test"} for record in records))

    def test_expand_parameter_values_supports_linspace(self):
        values = _expand_parameter_values(
            {
                "name": "freq",
                "linspace": {"start": 1, "stop": 5, "count": 3},
                "value_format": "{value:.1f}",
            }
        )
        self.assertEqual(values, ["1.0", "3.0", "5.0"])

    def test_expand_parameter_values_supports_logspace(self):
        values = _expand_parameter_values(
            {
                "name": "cap",
                "logspace": {"start": 1e-9, "stop": 1e-6, "count": 4},
                "value_format": "{value:.3e}",
            }
        )
        self.assertEqual(values[0], "1.000e-09")
        self.assertEqual(values[-1], "1.000e-06")
        self.assertEqual(len(values), 4)

    def test_expand_parameter_values_supports_integer_range(self):
        values = _expand_parameter_values(
            {
                "name": "stage_index",
                "range": {"start": 1, "stop": 5, "step": 2, "inclusive": True},
            }
        )
        self.assertEqual(values, [1, 3, 5])

    def test_format_spice_number_outputs_engineering_suffixes(self):
        self.assertEqual(_format_spice_number(1000), "1k")
        self.assertEqual(_format_spice_number(4.7e-9, precision=3), "4.7n")
        self.assertEqual(_format_spice_number(10e-6), "10u")

    def test_expand_parameter_values_supports_spice_engineering_format(self):
        values = _expand_parameter_values(
            {
                "name": "res",
                "linspace": {"start": 1000, "stop": 3000, "count": 3},
                "engineering_format": "spice",
                "engineering_precision": 3,
            }
        )
        self.assertEqual(values, ["1k", "2k", "3k"])


if __name__ == "__main__":
    unittest.main()
