import subprocess
import json
import os
import pytest
from pathlib import Path

# Path to the viora binary
VIORA_PATH = os.environ.get("VIORA_PATH", "./build/viora")
FIXTURES_DIR = Path(__file__).parent / "fixtures"

def run_viora(args):
    """Helper to run viora and return parsed JSON output."""
    cmd = [VIORA_PATH] + args
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        # Some commands might return non-zero but still output valid JSON (warnings)
        # but generally we expect 0.
        pass
    
    try:
        # Try to find JSON in the output (it might have some log messages before/after)
        output = result.stdout
        start_idx = output.find('{')
        if start_idx != -1:
            return json.loads(output[start_idx:])
        return output
    except json.JSONDecodeError:
        return output

def test_viora_help():
    result = subprocess.run([VIORA_PATH, "--help"], capture_output=True, text=True)
    assert result.returncode == 0
    assert "Usage: viora" in result.stdout

def test_schematic_query():
    sch_path = str(FIXTURES_DIR / "untitled.sch")
    data = run_viora(["schematic-query", sch_path, "--json"])
    assert isinstance(data, dict)
    assert "file" in data
    assert data["file"].endswith("untitled.sch")
    assert "components" in data
    assert isinstance(data["components"], list)

def test_schematic_bom():
    sch_path = str(FIXTURES_DIR / "untitled.sch")
    data = run_viora(["schematic-bom", sch_path, "--json"])
    assert isinstance(data, dict)
    assert "components" in data
    assert "groups" in data

def test_schematic_validate():
    sch_path = str(FIXTURES_DIR / "untitled.sch")
    data = run_viora(["schematic-validate", sch_path, "--json"])
    assert isinstance(data, dict)
    assert "summary" in data

def test_symbol_validate():
    # Create a temporary symbol file
    sym_content = {
        "name": "TestSymbol",
        "referencePrefix": "X",
        "primitives": [
            {
                "type": "pin",
                "x": 0,
                "y": 0,
                "number": 1,
                "name": "1",
                "orientation": "Right",
                "length": 10
            }
        ]
    }
    sym_path = "temp_symbol.viosym"
    with open(sym_path, "w") as f:
        json.dump(sym_content, f)
    
    try:
        data = run_viora(["symbol-validate", sym_path, "--json"])
        assert isinstance(data, dict)
        assert data["name"] == "TestSymbol"
        assert "issues" in data
    finally:
        if os.path.exists(sym_path):
            os.remove(sym_path)

def test_netlist_run_smart_signal():
    smart_sch_path = str(FIXTURES_DIR / "smart_signal_pwm.flxsch")
    data = run_viora([
        "netlist-run", smart_sch_path,
        "--analysis", "tran",
        "--stop", "2m",
        "--step", "5u",
        "--export-raw", "json",
        "--json"
    ])
    
    assert isinstance(data, dict)
    assert data.get("ok") is True
    assert "raw" in data
    raw = data["raw"]
    assert "signals" in raw
    
    # Check if OUT signal exists and toggles
    signals = raw["signals"]
    out_values = None
    for s in signals:
        if s["name"].upper() in ["V(OUT)", "OUT"]:
            out_values = s["values"]
            break
    
    assert out_values is not None
    assert len(out_values) > 0
    
    v_min = min(out_values)
    v_max = max(out_values)
    assert v_min < 0.1
    assert v_max > 4.0

if __name__ == "__main__":
    pytest.main([__file__])
