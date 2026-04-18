# Circuit Style Transfer Guide

## Overview

**Circuit Style Transfer** is an AI-powered feature that intelligently analyzes your schematic and applies visual transformations to improve clarity, consistency, and professional appearance. Using Google Gemini AI with context-aware analysis, this feature automatically detects layout issues and recommends optimal styling based on circuit type, density, and industry standards.

## ✨ Smart Features

### Context-Aware Analysis
The AI automatically analyzes your schematic to understand:
- **Circuit Type**: Power supply, analog, digital, RF, mixed-signal, etc.
- **Layout Density**: Sparse, moderate, or dense component placement
- **Complexity**: Simple (<10 components), moderate (10-50), or complex (50+)
- **Layout Issues**: Component overlaps, wire crossings, inconsistent alignment
- **Signal Flow**: Left-to-right, top-to-bottom, or mixed orientation

### Intelligent Recommendations
Instead of manual selection, the AI:
1. **Analyzes** your schematic context automatically
2. **Recommends** the optimal style based on circuit characteristics
3. **Shows** before/after quality metrics
4. **Explains** the reasoning behind the recommendation
5. **Applies** transformations with issue-specific fixes

## 🎨 Style Presets

Seven professionally-designed style presets are available:

| Preset | Name | Description |
|--------|------|-------------|
| `ti` | **Texas Instruments** | Clean, professional layout with horizontal components and generous spacing. Ideal for application notes and datasheets. |
| `adi` | **Analog Devices** | Compact layout with clear signal flow and detailed annotations. Optimized for analog circuit clarity. |
| `ltspice` | **LTspice Classic** | Traditional compact layout familiar to LTspice users. Maintains the classic SPICE aesthetic. |
| `iec` | **IEC European Standard** | Formal European standard with rectangular component symbols. Perfect for international documentation. |
| `military` | **MIL-STD Standard** | Conservative military standard with maximum clarity and spacing. Meets rigorous documentation requirements. |
| `clean_modern` | **Clean Modern** | Modern minimalist aesthetic with maximum spacing and clarity. Best for presentations and publications. |
| `vintage` | **Vintage Textbook** | Classic textbook style with traditional symbols and generous whitespace. Nostalgic and educational. |

### ✨ What Gets Transformed

The AI style transfer adjusts:

- **Component Spacing**: Optimal spacing between components based on style
- **Wire Routing**: Wire spacing and routing patterns
- **Font Sizes**: Text size for labels, values, and references
- **Alignment**: Component orientation (horizontal, vertical, mixed)
- **Signal Flow**: Direction of signal flow (left-to-right, top-to-bottom)
- **Visual Density**: Overall layout density and whitespace

## Usage

### Method 1: Natural Language (Recommended)

Simply describe what you want in the Gemini AI chat:

**General Improvement:**
```
Make this schematic look better
```
→ AI analyzes your circuit and recommends the optimal style

**Specific Style:**
```
Apply TI style to this circuit
```
→ AI applies Texas Instruments style directly

**Custom Requirements:**
```
Improve the layout with more spacing and horizontal alignment
```
→ AI recommends clean_modern style with custom adjustments

**Problem-Focused:**
```
This schematic is too crowded, fix it
```
→ AI detects high density and recommends clean_modern or military style

### Method 2: Programmatic

For advanced users, call the tool directly:

```python
# Get intelligent recommendation (no style specified)
result = transfer_schematic_style()

# Apply specific style
result = transfer_schematic_style(style_preset="ti", custom_instructions="increase spacing by 20%")
```

### How It Works

**Step 1: Context Analysis**
When you request style transfer, the AI:
- Loads all schematic components and wires
- Analyzes component types to determine circuit category
- Calculates layout density and bounding box
- Detects issues (overlaps, crossings, misalignment)
- Determines signal flow direction

**Step 2: Style Recommendation**
The AI scores each style preset based on:
- Circuit type match (e.g., analog → ADI style)
- Density appropriateness (dense → clean_modern)
- Issue resolution capability (overlaps → increased spacing)
- Complexity handling (complex → military/clean_modern)

**Step 3: Transformation**
Once confirmed, the AI:
- Generates per-component style commands
- Applies global style settings
- Adds issue-specific fix commands
- Calculates projected quality improvements

### Example Interactions

**Example 1: Automatic Recommendation**

```
You: "Improve this schematic's appearance"

AI: <ACTION>Analyzing schematic context...</ACTION>
    <ACTION>Calling transfer_schematic_style()...</ACTION>

📊 Analysis Complete

**Circuit Type:** Analog Signal Processing
**Complexity:** Moderate (23 components)
**Density:** Dense (18.5 components/10k px²)
**Issues Detected:** 4
  - 2 component overlaps (high severity)
  - 1 inconsistent alignment (low severity)
  - 1 high density warning (medium severity)

**Recommendation:** Clean Modern Style

**Why:** 
- Increased spacing will resolve component overlaps
- Modern layout ideal for complex analog circuits
- +25% quality improvement projected

**Projected Improvements:**
- +28% overall quality
- -3 layout issues resolved
- Improved component alignment
- Better component spacing

Apply Clean Modern style transformation?

You: "Yes"

AI: <ACTION>Applying Clean Modern style...</ACTION>
    <ACTION>Generating transformation commands...</ACTION>

✅ Style transfer complete! 23 components updated with Clean Modern styling.
```

