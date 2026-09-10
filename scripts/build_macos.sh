#!/usr/bin/env bash
# ==============================================================================
# VioraEDA macOS Easy One-Command Build Script for New Users
# ==============================================================================
# Automatically detects Apple Silicon / Intel Mac, sets up Homebrew toolchain,
# configures CMake, builds all targets, runs verification tests, and sets up
# the component library.
#
# Usage:
#   ./scripts/build_macos.sh           # Configure + Build + Test
#   ./scripts/build_macos.sh --clean   # Clean build from scratch
#   ./scripts/build_macos.sh --no-test # Build only, skip test suite
# ==============================================================================

set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# ANSI color codes
C_BLU=$'\033[1;34m'; C_GRN=$'\033[1;32m'; C_YLW=$'\033[1;33m'; C_RED=$'\033[1;31m'; C_END=$'\033[0m'
info() { printf "${C_BLU}==>${C_END} %s\n" "$*"; }
ok()   { printf "${C_GRN}==>${C_END} %s\n" "$*"; }
warn() { printf "${C_YLW}!! ${C_END} %s\n" "$*" >&2; }
die()  { printf "${C_RED}ERROR: %s${C_END}\n" "$*" >&2; exit 1; }

DO_CLEAN=0
RUN_TESTS=1
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
ARCH=$(uname -m)

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)     DO_CLEAN=1; shift ;;
        --no-test|--skip-tests) RUN_TESTS=0; shift ;;
        -j[0-9]*)    JOBS="${1#-j}"; shift ;;
        -j|--jobs)   JOBS="$2"; shift 2 ;;
        --jobs=*)    JOBS="${1#--jobs=}"; shift ;;
        --help|-h)
            cat <<HELP
Usage: ./scripts/build_macos.sh [options]

Options:
  --clean       Wipe the build directory and reconfigure from scratch
  --no-test     Skip running CTest verification suite after build
  -j, --jobs N  Number of parallel compilation jobs (default: $JOBS)
  -h, --help    Show this help message
HELP
            exit 0
            ;;
        *) die "Unknown option: $1 (run with --help for options)" ;;
    esac
done

echo "${C_BLU}====================================================${C_END}"
echo "${C_BLU}   VioraEDA macOS Automated Build & Setup Tool      ${C_END}"
echo "${C_BLU}   Architecture: $ARCH | CPU Cores: $JOBS            ${C_END}"
echo "${C_BLU}====================================================${C_END}"
echo

# ---------------------------------------------------------------------------
# 1) Homebrew & Toolchain Discovery
# ---------------------------------------------------------------------------
info "Checking macOS development toolchain..."

if ! command -v brew >/dev/null 2>&1; then
    die "Homebrew is required. Please install it from https://brew.sh and run this script again."
fi

export HOMEBREW_NO_REQUIRE_TAP_TRUST=1
export HOMEBREW_NO_AUTO_UPDATE=1
brew untap aws/tap 2>/dev/null || true

# Locate Bison
for bp in /usr/local/opt/bison/bin /opt/homebrew/opt/bison/bin; do
    if [ -d "$bp" ]; then
        export PATH="$bp:$PATH"
        break
    fi
done

# Locate LLVM
LLVM_DIR=""
for lp in /usr/local/opt/llvm@21 /opt/homebrew/opt/llvm@21 /usr/local/opt/llvm /opt/homebrew/opt/llvm /usr/local/opt/llvm@15 /opt/homebrew/opt/llvm@15; do
    if [ -d "$lp" ]; then
        export PATH="$lp/bin:$PATH"
        LLVM_DIR="$lp"
        break
    fi
done

# Locate Qt 6
QT_DIR=""
if [ -n "${Qt6_DIR:-}" ]; then
    if [ -d "$Qt6_DIR" ]; then
        QT_DIR="$(cd "$Qt6_DIR/../../.." 2>/dev/null && pwd || true)"
    fi
