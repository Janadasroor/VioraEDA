#!/usr/bin/env bash
# ==============================================================================
# install_macos.sh — 1-Command Local Installer for macOS
# Installs VioraEDA.app to /Applications and links CLI binaries to /usr/local/bin
# Copyright 2026 Janada Sroor
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================

set -euo pipefail

C_RED=$'\033[31m'; C_GRN=$'\033[32m'; C_YLW=$'\033[33m'; C_CYN=$'\033[36m'; C_END=$'\033[0m'
info() { printf "${C_CYN}==>${C_END} %s\n" "$*"; }
ok()   { printf "${C_GRN}==>${C_END} %s\n" "$*"; }
warn() { printf "${C_YLW}!! ${C_END} %s\n" "$*" >&2; }
die()  { printf "${C_RED}ERROR: %s${C_END}\n" "$*" >&2; exit 1; }

SELF="${BASH_SOURCE[0]}"
ROOT="${SELF%/*}"; case "$ROOT" in /*) ;; *) ROOT="$(cd "$ROOT" && pwd)" ;; esac
ROOT="$(cd "$ROOT/.." && pwd)"

BUILD_DIR="$ROOT/build"
APP_SRC=""
PREFIX="/Applications"
SILENT=0

usage() {
    cat <<'EOF'
Usage: ./scripts/install_macos.sh [OPTIONS]

Options:
  --prefix=PATH    App install parent (default: /Applications, fallback ~/Applications)
  --prefix PATH    Same as above
  -y, --silent     Non-interactive (auto-confirm)
  -h, --help       This help
EOF
    exit 0
}

while [ $# -gt 0 ]; do
    case "$1" in
        --prefix=*) PREFIX="${1#*=}"; shift ;;
        --prefix) PREFIX="$2"; shift 2 ;;
        -y|--silent|--yes) SILENT=1; shift ;;
        -h|--help) usage ;;
        *) die "Unknown option: $1 (try --help)" ;;
    esac
done

# Find built .app (staged bundle preferred — it carries Frameworks/Resources)
if [ -d "$BUILD_DIR/stage_macos/VioraEDA.app" ]; then
    APP_SRC="$BUILD_DIR/stage_macos/VioraEDA.app"
elif [ -d "$BUILD_DIR/VioraEDA.app" ]; then
    APP_SRC="$BUILD_DIR/VioraEDA.app"
fi

if [ -z "$APP_SRC" ]; then
    info "VioraEDA.app not found. Building with scripts/build_macos.sh..."
    bash "$ROOT/scripts/build_macos.sh"
    APP_SRC="$BUILD_DIR/VioraEDA.app"
fi

[ -d "$APP_SRC" ] || die "Application bundle $APP_SRC not found."

# Target Application directory (parity with Linux --prefix/--silent)
TARGET_APP_DIR="$PREFIX"
if [ ! -w "$TARGET_APP_DIR" ]; then
    TARGET_APP_DIR="$HOME/Applications"
    mkdir -p "$TARGET_APP_DIR"
fi

if [ "$SILENT" -eq 0 ]; then
    info "Install VioraEDA.app to $TARGET_APP_DIR? [Y/n]"
    read -r -n 1 REPLY || true; echo ""
    case "$REPLY" in [Nn]*) die "Installation cancelled." ;; esac
fi

info "Installing VioraEDA.app to $TARGET_APP_DIR..."
rm -rf "$TARGET_APP_DIR/VioraEDA.app"
cp -Rf "$APP_SRC" "$TARGET_APP_DIR/"
# Drop uninstaller alongside the app (parity with Linux $PREFIX/uninstall.sh)
cp -f "$ROOT/scripts/uninstall_macos.sh" "$TARGET_APP_DIR/VioraEDA.app/Contents/Resources/uninstall-macos.sh" 2>/dev/null || true
ok "VioraEDA.app installed to $TARGET_APP_DIR/VioraEDA.app"

# Target CLI directory
CLI_DIR="/usr/local/bin"
if [ ! -w "$CLI_DIR" ]; then
    CLI_DIR="$HOME/.local/bin"
    mkdir -p "$CLI_DIR"
fi

info "Installing CLI binaries to $CLI_DIR..."
for bin_name in viora flux_runner flux-lsp vioavr; do
    if [ -f "$TARGET_APP_DIR/VioraEDA.app/Contents/MacOS/$bin_name" ]; then
        ln -sf "$TARGET_APP_DIR/VioraEDA.app/Contents/MacOS/$bin_name" "$CLI_DIR/$bin_name"
        ok "Linked $CLI_DIR/$bin_name -> $TARGET_APP_DIR/VioraEDA.app/Contents/MacOS/$bin_name"
    elif [ -f "$BUILD_DIR/$bin_name" ]; then
        cp -f "$BUILD_DIR/$bin_name" "$CLI_DIR/$bin_name"
        chmod +x "$CLI_DIR/$bin_name"
        ok "Installed $CLI_DIR/$bin_name"
    fi
done

# Initialize ViospiceLib
LIB_DIR="$HOME/ViospiceLib"
if [ ! -d "$LIB_DIR/sym" ]; then
    info "Initializing component library at $LIB_DIR..."
    mkdir -p "$LIB_DIR"
    if [ -d "$TARGET_APP_DIR/VioraEDA.app/Contents/Resources/ViospiceLib" ]; then
        cp -Rf "$TARGET_APP_DIR/VioraEDA.app/Contents/Resources/ViospiceLib/." "$LIB_DIR/" 2>/dev/null || true
    elif [ -d "$HOME/viora-libs" ]; then
        cp -Rf "$HOME/viora-libs/." "$LIB_DIR/" 2>/dev/null || true
    fi
    ok "Component library initialized."
fi

# Refresh LaunchServices so .flxsch/.flxpcb/.flux/.cir/.sp open with VioraEDA
if command -v lsregister >/dev/null 2>&1; then
    lsregister -f "$TARGET_APP_DIR/VioraEDA.app" 2>/dev/null || true
elif [ -x "/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister" ]; then
    "/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister" -f "$TARGET_APP_DIR/VioraEDA.app" 2>/dev/null || true
fi

ok "Installation complete! You can launch VioraEDA from Applications or run 'viora' in Terminal."
echo "Uninstaller: $TARGET_APP_DIR/VioraEDA.app/Contents/Resources/uninstall-macos.sh"
