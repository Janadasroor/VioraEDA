cmake_minimum_required(VERSION 3.18)

# CMP0111: don't require IMPORTED_IMPLIB for IMPORTED SHARED targets on Windows
# (MinGW can link .dll directly; MSVC uses the .lib specified per-target)
if(POLICY CMP0111)
    cmake_policy(SET CMP0111 OLD)
endif()

project(viospice VERSION 0.2.0 LANGUAGES C CXX)

# Disable third-party (Eigen) unit testing & Fortran BLAS/LAPACK generation globally
set(EIGEN_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_BLAS OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_LAPACK OFF CACHE BOOL "" FORCE)
set(EIGEN_LEAVE_TESTING_OFF ON CACHE BOOL "" FORCE)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE) 
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) 

# Size optimizations for Release builds: strip dead code and debug symbols
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-ffunction-sections HAS_FUNCTION_SECTIONS)
if(HAS_FUNCTION_SECTIONS)
    add_compile_options($<$<CONFIG:Release>:-ffunction-sections>)
    add_compile_options($<$<CONFIG:Release>:-fdata-sections>)
    if(APPLE)
        add_link_options($<$<CONFIG:Release>:-Wl,-dead_strip>)
        add_link_options($<$<CONFIG:Release>:-Wl,-S>)
    elseif(MINGW OR UNIX)
        add_link_options($<$<CONFIG:Release>:-Wl,--gc-sections>)
        add_link_options($<$<CONFIG:Release>:-Wl,-s>)
    endif()
endif()

enable_testing()
include(CTest)
set(EIGEN_BUILD_TESTING OFF CACHE BOOL "Disable Eigen unit testing" FORCE)
set(EIGEN_BUILD_DOC OFF CACHE BOOL "Disable Eigen documentation build" FORCE)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)

# Suppress verbose qDebug()/qInfo() output in Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_definitions(QT_NO_DEBUG_OUTPUT QT_NO_INFO_OUTPUT)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Treat missing return statements as compilation errors to prevent undefined behaviors/CPU traps
if(MSVC)
    add_compile_options(/we4715)
else()
    add_compile_options(-Werror=return-type)
endif()

# Linker group flags for circular static library dependencies.
# Only GNU ld (Linux) supports --start-group/--end-group.
# Apple ld64 and MSVC resolve circular deps differently.
if(MSVC OR APPLE)
    set(VIOSPICE_LINKER_GROUP_BEGIN "")
    set(VIOSPICE_LINKER_GROUP_END "")
else()
    set(VIOSPICE_LINKER_GROUP_BEGIN "-Wl,--start-group")
    set(VIOSPICE_LINKER_GROUP_END "-Wl,--end-group")
endif()

# --- External Dependencies ---
include(FetchContent)

option(VIOSPICE_ENABLE_SLANG "Enable SystemVerilog co-simulation via Slang" OFF)

if(VIOSPICE_ENABLE_SLANG)
    # Fetch and configure Slang (SystemVerilog Frontend)
    if(WIN32)
        set(SLANG_USE_MIMALLOC OFF CACHE BOOL "Disable mimalloc on Windows to prevent exit heap corruption" FORCE)
    endif()
    FetchContent_Declare(
        slang
        GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
        GIT_TAG        d9c9603ca6908d5b2e55e9b535fcdf28099789ad
    )

    # We need to ensure dependencies are handled
    FetchContent_MakeAvailable(slang)
    link_directories(${slang_BINARY_DIR}/lib)
endif()
# -----------------------------

find_package(Qt6 6.2.0 REQUIRED COMPONENTS Widgets PrintSupport Sql OpenGLWidgets Svg Network Concurrent Test Qml Quick QuickWidgets)
set(QT_VERSION_MAJOR 6)
find_package(Qt6 QUIET COMPONENTS Charts Multimedia Pdf PdfWidgets WebSockets)
if(TARGET Qt${QT_VERSION_MAJOR}::WebSockets)
    add_compile_definitions(VIOSPICE_HAS_QT_WEBSOCKETS=1)
else()
    add_compile_definitions(VIOSPICE_HAS_QT_WEBSOCKETS=0)
    message(WARNING "Qt WebSockets not found. Remote display streaming will be disabled.")
endif()
if(TARGET Qt${QT_VERSION_MAJOR}::Pdf AND TARGET Qt${QT_VERSION_MAJOR}::PdfWidgets)
    set(VIOSPICE_HAS_QT_PDF ON)
else()
    set(VIOSPICE_HAS_QT_PDF OFF)
    message(WARNING "Qt Pdf/PdfWidgets not found. PDF viewer will use fallback behavior.")
endif()

# --- VIOSPICE ENGINE Support ---
include(FetchContent)

# Developer mode flag — OFF by default so new users download prebuilt binaries
option(VIOSPICE_DEV_MODE "Enable local development mode (use local repos / build from source)" OFF)
# Build only the VioMATRIXC engine from source (CI uses this so the d_cosim
# code model carries the vio_cosim_abi_tag; the v0.1.0 prebuilt predates it).
option(VIOSPICE_BUILD_VIOMATRIXC "Build the VioMATRIXC ngspice engine from source" OFF)

# v0.2.0 (latest tag) ships no prebuilt binary assets, so the download base must
# stay at v0.1.0 for fresh users. Source builds below pin GIT_TAG v0.2.3, the
# GPL-free line: v0.2.1 drops the GPL A-device codemodels (modulator/
# samplehold/varistor) and fixes analog/modpath.lst so analog/cmextrn.h
# generates cleanly; v0.2.3 adds the Windows JIT symbol export fix so smart
# signals resolve ngSpice_RegisterJitLogic*/viospice_jit_lookup* from the DLL
# (bare visibility attrs are not PE exports).
set(VIOMATRIXC_DOWNLOAD_BASE "https://github.com/Janadasroor/VioMATRIXC/releases/download/v0.1.0"
    CACHE STRING "URL prefix for prebuilt VioMATRIXC archive")

if(WIN32)
    set(VIOMATRIXC_ARCHIVE "viomatrixc-windows-x86_64.zip")
    set(VIOMATRIXC_SHA256 "b5fe544d92cad0c7d700585232760f0929346034586648a34aa0262b230880bc")
    if(MINGW)
        set(VIOMATRIXC_LIB_DIR "bin")
        set(VIOMATRIXC_LIB_NAME "libngspice-0.dll")
    else()
        set(VIOMATRIXC_LIB_NAME "libngspice.dll")
    endif()
elseif(APPLE)
    set(VIOMATRIXC_ARCHIVE "viomatrixc-macos-arm64.tar.gz")
    set(VIOMATRIXC_SHA256 "9ba34d19a9975b8182a49d5115ae3a98ed2f8d905a2464bdca00b5d7587b1799")
    set(VIOMATRIXC_LIB_NAME "libngspice.dylib")
else()
    set(VIOMATRIXC_ARCHIVE "viomatrixc-linux-x86_64.tar.gz")
    set(VIOMATRIXC_SHA256 "7313accda47cc6cda4cbfbd0f4fba24cd291cc59f6f037e87b5e1f11524dfc86")
    set(VIOMATRIXC_LIB_NAME "libngspice.so")
