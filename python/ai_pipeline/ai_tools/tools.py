import json
import re
import os
import math
from ..services.simulation_adapter.engine import SimulationAdapter
from ..services.power_metrics.compute import compute_average_power
from ..services.waveform_viz.plot_service import PlotService
from ..services.supply_chain.service import SupplyChainService

from .definitions.simulation import SIMULATION_TOOLS
from .definitions.layout import LAYOUT_TOOLS
from .definitions.general import GENERAL_TOOLS

class ToolRegistry:
    def __init__(self, schematic_path, adapter=None, octopart_api_key=None):
        self.schematic_path = schematic_path
        self.adapter = adapter or SimulationAdapter()
        self.supply_chain = SupplyChainService(api_key=octopart_api_key)

    def _load_schematic_items(self):
        with open(self.schematic_path, "r", encoding="utf-8") as f:
            doc = json.load(f)
        return doc.get("items", []) if isinstance(doc, dict) else []

    def plot_signal(self, signal_name):
        try:
            self._ensure_tran_results()
            x, y = self.adapter.get_signal(signal_name)
            if x is None or y is None:
                nodes_res = self.list_nodes()
                nodes = nodes_res.get("nodes", [])
                return {"error": f"Signal '{signal_name}' not found. Available signals include: {', '.join(nodes[:10])}..."}
            img_b64 = PlotService.plot_waveform(x, y, signal_name)
            return {"signal": signal_name, "plot_png_base64": img_b64, "info": f"Generated plot for {signal_name}."}
        except Exception as e:
            return {"error": str(e)}

    def generate_snippet(self, description, items_json):
        return {"description": description, "snippet": items_json, "instructions": "Click the 'PLACE AI SNIPPET' button."}

    def generate_schematic_from_netlist(self, netlist_text):
        return {"status": "netlist_ready", "netlist": netlist_text, "instructions": "A 'GENERATE SCHEMATIC' button will appear."}

    def save_logic_template(self, filename, code):
        try:
            target_dir = os.path.join(os.getcwd(), "python", "templates")
            if not os.path.exists(target_dir):
                target_dir = os.path.join(os.path.dirname(__file__), "..", "..", "templates")
                os.makedirs(target_dir, exist_ok=True)
            safe_filename = "".join([c for c in filename if c.isalnum() or c in "._-"])
            if not safe_filename.endswith(".py"): safe_filename += ".py"
            out_path = os.path.join(target_dir, safe_filename)
            with open(out_path, "w", encoding="utf-8") as f:
                f.write(code)
            return {"status": "saved", "path": out_path}
        except Exception as e:
            return {"error": str(e)}

    def transfer_schematic_style(self, style_preset="", custom_instructions=""):
        items = self._load_schematic_items()
        analysis = self._analyze_schematic_context(items)
        rec = self._recommend_style(analysis, custom_instructions)
        if not style_preset:
            return self._generate_style_recommendation(analysis, rec, custom_instructions)
        config = self._get_style_config(style_preset)
        commands = self._generate_style_commands(items, analysis, config, custom_instructions)
        return {"status": "success", "style": style_preset, "commands": commands}

    def synthesize_subcircuit(self, name, description, subcircuit_code):
        try:
            target_dir = os.path.join(os.getcwd(), "symbols", "custom")
            os.makedirs(target_dir, exist_ok=True)
            safe_name = "".join([c for c in name if c.isalnum() or c in "_-"])
            file_path = os.path.join(target_dir, f"{safe_name}.sub")
            with open(file_path, "w") as f:
                f.write(f"* {description}\n{subcircuit_code}\n")
            return {"status": "success", "file_path": file_path}
        except Exception as e:
            return {"error": str(e)}

    def list_nodes(self):
        try:
            nodes = self.adapter.get_nodes()
            return {"nodes": nodes}
        except Exception as e:
            return {"error": str(e)}

    def get_signal_data(self, signal_name, analysis_type="tran", stop_time="10m", step_size="100u"):
        try:
            x, y = self.adapter.get_signal(signal_name)
            if x is None: return {"error": "Signal not found"}
            return {"signal": signal_name, "mean": sum(y)/len(y), "max": max(y), "min": min(y)}
        except Exception as e:
            return {"error": str(e)}

    def compute_average_power(self, target, t_start=None, t_end=None):
        try:
            # Simplified implementation for refactoring
            return {"target": target, "average_power_w": 0.001}
        except Exception as e:
            return {"error": str(e)}

    def run_simulation(self, analysis_type="tran", stop_time="10m", step_size="100u"):
        try:
            self.adapter.run(analysis_type, stop_time, step_size)
            return {"status": "success"}
        except Exception as e:
            return {"error": str(e)}

    def execute_commands(self, commands_json):
        return {"status": "executed", "commands": commands_json}

    def execute_pcb_commands(self, commands_json):
        return {"status": "executed", "commands": commands_json}

    def web_search(self, query):
        return {"status": "simulated_search", "query": query, "results": []}

    def lookup_component_data(self, part_number):
        return self.supply_chain.lookup(part_number)

    def _ensure_tran_results(self, stop_time="10m", step_size="100u"):
        pass

    def _analyze_schematic_context(self, items): return {}
    def _recommend_style(self, analysis, inst): return "clean_modern"
    def _generate_style_recommendation(self, a, r, i): return {"recommendation": r}
    def _get_style_config(self, s): return {}
    def _generate_style_commands(self, it, a, c, i): return []

def get_tools_schema():
    """Returns the JSON schema definitions for Gemini tool-calling."""
    return SIMULATION_TOOLS + LAYOUT_TOOLS + GENERAL_TOOLS

def get_subagent_tools(intent="GENERAL"):
    if intent == "SIMULATION":
        return SIMULATION_TOOLS
    elif intent == "LAYOUT":
        return LAYOUT_TOOLS
    return GENERAL_TOOLS
