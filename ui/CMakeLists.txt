set(FLUXUI_SOURCES ${UI_SOURCES})
list(TRANSFORM FLUXUI_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/../")

# Add UI Command Server files
list(APPEND FLUXUI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ui_command_server.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui_command_server.cpp"
)

add_library(FluxUI STATIC ${FLUXUI_SOURCES})
set_target_properties(FluxUI PROPERTIES AUTOMOC ON)
vioraeda_configure_module_target(FluxUI)
target_link_libraries(FluxUI PUBLIC FluxCore FluxSymbols FluxSchematic FluxFootprints VioraPCBCore)
target_link_libraries(FluxUI PRIVATE Qt${QT_VERSION_MAJOR}::WebSockets)

# Disable AUTOUIC for UI Command Server (no .ui file)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/ui_command_server.cpp"
    PROPERTIES SKIP_AUTOUIC ON
)