endif()
set(VIOMATRIXC_DOWNLOAD_URL "${VIOMATRIXC_DOWNLOAD_BASE}/${VIOMATRIXC_ARCHIVE}")
if(VIOMATRIXC_SHA256)
    set(VIOMATRIXC_DOWNLOAD_HASH "SHA256=${VIOMATRIXC_SHA256}")
else()
    set(VIOMATRIXC_DOWNLOAD_HASH "")
endif()

if(VIOSPICE_DEV_MODE OR VIOSPICE_BUILD_VIOMATRIXC)
    # --- Build VioMATRIXC from source (local repo or FetchContent) ---
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../VioMATRIXC/configure.ac")
        message(STATUS "VioMATRIXC: using local ../VioMATRIXC")
        set(viomatrixc_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../VioMATRIXC")
    else()
        if(POLICY CMP0169)
            cmake_policy(SET CMP0169 OLD)
        endif()
        FetchContent_Declare(
            viomatrixc
            GIT_REPOSITORY https://github.com/Janadasroor/VioMATRIXC.git
            GIT_TAG        v0.2.3
        )
        FetchContent_GetProperties(viomatrixc)
        if(NOT viomatrixc_POPULATED)
            message(STATUS "Fetching VioMATRIXC engine from GitHub...")
            FetchContent_Populate(viomatrixc)
        endif()
    endif()

    if(APPLE)
        set(NGSPICE_LIB_NAME "libngspice.dylib")
    elseif(WIN32)
        if(MINGW)
            set(NGSPICE_LIB_NAME "libngspice-0.dll")
        else()
            set(NGSPICE_LIB_NAME "libngspice.dll")
        endif()
    else()
        set(NGSPICE_LIB_NAME "libngspice.so")
    endif()

    if(NOT EXISTS "${viomatrixc_SOURCE_DIR}/src/.libs/${NGSPICE_LIB_NAME}")
        message(STATUS "Building VioMATRIXC engine from source...")
        if(WIN32)
            # The cached MSYS2 image mangles aclocal's system path to
            # '/a/_temp/msys64/usr/share/aclocal/...', so aclocal reports even
            # real files (progtest.m4) as "does not exist". VioMATRIXC is
            # self-contained (libtool + ax macros in its own m4/, passed via
            # -I m4), so point --system-acdir at an empty dir to skip the broken
            # system dir. Autoconf also re-injects the default dir via the
            # ACLOCAL_PATH env var (see aclocal parse_ACLOCAL_PATH), so clear it.
            # autogen.sh honors $ACLOCAL_FLAGS.
            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/empty_aclocal")
            execute_process(
                COMMAND sh -c "ACLOCAL_PATH='' ACLOCAL_FLAGS='--system-acdir=${CMAKE_BINARY_DIR}/empty_aclocal' ./autogen.sh"
                WORKING_DIRECTORY ${viomatrixc_SOURCE_DIR}
                RESULT_VARIABLE res
            )
        else()
            execute_process(
                COMMAND sh ./autogen.sh
                WORKING_DIRECTORY ${viomatrixc_SOURCE_DIR}
                RESULT_VARIABLE res
            )
        endif()
        set(VIOMATRIXC_CONFIGURE_ARGS --with-ngshared --enable-cider --enable-vicompat --prefix=${CMAKE_BINARY_DIR}/viospice_engine)
        if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR APPLE OR WIN32)
            # ngspice's AC_OPENMP enables OpenMP for any compiler that accepts
            # -fopenmp. Clang without libomp (coverage job) fails to compile
            # omp.h, and a dlopen'd OpenMP engine segfaults at runtime on macOS
            # and hangs inside a simulation on Windows. Linux GCC (libgomp) is
            # unaffected, so keep OpenMP there.
            list(APPEND VIOMATRIXC_CONFIGURE_ARGS --enable-openmp=no)
        endif()
        execute_process(
            COMMAND sh ./configure ${VIOMATRIXC_CONFIGURE_ARGS}
            WORKING_DIRECTORY ${viomatrixc_SOURCE_DIR}
            RESULT_VARIABLE res
        )
        execute_process(
            COMMAND make -j8
            WORKING_DIRECTORY ${viomatrixc_SOURCE_DIR}
            RESULT_VARIABLE res
        )
        if(NOT res EQUAL 0)
            message(FATAL_ERROR "Failed to build VioMATRIXC!")
        endif()
    endif()

    set(VIOSPICE_ENGINE_INCLUDE_DIRS "${viomatrixc_SOURCE_DIR}/src/include")
    set(VIOSPICE_PREFERRED_ENGINE_DIR "${viomatrixc_SOURCE_DIR}/src/.libs")
    set(VIOSPICE_PREFERRED_ENGINE_LIB "${viomatrixc_SOURCE_DIR}/src/.libs/${NGSPICE_LIB_NAME}")

    # Copy .cm code models
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cm")
    set(CM_DIRS "analog" "digital" "spice2poly" "tlines" "xtradev" "xtraevt")
    foreach(DIR ${CM_DIRS})
        set(CM_FILE "${viomatrixc_SOURCE_DIR}/src/xspice/icm/${DIR}/${DIR}.cm")
        if(EXISTS "${CM_FILE}")
            file(COPY "${CM_FILE}" DESTINATION "${CMAKE_BINARY_DIR}/cm")
        endif()
    endforeach()

    message(STATUS "VIOSPICE ENGINE (dev build): ${VIOSPICE_PREFERRED_ENGINE_LIB}")

    # Stage the freshly built engine into the same layout the prebuilt download
    # produces (build/viomatrixc-prebuilt/<bin|lib>), so the runtime library
    # paths baked into the CI workflows (LD_LIBRARY_PATH / DYLD_LIBRARY_PATH at
    # viomatrixc-prebuilt/lib, and the Windows DLL copy at viomatrixc-prebuilt/bin)
    # keep resolving libngspice for test binaries that link it directly.
    if(WIN32)
        set(_engine_stage_dir "${CMAKE_BINARY_DIR}/viomatrixc-prebuilt/bin")
    else()
        set(_engine_stage_dir "${CMAKE_BINARY_DIR}/viomatrixc-prebuilt/lib")
    endif()
    file(MAKE_DIRECTORY "${_engine_stage_dir}")
    file(GLOB _engine_libs
         "${viomatrixc_SOURCE_DIR}/src/.libs/libngspice*.so*"
         "${viomatrixc_SOURCE_DIR}/src/.libs/libngspice*.dylib"
         "${viomatrixc_SOURCE_DIR}/src/.libs/libngspice*.dll")
    foreach(_engine_lib ${_engine_libs})
        file(COPY "${_engine_lib}" DESTINATION "${_engine_stage_dir}")
    endforeach()
    message(STATUS "VIOSPICE ENGINE staged: ${_engine_stage_dir}")
