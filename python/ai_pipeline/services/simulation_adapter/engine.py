import subprocess
import json
import os
import shutil
import re

from ai_pipeline.config import LOG_FILE_PATH, resolve_vio_cmd


class SimulationAdapter:
    def __init__(self, vio_cmd_path=None):
        self.vio_cmd_path = resolve_vio_cmd(vio_cmd_path)
        self.last_results = None
        self.last_error = ""

    def list_nodes(self, schematic_path):
        """Returns a list of all nodes/signals available in the schematic."""
        results = self.run_simulation(schematic_path, analysis_type="op")
        if not results:
            return []
        nodes = list(results.get("nodeVoltages", {}).keys())
        signals = [w["name"] for w in results.get("waveforms", [])]
        return sorted(list(set(nodes + signals)))

    def get_component_nets(self, schematic_path, component_ref):
        """Returns ordered pin-to-net mappings for a component using the schematic netlist JSON."""
        self.last_error = ""
        if not os.path.exists(schematic_path):
            self.last_error = f"Schematic file not found: {schematic_path}"
            return []

        cmd = [
            self.vio_cmd_path,
            "schematic-netlist",
            schematic_path,
            "--format",
            "json",
        ]
        try:
            result = subprocess.run(cmd, capture_output=True, text=True)
            output = (result.stdout or "").strip()
            if not output:
                self.last_error = f"schematic-netlist produced no output. Error: {(result.stderr or '').strip()}"
                return []
            data = json.loads(output)
            nets = data.get("nets", [])
            pin_pairs = []
            for net in nets:
                net_name = str(net.get("name", "")).strip()
                for pin in net.get("pins", []):
                    ref = str(pin.get("ref", "")).strip()
                    pin_name = str(pin.get("pin", "")).strip()
                    if ref.upper() == str(component_ref).strip().upper() and net_name:
                        pin_pairs.append((pin_name, net_name))
            pin_pairs.sort(key=lambda pair: int(pair[0]) if str(pair[0]).isdigit() else 9999)
            return pin_pairs
        except Exception as e:
            self.last_error = f"Failed to resolve component nets: {str(e)}"
            return []

    def run_simulation(self, schematic_path, analysis_type="op", stop_time="10m", step_size="100u"):
        self.last_error = ""
        if not os.path.exists(schematic_path):
            self.last_error = f"Schematic file not found: {schematic_path}"
            return None

        if not (
            (os.path.isabs(self.vio_cmd_path) and os.path.isfile(self.vio_cmd_path) and os.access(self.vio_cmd_path, os.X_OK))
            or shutil.which(self.vio_cmd_path)
        ):
            self.last_error = (
                f"Simulation CLI not found: '{self.vio_cmd_path}'. Build `vio-cmd` "
                "or add it to PATH."
            )
            return None

        cmd = [
            self.vio_cmd_path,
            "simulate",
            schematic_path,
            "--analysis", analysis_type,
            "--json"
        ]
        if analysis_type == "tran":
            cmd.extend(["--stop", stop_time, "--step", step_size])

        try:
            with open(LOG_FILE_PATH, "a", encoding="utf-8") as log:
                log.write(f"\n--- Simulation Run: {analysis_type} ---\n")
                log.write(f"Cmd: {' '.join(cmd)}\n")

            result = subprocess.run(cmd, capture_output=True, text=True)

            with open(LOG_FILE_PATH, "a", encoding="utf-8") as log:
                log.write(f"Return code: {result.returncode}\n")
                log.write(f"Stdout length: {len(result.stdout)}\n")
                if result.stderr:
                    log.write(f"Stderr: {result.stderr[:500]}...\n")
                if len(result.stdout) > 0:
                    log.write(f"Stdout start: {result.stdout[:200]}\n")

            if "Unknown option 'json'" in (result.stderr or ""):
                cmd = [c for c in cmd if c != "--json"]
                result = subprocess.run(cmd, capture_output=True, text=True)
                
            output = result.stdout.strip()
            if not output:
                self.last_error = f"Simulation produced no output. Error: {(result.stderr or '').strip()}"
                return None
            
            # Fast search for the JSON block
            json_start = output.find('{')
            json_end = output.rfind('}')
            
            if json_start != -1 and json_end != -1 and json_end > json_start:
                try:
                    json_data = output[json_start : json_end + 1]
                    self.last_results = json.loads(json_data)
                    with open(LOG_FILE_PATH, "a", encoding="utf-8") as log:
                        waves = self.last_results.get("waveforms", [])
                        nodes = self.last_results.get("nodeVoltages", {})
                        log.write(f"Parsed success. Waveforms: {len(waves)}, Nodes: {len(nodes)}\n")
                        if waves:
                            log.write(f"First 5 waves: {', '.join([w.get('name','') for w in waves[:5]])}\n")
                    return self.last_results
                except json.JSONDecodeError as e:
                    self.last_error = f"Failed to parse JSON: {str(e)}"
                    with open(LOG_FILE_PATH, "a", encoding="utf-8") as log:
                        log.write(f"JSON Parse Error: {str(e)}\n")
            
            self.last_error = "Simulation output did not contain valid JSON."
            return None
        except Exception as e:
            self.last_error = f"Simulation failed: {str(e)}"
            return None

    def get_signal(self, signal_name):
        if not self.last_results:
            return None, None
        
        target = str(signal_name).strip().upper()
        
        # 1. Try exact or case-insensitive match in waveforms
        for wave in self.last_results.get("waveforms", []):
            name = str(wave.get("name", "")).strip().upper()
            if name == target:
                return wave.get("x"), wave.get("y")
        
        # 2. Try common variations
        alt_targets = set()
        # Strip wrappers
        m = re.match(r"^[VI]\((.*)\)$", target, re.I)
        if m:
            clean = m.group(1).strip()
            alt_targets.add(clean)
        else:
            alt_targets.add(f"V({target})")
            alt_targets.add(f"I({target})")
        
        # Case-insensitive search on waveforms
        for alt in alt_targets:
            for wave in self.last_results.get("waveforms", []):
                w_name = str(wave.get("name", "")).strip().upper()
                if w_name == alt:
                    return wave.get("x"), wave.get("y")
                # Also try matching without V() wrapper if the waveform name has it
                m_wave = re.match(r"^[VI]\((.*)\)$", w_name, re.I)
                if m_wave and m_wave.group(1).strip() == alt:
                    return wave.get("x"), wave.get("y")

        # 3. Match in nodeVoltages
        clean_name = target
        if clean_name.startswith("V(") and clean_name.endswith(")"):
            clean_name = clean_name[2:-1]
            
        node_voltages = self.last_results.get("nodeVoltages", {})
        # Check case-insensitive
        for node, val in node_voltages.items():
            if str(node).upper() == clean_name:
                return [0.0], [val]

        # 4. Branch current fallback
        branch = self.last_results.get("branchCurrents", {})
        clean_i = target
        if clean_i.startswith("I(") and clean_i.endswith(")"):
            clean_i = clean_i[2:-1]
            
        for b, val in branch.items():
            ub = str(b).upper()
            if ub == clean_i or ub == target:
                return [0.0], [val]
            
        return None, None
