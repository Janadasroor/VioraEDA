"""
Gemini Terminal — bridges the AI assistant (Gemini) with the VioSpice Python API.

This module provides:
1. A terminal interface where AI-generated Python code can be executed directly
2. <PYTHON> tag parsing for AI responses containing executable code
3. A vspice API tool execution path that bypasses subprocess calls
4. Integration with the Python console widget for unified REPL experience

Usage in the embedded console:
    >>> from vspice.gemini_terminal import GeminiTerminal
    >>> term = GeminiTerminal()
    >>> term.execute_ai_script("Plot the voltage at node VOUT")
"""

from __future__ import annotations

import json
import os
import re
import sys
import textwrap
from typing import Any, Callable, Dict, List, Optional, Tuple


# ─── AI Response Tag Parser ───────────────────────────────────────

class AITagParser:
    """Parse structured tags from AI responses.

    Supported tags:
    - <THOUGHT>...</THOUGHT> — AI reasoning
    - <ACTION>...</ACTION> — Status updates
    - <PYTHON>...</PYTHON> — Executable Python code
    - <SNIPPET>...</SNIPPET> — JSON command snippets
    - <SUGGESTION>Label|command</SUGGESTION> — Follow-up buttons
    - <TOOL_CALL>...</TOOL_CALL> — Tool invocation
    - <TOOL_RESULT>...</TOOL_RESULT> — Tool output
    - <USAGE>...</USAGE> — Token usage stats
    """

    _TAG_PATTERN = re.compile(
        r'<(\w+)>(.*?)</\1>',
        re.DOTALL
    )

    @classmethod
    def parse(cls, text: str) -> Dict[str, List[str]]:
        """Extract all tagged content from text.

        Returns a dict mapping tag names to lists of content strings.
        """
        results: Dict[str, List[str]] = {}
        for match in cls._TAG_PATTERN.finditer(text):
            tag_name = match.group(1)
            content = match.group(2).strip()
            results.setdefault(tag_name, []).append(content)
        return results

    @classmethod
    def extract_python(cls, text: str) -> List[str]:
        """Extract all <PYTHON>...</PYTHON> blocks."""
        tags = cls.parse(text)
        return tags.get("PYTHON", [])

    @classmethod
    def extract_snippets(cls, text: str) -> List[dict]:
        """Extract and parse all <SNIPPET>...</SNIPPET> JSON blocks."""
        tags = cls.parse(text)
        snippets = []
        for s in tags.get("SNIPPET", []):
            try:
                data = json.loads(s)
                snippets.append(data)
            except json.JSONDecodeError:
                snippets.append({"raw": s})
        return snippets

    @classmethod
    def extract_suggestions(cls, text: str) -> List[Tuple[str, str]]:
        """Extract <SUGGESTION>Label|command</SUGGESTION> pairs."""
        tags = cls.parse(text)
        suggestions = []
        for s in tags.get("SUGGESTION", []):
            if "|" in s:
                label, command = s.split("|", 1)
                suggestions.append((label.strip(), command.strip()))
            else:
                suggestions.append((s.strip(), s.strip()))
        return suggestions

    @classmethod
    def strip_tags(cls, text: str) -> str:
        """Remove all tags from text, leaving only plain content."""
        return cls._TAG_PATTERN.sub("", text).strip()


# ─── vspice API Tool Executor ─────────────────────────────────────

