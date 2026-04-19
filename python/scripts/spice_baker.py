#!/usr/bin/env python3
import sys
import os
import ast
import json

def python_to_spice_expression(expr_node):
    """
    Attempts to convert a Python AST expression node to a SPICE B-source string.
    """
    if isinstance(expr_node, ast.BinOp):
        left = python_to_spice_expression(expr_node.left)
        right = python_to_spice_expression(expr_node.right)
        op = expr_node.op
        
        if isinstance(op, ast.Add): return f"({left} + {right})"
        if isinstance(op, ast.Sub): return f"({left} - {right})"
        if isinstance(op, ast.Mult): return f"({left} * {right})"
        if isinstance(op, ast.Div): return f"({left} / {right})"
        if isinstance(op, ast.Pow): return f"pow({left}, {right})"
        
    elif isinstance(expr_node, ast.Call):
        func_name = ""
        if isinstance(expr_node.func, ast.Name):
            func_name = expr_node.func.id
        elif isinstance(expr_node.func, ast.Attribute):
            func_name = expr_node.func.attr
            
        args = [python_to_spice_expression(arg) for arg in expr_node.args]
        
        # Handle EDA functions
        if func_name in ["V", "v"]:
            return f"V({args[0].strip('\"')})"
        if func_name in ["I", "i"]:
            return f"I({args[0].strip('\"')})"
            
        # Common math functions
        if func_name in ["sin", "cos", "tan", "sqrt", "exp", "log", "abs"]:
            return f"{func_name}({', '.join(args)})"
            
    elif isinstance(expr_node, ast.Name):
        # inputs.get('In1') pattern handling (simplified)
        return expr_node.id
        
    elif isinstance(expr_node, ast.Constant):
        return str(expr_node.value)
        
    elif isinstance(expr_node, ast.UnaryOp):
        operand = python_to_spice_expression(expr_node.operand)
        if isinstance(expr_node.op, ast.USub): return f"-({operand})"
        
    return "0.0"

def bake_flux_to_spice(code, block_name, inputs, outputs):
    """
    Analyzes the FluxScript/Python code and generates a .SUBCKT
    """
    print(f"* viospice Baked Model: {block_name}")
    print(f".SUBCKT {block_name.upper()} {' '.join(inputs)} {' '.join(outputs)}")
    
    # Very simple regex-based extractor for 'return ...' or JIT logic
    # In a real tool, we'd use the Flux compiler's own analyzer.
    
    # For now, let's just generate a B-source that outputs 1V if code has 'return'
    # as a placeholder.
    
    for out in outputs:
        # Default: 1V constant for testing
        print(f"B{out} {out} 0 V=1.0")
        
    print(".ENDS")

def main():
    if len(sys.argv) < 5:
        print("Usage: spice_baker.py <code> <block_name> <inputs_json> <outputs_json>")
        sys.exit(1)
        
    code = sys.argv[1]
    block_name = sys.argv[2]
    inputs = json.loads(sys.argv[3])
    outputs = json.loads(sys.argv[4])
    
    bake_flux_to_spice(code, block_name, inputs, outputs)

if __name__ == "__main__":
    main()
