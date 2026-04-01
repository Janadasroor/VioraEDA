#!/bin/bash
#
# QML Debug Helper - Run QML checks from project root
#
# Usage:
#   ./scripts/debug-qml.sh [qml_file ...]
#
# Example:
#   ./scripts/debug-qml.sh GeminiRoot.qml
#   ./scripts/debug-qml.sh python/qml/*.qml
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
QML_CHECKER="${PROJECT_ROOT}/python/qml/check_qml.sh"

if [ ! -x "$QML_CHECKER" ]; then
    echo "Error: QML checker not found at: $QML_CHECKER"
    exit 1
fi

cd "$PROJECT_ROOT/python/qml"
exec "$QML_CHECKER" "$@"
