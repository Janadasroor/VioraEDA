# Drawing Custom Waveforms

VioSpice features a powerful visual editor for creating piece-wise linear (PWL) signals. This guide explains how to use the advanced tools to design precise simulation waveforms.

## Interactive Drawing Modes

### 1. Sketch Mode (Hand-Draw)
The default mode allows you to draw freehand directly on the canvas. 
* **Fresh Start**: Simply click and drag to start a new waveform.
* **Incremental Drawing**: Hold **Ctrl** while clicking/dragging to **append** new points to the existing wave instead of clearing it.

### 2. Polyline Mode (High Precision)
Check the **Polyline** box in the toolbar for vector-style drawing.
* **Crosshair Cursor**: Provides exact pixel-perfect alignment.
* **Point-by-Point**: Click once for each node.
* **Rubber-Band Preview**: A dashed line shows you exactly where the next segment will fall before you click.
* **Finish**: Right-click to stop the preview of the current segment.

## Specialized Toolsets

### Logic Bitstream Generator
Instantly create digital sequences without drawing transitions.
1. Click **Bitstream**.
2. Enter a sequence of bits (e.g., `1011101`).
3. VioSpice automatically enables **Step Mode** and generates a perfectly timed digital pulse train.

### Pulse Generator
Generate standard SPICE pulses using a simple form.
* Define **Initial (V1)** and **Pulsed (V2)** levels.
* Set **Delay** and **Width** (relative to the period).
* The editor handles all coordinate math automatically.

## Accuracy & Simulation Stability

### High Fidelity (No Resampling)
By default, VioSpice uses your **exact drawn points** as SPICE nodes. 
* **Sharp Corners**: If you draw a sharp peak in Polyline mode, it remains sharp in the simulation.
* **Resample Option**: Check **Resample** if you want to force the waveform into a fixed number of evenly spaced points (useful for smoothing out hand-drawn jitters).

### Step Mode (Zero-Order Hold)
Essential for digital and switching signals. When active, all transitions are forced to be perfectly vertical. VioSpice automatically adds a hidden **1 nanosecond transition ramp** to ensure SPICE simulations remain stable and accurate without numerical noise.

## Transformations
* **Scale T/V**: Stretch or compress the waveform in time or voltage.
* **Shift T**: Slide the waveform forward or backward.
* **Reverse**: Flip the waveform horizontally.
* **Mirror V**: Flip the waveform vertically.
* **Smooth**: Run a three-point moving average filter to remove noise.
* **Noise**: Add random jitter to your signal for stress testing.
