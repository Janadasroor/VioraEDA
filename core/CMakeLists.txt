# Set up VioCore sources from root-relative paths
set(LOCAL_CORE_SOURCES ${CORE_SOURCES})
list(TRANSFORM LOCAL_CORE_SOURCES PREPEND "${CMAKE_SOURCE_DIR}/")

list(APPEND LOCAL_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/flux_script_engine.cpp"
)

qt6_wrap_cpp(VIO_CORE_MOC_SOURCES
    jit_context_manager.h
)

add_library(VioCore STATIC ${LOCAL_CORE_SOURCES} ${VIO_CORE_MOC_SOURCES})
set_target_properties(VioCore PROPERTIES AUTOMOC ON)
vioraeda_configure_module_target(VioCore)
target_link_libraries(VioCore PUBLIC FluxScript)

# Headless library for Clean Architecture models and engines
set(HEADLESS_SOURCES
    # Schematic models
    ../schematic/models/schematic_item_model.h
    ../schematic/models/wire_model.h
    ../schematic/models/schematic_component_model.h
    ../schematic/models/schematic_page_model.h
    ../schematic/models/erc_engine.h
    ../schematic/analysis/netlist_engine.h
    # PCB models
    ../pcb/models/drc_types.h
    ../pcb/models/drc_types.cpp
    ../pcb/models/trace_model.h
    ../pcb/models/via_model.h
    ../pcb/models/pad_model.h
    ../pcb/models/component_model.h
    ../pcb/models/copper_pour_model.h
    ../pcb/models/board_model.h
    ../pcb/analysis/connectivity_engine.h
    ../pcb/analysis/drc_engine.h
)
add_library(VioHeadless STATIC ${HEADLESS_SOURCES})
set_target_properties(VioHeadless PROPERTIES AUTOMOC ON)
vioraeda_configure_module_target(VioHeadless)
target_link_libraries(VioHeadless PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)

