# Layout subagent tools (Placement, Routing, Styling)
LAYOUT_TOOLS = [
    {
        "name": "generate_snippet",
        "description": "Generates a schematic sub-circuit (snippet) based on a functional request (e.g. 'Add a 5V regulator') or a repair request (e.g. 'Fix the floating pin on U1').",
        "parameters": {
            "type": "object",
            "properties": {
                "description": {
                    "type": "string",
                    "description": "Brief summary of what the snippet does (e.g. 'LM7805 5V Regulator Circuit').",
                },
                "items_json": {
                    "type": "string",
                    "description": "JSON string containing an array of schematic items (Resistor, IC, Wire, etc.). Each item MUST have: 'type', 'x', 'y', 'reference', 'value'. Origin (0,0) should be the center of the snippet.",
                },
            },
            "required": ["description", "items_json"],
        },
    },
    {
        "name": "execute_commands",
        "description": "Executes a batch of high-level schematic manipulation commands (addComponent, addWire, connect, removeComponent, setProperty, runERC, annotate). Use this to add multiple components and connect them properly.",
        "parameters": {
            "type": "object",
            "properties": {
                "commands_json": {
                    "type": "string",
                    "description": "JSON string containing an array of command objects. Example: [{'cmd': 'addComponent', 'type': 'Resistor', 'x': 0, 'y': 0, 'properties': {'value': '1k'}}, {'cmd': 'connect', 'ref1': 'R1', 'pin1': '1', 'ref2': 'U1', 'pin2': 'VCC'}]",
                },
            },
            "required": ["commands_json"],
        },
    },
    {
        "name": "execute_pcb_commands",
        "description": "Executes a batch of high-level PCB manipulation commands (addComponent, addTrace, addVia, removeComponent, setProperty, runDRC). Use this to place footprints and route traces.",
        "parameters": {
            "type": "object",
            "properties": {
                "commands_json": {
                    "type": "string",
                    "description": "JSON string containing an array of PCB command objects. Example: [{'cmd': 'addComponent', 'footprint': 'R_0805', 'x': 10, 'y': 10, 'reference': 'R1'}, {'cmd': 'addTrace', 'points': [{'x': 10, 'y': 10}, {'x': 20, 'y': 10}], 'width': 0.2, 'layer': 0}]",
                },
            },
            "required": ["commands_json"],
        },
    },
    {
        "name": "generate_schematic_from_netlist",
        "description": "Triggers the automatic generation of a complete electronic schematic by providing a standard SPICE netlist. Use this when the user asks you to design or generate a complete circuit (e.g., 'Make a boost converter circuit'). Ensure the netlist is valid SPICE.",
        "parameters": {
            "type": "object",
            "properties": {
                "netlist_text": {
                    "type": "string",
                    "description": "The raw SPICE netlist text string. Must contain standard SPICE primitives (R, L, C, Q, M, D, V, I, etc.).",
                },
            },
            "required": ["netlist_text"],
        },
    },
    {
        "name": "transfer_schematic_style",
        "description": "Applies a visual style transformation to the schematic OR provides intelligent style recommendations based on circuit analysis. Use this when the user wants to improve schematic appearance, fix layout issues, or match a specific standard. If no style is specified, automatically analyzes the circuit and recommends the optimal style.",
        "parameters": {
            "type": "object",
            "properties": {
                "style_preset": {
                    "type": "string",
                    "enum": ["ti", "adi", "ltspice", "iec", "military", "clean_modern", "vintage"],
                    "description": "Optional: The target style preset. If omitted, AI will analyze and recommend the best style."
                },
                "custom_instructions": {
                    "type": "string",
                    "description": "Optional custom styling instructions (e.g., 'increase component spacing', 'align all resistors horizontally')."
                }
            },
            "required": [],
        },
    },
]