fi

if [ -z "$QT_DIR" ]; then
    for qp in ${RUNNER_TEMP:-/tmp}/Qt/6.*/macos* /Users/runner/work/_temp/Qt/6.*/macos* "$HOME/Qt/6.*/macos*" "$HOME/Qt/6.6.3/macos" /opt/homebrew/opt/qt@6 /opt/homebrew/opt/qt /usr/local/opt/qt@6 /usr/local/opt/qt; do
        if [ -d "$qp" ]; then
            QT_DIR="$qp"
            export Qt6_DIR="$qp/lib/cmake/Qt6"
            break
        fi
    done
fi

if [ -z "$QT_DIR" ]; then
    info "Installing Qt 6 from Homebrew..."
    brew install qt@6 2>/dev/null || true
    QT_DIR="$(brew --prefix qt@6 2>/dev/null || brew --prefix qt 2>/dev/null || true)"
    [ -n "$QT_DIR" ] && export Qt6_DIR="$QT_DIR/lib/cmake/Qt6"
fi

# ---------------------------------------------------------------------------
# 2) Component Library Discovery & Setup
# ---------------------------------------------------------------------------
LIB_DIR="$HOME/ViospiceLib"
if [ ! -d "$LIB_DIR/sym" ]; then
    if [ -d "$HOME/viora-libs" ]; then
        info "Linking component library from ~/viora-libs to ~/ViospiceLib..."
        mkdir -p "$LIB_DIR"
        cp -Rf "$HOME/viora-libs/." "$LIB_DIR/" 2>/dev/null || true
    fi
fi

# ---------------------------------------------------------------------------
# 3) CMake Configure
# ---------------------------------------------------------------------------
if [ "$DO_CLEAN" = "1" ]; then
    info "Cleaning build directory: $ROOT/build ..."
    rm -rf "$ROOT/build"
fi

CMAKE_ARGS=(
    -B "$ROOT/build"
    -S "$ROOT"
    -DCMAKE_BUILD_TYPE=Release
    -DVIOSPICE_BUILD_VIOMATRIXC=OFF
    -DVIOSPICE_BUILD_FLUXSCRIPT=ON
    -DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER
)

if [ -n "${Qt6_DIR:-}" ]; then
    CMAKE_ARGS+=(-DQt6_DIR="$Qt6_DIR")
fi

PREFIX_DIRS=()
[ -n "$QT_DIR" ] && PREFIX_DIRS+=("$QT_DIR")
[ -n "$LLVM_DIR" ] && PREFIX_DIRS+=("$LLVM_DIR")
ZSTD_PREFIX="$(brew --prefix zstd 2>/dev/null || true)"
[ -n "$ZSTD_PREFIX" ] && PREFIX_DIRS+=("$ZSTD_PREFIX")
# Eigen3 (header-only) for FluxScript AI surrogate module. Brew's Eigen 5 is
# not in CMake's default prefix path on Apple Silicon, so discover and forward
# it explicitly (mirrors top-level CMakeLists pre-seed).
EIGEN_PREFIX="$(brew --prefix eigen 2>/dev/null || true)"
if [ -z "$EIGEN_PREFIX" ] || [ ! -d "$EIGEN_PREFIX" ]; then
    info "Installing Eigen3 headers via Homebrew..."
    brew install eigen 2>/dev/null || true
    EIGEN_PREFIX="$(brew --prefix eigen 2>/dev/null || true)"
fi
EIGEN_INCLUDE=""
for cand in "$EIGEN_PREFIX/include/eigen3" "$EIGEN_PREFIX/include" /opt/homebrew/include/eigen3 /opt/homebrew/include /usr/local/include/eigen3 /usr/local/include; do
    if [ -f "$cand/Eigen/Dense" ]; then
        EIGEN_INCLUDE="$cand"
        break
    fi
