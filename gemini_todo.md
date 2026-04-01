# Viora Gemini Panel - Future Enhancements

This document tracks potential high-impact features inspired by the official `gemini-cli` repository to enhance the Viora AI Panel in VioSpice.

## ✅ Completed Features

### Circuit Style Transfer (2026-04-01)
- [x] Python tool `transfer_schematic_style()` with 7 style presets
- [x] C++ integration `onApplyAIStyleTransfer()` slot
- [x] QML UI with style preset popup (🎨 button)
- [x] Style presets: TI, ADI, LTspice, IEC, Military, Clean Modern, Vintage
- [x] Documentation: `docs/style_transfer_guide.md`

## 1. Model Context Protocol (MCP) Integration
- [ ] Implement an MCP Client within Viora.
- [ ] Allow dynamic attachment of remote tools and servers.
- [ ] Enable Viora to search the local filesystem, read GitHub issues, or fetch component datasheets without writing hardcoded C++ parsers.

## 2. Session Checkpointing & Time Travel (Rewind)
- [ ] Implement lightweight schema snapshots (e.g., saving `.sch` state) before Viora executes a tool that modifies the schematic.
- [ ] Create a `/rewind` command in the Viora chat to jump the schematic and conversation state back to the previous turn if the AI routes a wire poorly or places components incorrectly.

## 3. Agent Skills & Specialized Subagents
- [ ] Architect a subrouting system where Viora acts as an Orchestrator.
- [ ] **Simulation Subagent:** Highly tuned exclusively for analyzing NgSpice log errors and stepping waveforms.
- [ ] **Layout Subagent:** Exclusively trained on Cartesian geometry to efficiently auto-route wires without crossing components.

## 4. Persistent Project Memory (`.viora-context.md`)
- [ ] Add support for a `.viora-context.md` file in the root of any VioSpice project.
- [ ] Automatically inject this file's contents into the system prompt upon loading the project. 
- [ ] *Example Use Case:* Define rules like "Always use 0805 Imperial sizes for capacitors" or "Keep all bypass capacitors on the left phase plane."

## 5. "Plan Mode" (Safe Execution)
- [ ] Implement a read-only mode for planning complex operations before executing them.
- [ ] **Workflow:** When asked for a complex task, Viora outputs a Markdown checklist (`task.md`) in the chat instead of automatically creating files or auto-placing components.
- [ ] Execution only occurs after the user explicitly clicks an "Approve Execution" action card.

## 6. Action Policy Engine (Sandboxing)
- [ ] Implement a configurable permissions model via a UI settings page.
- [ ] Provide fine-grained execution control over what the AI is allowed to touch.
- [ ] *Example Use Case:* Allow Viora to run simulation engines and list files, but request explicit user permission before overriding any `.sch` or `.lib` file.
