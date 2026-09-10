#!/usr/bin/env bash
# ==============================================================================
# uninstall_macos.sh — remove VioraEDA.app, CLI symlinks, and desktop integration
# Parity with tools/installer/linux/uninstall.sh
# Copyright 2026 Janada Sroor
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================

set -euo pipefail

C_RED=$'\033[31m'; C_GRN=$'\033[32m'; C_YLW=$'\033[33m'; C_CYN=$'\033[36m'; C_END=$'\033[0m'
info() { printf "${C_CYN}==>${C_END} %s\n" "$*"; }
ok()   { printf "${C_GRN}==>${C_END} %s\n" "$*"; }
warn() { printf "${C_YLW}!! ${C_END} %s\n" "$*" >&2; }

SILENT=0
while [ $# -gt 0 ]; do
    case "$1" in
        -y|--silent|--yes) SILENT=1; shift ;;
        -h|--help)
            echo "Usage: ./scripts/uninstall_macos.sh [-y|--silent]"
            exit 0 ;;
        *) shift ;;
    esac
done

APP_CANDIDATES=("/Applications/VioraEDA.app" "$HOME/Applications/VioraEDA.app")
CLI_DIRS=("/usr/local/bin" "$HOME/.local/bin")

if [ "$SILENT" -eq 0 ]; then
    echo "This will remove VioraEDA.app and its CLI symlinks. Continue? [y/N]"
    read -r -n 1 REPLY || true; echo ""
    case "$REPLY" in [Yy]*) ;; *) warn "Uninstallation cancelled."; exit 0 ;; esac
fi

info "[1/3] Removing CLI symlinks..."
for d in "${CLI_DIRS[@]}"; do
    for b in VioraEDA viora flux_runner flux-lsp vioavr; do
        rm -f "$d/$b" 2>/dev/null || true
    done
done

info "[2/3] Removing application bundle(s)..."
for app in "${APP_CANDIDATES[@]}"; do
    if [ -d "$app" ]; then
        rm -rf "$app" && ok "Removed $app"
    fi
done

info "[3/3] Clearing caches (LaunchServices re-registers on next install)..."
rm -rf "$HOME/Library/Caches/io.viora.VioraEDA" 2>/dev/null || true

ok "VioraEDA has been uninstalled. (~/ViospiceLib kept — delete manually to purge libraries.)"
