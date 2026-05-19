cmake_minimum_required(VERSION 3.16)

project(viospice VERSION 0.1 LANGUAGES CXX)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE) 
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) 

enable_testing()
include(CTest)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- External Dependencies ---
include(FetchContent)

# Fetch and configure Slang (SystemVerilog Frontend)
FetchContent_Declare(
    slang
    GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
    GIT_TAG        d9c9603ca6908d5b2e55e9b535fcdf28099789ad
)

# We need to ensure dependencies are handled
FetchContent_MakeAvailable(slang)
link_directories(${slang_BINARY_DIR}/lib)
# -----------------------------

find_package(Qt6 6.10.0 REQUIRED COMPONENTS Widgets PrintSupport Sql OpenGLWidgets Charts Svg Network Multimedia Concurrent Test Qml Quick QuickWidgets)
set(QT_VERSION_MAJOR 6)
find_package(Qt6 QUIET COMPONENTS Pdf PdfWidgets)
find_package(Qt6 QUIET COMPONENTS WebSockets)
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

set(VIOMATRIXC_DOWNLOAD_BASE "https://github.com/Janadasroor/VioMATRIXC/releases/download/v0.1.0/viomatrixc"
    CACHE STRING "URL prefix for prebuilt VioMATRIXC archive")

if(WIN32)
    set(VIOMATRIXC_ARCHIVE "viomatrixc-windows-x86_64.zip")
    set(VIOMATRIXC_LIB_NAME "libngspice.dll")
elseif(APPLE)
    set(VIOMATRIXC_ARCHIVE "viomatrixc-macos-arm64.tar.gz")
    set(VIOMATRIXC_LIB_NAME "libngspice.dylib")
else()
    set(VIOMATRIXC_ARCHIVE "viomatrixc-linux-x86_64.tar.gz")
    set(VIOMATRIXC_LIB_NAME "libngspice.so")
endif()
set(VIOMATRIXC_DOWNLOAD_URL "${VIOMATRIXC_DOWNLOAD_BASE}-${VIOMATRIXC_ARCHIVE}")

