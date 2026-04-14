set(FLUXUI_SOURCES ${UI_SOURCES})
list(TRANSFORM FLUXUI_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/../")

# Add UI Command Server files
list(APPEND FLUXUI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ui_command_server.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui_command_server.cpp"
)

# Add Python REPL Console files
list(APPEND FLUXUI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/python_console_widget.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/python_console_widget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/python_executor.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/python_executor.cpp"
)

add_library(FluxUI STATIC ${FLUXUI_SOURCES})
set_target_properties(FluxUI PROPERTIES AUTOMOC ON)
vioraeda_configure_module_target(FluxUI)
target_link_libraries(FluxUI PUBLIC FluxCore FluxSymbols FluxSchematic FluxFootprints VioraPCBCore)
target_link_libraries(FluxUI PRIVATE Qt${QT_VERSION_MAJOR}::WebSockets)

# Enable Python embedding in UI components
if(Python3_FOUND)
    target_compile_definitions(FluxUI PRIVATE HAVE_PYTHON)
    target_include_directories(FluxUI PRIVATE ${Python3_INCLUDE_DIRS})
    target_link_libraries(FluxUI PRIVATE Python3::Python)
endif()

# Disable AUTOUIC for files without .ui files
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/ui_command_server.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/python_executor.cpp"
    PROPERTIES SKIP_AUTOUIC ON
)
