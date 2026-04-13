# Python Legacy Code

These files are kept for reference but are **not imported** by any active code.

- **`scripts/ai_signal_generator.py`** — Old Gemini wrapper for generating `SmartSignal` classes (superseded by `gemini_query.py`).
- **`scripts/jit_simulation_runner.py`** — JIT preview runner; never wired into the UI.
- **`scripts/adk_agent.py`** — Google ADK (Agent Development Kit) experiment; replaced by `gemini_query.py` using the direct `google-genai` API.
- **`scripts/test_adk_agent.py`** — Tests for the ADK experiment.
- **`api/ai_query.py`** — Alternative Gemini query runner; duplicative of `gemini_query.py`.
- **`agents/specialized/simulation_agent.py`** — ADK-based `LlmAgent` wrapper; only imported by the legacy `adk_agent.py`.

If you want to resurrect any of these, move them back to their original locations and update the imports.
