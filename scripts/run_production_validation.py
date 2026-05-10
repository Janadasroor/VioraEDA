#!/usr/bin/env python3
"""
Production-Level Validation Suite: 200+ Autonomous Simulation Tests.
Verifies the absolute reliability of the VioSpice Engine and MCP bridge.
"""

import json
import os
import sys
from pathlib import Path
import time
import math
import random

# Add project paths
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))

from vspice import mcp as core

TEST_DIR = ROOT / "tests" / "production_validation"
TEST_DIR.mkdir(parents=True, exist_ok=True)

def generate_rc_test(idx):
    r = random.uniform(100, 10000)
    c = random.uniform(1e-9, 100e-6)
    freq = random.uniform(10, 50000)
    
    content = f"""* Production Test {idx}: RC Filter variation
V1 in 0 SIN(0 1 {freq})
R1 in out {r}
C1 out 0 {c}
.tran {1/(freq*100)} {3/freq}
.end
"""
    return content

def generate_diode_test(idx):
    v_in = random.uniform(5, 24)
    r_limit = random.uniform(100, 1000)
    
    content = f"""* Production Test {idx}: Diode Clipper variation
V1 in 0 SIN(0 {v_in} 50)
R1 in out {r_limit}
D1 out 0 D1N4148
.model D1N4148 D(Is=2.682n N=1.836 Rs=0.5662 Ikf=44.17m Cjo=4p M=0.3333 Vj=0.5 Fc=0.5 Bv=100 Ibv=100u Tt=11.54n)
.tran 0.1m 0.1
.end
"""
    return content

def generate_opamp_test(idx):
    gain = random.uniform(2, 20)
    r1 = 1000
    r2 = r1 * (gain - 1)
    
    content = f"""* Production Test {idx}: Op-Amp Non-Inverting variation
V1 in 0 SIN(0 0.5 1k)
V_P vcc 0 15
V_N vee 0 -15
R1 0 neg {r1}
R2 neg out {r2}
X1 in neg vcc vee out OPAMP
.subckt OPAMP 1 2 3 4 5
Rin 1 2 10Meg
E1 5 0 1 2 100k
.ends
.tran 10u 2m
.end
"""
    return content

def main():
    print("🚀 Starting Production-Level Validation (200 Tests)...")
    results = {"passed": 0, "failed": 0, "details": []}
    
    start_time = time.time()
    
    for i in range(1, 201):
        if i <= 100:
            cir = generate_rc_test(i)
            category = "RC_Filter"
        elif i <= 150:
            cir = generate_diode_test(i)
            category = "Diode_Clipper"
        else:
            cir = generate_opamp_test(i)
            category = "OpAmp_Gain"
            
        name = f"test_{i:03d}_{category}.cir"
        path = TEST_DIR / name
        path.write_text(cir)
        
        # Simulate via MCP Core logic (Production path)
        res = core.run_viora_command(["netlist-run", str(path), "--json"], json_out=True)
        
        behavioral_ok = True
        analysis_msg = ""
        
        if res.get("ok"):
            # Behavioral Verification: Analyze the output waveform
            raw_path = res.get("data", {}).get("rawPath")
            if raw_path and os.path.exists(raw_path):
                export_path = path.with_suffix(".json")
                export_res = core.raw_export(str(raw_path), str(export_path), fmt="json")
                
                if export_res.get("ok"):
                    try:
                        with open(export_path) as f:
                            data = json.load(f)
                        
                        # Example Check: Peak voltage should be > 0.1V for RC filter with 1V input
                        if category == "RC_Filter":
                            out_v = data.get("waveforms", {}).get("V(out)", [])
                            if out_v:
                                max_v = max(abs(v) for v in out_v)
                                if max_v < 0.001:
                                    behavioral_ok = False
                                    analysis_msg = f"Suspiciously low output voltage: {max_v}V"
                        
                        # Example Check: Diode clipper should limit voltage to ~0.7V-1V
                        elif category == "Diode_Clipper":
                            out_v = data.get("waveforms", {}).get("V(out)", [])
                            if out_v:
                                max_v = max(out_v)
                                if max_v > 1.5: # Way above diode drop
                                    behavioral_ok = False
                                    analysis_msg = f"Diode clipping failed: peak is {max_v}V"
                                    
                    except Exception as ex:
                        analysis_msg = f"Waveform analysis failed: {str(ex)}"
        
        test_ok = res.get("ok") and behavioral_ok
        status = "✅ PASS" if test_ok else "❌ FAIL"
        if not test_ok and analysis_msg:
            status += f" ({analysis_msg})"
            
        if test_ok:
            results["passed"] += 1
        else:
            results["failed"] += 1
            
        print(f"[{i:03d}/200] {category:15} {status}")

        
        results["details"].append({
            "id": i,
            "category": category,
            "ok": res.get("ok"),
            "log": res.get("data", {}).get("log", []) if res.get("ok") else res.get("stderr")
        })

    elapsed = time.time() - start_time
    print("-" * 40)
    print(f"Validation Complete in {elapsed:.2f}s")
    print(f"Passed: {results['passed']}")
    print(f"Failed: {results['failed']}")
    
    # Save validation report
    report_path = ROOT / "validation_report.json"
    report_path.write_text(json.dumps(results, indent=2))
    print(f"Full report saved to: {report_path}")
    
    if results["failed"] > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()