done
if [ -n "$EIGEN_INCLUDE" ]; then
    info "Eigen3 headers: $EIGEN_INCLUDE"
    CMAKE_ARGS+=(-DEIGEN3_INCLUDE_DIR="$EIGEN_INCLUDE")
else
    warn "Eigen3 headers not found — FluxScript AI surrogate build may fail"
fi
[ -n "$EIGEN_PREFIX" ] && [ -d "$EIGEN_PREFIX" ] && PREFIX_DIRS+=("$EIGEN_PREFIX")

if [ "${#PREFIX_DIRS[@]}" -gt 0 ]; then
    CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="$(IFS=';'; echo "${PREFIX_DIRS[*]}")")
fi

BREW_PREFIX="$(brew --prefix 2>/dev/null || true)"
if [ -n "$BREW_PREFIX" ] && [ -d "$BREW_PREFIX/lib" ]; then
    CMAKE_ARGS+=(
        "-DCMAKE_EXE_LINKER_FLAGS=-L$BREW_PREFIX/lib"
        "-DCMAKE_SHARED_LINKER_FLAGS=-L$BREW_PREFIX/lib"
    )
fi

info "Configuring project with CMake..."
cmake "${CMAKE_ARGS[@]}"

# ---------------------------------------------------------------------------
# 4) Compilation
# ---------------------------------------------------------------------------
ok "Building VioraEDA.app, viora CLI, and flux_runner..."
if ! cmake --build "$ROOT/build" -j"$JOBS"; then
    warn "Compilation failed. Retrying with verbose output to identify cause..."
    cmake --build "$ROOT/build" -v -j1
fi

# Stage native dylibs
if [ -f "$ROOT/build/_deps/viomatrixc-src/src/.libs/libngspice.dylib" ]; then
    mkdir -p "$ROOT/build/viospice_engine/lib" "$ROOT/build/viomatrixc-prebuilt/lib"
    cp -f "$ROOT/build/_deps/viomatrixc-src/src/.libs/libngspice"* "$ROOT/build/viospice_engine/lib/" 2>/dev/null || true
    cp -f "$ROOT/build/_deps/viomatrixc-src/src/.libs/libngspice"* "$ROOT/build/viomatrixc-prebuilt/lib/" 2>/dev/null || true
fi
if [ -f "$ROOT/build/_deps/fluxscript-build/libFluxScript.dylib" ]; then
    mkdir -p "$ROOT/build/fluxscript-prebuilt/lib"
    cp -f "$ROOT/build/_deps/fluxscript-build/libFluxScript.dylib" "$ROOT/build/fluxscript-prebuilt/lib/" 2>/dev/null || true
fi

# ---------------------------------------------------------------------------
# 5) Verification Test Suite
# ---------------------------------------------------------------------------
if [ "$RUN_TESTS" = "1" ]; then
    info "Running verification tests..."
    set +e
    DYLD_LIBRARY_PATH="$ROOT/build:$ROOT/build/_deps/viomatrixc-src/src/.libs:$ROOT/build/_deps/fluxscript-build:${LLVM_DIR:-}/lib" \
    QT_QPA_PLATFORM="offscreen" \
    ctest --test-dir "$ROOT/build" --output-on-failure -E "daemon"
    CTEST_RC=$?
    set -e
    if [ "$CTEST_RC" = "0" ]; then
        ok "All tests passed cleanly!"
    else
        warn "Some tests reported issues (code $CTEST_RC). See output above."
    fi
fi

# ---------------------------------------------------------------------------
# 6) Completion
# ---------------------------------------------------------------------------
echo
ok "===================================================="
ok "  VioraEDA macOS Build Completed Successfully!      "
ok "===================================================="
echo
echo "To launch the GUI application:"
echo "   open $ROOT/build/VioraEDA.app"
echo
echo "To run the CLI tool:"
echo "   $ROOT/build/viora --help"
echo
echo "To install system-wide (/Applications & /usr/local/bin):"
echo "   ./install.sh"
echo
