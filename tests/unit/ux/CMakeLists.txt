add_executable(test_ux_logic
    test_ux_logic.cpp
    test_ux_logic.h
)

add_executable(test_panel_tab_preserve
    test_panel_tab_preserve.cpp
)

add_executable(test_switch_click_routing
    test_switch_click_routing.cpp
)

vioraeda_configure_cli_target(test_ux_logic)
target_link_libraries(test_ux_logic PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    VioSchematic
    VioUI
    VioCore
    VioSymbols
    VioVioraIDE
)

add_test(NAME unit_ux_logic COMMAND test_ux_logic)
set_tests_properties(unit_ux_logic PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

vioraeda_configure_cli_target(test_panel_tab_preserve)
target_link_libraries(test_panel_tab_preserve PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
    VioSchematic
    VioUI
    VioCore
    VioSymbols
    VioVioraIDE
)

add_test(NAME unit_panel_tab_preserve COMMAND test_panel_tab_preserve)
set_tests_properties(unit_panel_tab_preserve PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

vioraeda_configure_cli_target(test_switch_click_routing)
target_link_libraries(test_switch_click_routing PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
    VioSchematic
    VioUI
    VioCore
    VioSymbols
    VioVioraIDE
)

add_test(NAME unit_switch_click_routing COMMAND test_switch_click_routing)
set_tests_properties(unit_switch_click_routing PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
