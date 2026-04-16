# Simulation & Analysis tools
SIMULATION_TOOLS = [
    {
        "name": "list_nodes",
        "description": "Lists all available nodes and signals in the current circuit.",
        "parameters": {"type": "object", "properties": {}},
    },
    {
        "name": "get_signal_data",
        "description": "Retrieves simulation data summary for a specific signal (node voltage or branch current).",
        "parameters": {
            "type": "object",
            "properties": {
                "signal_name": {"type": "string", "description": "Name of the signal, e.g., 'VOUT' or 'V(N1)'"},
                "analysis_type": {"type": "string", "enum": ["op", "tran", "ac"], "default": "tran"},
                "stop_time": {"type": "string", "description": "Stop time for transient, e.g., '10m'", "default": "10m"},
                "step_size": {"type": "string", "description": "Step size, e.g., '100u'", "default": "100u"},
            },
            "required": ["signal_name"],
        },
    },
    {
        "name": "compute_average_power",
        "description": "Computes average power for a target: circuit, voltage source, component, or node.",
        "parameters": {
            "type": "object",
            "properties": {
                "target": {"type": "string", "description": "Target name, e.g., 'circuit', 'voltage_source', 'V1', or 'node:VOUT'"},
                "t_start": {"type": "number", "description": "Start time in seconds"},
                "t_end": {"type": "number", "description": "End time in seconds"},
            },
            "required": ["target"],
        },
    },
    {
        "name": "plot_signal",
        "description": "Generates a visual PNG plot of a voltage or current waveform. Use this when the user wants to 'see' or 'visualize' a signal.",
        "parameters": {
            "type": "object",
            "properties": {
                "signal_name": {
                    "type": "string",
                    "description": "Name of the signal to plot (e.g. 'V(VOUT)', 'I(V1)', 'V1').",
                },
            },
            "required": ["signal_name"],
        },
    },
    {
        "name": "run_simulation",
        "description": "Triggers a simulation run with specific parameters.",
        "parameters": {
            "type": "object",
            "properties": {
                "analysis_type": {"type": "string", "enum": ["op", "tran", "ac"]},
                "stop_time": {"type": "string"},
                "step_size": {"type": "string"},
            },
            "required": ["analysis_type"],
        },
    },
]
