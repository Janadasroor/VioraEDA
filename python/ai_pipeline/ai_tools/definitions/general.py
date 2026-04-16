# General Purpose & Library tools
GENERAL_TOOLS = [
    {
        "name": "web_search",
        "description": "Performs a search for electronic components, datasheets, or technical specs. Use this when the user asks for part recommendations or generic data (e.g. 'Find a 5V LDO').",
        "parameters": {
            "type": "object",
            "properties": {
                "query": {"type": "string", "description": "The search query, e.g., 'low-noise rail-to-rail op-amp'"},
            },
            "required": ["query"],
        },
    },
    {
        "name": "lookup_component_data",
        "description": "Retrieves detailed technical data for a specific part number (pinout, operating voltage, key specs). Use this when the user provides a specific part number like 'LM317' or 'NE555'.",
        "parameters": {
            "type": "object",
            "properties": {
                "part_number": {"type": "string", "description": "The specific part number to lookup, e.g., 'LM7805'"},
            },
            "required": ["part_number"],
        },
    },
    {
        "name": "save_logic_template",
        "description": "Saves a custom generated Python logic script directly to the user's template library so they can load it smoothly into Smart Signal Blocks.",
        "parameters": {
            "type": "object",
            "properties": {
                "filename": {
                    "type": "string",
                    "description": "The filename to save as, e.g., '1khz_pwm.py' or 'fm_modulator.py'."
                },
                "code": {
                    "type": "string",
                    "description": "The complete Python source code for the logic block."
                }
            },
            "required": ["filename", "code"],
        },
    },
    {
        "name": "synthesize_subcircuit",
        "description": "Synthesizes a SPICE .subckt macro definition from natural language and saves it to a .sub file. Call this when the user asks you to create a custom component model, subcircuit, or macro (like a 555 timer or op-amp model).",
        "parameters": {
            "type": "object",
            "properties": {
                "name": {
                    "type": "string",
                    "description": "A short, descriptive name for the subcircuit (e.g. 'NE555' or 'LM7805')."
                },
                "description": {
                    "type": "string",
                    "description": "A brief description of the subcircuit."
                },
                "subcircuit_code": {
                    "type": "string",
                    "description": "The complete, raw SPICE text containing the .subckt definition, including the pins/nodes and internal components, ending with .ends."
                }
            },
            "required": ["name", "description", "subcircuit_code"],
        },
    },
]
