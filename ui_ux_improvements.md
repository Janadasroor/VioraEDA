# Schematic Editor UI/UX Improvements

This document outlines proposed UI/UX enhancements for the viospice schematic editor, focusing on modernizing the workflow, improving SPICE integration, and enhancing overall usability.

## 1. Canvas & Navigation
*   **Pan & Zoom:** Smooth, hardware-accelerated pan (middle-click drag) and zoom (scroll-wheel centered on cursor).
*   **Grid System:** Subtle visual grid (dots or faint lines) with configurable snap-to-grid. Allow temporary snap disable (e.g., holding `Alt`).
*   **Mini-map / Overview:** A small dockable navigator for large hierarchical designs to quickly jump to different areas.
*   **Net Highlighting:** Hovering over a wire should slightly highlight the entire connected electrical net to quickly trace connections visually.

## 2. Component Placement & Wiring
*   **Smart Wiring:** Orthogonal routing by default. Wires should "rubber-band" (stay connected) when moving components.
*   **Junction Dots:** Auto-generate clear, visible junction dots when three or more wires intersect.
*   **Quick Add / Command Palette:** Pressing `Space` or `/` opens a quick-search palette at the cursor to instantly find and place components without digging through menus.
*   **Standard Hotkeys:** Adopt standard EDA hotkeys (`R` for Resistor, `C` for Capacitor, `G` for Ground, `W` for Wire, `Ctrl+R` to rotate, `X`/`Y` to mirror).

## 3. SPICE Integration & Directives (Aligns with TODO.md)
*   **Rich Text Directive Editor:** Instead of plain text on the canvas, SPICE directives (`.tran`, `.op`, `.options`) should have a dedicated editor panel or rich-text canvas item with:
    *   **Syntax Highlighting:** Colorize commands, values, and comments.
    *   **Auto-complete:** Suggest analysis types, parameters, and available `.model`/`.subckt` names.
    *   **Inline Validation:** Red squiggly lines under malformed SPICE syntax (e.g., duplicate analysis cards as noted in TODO).
*   **Cross-Probing:** Seamless integration with the waveform viewer. Clicking a net on the schematic adds the trace to the graph; hovering a trace highlights the net.

## 4. Property Editing
*   **Contextual Inspector Dock:** A dockable property inspector that updates based on the selected item, reducing the need for modal popups.
*   **Inline Value Editing:** Double-clicking a component's value (e.g., `10k`) allows immediate text editing without opening a full component properties dialog.
*   **SI Prefix Parsing:** Input fields should natively understand and format SI prefixes (e.g., typing `4u7` or `4.7u` correctly parses to `4.7e-6`).

## 5. Visual Aesthetics & State
*   **Theme Support:** Full Dark/Light mode support respecting OS preferences, with high-contrast options for component symbols to ensure readability.
*   **Clear Selection States:** Distinct visual cues for Hovered (outline glow), Selected (solid highlight color), and Error (red outline for unconnected pins or missing models).

## 6. Subcircuits & Hierarchy
*   **Breadcrumb Navigation:** When descending into a `.subckt` block, show breadcrumbs at the top (e.g., `Main Schematic > Audio Amplifier > Pre-amp`).
*   **Symbol Generator:** A wizard to auto-generate a rectangular symbol with pins automatically placed based on a pasted `.subckt` text definition.