if(VIOSPICE_DEV_MODE)
    # --- Build VioMATRIXC from source (local repo or FetchContent) ---
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../VioMATRIXC/configure.ac")
        message(STATUS "VioMATRIXC: using local ../VioMATRIXC")
        set(viomatrixc_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../VioMATRIXC")
    else()
        FetchContent_Declare(
            viomatrixc
            GIT_REPOSITORY https://github.com/Janadasroor/VioMATRIXC.git
            GIT_TAG        main
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
        set(NGSPICE_LIB_NAME "libngspice.dll")
    else()
        set(NGSPICE_LIB_NAME "libngspice.so")
    endif()

    if(NOT EXISTS "${viomatrixc_SOURCE_DIR}/src/.libs/${NGSPICE_LIB_NAME}")
        message(STATUS "Building VioMATRIXC engine from source...")
        execute_process(
            COMMAND sh ./autogen.sh
            WORKING_DIRECTORY ${viomatrixc_SOURCE_DIR}
            RESULT_VARIABLE res
        )
        execute_process(
            COMMAND sh ./configure --with-ngshared --enable-cider --enable-vicompat --prefix=${CMAKE_BINARY_DIR}/viospice_engine
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
else()
    # --- Download prebuilt VioMATRIXC ---
    message(STATUS "Downloading prebuilt VioMATRIXC engine...")
    set(_vm_dir "${CMAKE_BINARY_DIR}/viomatrixc-prebuilt")
    set(_vm_archive "${_vm_dir}/${VIOMATRIXC_ARCHIVE}")
    file(DOWNLOAD "${VIOMATRIXC_DOWNLOAD_URL}"
         "${_vm_archive}" STATUS _dl_status)
    list(GET _dl_status 0 _dl_rc)
    if(_dl_rc EQUAL 0)
        file(ARCHIVE_EXTRACT INPUT "${_vm_archive}"
             DESTINATION "${_vm_dir}")
        file(REMOVE "${_vm_archive}")
    else()
        message(FATAL_ERROR "Failed to download VioMATRIXC from ${VIOMATRIXC_DOWNLOAD_URL}")
    endif()

    set(VIOSPICE_ENGINE_INCLUDE_DIRS "${_vm_dir}/include")
    set(VIOSPICE_PREFERRED_ENGINE_DIR "${_vm_dir}/lib")
    set(VIOSPICE_PREFERRED_ENGINE_LIB "${_vm_dir}/lib/${VIOMATRIXC_LIB_NAME}")

    # Copy .cm code models
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cm")
    file(GLOB _cm_files "${_vm_dir}/cm/*.cm")
    foreach(_cm ${_cm_files})
        file(COPY "${_cm}" DESTINATION "${CMAKE_BINARY_DIR}/cm")
    endforeach()

    message(STATUS "VIOSPICE ENGINE (prebuilt): ${VIOSPICE_PREFERRED_ENGINE_LIB}")
endif()

add_compile_definitions(HAVE_NGSPICE)
set(VIOSPICE_ENGINE_FOUND TRUE)
set(VIOSPICE_ENGINE_LIBRARIES "${VIOSPICE_PREFERRED_ENGINE_LIB}")

# --- Python Embedding Support (App) ---
find_package(Python3 3.10 COMPONENTS Interpreter Development QUIET)
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

    find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development)

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
set(FLUXSCRIPT_DOWNLOAD_BASE "https://github.com/Janadasroor/fluxscript/releases/download/v0.1.0/fluxscript"
    CACHE STRING "URL prefix for prebuilt FluxScript archive")

if(WIN32)
    set(FLUXSCRIPT_ARCHIVE "fluxscript-windows-x86_64.zip")
    set(FLUXSCRIPT_LIB_NAME "FluxScript.dll")
elseif(APPLE)
    set(FLUXSCRIPT_ARCHIVE "fluxscript-macos-arm64.tar.gz")
    set(FLUXSCRIPT_LIB_NAME "libFluxScript.dylib")
else()
    set(FLUXSCRIPT_ARCHIVE "fluxscript-linux-x86_64.tar.gz")
    set(FLUXSCRIPT_LIB_NAME "libFluxScript.so")
endif()
set(FLUXSCRIPT_DOWNLOAD_URL "${FLUXSCRIPT_DOWNLOAD_BASE}-${FLUXSCRIPT_ARCHIVE}")

set(FLUX_VIO_MATRIXC_LIB_DIR "${VIOSPICE_PREFERRED_ENGINE_DIR}" CACHE STRING "Path to VioMATRIXC libngspice directory" FORCE)

if(VIOSPICE_DEV_MODE)
    # --- Build FluxScript from source (local repo or FetchContent) ---
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../fluxscript/CMakeLists.txt")
        message(STATUS "FluxScript: using local ../fluxscript")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../fluxscript FluxScript)
        set(FLUXSCRIPT_LIB_DIR "${CMAKE_BINARY_DIR}/FluxScript")
    else()
        message(STATUS "FluxScript: fetching from GitHub")
        FetchContent_Declare(
            FluxScript
            GIT_REPOSITORY https://github.com/Janadasroor/fluxscript.git
            GIT_TAG        2717da0   # LLVM 21 compat + JIT fixes + eager compile cache
        )
        FetchContent_MakeAvailable(FluxScript)
        set(FLUXSCRIPT_LIB_DIR "${fluxscript_BINARY_DIR}")
    endif()
    add_compile_definitions(HAVE_FLUXSCRIPT FLUXSCRIPT_EXAMPLES_DIR="${fluxscript_SOURCE_DIR}/examples")
    message(STATUS "FluxScript (dev build): ${fluxscript_SOURCE_DIR}")

    # Locate LLVM shared library for bundling
    find_library(FLUX_LLVM_LIB LLVM LLVM-21 LLVM.so.21.1
        PATHS /usr/lib/llvm-21/lib /usr/lib/x86_64-linux-gnu
        NO_DEFAULT_PATH
    )
    if(FLUX_LLVM_LIB)
        message(STATUS "FluxScript LLVM lib: ${FLUX_LLVM_LIB}")
    else()
        message(WARNING "LLVM lib not found — FluxScript runtime will require system LLVM")
    endif()
else()
    # --- Download prebuilt FluxScript ---
    message(STATUS "Downloading prebuilt FluxScript...")
    set(_fs_dir "${CMAKE_BINARY_DIR}/fluxscript-prebuilt")
    set(_fs_archive "${_fs_dir}/${FLUXSCRIPT_ARCHIVE}")
    file(DOWNLOAD "${FLUXSCRIPT_DOWNLOAD_URL}"
         "${_fs_archive}" STATUS _dl_status)
    list(GET _dl_status 0 _dl_rc)
    if(_dl_rc EQUAL 0)
        file(ARCHIVE_EXTRACT INPUT "${_fs_archive}"
             DESTINATION "${_fs_dir}")
        file(REMOVE "${_fs_archive}")
    else()
        message(FATAL_ERROR "Failed to download FluxScript from ${FLUXSCRIPT_DOWNLOAD_URL}")
    endif()

    find_package(CURL REQUIRED)
    find_package(Eigen3 QUIET)

    # Eigen is needed by prebuilt FluxScript headers — bundle if not found
    if(NOT Eigen3_FOUND)
        message(STATUS "Eigen3 not found, downloading bundled Eigen...")
        include(FetchContent)
        FetchContent_Declare(
            Eigen3
            GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
            GIT_TAG        3.4.0
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(Eigen3)
    endif()

    include_directories(${EIGEN3_INCLUDE_DIR})

    # Create an imported shared library target (LLVM is bundled in the .so)
    add_library(FluxScript SHARED IMPORTED GLOBAL)
    set_target_properties(FluxScript PROPERTIES
        IMPORTED_LOCATION "${_fs_dir}/lib/${FLUXSCRIPT_LIB_NAME}"
        INTERFACE_INCLUDE_DIRECTORIES "${_fs_dir}/include"
    )
    target_link_libraries(FluxScript INTERFACE CURL::libcurl)

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

if(BUILD_TESTING)
    add_executable(test_spice_directive_netlist schematic/tests/test_spice_directive_netlist.cpp)
    set_target_properties(test_spice_directive_netlist PROPERTIES AUTOMOC ON)
    target_include_directories(test_spice_directive_netlist PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/schematic
        ${CMAKE_CURRENT_SOURCE_DIR}/schematic/items
    )
    target_link_libraries(test_spice_directive_netlist PRIVATE
        VioSchematic
        VioSymbols
        VioSimulator
        VioCore
        VioUI
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME schematic.spice_directive_netlist COMMAND test_spice_directive_netlist)

    # Design Rule Management System Tests
    add_executable(test_design_rule
        tests/test_design_rule.cpp
        tests/test_design_rule.h
    )
    set_target_properties(test_design_rule PROPERTIES AUTOMOC ON)
    target_link_libraries(test_design_rule PRIVATE
        VioCore
        VioSymbols
        VioSimulator
        FluxScript
        FluxScriptManager
        VioUI
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME core.design_rule COMMAND test_design_rule)
endif()

if(BUILD_TESTING AND VIOSPICE_ENABLE_TESTS)
    add_subdirectory(tests)
endif()

option(BUILD_VIOSPICE_CLI "Build the CLI tool" ON)
option(BUILD_FLUX_RUNNER "Build standalone flux_runner tool" ON)
if(BUILD_FLUX_RUNNER)
    qt_add_executable(flux_runner tools/flux_runner.cpp)
    target_include_directories(flux_runner PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(flux_runner PRIVATE "${VIOSPICE_PREFERRED_ENGINE_LIB}"
        -Wl,--start-group
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
        -Wl,--end-group
    )
    set_target_properties(flux_runner PROPERTIES
        BUILD_RPATH "\$ORIGIN;${VIOSPICE_PREFERRED_ENGINE_DIR};${FLUXSCRIPT_LIB_DIR}"
        INSTALL_RPATH "\$ORIGIN;${VIOSPICE_PREFERRED_ENGINE_DIR};${FLUXSCRIPT_LIB_DIR}"
        BUILD_WITH_INSTALL_RPATH TRUE
        BUILD_RPATH_USE_ORIGIN TRUE
    )
    if(VIOSPICE_DEV_MODE AND FLUX_LLVM_LIB)
        add_custom_command(TARGET flux_runner POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FLUX_LLVM_LIB}"
                "$<TARGET_FILE_DIR:flux_runner>/libLLVM.so.21.1"
            COMMENT "Copying libLLVM.so.21.1 next to flux_runner"
        )
    endif()
endif()

if(BUILD_VIOSPICE_CLI)
    # CLI tool
    qt_add_executable(viora
        cli/main.cpp
        cli/flux_command.cpp
        cli/extension_command.cpp
        resources/resources.qrc
    )

    # Guardrail:
    # These static module libraries reference each other cyclically (for example
    # VioSimulator -> VioSchematic and app/UI code -> simulator/core helpers).
    # Use raw linker group flags here rather than CMake LINK_GROUP because the
    # Qt target graph also brings in VioHeadless and CMake rejects that cycle
    # during generation.
    target_link_libraries(viora PRIVATE "${VIOSPICE_PREFERRED_ENGINE_LIB}"
        -Wl,--start-group
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
        -Wl,--end-group
    )

    if(VIOSPICE_ENGINE_FOUND)
        target_include_directories(viora PRIVATE ${VIOSPICE_ENGINE_INCLUDE_DIRS})
        # Guardrail:
        # Keep the preferred VioMATRIXC engine dir first in RPATH. If the
        # system libngspice resolves first, native code models load without the
        # VioSpice bridge symbols and SmartSignal A-devices fail at deck load.
        set_target_properties(viora PROPERTIES
            BUILD_RPATH "\$ORIGIN;${VIOSPICE_PREFERRED_ENGINE_DIR};${FLUXSCRIPT_LIB_DIR}"
            BUILD_RPATH_USE_ORIGIN TRUE
            INSTALL_RPATH "\$ORIGIN;${VIOSPICE_PREFERRED_ENGINE_DIR};${FLUXSCRIPT_LIB_DIR}"
        )
        if(VIOSPICE_DEV_MODE AND FLUX_LLVM_LIB)
            add_custom_command(TARGET viora POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${FLUX_LLVM_LIB}"
                    "$<TARGET_FILE_DIR:viora>/libLLVM.so.21.1"
                COMMENT "Copying libLLVM.so.21.1 next to viora"
            )
        endif()
    endif()

    vioraeda_configure_app_target(viora)
    
    # Automatically link viora CLI globally in user's local bin after every build
    add_custom_command(TARGET viora POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/.local/bin"
        COMMAND ${CMAKE_COMMAND} -E create_symlink "$<TARGET_FILE:viora>" "$ENV{HOME}/.local/bin/viora"
        COMMENT "Linking viora CLI to ~/.local/bin/viora"
    )
endif()

option(BUILD_VIOSPICE_APP "Build the desktop application" ON)
if(BUILD_VIOSPICE_APP)
    # Main executable
    qt_add_executable(VioraEDA
        main.cpp
        core/python/python_embed.cpp
        resources/resources.qrc
    )

    # Guardrail:
    # Keep the static module set inside a raw linker group. Plain left-to-right
    # archive resolution breaks this repo because the schematic/simulator/core/UI
    # layers have intentional cross-module references.
    target_link_libraries(VioraEDA PRIVATE "${VIOSPICE_PREFERRED_ENGINE_LIB}"
        -Wl,--start-group
        VioUI
        VioSchematic
        VioSimulator
        VioSymbols
        VioCore
        FluxScript
        FluxScriptManager
        VioFootprints
        VioraPCBCore
        -Wl,--end-group
    )


    set_target_properties(VioraEDA PROPERTIES
        BUILD_RPATH "\$ORIGIN;${VIOSPICE_PREFERRED_ENGINE_DIR};${FLUXSCRIPT_LIB_DIR}"
        INSTALL_RPATH "\$ORIGIN;${VIOSPICE_PREFERRED_ENGINE_DIR};${FLUXSCRIPT_LIB_DIR}"
        BUILD_WITH_INSTALL_RPATH TRUE
        BUILD_RPATH_USE_ORIGIN TRUE
    )
    if(VIOSPICE_DEV_MODE AND FLUX_LLVM_LIB)
        add_custom_command(TARGET VioraEDA POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FLUX_LLVM_LIB}"
                "$<TARGET_FILE_DIR:VioraEDA>/libLLVM.so.21.1"
            COMMENT "Copying libLLVM.so.21.1 next to VioraEDA"
        )
    endif()

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

    if(QT_VERSION_MAJOR EQUAL 6)
        qt_finalize_executable(VioraEDA)
    endif()
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
