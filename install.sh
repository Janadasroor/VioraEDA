#!/bin/bash

# VioSpice Installation Script for Linux
# Installs VioSpice to ~/.local/bin and integrates it into the system menu.

set -e

APP_NAME="viospice"
INSTALL_DIR="$HOME/.local/bin"
ICON_DIR="$HOME/.local/share/icons/hicolor/256x256/apps"
DESKTOP_DIR="$HOME/.local/share/applications"
LIB_DIR="$HOME/ViospiceLib"

echo "🚀 Installing VioSpice..."

# 1. Create Directories
mkdir -p "$INSTALL_DIR"
mkdir -p "$ICON_DIR"
mkdir -p "$DESKTOP_DIR"
mkdir -p "$LIB_DIR/sym"
mkdir -p "$LIB_DIR/sub"
mkdir -p "$LIB_DIR/cmp"
mkdir -p "$LIB_DIR/lib"

# 2. Copy Binary
if [ -f "./build-release/viospice" ]; then
    echo "📦 Copying Release binary..."
    rm -f "$INSTALL_DIR/viospice"
    cp -f "./build-release/viospice" "$INSTALL_DIR/viospice"
    chmod +x "$INSTALL_DIR/viospice"
else
    echo "❌ Error: Release binary not found in ./build-release/viospice. Please build the project first."
    exit 1
fi

# 3. Copy Icon
if [ -f "./resources/icons/logo_viospice.png" ]; then
    echo "🖼️  Installing application icon..."
    cp -f "./resources/icons/logo_viospice.png" "$ICON_DIR/viospice.png"
else
    echo "⚠️  Warning: Icon not found. App will use generic icon."
fi

# 4. Create Desktop Entry
echo "📝 Creating desktop entry..."
cat > "$DESKTOP_DIR/viospice.desktop" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=VioSpice
GenericName=AI-Assisted EDA & SPICE Simulator
Comment=Modern electronic design automation with Gemini AI integration
Exec=$INSTALL_DIR/viospice
Icon=viospice
Terminal=false
Categories=Development;Engineering;Electronics;
Keywords=spice;eda;schematic;pcb;ai;gemini;
StartupNotify=true
EOF

chmod +x "$DESKTOP_DIR/viospice.desktop"

# 5. Handle CLI Tool (viora)
if [ -f "./build-release/viora" ]; then
    echo "🛠️  Installing 'viora' CLI tool..."
    rm -f "$INSTALL_DIR/viora"
    cp -f "./build-release/viora" "$INSTALL_DIR/viora"
    chmod +x "$INSTALL_DIR/viora"
fi

echo "✅ VioSpice has been successfully installed!"
echo "📍 Binary: $INSTALL_DIR/viospice"
echo "📍 Desktop Entry: $DESKTOP_DIR/viospice.desktop"
echo ""
echo "You can now find VioSpice in your application menu."
echo "Note: Ensure $INSTALL_DIR is in your PATH to run 'viospice' or 'viora' from terminal."