class VspiceToolExecutor:
    """Execute simulation and editor tools via the vspice Python API.

    This bypasses subprocess calls to vio-cmd and uses the nanobind
    vspice module directly for faster, in-process execution.
    """

    def __init__(self):
        self._last_results: Optional[dict] = None
        self._available = False
        self._init_vspice()

    def _init_vspice(self) -> None:
        """Try to import vspice module."""
        try:
            import vspice._core as _core
            self._vspice = _core
            self._available = True
        except ImportError:
            self._available = False

    @property
    def is_available(self) -> bool:
        return self._available

    def run_simulation(self, netlist: str, analysis: str = "tran",
                       t_stop: str = "10m", t_step: str = "100u") -> dict:
        """Run a SPICE simulation via vspice module."""
        if not self._available:
            return {"error": "vspice module not available"}
        try:
            result = self._vspice.run_simulation(
                netlist, analysis=analysis,
                t_stop=t_stop, t_step=t_step
            )
            self._last_results = result
            return result
        except Exception as e:
            return {"error": str(e)}

    def list_nodes(self, netlist: Optional[str] = None) -> dict:
        """List all nodes in a netlist."""
        if not self._available:
            return {"error": "vspice module not available"}
        try:
            from vspice._core import SimNetlist
            nl = SimNetlist()
            # Parse netlist if provided
            if netlist:
                for line in netlist.strip().split('\n'):
                    line = line.strip()
                    if line and not line.startswith('*'):
                        nl._add_raw_line(line)
            nodes = [n.name for n in nl.nodes] if hasattr(nl, 'nodes') else []
            return {"nodes": nodes, "count": len(nodes)}
        except Exception as e:
            return {"error": str(e)}

    def get_signal_data(self, signal_name: str) -> dict:
        """Get statistics for a signal from last simulation results."""
        if not self._last_results:
            return {"error": "No simulation results available. Run a simulation first."}
        try:
            waveforms = self._last_results.get("waveforms", {})
            if signal_name in waveforms:
                wf = waveforms[signal_name]
                return {
                    "name": signal_name,
                    "points": len(wf.get("x", [])),
                    "min": wf.get("stats", {}).get("min", None),
                    "max": wf.get("stats", {}).get("max", None),
                    "avg": wf.get("stats", {}).get("avg", None),
                    "rms": wf.get("stats", {}).get("rms", None),
                }
            return {"error": f"Signal '{signal_name}' not found in results"}
        except Exception as e:
            return {"error": str(e)}

    def compute_average_power(self, target: str = "circuit",
                              t_start: Optional[float] = None,
                              t_end: Optional[float] = None) -> dict:
        """Compute average power for a target."""
        if not self._last_results:
            return {"error": "No simulation results available."}
        try:
            from vspice._core import SimResults
            # Access via last results
            waveforms = self._last_results.get("waveforms", {})
            if not waveforms:
                return {"error": "No waveforms in results"}
            # Simplified power computation
            return {
                "target": target,
                "t_start": t_start or 0.0,
                "t_end": t_end or 1.0,
                "average_power_w": 0.0,  # Would need actual waveform data
                "note": "Power computation requires waveform data",
            }
        except Exception as e:
            return {"error": str(e)}

    def parse_netlist(self, netlist_text: str) -> dict:
        """Parse a SPICE netlist and return structured data."""
        if not self._available:
            return {"error": "vspice module not available"}
        try:
            from vspice._core import SimNetlist
            nl = SimNetlist()
            # Parse
            for line in netlist_text.strip().split('\n'):
                line = line.strip()
                if line and not line.startswith('*'):
                    nl._add_raw_line(line)
            return {
                "components": [c.name for c in nl.components] if hasattr(nl, 'components') else [],
                "nodes": [n.name for n in nl.nodes] if hasattr(nl, 'nodes') else [],
                "models": [m.name for m in nl.models] if hasattr(nl, 'models') else [],
            }
        except Exception as e:
            return {"error": str(e)}


# ─── Gemini Terminal ──────────────────────────────────────────────

