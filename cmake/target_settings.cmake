set(VIORAEDA_COMMON_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/core/project
    ${CMAKE_SOURCE_DIR}/core/simulation
    ${CMAKE_SOURCE_DIR}/core/design_rules
    ${CMAKE_SOURCE_DIR}/core/sync
    ${CMAKE_SOURCE_DIR}/core/visuals
    ${CMAKE_SOURCE_DIR}/core/python
    ${CMAKE_SOURCE_DIR}/schematic/analysis
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/symbols
    ${CMAKE_SOURCE_DIR}/schematic
    ${CMAKE_SOURCE_DIR}/schematic/editor
    ${CMAKE_SOURCE_DIR}/schematic/ui
    ${CMAKE_SOURCE_DIR}/schematic/dialogs
    ${CMAKE_SOURCE_DIR}/schematic/items
    ${CMAKE_SOURCE_DIR}/schematic/tools
    ${CMAKE_SOURCE_DIR}/schematic/io
    ${CMAKE_SOURCE_DIR}/schematic/analysis
    ${CMAKE_SOURCE_DIR}/schematic/factories
    ${CMAKE_SOURCE_DIR}/ui
    ${CMAKE_SOURCE_DIR}/python
    ${CMAKE_SOURCE_DIR}/simulator
    ${CMAKE_SOURCE_DIR}/footprints
    ${CMAKE_SOURCE_DIR}/pcb
    ${CMAKE_SOURCE_DIR}/pcb/editor
    ${CMAKE_SOURCE_DIR}/pcb/items
    ${CMAKE_SOURCE_DIR}/pcb/tools
    ${CMAKE_SOURCE_DIR}/pcb/models
    ${CMAKE_SOURCE_DIR}/pcb/analysis
    ${CMAKE_SOURCE_DIR}/pcb/ui
    ${CMAKE_SOURCE_DIR}/pcb/dialogs
)
if(fluxscript_SOURCE_DIR)
    list(APPEND VIORAEDA_COMMON_INCLUDE_DIRS ${fluxscript_SOURCE_DIR}/include)
endif()
list(REMOVE_DUPLICATES VIORAEDA_COMMON_INCLUDE_DIRS)

set(VIORAEDA_QT_LINK_LIBS
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickWidgets
)

if(TARGET Qt${QT_VERSION_MAJOR}::WebSockets)
    list(APPEND VIORAEDA_QT_LINK_LIBS Qt${QT_VERSION_MAJOR}::WebSockets)
endif()

set(VIORAEDA_APP_LINK_LIBS
    VioSymbols
    VioSchematicCore
    VioSchematicUI
    VioUI
    FluxScript
    VioFootprints
    VioraPCBCore
    VioCore
    ${VIORAEDA_QT_LINK_LIBS}
)

set(VIORAEDA_CLI_LINK_LIBS
    VioSymbols
    VioSchematicCore
    VioSchematicUI
    VioUI
    FluxScript
    VioFootprints
    VioraPCBCore
    VioCore
    ${VIORAEDA_QT_LINK_LIBS}
)

set(VIORAEDA_PCH_HEADER "${CMAKE_SOURCE_DIR}/cmake/vioraeda_pch.h")

function(vioraeda_configure_module_target target)
    target_include_directories(${target}
        PUBLIC 
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE 
            ${VIORAEDA_COMMON_INCLUDE_DIRS}
    )
    target_link_libraries(${target} PRIVATE ${VIORAEDA_QT_LINK_LIBS} FluxScript)
    if(VIORAEDA_ENABLE_PCH)
        target_precompile_headers(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${VIORAEDA_PCH_HEADER}>")
    endif()
endfunction()

function(vioraeda_configure_app_target target)
    set(options INCLUDE_CLI_DIR)
    cmake_parse_arguments(FLUX "${options}" "" "" ${ARGN})

    target_include_directories(${target} PRIVATE ${VIORAEDA_COMMON_INCLUDE_DIRS})
    if(FLUX_INCLUDE_CLI_DIR)
        target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli)
    endif()

    target_link_libraries(${target} PRIVATE ${VIORAEDA_APP_LINK_LIBS})
    if(VIORAEDA_ENABLE_PCH)
        target_precompile_headers(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${VIORAEDA_PCH_HEADER}>")
    endif()
endfunction()

function(vioraeda_configure_cli_target target)
    set(options INCLUDE_CLI_DIR)
    cmake_parse_arguments(FLUX "${options}" "" "" ${ARGN})

    target_include_directories(${target} PRIVATE ${VIORAEDA_COMMON_INCLUDE_DIRS})
    if(FLUX_INCLUDE_CLI_DIR)
        target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli)
    endif()

    target_link_libraries(${target} PRIVATE ${VIORAEDA_CLI_LINK_LIBS})
    if(VIORAEDA_ENABLE_PCH)
        target_precompile_headers(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${VIORAEDA_PCH_HEADER}>")
    endif()
    vioraeda_setup_rpath(${target})
endfunction()

# Setup platform-aware RPATH for an executable target.
# Windows: no RPATH (DLLs resolved via PATH/same directory).
# macOS: uses @loader_path.
# Linux: uses $ORIGIN.
function(vioraeda_setup_rpath target)
    if(WIN32)
        return()
    endif()
    if(APPLE)
        set(_origin "@loader_path")
    else()
        set(_origin "\$ORIGIN")
    endif()
    # Source-built FluxScript links the system LLVM; a stale
    # build/fluxscript-prebuilt/lib (bundled libLLVM needing libxml2.so.2,
    # absent on Ubuntu 26.04+) must never shadow it via RPATH.
    if(VIOSPICE_DEV_MODE OR VIOSPICE_BUILD_FLUXSCRIPT)
        set(_flux_dirs "${FLUXSCRIPT_LIB_DIR}")
    else()
        set(_flux_dirs "${CMAKE_BINARY_DIR}/fluxscript-prebuilt/lib;${FLUXSCRIPT_LIB_DIR}")
    endif()
    # Keep the preferred VioMATRIXC engine dir before the generic fallbacks
    # so the matching native code models resolve first.
    set(_rpath "${CMAKE_BINARY_DIR};${_flux_dirs};${VIOSPICE_PREFERRED_ENGINE_DIR};${CMAKE_BINARY_DIR}/_deps/fluxscript-build;${CMAKE_BINARY_DIR}/_deps/viomatrixc-src/src/.libs;${CMAKE_BINARY_DIR}/viomatrixc-prebuilt/lib;/usr/local/opt/llvm@15/lib")
    set_target_properties(${target} PROPERTIES
        BUILD_RPATH "${_rpath}"
        INSTALL_RPATH "${_origin};${_rpath}"
    )
endfunction()

# Convenience helper: define a C++ test executable and register it with CTest.
# Usage:
#   vioraeda_add_test(my_test_name
#       SOURCES test_my_test.cpp
#       LINK_LIBS VioCore VioUI
#   )
function(vioraeda_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LINK_LIBS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    set_target_properties(${name} PROPERTIES
        AUTOMOC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    target_include_directories(${name} PRIVATE ${VIORAEDA_COMMON_INCLUDE_DIRS})
    target_link_libraries(${name} PRIVATE ${ARG_LINK_LIBS} Qt${QT_VERSION_MAJOR}::Test)
    vioraeda_setup_rpath(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()