else()
    # --- Download prebuilt VioMATRIXC ---
    message(STATUS "Downloading prebuilt VioMATRIXC engine...")
    set(_vm_dir "${CMAKE_BINARY_DIR}/viomatrixc-prebuilt")
    set(_vm_archive "${_vm_dir}/${VIOMATRIXC_ARCHIVE}")
    file(DOWNLOAD "${VIOMATRIXC_DOWNLOAD_URL}"
         "${_vm_archive}" STATUS _dl_status
         EXPECTED_HASH "${VIOMATRIXC_DOWNLOAD_HASH}")
    list(GET _dl_status 0 _dl_rc)
    if(_dl_rc EQUAL 0)
        file(ARCHIVE_EXTRACT INPUT "${_vm_archive}"
             DESTINATION "${_vm_dir}")
        file(REMOVE "${_vm_archive}")
        if(EXISTS "${_vm_dir}/lib/libcurl.so.4")
            file(REMOVE "${_vm_dir}/lib/libcurl.so.4")
        endif()
        file(GLOB_RECURSE _vm_libnghttp2 "${_vm_dir}/lib/libnghttp2.*")
        if(_vm_libnghttp2)
            file(REMOVE ${_vm_libnghttp2})
            message(STATUS "Removed bundled libnghttp2 (ABI mismatch with system libcurl): ${_vm_libnghttp2}")
        endif()
        unset(_vm_libnghttp2)
    else()
        message(FATAL_ERROR "Failed to download VioMATRIXC from ${VIOMATRIXC_DOWNLOAD_URL}")
    endif()

    set(VIOSPICE_ENGINE_INCLUDE_DIRS "${_vm_dir}/include")
    if(NOT VIOMATRIXC_LIB_DIR)
        set(VIOMATRIXC_LIB_DIR "lib")
    endif()
    set(VIOSPICE_PREFERRED_ENGINE_DIR "${_vm_dir}/${VIOMATRIXC_LIB_DIR}")
    set(VIOSPICE_PREFERRED_ENGINE_LIB "${_vm_dir}/${VIOMATRIXC_LIB_DIR}/${VIOMATRIXC_LIB_NAME}")

    if(WIN32)
        file(GLOB_RECURSE _found_dll "${_vm_dir}/*.dll")
        if(_found_dll)
            list(GET _found_dll 0 _first_dll)
            get_filename_component(VIOSPICE_PREFERRED_ENGINE_DIR "${_first_dll}" DIRECTORY)
            set(VIOSPICE_PREFERRED_ENGINE_LIB "${_first_dll}")
            message(STATUS "Resolved Windows ngspice DLL: ${VIOSPICE_PREFERRED_ENGINE_LIB}")
        endif()
    endif()

    # Copy .cm code models
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cm")
    file(GLOB_RECURSE _cm_files "${_vm_dir}/*.cm")
    foreach(_cm ${_cm_files})
        file(COPY "${_cm}" DESTINATION "${CMAKE_BINARY_DIR}/cm")
    endforeach()

    message(STATUS "VIOSPICE ENGINE (prebuilt): ${VIOSPICE_PREFERRED_ENGINE_LIB}")
endif()

