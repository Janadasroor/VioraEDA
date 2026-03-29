find_package(Qt6 REQUIRED COMPONENTS Core Widgets Quick QuickWidgets)

if(VIOSPICE_ENABLE_PYTHON)
add_library(FluxPython STATIC
    python_manager.cpp
    python_manager.h
    gemini_dialog.cpp
    gemini_dialog.h
    gemini_bridge.cpp
    gemini_bridge.h
    gemini_panel.cpp
    gemini_panel.h
    gemini_instructions_dialog.cpp
    gemini_instructions_dialog.h
)

target_link_libraries(FluxPython PRIVATE 
    Qt6::Core
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickWidgets
)

target_include_directories(FluxPython PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../schematic/io
    ${CMAKE_CURRENT_SOURCE_DIR}/../schematic/items
)
endif()