class GeminiTerminal:
    """Terminal interface for AI-assisted VioSpice workflows.

    This class bridges the Gemini AI assistant with the VioSpice Python API,
    allowing AI-generated Python code to be executed directly in the
    embedded console.

    Features:
    - Execute natural language commands through the AI
    - Parse <PYTHON> tags from AI responses and execute the code
    - Access vspice API tools directly (no subprocess overhead)
    - Stream AI responses with structured tag extraction

    Example usage in the embedded Python console:

        >>> import v
        >>> term = v.gemini  # Auto-created instance
        >>> term.ask("What nodes are available?")
        >>> term.execute("Plot V(out) from the last transient analysis")
        >>> term.run_python("import numpy as np; print(np.pi)")
    """

    def __init__(self, executor: Optional[VspiceToolExecutor] = None):
        self._executor = executor or VspiceToolExecutor()
        self._output_callback: Optional[Callable[[str, bool], None]] = None
        self._python_executor: Optional[Callable[[str], Tuple[str, bool]]] = None
        self._history: List[dict] = []
        self._api_key: Optional[str] = os.environ.get("GEMINI_API_KEY", "")

    def set_output_callback(self, fn: Callable[[str, bool], None]) -> None:
        """Set a callback for output text.

        Parameters
        ----------
        fn : callable
            Function(text: str, is_error: bool) -> None
        """
        self._output_callback = fn

    def set_python_executor(self, fn: Callable[[str], Tuple[str, bool]]) -> None:
        """Set the Python code executor function.

        Parameters
        ----------
        fn : callable
            Function(code: str) -> (output: str, is_error: bool)
        """
        self._python_executor = fn

    # ── Output helpers ────────────────────────────────────────────

    def _emit(self, text: str, is_error: bool = False) -> None:
        """Emit output text."""
        if self._output_callback:
            self._output_callback(text, is_error)
        else:
            # Fallback to print
            if is_error:
                print(f"[ERROR] {text}", file=sys.stderr)
            else:
                print(text)

    def _emit_python(self, code: str) -> str:
        """Execute Python code and return output."""
        if self._python_executor:
            output, is_error = self._python_executor(code)
            if output:
                self._emit(output, is_error)
            return output
        # Fallback: exec
        import io
        old_stdout = sys.stdout
        old_stderr = sys.stderr
        captured = io.StringIO()
        sys.stdout = captured
        sys.stderr = captured
        try:
            exec(code, {"__name__": "__gemini_terminal__"})
            output = captured.getvalue()
            self._emit(output, False)
            return output
        except Exception as e:
            output = captured.getvalue() + f"\nError: {e}\n"
            self._emit(output, True)
            return output
        finally:
            sys.stdout = old_stdout
            sys.stderr = old_stderr

    # ── Core API ──────────────────────────────────────────────────

    @property
    def tools(self) -> VspiceToolExecutor:
        """Access the vspice tool executor."""
        return self._executor

    @property
    def history(self) -> List[dict]:
        """Get the conversation history."""
        return list(self._history)

    def clear_history(self) -> None:
        """Clear conversation history."""
        self._history.clear()
        self._emit("Conversation history cleared.")

    def ask(self, question: str) -> str:
        """Ask the AI a question and process any <PYTHON> tags.

        This method would normally call the Gemini API, but since the
        embedded console doesn't have network access by default, it
        instead processes the question using available vspice tools.

        Parameters
        ----------
        question : str
            Natural language question or command.

        Returns
        -------
        str
            AI response text with tags processed.
        """
        self._history.append({"role": "user", "text": question})

        # Try to answer using available tools
        response = self._answer_with_tools(question)

        self._history.append({"role": "model", "text": response})
        self._emit(response)

        # Execute any <PYTHON> blocks
        python_blocks = AITagParser.extract_python(response)
        for code in python_blocks:
            self._emit("\n--- Executing AI-generated Python code ---")
            self._emit_python(code)

        return response

    def execute(self, command: str) -> str:
        """Execute a natural language command via tools.

        Parameters
        ----------
        command : str
            Command like "Plot V(out)" or "List all nodes".

        Returns
        -------
        str
            Result text.
        """
        self._history.append({"role": "user", "text": command})

        result = self._execute_command(command)

        self._history.append({"role": "model", "text": result})
        self._emit(result)
        return result

    def run_python(self, code: str) -> str:
        """Execute Python code directly.

        Parameters
        ----------
        code : str
            Python code to execute.

        Returns
        -------
        str
            Output text.
        """
        output = self._emit_python(code)
        self._history.append({"role": "python", "text": code, "output": output})
        return output

    def run_snippet(self, snippet: dict) -> str:
        """Execute a command snippet (JSON dict).

        Parameters
        ----------
        snippet : dict
            Command snippet, e.g. {"commands": ["plot V(out)"]}.

        Returns
        -------
        str
            Output text.
        """
        commands = snippet.get("commands", [])
        if isinstance(commands, str):
            commands = [commands]

        results = []
        for cmd in commands:
            output = self._execute_command(cmd)
            results.append(f">>> {cmd}\n{output}")

        result_text = "\n".join(results)
        self._emit(result_text)
        return result_text

    # ── Internal: Tool-based command routing ──────────────────────

    def _answer_with_tools(self, question: str) -> str:
        """Answer a question using available vspice tools (no network)."""
        q = question.lower()

        # Node listing
        if "list" in q and ("node" in q or "signal" in q):
            result = self._executor.list_nodes()
            if "error" in result:
                return f"Error: {result['error']}"
            nodes = result.get("nodes", [])
            if nodes:
                return (
                    f"Found {len(nodes)} nodes:\n"
                    + ", ".join(nodes)
                    + "\n\n<SUGGESTION>Simulate|run simulation</SUGGESTION>"
                    + "<SUGGESTION>Plot first node|plot_signal(" + repr(nodes[0]) + ")</SUGGESTION>"
                )
            return "No nodes found. Try loading a netlist first."

        # Simulation
        if "simul" in q or "run" in q:
            analysis = "tran"
            if "ac" in q:
                analysis = "ac"
            elif "op" in q or "operating point" in q or "dc" in q:
                analysis = "op"

            return (
                f"To run a simulation, you need a SPICE netlist.\n"
                f"Use v.simulation.run(netlist, analysis='{analysis}') in the console.\n"
                f"Or load a schematic and use the simulation tools.\n\n"
                f"<SUGGESTION>List nodes|list nodes</SUGGESTION>"
            )

        # Signal data
        if "signal" in q or "data" in q or "value" in q:
            m = re.search(r"(\w+\([^)]+\)|\w+)", question)
            if m:
                signal_name = m.group(1)
                result = self._executor.get_signal_data(signal_name)
                if "error" in result:
                    return f"Signal '{signal_name}': {result['error']}"
                return (
                    f"Signal: {result['name']}\n"
                    f"Points: {result['points']}\n"
                    f"Min: {result['min']}\n"
                    f"Max: {result['max']}\n"
                    f"Average: {result['avg']}\n"
                    f"RMS: {result['rms']}"
                )

        # Default
        return (
            "I can help you with circuit simulation and analysis using the vspice API.\n"
            "Available commands:\n"
            "  - 'list nodes' — Show available circuit nodes\n"
            "  - 'get signal data for V(node)' — Get signal statistics\n"
            "  - 'run simulation' — Start a transient analysis\n"
            "  - 'compute power' — Calculate average power\n\n"
            "For full AI capabilities, use the Gemini Panel in the GUI."
        )

    def _execute_command(self, command: str) -> str:
        """Execute a natural language command."""
        cmd = command.lower().strip()

        # Plot signal
        if cmd.startswith("plot"):
            m = re.search(r"(\w+\([^)]+\)|\w+)", command)
            if m:
                signal_name = m.group(1)
                result = self._executor.get_signal_data(signal_name)
                if "error" not in result:
                    return (
                        f"Signal '{signal_name}' data:\n"
                        f"  Points: {result.get('points', 0)}\n"
                        f"  Min: {result.get('min', 'N/A')}\n"
                        f"  Max: {result.get('max', 'N/A')}\n"
                        f"  Avg: {result.get('avg', 'N/A')}\n"
                        f"  RMS: {result.get('rms', 'N/A')}"
                    )
                return result.get("error", "Unknown signal")

        # List nodes
        if cmd.startswith("list") and "node" in cmd:
            result = self._executor.list_nodes()
            if "error" in result:
                return result["error"]
            nodes = result.get("nodes", [])
            return f"Nodes ({result.get('count', 0)}): " + ", ".join(nodes) if nodes else "No nodes found."

        # Get signal
        if cmd.startswith("get") and "signal" in cmd:
            m = re.search(r"(\w+\([^)]+\)|\w+)", command)
            if m:
                return self._execute_command(f"plot {m.group(1)}")

        # Compute power
        if "power" in cmd:
            result = self._executor.compute_average_power()
            return json.dumps(result, indent=2)

        # Parse netlist
        if "parse" in cmd and "netlist" in cmd:
            # Would need netlist text from context
            return "Provide the netlist text to parse."

        # Unknown
        return f"Command not recognized: {command}\nTry: list nodes, plot V(node), compute power"

    def help(self) -> str:
        """Show Gemini Terminal help."""
        return textwrap.dedent("""\
            Gemini Terminal Help
            ====================

            The Gemini Terminal bridges AI responses with the VioSpice Python API.

            Methods:
              term.ask("question")          — Ask AI, auto-execute <PYTHON> blocks
              term.execute("command")       — Execute natural language commands
              term.run_python(code)         — Execute Python code directly
              term.run_snippet(snippet)     — Execute command snippets
              term.tools.*                  — Direct tool access
              term.history                  — Conversation history
              term.clear_history()          — Clear conversation
              term.help()                   — Show this help

            AI Response Tags (auto-processed):
              <PYTHON>...</PYTHON>          — Executed automatically
              <SNIPPET>...</SNIPPET>        — Command JSON, use term.run_snippet()
              <SUGGESTION>Label|cmd</SUGGESTION> — Follow-up buttons (GUI only)

            Example:
              >>> import v
              >>> v.gemini.ask("List all nodes")
              >>> v.gemini.execute("Plot V(out)")
              >>> v.gemini.run_python("import numpy; print(numpy.pi)")
        """)


# ─── Console Integration Helper ───────────────────────────────────

def _install_in_console() -> Optional[GeminiTerminal]:
    """Install the Gemini terminal into the Python __main__ namespace.

    Returns the GeminiTerminal instance, or None if unavailable.
    """
    try:
        import __main__
        term = GeminiTerminal()
        __main__.__dict__["gemini"] = term
        __main__.__dict__["term"] = term
        return term
    except Exception:
        return None


# Auto-install when module is imported in console context
if __name__ != "__main__":
    _install_in_console()