# --- VioAVR Co-Simulation Support ---
option(VIOSPICE_ENABLE_VIOAVR "Enable AVR microcontroller co-simulation via VioAVR" ON)
if(VIOSPICE_ENABLE_VIOAVR)
    set(VIOAVR_DOWNLOAD_BASE "https://github.com/Janadasroor/VioAVR/releases/download/v0.2.5"
        CACHE STRING "URL prefix for prebuilt VioAVR archive")

    if(WIN32)
        set(VIOAVR_ARCHIVE "vioavr-v0.2.5-windows-x64.zip")
        set(VIOAVR_SHA256 "b370ad578466f6229663c55422728ed83799e2cb2e155db4ed9a6129f8256d43")
        set(VIOAVR_COSIM_LIB_NAME "avr_cosim.dll")
    elseif(APPLE)
        set(VIOAVR_ARCHIVE "vioavr-v0.2.5-macos-arm64.tar.gz")
        set(VIOAVR_SHA256 "2f79a0afac03187553d25bf8508e4b27d514a1df5e87c14ef3d33f24b64b6d8c")
        set(VIOAVR_COSIM_LIB_NAME "libavr_cosim.dylib")
    else()
        set(VIOAVR_ARCHIVE "vioavr-v0.2.5-linux-x86_64.tar.gz")
        set(VIOAVR_SHA256 "a1c862b132035bedfba2d74f381f83078ed85ff7288e3a2cd0ecf1a8a3b5d400")
        set(VIOAVR_COSIM_LIB_NAME "libavr_cosim.so")
    endif()
    set(VIOAVR_DOWNLOAD_URL "${VIOAVR_DOWNLOAD_BASE}/${VIOAVR_ARCHIVE}")
    if(VIOAVR_SHA256)
        set(VIOAVR_DOWNLOAD_HASH "SHA256=${VIOAVR_SHA256}")
    else()
        set(VIOAVR_DOWNLOAD_HASH "")
    endif()

    if(VIOSPICE_DEV_MODE)
        # --- Build VioAVR from source (local repo or FetchContent) ---
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../VioAVR/CMakeLists.txt")
            message(STATUS "VioAVR: using local ../VioAVR")
            set(vioavr_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../VioAVR")
        else()
            FetchContent_Declare(
                vioavr
                GIT_REPOSITORY https://github.com/Janadasroor/VioAVR.git
                GIT_TAG        v0.2.5
            )
            FetchContent_GetProperties(vioavr)
            if(NOT vioavr_POPULATED)
                message(STATUS "Fetching VioAVR from GitHub...")
                if(POLICY CMP0169)
                    cmake_policy(SET CMP0169 OLD)
                endif()
                FetchContent_Populate(vioavr)
                if(POLICY CMP0169)
                    cmake_policy(SET CMP0169 NEW)
                endif()
            endif()
        endif()

        set(VIOSPAVR_INCLUDE_DIR "${vioavr_SOURCE_DIR}/include")

        # Copy viospice.cm to build/cm/
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cm")
        set(_vioavr_cm "${vioavr_SOURCE_DIR}/cosim/avr_analog_bridge/viospice.cm")
        if(EXISTS "${_vioavr_cm}")
            file(COPY "${_vioavr_cm}" DESTINATION "${CMAKE_BINARY_DIR}/cm")
        endif()

        # Find libavr_cosim.so
        find_library(_vioavr_cosim_FOUND avr_cosim
            PATHS "${vioavr_SOURCE_DIR}/build" "${vioavr_SOURCE_DIR}/cosim"
            NO_DEFAULT_PATH)
        if(_vioavr_cosim_FOUND)
            set(VIOSPAVR_COSIM_LIB "${_vioavr_cosim_FOUND}")
        endif()
        # Fallback: local VioAVR build or own build dir
        if(NOT VIOSPAVR_COSIM_LIB)
            foreach(_search "${CMAKE_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../VioAVR/build/cosim" "${CMAKE_CURRENT_SOURCE_DIR}/../VioAVR/build" "${CMAKE_CURRENT_SOURCE_DIR}/../VioAVR/cosim")
                if(EXISTS "${_search}/libavr_cosim.so")
                    set(VIOSPAVR_COSIM_LIB "${_search}/libavr_cosim.so")
                    break()
                endif()
            endforeach()
        endif()
        # Copy viospice.cm from local build if needed
        if(VIOSPAVR_COSIM_LIB AND NOT EXISTS "${CMAKE_BINARY_DIR}/cm/viospice.cm")
            foreach(_cm_search "${CMAKE_CURRENT_SOURCE_DIR}/../VioAVR/build/cosim/avr_adc_bridge.cm"
                               "${CMAKE_CURRENT_SOURCE_DIR}/../VioAVR/cosim/avr_analog_bridge/viospice.cm")
                if(EXISTS "${_cm_search}")
                    file(COPY "${_cm_search}" DESTINATION "${CMAKE_BINARY_DIR}/cm/viospice.cm")
                    break()
                endif()
            endforeach()
        endif()

        message(STATUS "VioAVR (dev): include=${VIOSPAVR_INCLUDE_DIR}")
    else()
        # --- Download prebuilt VioAVR ---
        message(STATUS "Downloading prebuilt VioAVR...")
        set(_va_dir "${CMAKE_BINARY_DIR}/vioavr-prebuilt")
        set(_va_archive "${_va_dir}/${VIOAVR_ARCHIVE}")

        if(NOT EXISTS "${_va_dir}/lib/${VIOAVR_COSIM_LIB_NAME}")
            file(DOWNLOAD "${VIOAVR_DOWNLOAD_URL}" "${_va_archive}" STATUS _dl_status
                 EXPECTED_HASH "${VIOAVR_DOWNLOAD_HASH}")
            list(GET _dl_status 0 _dl_rc)
            if(_dl_rc EQUAL 0)
                file(ARCHIVE_EXTRACT INPUT "${_va_archive}" DESTINATION "${_va_dir}")
                file(REMOVE "${_va_archive}")
            else()
                message(WARNING "Failed to download VioAVR from ${VIOAVR_DOWNLOAD_URL} — AVR co-simulation disabled")
                set(VIOSPICE_ENABLE_VIOAVR OFF)
            endif()
        endif()

        if(VIOSPICE_ENABLE_VIOAVR)
            set(VIOSPAVR_INCLUDE_DIR "${_va_dir}/include")
            set(VIOSPAVR_COSIM_LIB "${_va_dir}/lib/${VIOAVR_COSIM_LIB_NAME}")

            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cm")
            file(GLOB _vioavr_cm_files "${_va_dir}/lib/*.cm" "${_va_dir}/cm/*.cm")
            foreach(_cm ${_vioavr_cm_files})
                file(COPY "${_cm}" DESTINATION "${CMAKE_BINARY_DIR}/cm")
            endforeach()

            message(STATUS "VioAVR (prebuilt): ${VIOSPAVR_COSIM_LIB}")
        endif()
    endif()

        if(VIOSPICE_ENABLE_VIOAVR)
            add_compile_definitions(HAVE_VIOAVR)
            set(VIOSPAVR_FOUND TRUE)
            if(VIOSPAVR_COSIM_LIB)
                message(STATUS "VioAVR cosim library: ${VIOSPAVR_COSIM_LIB}")
                add_compile_definitions(VIOAVR_COSIM_PATH="${VIOSPAVR_COSIM_LIB}")
            else()
                message(WARNING "VioAVR cosim library NOT found — AVR co-simulation will use fallback path")
            endif()
        endif()
endif()

add_compile_definitions(HAVE_NGSPICE)
add_compile_definitions(VIOSPICE_CM_DIR="${CMAKE_BINARY_DIR}/cm")
set(VIOSPICE_ENGINE_FOUND TRUE)
set(VIOSPICE_ENGINE_LIBRARIES "${VIOSPICE_PREFERRED_ENGINE_LIB}")

# --- Python Embedding Support (App) ---
if(MINGW)
    set(Python3_USE_STATIC_LIBS TRUE)
endif()
find_package(Python3 3.8 COMPONENTS Interpreter Development QUIET)
if(Python3_FOUND)
    set(VIOSPICE_ENABLE_PYTHON ON)
    add_compile_definitions(HAVE_PYTHON)
    message(STATUS "Embedded Python enabled: ${Python3_INCLUDE_DIRS}")
else()
    message(STATUS "Embedded Python DISABLED (Python3 development files not found)")
endif()

# --- nanobind Python Bindings ---
option(VIOSPICE_ENABLE_NANOBIND "Build Python bindings via nanobind" ON)
if(VIOSPICE_ENABLE_NANOBIND)
    # Fetch and configure Nanobind (Python Bindings)
    FetchContent_Declare(
        nanobind
        GIT_REPOSITORY https://github.com/wjakob/nanobind.git
        GIT_TAG        v2.0.0 # Use a stable version
    )
    FetchContent_MakeAvailable(nanobind)

    find_package(Python 3.8 REQUIRED COMPONENTS Interpreter Development)

    # Headless solver library (zero Qt, shared by simulator and Python bindings)
    add_library(VioSolverCore STATIC
        simulator/core/sim_value_parser.cpp
        simulator/core/sim_results.h
        simulator/core/sim_netlist.h
        simulator/core/sim_netlist.cpp
        simulator/core/sim_math.h
        simulator/core/sim_expression.h
        simulator/core/sim_meas_evaluator.h
        simulator/core/sim_meas_evaluator.cpp
        simulator/core/sim_model_parser.h
        simulator/core/sim_model_parser.cpp
        simulator/core/raw_data_parser.h
        simulator/core/raw_data_parser.cpp
    )
    target_include_directories(VioSolverCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/simulator)
    set_target_properties(VioSolverCore PROPERTIES
        CXX_STANDARD 17
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_definitions(VioSolverCore PRIVATE VIO_SOLVER_HEADLESS)

    # Python binding module
    nanobind_add_module(vspice_core MODULE LTO NOMINSIZE
        bindings/bindings_core.cpp
    )
    target_include_directories(vspice_core PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/simulator
        ${CMAKE_CURRENT_SOURCE_DIR}/simulator/core
        ${CMAKE_CURRENT_SOURCE_DIR}/bindings
    )
    target_link_libraries(vspice_core PRIVATE VioSolverCore)
    # Output as "_core" inside the vspice/ Python package
    set_target_properties(vspice_core PROPERTIES
        OUTPUT_NAME "_core"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python/vspice"
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
    )
    message(STATUS "nanobind Python bindings enabled: output → ${CMAKE_BINARY_DIR}/python/vspice")
endif()
# -----------------------

# --- FluxScript Support ---
option(VIOSPICE_DOWNLOAD_FLUXSCRIPT "Download prebuilt FluxScript" OFF)
option(VIOSPICE_BUILD_FLUXSCRIPT "Build FluxScript from source" OFF)
if(APPLE)
    # macOS: v1.2.0 provides ARM64 prebuilt
    set(FLUXSCRIPT_DOWNLOAD_BASE "https://github.com/Janadasroor/fluxscript/releases/download/v1.2.0")
    set(FLUXSCRIPT_ARCHIVE "fluxscript-macos-arm64.tar.gz")
    set(FLUXSCRIPT_SHA256 "64440628be5da62a5440a070e1dfaa0a491bab8b618911789192d1a45a9bf411")
    set(FLUXSCRIPT_LIB_NAME "libFluxScript.dylib")
elseif(WIN32)
    # Windows: v1.2.0 provides x86_64 prebuilt (DLL + import lib)
    set(FLUXSCRIPT_DOWNLOAD_BASE "https://github.com/Janadasroor/fluxscript/releases/download/v1.2.0")
    set(FLUXSCRIPT_ARCHIVE "fluxscript-windows-x86_64.zip")
    set(FLUXSCRIPT_SHA256 "deb5d098e0fa7419da0d032aaf337f5c1dd629713f435f187866610d8d86d41f")
    set(FLUXSCRIPT_LIB_NAME "FluxScript.dll")
else()
    # Linux: v1.2.0 provides x86_64 prebuilt
    set(FLUXSCRIPT_DOWNLOAD_BASE "https://github.com/Janadasroor/fluxscript/releases/download/v1.2.0")
    set(FLUXSCRIPT_ARCHIVE "fluxscript-linux-x86_64.tar.gz")
    set(FLUXSCRIPT_SHA256 "693ebaee4e0a64e86d0224e79b9d9ff19d11e34f08f66a561886ec02f142946c")
    set(FLUXSCRIPT_LIB_NAME "libFluxScript.so")
endif()
set(FLUXSCRIPT_DOWNLOAD_URL "${FLUXSCRIPT_DOWNLOAD_BASE}/${FLUXSCRIPT_ARCHIVE}")
if(FLUXSCRIPT_SHA256)
    set(FLUXSCRIPT_DOWNLOAD_HASH "SHA256=${FLUXSCRIPT_SHA256}")
else()
    set(FLUXSCRIPT_DOWNLOAD_HASH "")
endif()

set(FLUX_VIO_MATRIXC_LIB_DIR "${VIOSPICE_PREFERRED_ENGINE_DIR}" CACHE STRING "Path to VioMATRIXC libngspice directory" FORCE)

if(VIOSPICE_DEV_MODE OR VIOSPICE_BUILD_FLUXSCRIPT)
    # Auto-discover LLVM_DIR across common system and brew paths if not explicitly set
    if(NOT LLVM_DIR)
        foreach(_llvm_candidate
            "/usr/lib/llvm-21/lib/cmake/llvm"
            "/usr/lib/llvm-20/lib/cmake/llvm"
            "/usr/lib/llvm-19/lib/cmake/llvm"
            "/usr/lib/llvm-18/lib/cmake/llvm"
            "/usr/lib/llvm-17/lib/cmake/llvm"
            "/opt/homebrew/opt/llvm/lib/cmake/llvm"
            "/usr/local/opt/llvm/lib/cmake/llvm"
            "/mingw64/lib/cmake/llvm"
        )
            if(EXISTS "${_llvm_candidate}")
                set(LLVM_DIR "${_llvm_candidate}" CACHE PATH "LLVM cmake directory" FORCE)
                message(STATUS "Auto-detected LLVM_DIR: ${LLVM_DIR}")
                break()
            endif()
        endforeach()
    endif()
    # --- Eigen3 for FluxScript AI surrogate module (header-only) ---
    # FluxScript does its own find_package(Eigen3), but brew's Eigen 5 on macOS
    # ARM lives in /opt/homebrew/opt/eigen which is not in CMake's default
    # prefix path, so pre-seed EIGEN3_INCLUDE_DIR here where we control
    # CMAKE_PREFIX_PATH and brew discovery.
    if(NOT EIGEN3_INCLUDE_DIR OR EIGEN3_INCLUDE_DIR STREQUAL "")
        find_package(Eigen3 QUIET)
        if(NOT Eigen3_FOUND)
            find_package(Eigen QUIET)
            if(Eigen_FOUND AND Eigen_INCLUDE_DIRS)
                set(EIGEN3_INCLUDE_DIR "${Eigen_INCLUDE_DIRS}" CACHE PATH "Eigen3 include dir (via Eigen v5 config)" FORCE)
            endif()
        elseif(Eigen3_INCLUDE_DIRS)
            set(EIGEN3_INCLUDE_DIR "${Eigen3_INCLUDE_DIRS}" CACHE PATH "Eigen3 include dir (via Eigen3 config)" FORCE)
        endif()
    endif()
    if(NOT EIGEN3_INCLUDE_DIR OR EIGEN3_INCLUDE_DIR STREQUAL "")
        foreach(_eigen_cand
            "/usr/include/eigen3"
            "/usr/local/include/eigen3"
            "/opt/homebrew/include/eigen3"
            "/opt/homebrew/include"
            "/usr/local/opt/eigen/include/eigen3"
            "/opt/homebrew/opt/eigen/include/eigen3"
            "/opt/homebrew/opt/eigen/include"
        )
            if(EXISTS "${_eigen_cand}/Eigen/Dense")
                set(EIGEN3_INCLUDE_DIR "${_eigen_cand}" CACHE PATH "Eigen3 include dir (path probe)" FORCE)
                break()
            endif()
        endforeach()
    endif()
    if((NOT EIGEN3_INCLUDE_DIR OR EIGEN3_INCLUDE_DIR STREQUAL "") AND APPLE)
        execute_process(COMMAND brew --prefix eigen
            OUTPUT_VARIABLE _brew_eigen_prefix
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_brew_eigen_prefix)
            foreach(_eigen_cand
                "${_brew_eigen_prefix}/include/eigen3"
                "${_brew_eigen_prefix}/include"
            )
                if(EXISTS "${_eigen_cand}/Eigen/Dense")
                    set(EIGEN3_INCLUDE_DIR "${_eigen_cand}" CACHE PATH "Eigen3 include dir (brew)" FORCE)
                    break()
                endif()
            endforeach()
            list(APPEND CMAKE_PREFIX_PATH "${_brew_eigen_prefix}")
        endif()
        unset(_brew_eigen_prefix)
    endif()
    if(EIGEN3_INCLUDE_DIR AND NOT EIGEN3_INCLUDE_DIR STREQUAL "")
        message(STATUS "Eigen3 (top-level pre-seed): ${EIGEN3_INCLUDE_DIR}")
        list(APPEND CMAKE_PREFIX_PATH "${EIGEN3_INCLUDE_DIR}")
    else()
        message(WARNING "Eigen3 not found at configure time — FluxScript AI surrogate may fail (install eigen / libeigen3-dev)")
    endif()
    # --- Build FluxScript from source (local repo or FetchContent) ---
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../fluxscript/CMakeLists.txt")
        message(STATUS "FluxScript: using local ../fluxscript")
        # EXCLUDE_FROM_ALL: only the FluxScript library (linked below) and any
        # targets we explicitly depend on are built; the ~19 FluxScript CLI tools
        # and validate_examples are skipped unless requested.
        # Point FluxScript at the ngspice shipped with VioMATRIXC (mirrors the
        # CMakeLists patch applied in the FetchContent branch below). Clearing
        # FLUX_VIO_MATRIXC_LIB_DIR lets FluxScript fall through to find_path/
        # find_library, which honor the pre-seeded cache vars.
        set(FLUX_VIO_MATRIXC_LIB_DIR "" CACHE STRING "Path to VioMATRIXC libngspice directory" FORCE)
        set(NGSPICE_INCLUDE_DIR "${VIOSPICE_ENGINE_INCLUDE_DIRS}" CACHE PATH "VioMATRIXC ngspice include dir" FORCE)
        set(NGSPICE_LIBRARY "${VIOSPICE_PREFERRED_ENGINE_LIB}" CACHE FILEPATH "VioMATRIXC ngspice library" FORCE)
        set(fluxscript_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../fluxscript")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../fluxscript FluxScript EXCLUDE_FROM_ALL)
        set(FLUXSCRIPT_LIB_DIR "${CMAKE_BINARY_DIR}/FluxScript")
    else()
        set(FLUX_VIO_MATRIXC_LIB_DIR "" CACHE STRING "Path to VioMATRIXC libngspice directory" FORCE)
        set(NGSPICE_INCLUDE_DIR "${VIOSPICE_ENGINE_INCLUDE_DIRS}" CACHE PATH "VioMATRIXC ngspice include dir" FORCE)
        set(NGSPICE_LIBRARY "${VIOSPICE_PREFERRED_ENGINE_LIB}" CACHE FILEPATH "VioMATRIXC ngspice library" FORCE)
        FetchContent_Declare(
            FluxScript
            GIT_REPOSITORY https://github.com/Janadasroor/fluxscript.git
            GIT_TAG        48cdf4402428282aaeff85a2c26b5caa3eec9cd1
        )
        FetchContent_MakeAvailable(FluxScript)
        set(FLUXSCRIPT_LIB_DIR "${fluxscript_BINARY_DIR}")
        # FluxScript's validate_examples/stdlib_bc custom targets run the
        # just-built `flux` binary at build time. On Linux/macOS the loader
        # cannot find the just-built libFluxScript.so/dylib without RPATH,
        # which broke CI (`flux: error while loading shared libraries`).
        # Like the local-checkout branch above (EXCLUDE_FROM_ALL), keep these
        # validation targets opt-in so the default build stays green; they can
        # still be built explicitly via `cmake --build --target validate_examples`.
        foreach(_flux_opt_target validate_examples stdlib_bc)
            if(TARGET ${_flux_opt_target})
                set_target_properties(${_flux_opt_target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
                message(STATUS "FluxScript: excluded ${_flux_opt_target} from default ALL build")
            endif()
        endforeach()
        # Belt-and-suspenders: give the FluxScript CLI tools an $ORIGIN /
        # @loader_path RPATH so manual `validate_examples` runs also resolve
        # the sibling shared library without LD_LIBRARY_PATH.
        foreach(_flux_tool flux flux-pkg flux-spice flux-run flux-doctor flux-sweep flux-fft flux-repl fluxc flux-format flux-doc flux-test flux-instrument flux-optimize flux-worstcase flux-stability flux-monte-carlo flux-lsp)
            if(TARGET ${_flux_tool})
                if(APPLE)
                    set_target_properties(${_flux_tool} PROPERTIES
                        BUILD_WITH_INSTALL_RPATH FALSE
                        INSTALL_RPATH "@loader_path"
                        BUILD_RPATH_USE_ORIGIN TRUE)
                elseif(UNIX)
                    set_target_properties(${_flux_tool} PROPERTIES
                        BUILD_WITH_INSTALL_RPATH FALSE
                        INSTALL_RPATH "$ORIGIN"
                        BUILD_RPATH_USE_ORIGIN TRUE)
                endif()
            endif()
        endforeach()
    endif()
    if(TARGET FluxScript AND TARGET viomatrixc_prebuilt)
        add_dependencies(FluxScript viomatrixc_prebuilt)
    endif()
    if(TARGET FluxScript AND TARGET ngspice_engine)
        add_dependencies(FluxScript ngspice_engine)
    endif()
    if(APPLE AND TARGET FluxScript)
        find_library(ZSTD_LIB NAMES zstd HINTS /opt/homebrew/lib /opt/homebrew/opt/zstd/lib /usr/local/lib)
        if(ZSTD_LIB)
            target_link_libraries(FluxScript PRIVATE "${ZSTD_LIB}")
            message(STATUS "FluxScript linked with macOS zstd: ${ZSTD_LIB}")
        endif()
    endif()
    add_compile_definitions(HAVE_FLUXSCRIPT FLUXSCRIPT_EXAMPLES_DIR="${fluxscript_SOURCE_DIR}/examples")
    message(STATUS "FluxScript (dev build): ${fluxscript_SOURCE_DIR}")
    # The IDE's FluxScript LSP client launches flux-lsp at runtime; keep it built
    # even though EXCLUDE_FROM_ALL above skips the other FluxScript CLI tools.
    if(TARGET flux-lsp)
        set(VIOSPICE_FLUX_LSP_TARGET "flux-lsp")
    endif()
else()
    # --- Download prebuilt FluxScript ---
    set(_fs_dir "${CMAKE_BINARY_DIR}/fluxscript-prebuilt")
    set(_fs_archive "${_fs_dir}/${FLUXSCRIPT_ARCHIVE}")
    if(NOT EXISTS "${_fs_dir}/lib/${FLUXSCRIPT_LIB_NAME}")
        message(STATUS "Downloading prebuilt FluxScript...")
        file(DOWNLOAD "${FLUXSCRIPT_DOWNLOAD_URL}"
             "${_fs_archive}" STATUS _dl_status
             EXPECTED_HASH "${FLUXSCRIPT_DOWNLOAD_HASH}")
        list(GET _dl_status 0 _dl_rc)
        if(_dl_rc EQUAL 0)
            file(ARCHIVE_EXTRACT INPUT "${_fs_archive}"
                 DESTINATION "${_fs_dir}")
            file(REMOVE "${_fs_archive}")
            # Remove bundled libxml2 and libcurl to avoid ICU/OpenSSL version mismatch with the system
            file(GLOB_RECURSE _fs_libxml2 "${_fs_dir}/lib/libxml2.*")
            file(GLOB_RECURSE _fs_libcurl "${_fs_dir}/lib/libcurl.*")
            file(GLOB_RECURSE _fs_libnghttp2 "${_fs_dir}/lib/libnghttp2.*")
            if(_fs_libxml2)
                file(REMOVE ${_fs_libxml2})
                message(STATUS "Removed bundled libxml2 (ICU version mismatch): ${_fs_libxml2}")
            endif()
            if(_fs_libcurl)
                file(REMOVE ${_fs_libcurl})
                message(STATUS "Removed bundled libcurl (OpenSSL version mismatch): ${_fs_libcurl}")
            endif()
            if(_fs_libnghttp2)
                file(REMOVE ${_fs_libnghttp2})
                message(STATUS "Removed bundled libnghttp2 (ABI mismatch with system libcurl): ${_fs_libnghttp2}")
            endif()
            unset(_fs_libxml2)
            unset(_fs_libcurl)
            unset(_fs_libnghttp2)
        else()
            message(FATAL_ERROR "Failed to download FluxScript from ${FLUXSCRIPT_DOWNLOAD_URL}")
        endif()
    endif()

    if(EXISTS "${_fs_dir}/include/flux/compiler/compiler_instance.h")
        file(READ "${_fs_dir}/include/flux/compiler/compiler_instance.h" _ci_hdr)
        if(NOT _ci_hdr MATCHES "unordered_set")
            string(REGEX REPLACE "(#include[ \t]*<string>[^\r\n]*)" "\\1\n#include <unordered_set>\n#include <unordered_map>" _ci_hdr "${_ci_hdr}")
            file(WRITE "${_fs_dir}/include/flux/compiler/compiler_instance.h" "${_ci_hdr}")
            message(STATUS "Patched compiler_instance.h with missing std::unordered_set header")
        endif()
    endif()

    find_package(CURL QUIET)

    # Create an imported shared library target (LLVM is bundled in the .so)
    add_library(FluxScript SHARED IMPORTED GLOBAL)
    set_target_properties(FluxScript PROPERTIES
        IMPORTED_LOCATION "${_fs_dir}/lib/${FLUXSCRIPT_LIB_NAME}"
        INTERFACE_INCLUDE_DIRECTORIES "${_fs_dir}/include"
    )
    if(WIN32)
        if(MINGW)
            # MinGW: use the DLL itself as import library (ld can link against .dll)
            set_target_properties(FluxScript PROPERTIES
                IMPORTED_IMPLIB "${_fs_dir}/lib/${FLUXSCRIPT_LIB_NAME}"
            )
        else()
            string(REPLACE ".dll" ".lib" _fs_implib_name "${FLUXSCRIPT_LIB_NAME}")
            set_target_properties(FluxScript PROPERTIES
                IMPORTED_IMPLIB "${_fs_dir}/lib/${_fs_implib_name}"
            )
        endif()
    endif()
    if(CURL_FOUND)
        target_link_libraries(FluxScript INTERFACE CURL::libcurl)
    else()
        message(STATUS "CURL not found — FluxScript HTTP support will be disabled")
    endif()

    # Prebuilt FluxScript headers reference LLVM headers. Find and expose them.
    find_package(LLVM QUIET)
    set(_llvm_hdr_found FALSE)
    if(LLVM_FOUND)
        target_include_directories(FluxScript INTERFACE ${LLVM_INCLUDE_DIRS})
        message(STATUS "LLVM headers found via cmake config: ${LLVM_INCLUDE_DIRS}")
        set(_llvm_hdr_found TRUE)
    else()
        file(GLOB _system_llvm_dirs "/usr/include/llvm*" "/usr/local/include/llvm*")
        foreach(_llvm_chk ${_system_llvm_dirs} "C:/Program Files/LLVM/include" "C:/Program Files (x86)/LLVM/include" "/usr/local/opt/llvm/include" "/usr/local/opt/llvm@15/include" "/opt/homebrew/opt/llvm/include" "/opt/homebrew/opt/llvm@15/include")
            if(EXISTS "${_llvm_chk}/llvm/Config/llvm-config.h" OR EXISTS "${_llvm_chk}/llvm/ExecutionEngine/Orc/LLJIT.h")
                target_include_directories(FluxScript INTERFACE "${_llvm_chk}")
                message(STATUS "LLVM headers found via path check: ${_llvm_chk}")
                set(_llvm_hdr_found TRUE)
                break()
            endif()
        endforeach()
    endif()

    if(NOT _llvm_hdr_found)
        set(_stub_dir "${CMAKE_BINARY_DIR}/llvm_stub/include")
        file(MAKE_DIRECTORY "${_stub_dir}/llvm/Config")
        file(WRITE "${_stub_dir}/llvm/Config/llvm-config.h"
            "#ifndef LLVM_CONFIG_H\n#define LLVM_CONFIG_H\n#define LLVM_VERSION_MAJOR 21\n#define LLVM_VERSION_MINOR 0\n#define LLVM_VERSION_PATCH 0\n#define LLVM_ENABLE_THREADS 1\n#define EnvPathSeparator ';'\n#endif\n"
        )
        target_include_directories(FluxScript INTERFACE "${_stub_dir}")
        message(STATUS "Created fallback LLVM header stub at ${_stub_dir}/llvm/Config/llvm-config.h")
    endif()

    # Link against the bundled (or system) LLVM runtime library so that
    # targets referencing LLVM symbols (e.g. VioCore JIT code) resolve them.
    # On MSVC a bare DLL cannot be linked (LNK1107) and the prebuilt FluxScript
    # zip ships no LLVM import library, so prefer the system LLVM .lib there.
    if(WIN32 AND NOT MINGW AND LLVM_FOUND)
        find_library(LLVM_LIB LLVM HINTS ${LLVM_LIBRARY_DIRS} ${LLVM_DIR})
        if(LLVM_LIB AND NOT TARGET FluxScript::LLVM)
            add_library(FluxScript::LLVM SHARED IMPORTED GLOBAL)
            set_target_properties(FluxScript::LLVM PROPERTIES
                IMPORTED_LOCATION "${LLVM_LIB}"
                IMPORTED_IMPLIB "${LLVM_LIB}"
            )
            target_link_libraries(FluxScript INTERFACE FluxScript::LLVM)
            message(STATUS "System LLVM library (MSVC): ${LLVM_LIB}")
        endif()
    endif()
    if(NOT TARGET FluxScript::LLVM AND NOT (WIN32 AND NOT MINGW))
        # MSVC: no bundled LLVM import lib exists and no source references llvm::,
        # so FluxScript::LLVM degrades to a no-op interface library below.
        file(GLOB_RECURSE _fs_llvm "${_fs_dir}/*LLVM*")
        if(_fs_llvm)
            list(GET _fs_llvm 0 _fs_llvm_path)
            if(NOT TARGET FluxScript::LLVM)
                add_library(FluxScript::LLVM SHARED IMPORTED GLOBAL)
                set_target_properties(FluxScript::LLVM PROPERTIES
                    IMPORTED_LOCATION "${_fs_llvm_path}"
                )
                if(WIN32 AND MINGW)
                    set_target_properties(FluxScript::LLVM PROPERTIES
                        IMPORTED_IMPLIB "${_fs_llvm_path}"
                    )
                endif()
                target_link_libraries(FluxScript INTERFACE FluxScript::LLVM)
                message(STATUS "Bundled LLVM library: ${_fs_llvm_path}")
            endif()
        elseif(LLVM_FOUND)
            find_library(LLVM_LIB LLVM HINTS ${LLVM_LIBRARY_DIRS} ${LLVM_DIR})
            if(LLVM_LIB AND NOT TARGET FluxScript::LLVM)
                add_library(FluxScript::LLVM SHARED IMPORTED GLOBAL)
                set_target_properties(FluxScript::LLVM PROPERTIES
                    IMPORTED_LOCATION "${LLVM_LIB}"
                )
                if(WIN32 AND MINGW)
                    set_target_properties(FluxScript::LLVM PROPERTIES
                        IMPORTED_IMPLIB "${LLVM_LIB}"
                    )
                endif()
                target_link_libraries(FluxScript INTERFACE FluxScript::LLVM)
                message(STATUS "System LLVM library: ${LLVM_LIB}")
            endif()
        endif()
    endif()
    if(NOT TARGET FluxScript::LLVM)
        add_library(FluxScript::LLVM INTERFACE IMPORTED GLOBAL)
        message(STATUS "No LLVM runtime library linked — FluxScript is self-contained")
    endif()
    unset(_fs_llvm)

    set(FLUXSCRIPT_LIB_DIR "${_fs_dir}/lib")

    set(fluxscript_SOURCE_DIR "${_fs_dir}")
    add_compile_definitions(HAVE_FLUXSCRIPT FLUXSCRIPT_EXAMPLES_DIR="${_fs_dir}/examples")
    message(STATUS "FluxScript (prebuilt shared): ${_fs_dir}/lib/${FLUXSCRIPT_LIB_NAME}")
endif()
# -----------------------

include(cmake/project_sources.cmake)
include(cmake/target_settings.cmake)

# Define FluxSchematicCore and FluxSchematicUI targets
add_subdirectory(schematic)
add_subdirectory(core)
add_subdirectory(symbols)
add_subdirectory(ui)
add_subdirectory(simulator)
add_subdirectory(python)
add_subdirectory(footprints)
add_subdirectory(pcb)
add_subdirectory(viora_ide)

option(VIOSPICE_ENABLE_TESTS "Enable tests" ON)
if(BUILD_TESTING AND VIOSPICE_ENABLE_TESTS)
    add_subdirectory(tests)

    # Convenience: run only fast tests
    add_custom_target(quick-test
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L fast
        COMMENT "Running fast unit/regression tests only"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

option(BUILD_VIOSPICE_CLI "Build the CLI tool" ON)
option(BUILD_FLUX_RUNNER "Build standalone flux_runner tool" ON)
if(BUILD_FLUX_RUNNER)
    qt_add_executable(flux_runner tools/flux_runner.cpp)
    target_include_directories(flux_runner PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(flux_runner PRIVATE "${VIOSPICE_PREFERRED_ENGINE_LIB}"
        ${VIOSPICE_LINKER_GROUP_BEGIN}
        VioSchematicUI
        VioSchematicCore
        VioSymbols
        VioCore
        VioSimulator
        FluxScript
        FluxScriptManager
        VioUI
        VioFootprints
        VioraPCBCore
        ${VIOSPICE_LINKER_GROUP_END}
    )
    vioraeda_setup_rpath(flux_runner)
    qt_finalize_executable(flux_runner)
endif()

if(BUILD_VIOSPICE_CLI)
    # CLI tool
    qt_add_executable(viora
        cli/main.cpp
        cli/cli_daemon.cpp
        cli/flux_command.cpp
        cli/extension_command.cpp
        cli/command_registry.cpp
        cli/commands/common.cpp
        cli/commands/schematic_commands.cpp
        cli/commands/netlist_commands.cpp
        cli/commands/symbol_commands.cpp
        cli/commands/footprint_commands.cpp
        cli/commands/pcb_commands.cpp
        cli/commands/plugin_commands.cpp
        cli/commands/ext_commands.cpp
        cli/commands/ext_test.cpp
        cli/commands/ext_preview.cpp
        cli/commands/ext_schema.cpp
        cli/commands/ext_watch.cpp
        cli/commands/ext_compose.cpp
        cli/commands/misc_commands.cpp
        resources/resources.qrc
    )
    set_target_properties(viora PROPERTIES
        WIN32_EXECUTABLE FALSE
    )


    # Guardrail:
    # These static module libraries reference each other cyclically (for example
    # VioSimulator -> VioSchematic and app/UI code -> simulator/core helpers).
    # Use raw linker group flags here rather than CMake LINK_GROUP because the
    # Qt target graph also brings in VioHeadless and CMake rejects that cycle
    # during generation.
    target_link_libraries(viora PRIVATE "${VIOSPICE_PREFERRED_ENGINE_LIB}"
        ${VIOSPICE_LINKER_GROUP_BEGIN}
        VioSchematicUI
        VioSchematicCore
        VioSymbols
        VioCore
        VioSimulator
        FluxScript
        FluxScriptManager
        VioUI
        VioVioraIDE
        VioFootprints
        VioraPCBCore
        ${VIOSPICE_LINKER_GROUP_END}
    )

    if(VIOSPICE_ENGINE_FOUND)
        target_include_directories(viora PRIVATE ${VIOSPICE_ENGINE_INCLUDE_DIRS})
        # Guardrail:
        # Keep the preferred VioMATRIXC engine dir first in RPATH. If the
        # system libngspice resolves first, native code models load without the
        # VioSpice bridge symbols and SmartSignal A-devices fail at deck load.
        vioraeda_setup_rpath(viora)
    endif()

    vioraeda_configure_app_target(viora)
    qt_finalize_executable(viora)
    
    # Automatically link viora CLI globally in user's local bin after every build
    if(NOT WIN32)
        add_custom_command(TARGET viora POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/.local/bin"
            COMMAND ${CMAKE_COMMAND} -E create_symlink "$<TARGET_FILE:viora>" "$ENV{HOME}/.local/bin/viora"
            COMMENT "Linking viora CLI to ~/.local/bin/viora"
        )
    endif()
endif()

option(BUILD_VIOSPICE_APP "Build the desktop application" ON)
if(BUILD_VIOSPICE_APP)
    # Main executable
    qt_add_executable(VioraEDA
        main.cpp
        core/python/python_embed.cpp
        resources/resources.qrc
    )
    if(WIN32)
        target_sources(VioraEDA PRIVATE resources/vioraeda.rc)
    endif()

    # Guardrail:
    # Keep the static module set inside a raw linker group. Plain left-to-right
    # archive resolution breaks this repo because the schematic/simulator/core/UI
    # layers have intentional cross-module references.
    target_link_libraries(VioraEDA PRIVATE "${VIOSPICE_PREFERRED_ENGINE_LIB}"
        ${VIOSPICE_LINKER_GROUP_BEGIN}
        VioUI
        VioSchematic
        VioSimulator
        VioSymbols
        VioCore
        VioVioraIDE
        FluxScript
        FluxScriptManager
        VioFootprints
        VioraPCBCore
        ${VIOSPICE_LINKER_GROUP_END}
    )

    # Link embedded Python runtime (Phase 4)
    if(Python3_FOUND)
        target_link_libraries(VioraEDA PRIVATE Python3::Python)
        target_include_directories(VioraEDA PRIVATE ${Python3_INCLUDE_DIRS})
    endif()

    vioraeda_configure_app_target(VioraEDA)

    # Disable AUTOUIC for files that include headers without .ui files
    set_source_files_properties(main.cpp PROPERTIES SKIP_AUTOUIC ON)

    set_target_properties(VioraEDA PROPERTIES
        WIN32_EXECUTABLE TRUE
    )

    if(APPLE)
        set_target_properties(VioraEDA PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "VioraEDA"
            MACOSX_BUNDLE_GUI_IDENTIFIER "io.viora.VioraEDA"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/installer/macos/Info.plist.in"
            MACOSX_BUNDLE_ICON_FILE "vioraeda.icns"
        )
        set_source_files_properties(resources/installer/macos/vioraeda.icns PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )
        target_sources(VioraEDA PRIVATE resources/installer/macos/vioraeda.icns)
    endif()

    if(QT_VERSION_MAJOR EQUAL 6)
        qt_finalize_executable(VioraEDA)
    endif()

    if(VIOSPICE_FLUX_LSP_TARGET)
        add_dependencies(VioraEDA ${VIOSPICE_FLUX_LSP_TARGET})
    endif()

    # RPATH must be set after qt_finalize_executable to avoid being overridden
    vioraeda_setup_rpath(VioraEDA)
endif()

# Semantic merge tool (for Git integration)
add_executable(viospice-merge
    tools/viospice_merge.cpp
)
target_link_libraries(viospice-merge PRIVATE
    VioCore
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)
set_target_properties(viospice-merge PROPERTIES
    CXX_STANDARD 17
)

# Standalone Official Qt Setup Installer Framework
add_executable(VioraEDA_Setup WIN32
    tools/installer/main.cpp
    tools/installer/installer_types.h
    tools/installer/installer_window.h
    tools/installer/installer_window.cpp
    tools/installer/installer_worker.h
    tools/installer/installer_worker.cpp
    ${RESOURCES_RCC}
)
target_link_libraries(VioraEDA_Setup PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Core
)
if(WIN32)
    target_link_libraries(VioraEDA_Setup PRIVATE ole32 shell32 advapi32 dwmapi)
endif()


# --- CPack Packaging ---
include(cmake/packaging.cmake)

