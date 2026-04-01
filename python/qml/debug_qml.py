#!/usr/bin/env python3
"""
QML Debug Tool - Load and validate QML files independently.

Usage:
    python debug_qml.py <qml_file> [qml_file2 ...]
    
Example:
    python debug_qml.py GeminiRoot.qml
    python debug_qml.py *.qml
"""

import sys
import os
from pathlib import Path

# Add the venv python to path for imports
venv_python = Path(__file__).parent / "venv/bin/python"
if venv_python.exists():
    os.environ["PATH"] = f"{venv_python.parent}:{os.environ.get('PATH', '')}"

try:
    from PySide6.QtCore import QUrl, QObject, Signal, Slot, QCoreApplication
    from PySide6.QtQml import QQmlApplicationEngine, qmlRegisterType
    from PySide6.QtGui import QGuiApplication
    from PySide6.QtWidgets import QApplication
except ImportError as e:
    print(f"Error: PySide6 not available: {e}")
    print("Make sure you have PySide6 installed in your Python environment.")
    sys.exit(1)


class QMLDebugger(QObject):
    """QML Debugger to catch and report errors."""
    
    errors_caught = Signal(list)
    
    def __init__(self):
        super().__init__()
        self.errors = []
    
    @Slot(str, str, str, int, int)
    def log_warnings(self, category, message, file_path, line_number, column_number):
        error_msg = f"[{category}] {file_path}:{line_number}:{column_number} - {message}"
        self.errors.append(error_msg)
        print(error_msg)


def debug_qml_file(qml_path: str) -> bool:
    """
    Load a QML file and report any errors.
    
    Args:
        qml_path: Path to the QML file to validate
        
    Returns:
        True if the file loaded successfully, False otherwise
    """
    qml_path = Path(qml_path).resolve()
    
    if not qml_path.exists():
        print(f"Error: File not found: {qml_path}")
        return False
    
    print(f"\n{'='*60}")
    print(f"Debugging: {qml_path}")
    print(f"{'='*60}")
    
    # Create application instance
    app = QCoreApplication.instance()
    if app is None:
        app = QGuiApplication(sys.argv)
    
    # Create engine
    engine = QQmlApplicationEngine()
    
    # Set up error handling
    debugger = QMLDebugger()
    
    def handle_warnings(category, message, file_path, line_number, column_number):
        error_msg = f"[{category}] {file_path}:{line_number}:{column_number} - {message}"
        debugger.errors.append(error_msg)
        print(error_msg)
    
    # Connect to engine's warnings signal
    engine.warnings.connect(handle_warnings)
    
    # Load the QML file
    qml_url = QUrl.fromLocalFile(str(qml_path))
    engine.load(qml_url)
    
    # Process events to catch any async errors
    app.processEvents()
    
    # Check if loading succeeded
    success = bool(engine.rootObjects())
    
    if success and not debugger.errors:
        print(f"\n✓ QML file loaded successfully!")
    elif debugger.errors:
        print(f"\n✗ Found {len(debugger.errors)} error(s):")
        for error in debugger.errors:
            print(f"  {error}")
    else:
        print("\n✗ Failed to load QML file (no root objects created)")
    
    # Clean up
    engine.deleteLater()
    
    return success and len(debugger.errors) == 0


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        print("\nNo QML files specified.")
        sys.exit(1)
    
    qml_files = sys.argv[1:]
    
    print(f"QML Debug Tool")
    print(f"Checking {len(qml_files)} file(s)...")
    
    results = {}
    for qml_file in qml_files:
        results[qml_file] = debug_qml_file(qml_file)
    
    # Summary
    print(f"\n{'='*60}")
    print("Summary:")
    print(f"{'='*60}")
    
    all_passed = True
    for qml_file, passed in results.items():
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"  {status}: {qml_file}")
        if not passed:
            all_passed = False
    
    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()