**Example 2: Direct Style Application**

```
You: "Apply TI style like in application notes"

AI: <ACTION>Analyzing schematic for TI style compatibility...</ACTION>

✅ TI Style Analysis

**Circuit Match:** Excellent (power supply circuit)
**Components Affected:** 15
**Spacing:** 80px (horizontal alignment)
**Signal Flow:** Left-to-right

**TI Style Characteristics:**
- Clean, professional layout
- Horizontal component alignment
- Generous 80px spacing
- 12pt fonts for clarity

Apply transformation?

You: "Yes, apply it"

AI: <ACTION>Applying Texas Instruments style...</ACTION>

✅ TI style applied successfully!
```

## Technical Implementation

### Python Tool: `transfer_schematic_style()`

The core functionality is implemented in `/python/ai_pipeline/ai_tools/tools.py`:

```python
def transfer_schematic_style(self, style_preset, custom_instructions=""):
    """
    Applies a visual style transformation to the schematic.
    
    Args:
        style_preset: One of ["ti", "adi", "ltspice", "iec", "military", "clean_modern", "vintage"]
        custom_instructions: Optional custom styling instructions
    """
```

### C++ Integration

The feature is integrated into the schematic editor via:

- **Header**: `schematic/editor/schematic_editor.h`
  ```cpp
  void onApplyAIStyleTransfer(const QString& stylePreset, const QString& customInstructions = QString());
  ```

- **Implementation**: `schematic/editor/schematic_editor.cpp`

### QML UI Components

- **Composer**: `python/qml/Composer.qml` - Style transfer popup and buttons
- **Bridge**: `python/gemini_bridge.h` - Communication between C++ and QML

## Example Transformations

### Before → After (TI Style)

**Before** (Default):
- Compact component placement
- Mixed orientation
- Minimal spacing

**After** (TI Style):
- Horizontal component alignment
- 80px component spacing
- 40px wire spacing
- 12pt fonts
- Left-to-right signal flow

### Before → After (Clean Modern)

**Before** (Default):
- Traditional layout
- Standard spacing

**After** (Clean Modern):
- Maximum 100px component spacing
- 50px wire spacing
- 13pt large fonts
- Clean horizontal alignment
- Generous whitespace

## Advanced Usage

### Programmatic Usage

You can trigger style transfer programmatically via the AI chat:

```python
# In Gemini chat or script
gemini_bridge.sendMessage("Apply ti style transfer with custom spacing of 100px")
```

### Custom Style Definitions

To create custom style presets, modify the `style_configs` dictionary in `tools.py`:

```python
style_configs = {
    "my_custom_style": {
        "name": "My Custom Style",
        "component_spacing": 85,
        "wire_spacing": 42,
        "font_size": 12,
        "alignment": "horizontal",
        "signal_flow": "left_to_right",
        "description": "My personalized layout style"
    }
}
```

## Limitations

1. **AI-Driven**: The feature relies on Google Gemini AI, so an API key is required
2. **Incremental Changes**: Complex schematics may require multiple passes
3. **Manual Review**: Always review AI-suggested changes before applying
4. **Netlist Preservation**: Style transfer only affects visual layout, not electrical connectivity

## Troubleshooting

### "No schematic items found"
- Ensure a schematic is open and contains components
- Try exporting the schematic first: File → Export AI JSON

### "Unknown style preset"
- Check the preset name spelling
- Available presets: `ti`, `adi`, `ltspice`, `iec`, `military`, `clean_modern`, `vintage`

### Style transfer not applying
- Check Gemini AI connection and API key
- Verify the schematic is not locked or read-only
- Try a simpler style preset first (e.g., `ltspice`)

## Future Enhancements

Planned improvements:
- [ ] Real-time preview before applying
- [ ] Custom user-defined style presets
- [ ] Batch style transfer for multiple sheets
- [ ] Style comparison diff view
- [ ] Undo/redo integration for style changes
- [ ] Export/import style configurations
- [ ] Company-specific style templates

## Related Features

- **Layout Optimization**: `onOptimizeLayout()` - Algorithmic component placement
- **Orthogonal Routing**: `onApplyOrthogonalRouting()` - Wire routing cleanup
- **Theme System**: PCBTheme - Global UI theming
- **AI Snippet Generation**: Auto-generate circuit sub-sections

## Support

For issues or feature requests, please file a GitHub issue or consult the main README.md.

---

**Version**: 1.0  
**Added**: 2026-04-01  
**Author**: Viospice AI Team
