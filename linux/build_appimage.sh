#!/bin/bash
set -e

# Configuration
APP_NAME="viospice"
BUILD_DIR="build-appimage"
APP_DIR="AppDir"
QT_PATH="/home/jnd/Qt/6.11.0/gcc_64"
LLVM_ROOT="/usr/lib/llvm-18"

echo "=== Starting AppImage build for ${APP_NAME} (Comprehensive Fixes) ==="

# 1. Clean and build the project
echo "--- Building project ---"
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# Note: Python and Nanobind are disabled for the portable AppImage to avoid bundling complex Python environments
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=${QT_PATH} \
      -DLLVM_DIR=${LLVM_ROOT}/lib/cmake/llvm \
      -DClang_DIR=${LLVM_ROOT}/lib/cmake/clang \
      -DVIOSPICE_ENABLE_NANOBIND=OFF \
      -DVIOSPICE_ENABLE_PYTHON=OFF \
      ..
make -j8
cd ..

# 2. Prepare AppDir
echo "--- Preparing AppDir ---"
rm -rf ${APP_DIR}

# Ensure tools are present and executable
chmod +x linuxdeploy appimagetool linuxdeploy-plugin-qt

# --- Workaround for broken SQL plugins in Qt SDK ---
# Several SQL plugins have missing dependencies in the official Qt build.
# We move them to a temporary backup location to prevent linuxdeploy from failing.
SQL_PLUGINS_DIR="${QT_PATH}/plugins/sqldrivers"
BACKUP_DIR="/home/jnd/qt_sql_bak"
mkdir -p ${BACKUP_DIR}

BROKEN_PLUGINS=("libqsqlmimer.so" "libqsqlibase.so" "libqsqlmysql.so" "libqsqloci.so")

for plugin in "${BROKEN_PLUGINS[@]}"; do
    if [ -f "${SQL_PLUGINS_DIR}/${plugin}" ]; then
        echo "Hiding broken plugin: ${plugin}"
        mv "${SQL_PLUGINS_DIR}/${plugin}" "${BACKUP_DIR}/"
    fi
done

cleanup_plugins() {
    echo "Restoring SQL plugins..."
    for plugin in "${BROKEN_PLUGINS[@]}"; do
        if [ -f "${BACKUP_DIR}/${plugin}" ]; then
            mv "${BACKUP_DIR}/${plugin}" "${SQL_PLUGINS_DIR}/"
        fi
    done
}
trap cleanup_plugins EXIT

# --- Set up environment ---
export PATH=".:${QT_PATH}/bin:$PATH"
export LD_LIBRARY_PATH="${QT_PATH}/lib:${LLVM_ROOT}/lib:$LD_LIBRARY_PATH"
export EXTRA_QT_PLUGINS="wayland;svg"
export QML_SOURCES_PATHS="python/qml" # Scan QML files for dependencies

# Ensure the icon matches the desktop file expectation
cp resources/icons/app_icon.svg resources/icons/viospice.svg

# Run linuxdeploy
./linuxdeploy --appdir ${APP_DIR} \
              --executable ${BUILD_DIR}/viospice \
              --desktop-file linux/viospice.desktop \
              --icon-file resources/icons/viospice.svg \
              --plugin qt

# 3. Manual Fixups
echo "--- Performing manual fixups ---"

# Copy ngspice code models
mkdir -p ${APP_DIR}/usr/bin/cm
cp ${BUILD_DIR}/cm/*.cm ${APP_DIR}/usr/bin/cm/

# Copy libngspice (sometimes missed by linuxdeploy due to internal loading)
mkdir -p ${APP_DIR}/usr/lib
cp ${BUILD_DIR}/_deps/viomatrixc-src/src/.libs/libngspice.so* ${APP_DIR}/usr/lib/

# Copy Python scripts and QML
echo "Bundling Python scripts..."
mkdir -p ${APP_DIR}/usr/python
cp -r python/* ${APP_DIR}/usr/python/

# Install Python dependencies into the AppDir for portability
# We use --target to put them in a dedicated directory
echo "Installing Python dependencies (Gemini support)..."
mkdir -p ${APP_DIR}/usr/python/site-packages
pip3 install --target ${APP_DIR}/usr/python/site-packages google-genai --quiet

# Copy project assets (resources)
echo "Bundling resources..."
mkdir -p ${APP_DIR}/usr/share/viospice/resources
cp -r resources/* ${APP_DIR}/usr/share/viospice/resources/

# Create a small wrapper script for AppRun if needed, or rely on qt.conf/environment
# We'll set PYTHONPATH in the environment via a custom AppRun or just let the app handle it.

# Regenerate AppImage with all bundled components
./appimagetool ${APP_DIR} viospice-x86_64.AppImage

echo "=== AppImage build complete ==="
ls -lh viospice-x86_64.AppImage || true
