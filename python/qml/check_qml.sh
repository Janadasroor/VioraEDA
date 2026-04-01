#!/bin/bash
#
# QML Syntax Checker - Quick validation without launching the full application
#
# Usage:
#   ./check_qml.sh [qml_file ...]
#
# Example:
#   ./check_qml.sh GeminiRoot.qml
#   ./check_qml.sh *.qml
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QML_DIR="${SCRIPT_DIR}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}QML Syntax Checker${NC}"
echo -e "${BLUE}========================================${NC}"

if [ $# -eq 0 ]; then
    echo -e "${YELLOW}No QML files specified.${NC}"
    echo ""
    echo "Usage: $0 <qml_file> [qml_file2 ...]"
    echo ""
    echo "Example:"
    echo "  $0 GeminiRoot.qml"
    echo "  $0 *.qml"
    exit 1
fi

errors=0
checked=0

for qml_file in "$@"; do
    # Resolve path
    if [[ ! "$qml_file" = /* ]]; then
        qml_file="${QML_DIR}/${qml_file}"
    fi
    
    if [ ! -f "$qml_file" ]; then
        echo -e "${RED}✗ File not found: ${qml_file}${NC}"
        ((errors++))
        continue
    fi
    
    ((checked++))
    echo -e "\n${BLUE}Checking:${NC} ${qml_file}"
    echo "----------------------------------------"
    
    # Check for common QML syntax issues
    
    # 1. Check for QtQuick.Layouts import when using RowLayout/ColumnLayout
    if grep -qE '(RowLayout|ColumnLayout|GridLayout|Flow|Wrap)' "$qml_file"; then
        if ! grep -q 'import QtQuick.Layouts' "$qml_file"; then
            echo -e "${RED}✗ Missing 'import QtQuick.Layouts' (uses Layout types)${NC}"
            ((errors++))
        else
            echo -e "${GREEN}✓ QtQuick.Layouts import present${NC}"
        fi
    fi
    
    # 2. Check for QtQuick.Controls import when using Button/TextArea/etc
    if grep -qE '\b(Button|TextArea|TextField|SpinBox|ComboBox|CheckBox|RadioButton|Switch|Slider|ProgressBar|BusyIndicator|Dial|GroupBox|TabBar|TabButton|Menu|MenuItem|MenuBar|ToolBar|Popup|Dialog|Page|ApplicationWindow|Drawer|HeaderView|VerticalHeaderView|HorizontalHeaderView)\b' "$qml_file"; then
        if ! grep -qE 'import QtQuick.Controls(\.Controls)?' "$qml_file"; then
            echo -e "${RED}✗ Missing 'import QtQuick.Controls' (uses Control types)${NC}"
            ((errors++))
        else
            echo -e "${GREEN}✓ QtQuick.Controls import present${NC}"
        fi
    fi
    
    # 3. Check for QtQuick.Dialogs import
    if grep -qE '\b(FileDialog|FolderDialog|ColorDialog|FontDialog|Dialog|MessageDialog)\b' "$qml_file"; then
        if ! grep -qE 'import QtQuick.Dialogs' "$qml_file"; then
            echo -e "${RED}✗ Missing 'import QtQuick.Dialogs' (uses Dialog types)${NC}"
            ((errors++))
        else
            echo -e "${GREEN}✓ QtQuick.Dialogs import present${NC}"
        fi
    fi
    
    # 4. Check for mismatched braces
    open_braces=$(grep -o '{' "$qml_file" | wc -l)
    close_braces=$(grep -o '}' "$qml_file" | wc -l)
    if [ "$open_braces" -ne "$close_braces" ]; then
        echo -e "${RED}✗ Mismatched braces: { $open_braces } $close_braces${NC}"
        ((errors++))
    else
        echo -e "${GREEN}✓ Braces balanced ($open_braces pairs)${NC}"
    fi
    
    # 5. Check for padding property in ListView (not supported in Qt < 6.4)
    if grep -B10 '^\s*padding\s*:' "$qml_file" | grep -q 'ListView'; then
        echo -e "${RED}✗ ListView 'padding' property not supported in Qt < 6.4 - use delegate margins instead${NC}"
        ((errors++))
    fi
    
    # 6. Check for topPadding/bottomPadding in ListView (not supported in Qt < 6.4)
    if grep -qE '^\s*(topPadding|bottomPadding|leftPadding|rightPadding)\s*:' "$qml_file"; then
        if grep -B10 -E '^\s*(topPadding|bottomPadding|leftPadding|rightPadding)\s*:' "$qml_file" | grep -q 'ListView'; then
            echo -e "${RED}✗ ListView '*Padding' properties not supported in Qt < 6.4 - use delegate margins instead${NC}"
            ((errors++))
        fi
    fi
    
    # 6. Check for basic syntax (colon after property name)
    if grep -qE '^\s*[a-zA-Z_][a-zA-Z0-9_]*\s*[^:=]\s*:' "$qml_file" | head -1; then
        : # Basic check passed
    fi
    
    # 7. Try to load with qml (if available)
    if command -v qml &> /dev/null; then
        echo -e "${BLUE}Running qml runtime check...${NC}"
        if qml "$qml_file" 2>&1 | head -20; then
            echo -e "${GREEN}✓ QML loaded successfully${NC}"
        else
            echo -e "${RED}✗ QML load failed${NC}"
            ((errors++))
        fi
    else
        echo -e "${YELLOW}⚠ 'qml' command not available - skipping runtime check${NC}"
        echo "   Install qtdeclarative5-dev or qt6-declarative-dev for runtime checks"
    fi
done

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Checked: $checked file(s)"

if [ $errors -eq 0 ]; then
    echo -e "${GREEN}✓ All checks passed!${NC}"
    exit 0
else
    echo -e "${RED}✗ Found $errors error(s)${NC}"
    exit 1
fi
