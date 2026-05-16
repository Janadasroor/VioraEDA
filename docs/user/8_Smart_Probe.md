# Smart Probe Overlay

The Smart Probe is a hover-activated information panel that displays real-time net information directly on the schematic canvas.

## Enabling

Toggle Smart Probe in **AI Assistant Settings** (Preferences > AI Assistant > AI Feature Toggles > Smart Probe Overlay). Requires a Gemini API key for AI annotations.

## Behavior

- **Hover** over a wire or net for ~600ms to activate the overlay.
- A translucent panel appears near the cursor showing:
  - **Net name**
  - **Instantaneous voltage** (DC operating point or final transient value)
  - **Connected nets** — other nets directly connected through components
  - **AI annotation** — If enabled, Gemini generates a brief functional description of the net's role in the circuit.
- The overlay follows the mouse with a debounce delay.
- Move away or wait ~250ms for the overlay to fade out.

## Without AI

If no Gemini API key is configured, the overlay still shows the net name, voltage value, and connected nets. AI annotation is omitted.

## Use Cases

- Quickly identify net voltages during DC bias analysis.
- Understand signal flow by hovering across nets.
- Get AI-powered explanations of circuit nodes during debugging.
