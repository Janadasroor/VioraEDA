# test_integration_schematic_simulator removed (Internal solver deleted)

add_executable(test_integration_ngspice_shared
    test_ngspice_shared_lib.cpp
    ${VIORAEDA_CLI_MODULE_OBJECTS}
)

target_link_libraries(test_integration_ngspice_shared PRIVATE Qt${QT_VERSION_MAJOR}::Test)
vioraeda_configure_cli_target(test_integration_ngspice_shared)
# Ensure circular static lib deps resolve (Gemini/Python symbols).
# --start-group/--end-group is GNU ld only; Apple ld64/MSVC resolve circular deps differently.
target_link_libraries(test_integration_ngspice_shared PRIVATE
    ${VIOSPICE_LINKER_GROUP_BEGIN}
    VioCore
    VioSymbols
    VioSchematic
    VioUI
    VioSimulator
    FluxScript
    FluxScriptManager
    ${VIOSPICE_LINKER_GROUP_END}
)

add_test(
    NAME integration.ngspice_shared
    COMMAND test_integration_ngspice_shared
)
set_tests_properties(integration.ngspice_shared PROPERTIES LABELS "fast;integration;simulator;ngspice")

add_test(
    NAME integration.error_popup_path
    COMMAND test_error_popup_path
)
set_tests_properties(integration.error_popup_path PROPERTIES LABELS "fast;integration;simulator;ngspice;regression")

add_test(
    NAME integration.rerun_error_staleness
    COMMAND test_rerun_error_staleness
)
set_tests_properties(integration.rerun_error_staleness PROPERTIES LABELS "fast;integration;simulator;ngspice;regression")

add_test(
    NAME integration.bridge_rerun
    COMMAND test_bridge_rerun
)
set_tests_properties(integration.bridge_rerun PROPERTIES LABELS "fast;integration;simulator;ngspice;regression")

add_test(
    NAME integration.bridge_stop_restart
    COMMAND test_bridge_stop_restart
)
set_tests_properties(integration.bridge_stop_restart PROPERTIES LABELS "fast;integration;simulator;ngspice;regression")

add_test(
    NAME integration.shared_pause_resume
    COMMAND test_shared_pause_resume
)
set_tests_properties(integration.shared_pause_resume PROPERTIES LABELS "fast;integration;simulator;ngspice;regression")

add_test(
    NAME integration.project_manager_launch
    COMMAND test_project_manager_launch
)
set_tests_properties(integration.project_manager_launch PROPERTIES LABELS "fast;integration;ui;regression")

add_executable(test_error_popup_path
    test_error_popup_path.cpp
    ${VIORAEDA_CLI_MODULE_OBJECTS}
)

add_executable(test_rerun_error_staleness
    test_rerun_error_staleness.cpp
    ${VIORAEDA_CLI_MODULE_OBJECTS}
)

add_executable(test_bridge_rerun
    test_bridge_rerun.cpp
    ${VIORAEDA_CLI_MODULE_OBJECTS}
)

target_link_libraries(test_error_popup_path PRIVATE Qt${QT_VERSION_MAJOR}::Test)
vioraeda_configure_cli_target(test_error_popup_path)
# Ensure circular static lib deps resolve (Gemini/Python symbols).
target_link_libraries(test_error_popup_path PRIVATE
    ${VIOSPICE_LINKER_GROUP_BEGIN}
    VioCore
    VioSymbols
    VioSchematic
    VioUI
    VioSimulator
    FluxScript
    FluxScriptManager
    ${VIOSPICE_LINKER_GROUP_END}
)

target_link_libraries(test_rerun_error_staleness PRIVATE Qt${QT_VERSION_MAJOR}::Test)
vioraeda_configure_cli_target(test_rerun_error_staleness)
target_link_libraries(test_rerun_error_staleness PRIVATE
    ${VIOSPICE_LINKER_GROUP_BEGIN}
    VioCore
    VioSymbols
    VioSchematic
    VioUI
    VioSimulator
    FluxScript
    FluxScriptManager
    ${VIOSPICE_LINKER_GROUP_END}
)

target_link_libraries(test_bridge_rerun PRIVATE Qt${QT_VERSION_MAJOR}::Test)
vioraeda_configure_cli_target(test_bridge_rerun)
target_link_libraries(test_bridge_rerun PRIVATE
    ${VIOSPICE_LINKER_GROUP_BEGIN}
    VioCore
    VioSymbols
    VioSchematic
    VioUI
    VioSimulator
    FluxScript
    FluxScriptManager
    ${VIOSPICE_LINKER_GROUP_END}
)

add_executable(test_bridge_stop_restart
    test_bridge_stop_restart.cpp
    ${VIORAEDA_CLI_MODULE_OBJECTS}
)

add_executable(test_shared_pause_resume
    test_shared_pause_resume.cpp
    ${VIORAEDA_CLI_MODULE_OBJECTS}
)

target_link_libraries(test_shared_pause_resume PRIVATE Qt${QT_VERSION_MAJOR}::Test)
vioraeda_configure_cli_target(test_shared_pause_resume)
target_link_libraries(test_shared_pause_resume PRIVATE
    ${VIOSPICE_LINKER_GROUP_BEGIN}
    VioCore
    VioSymbols
    VioSchematic
    VioUI
    VioSimulator
    FluxScript
    FluxScriptManager
    ${VIOSPICE_LINKER_GROUP_END}
)

target_link_libraries(test_bridge_stop_restart PRIVATE Qt${QT_VERSION_MAJOR}::Test)
vioraeda_configure_cli_target(test_bridge_stop_restart)
target_link_libraries(test_bridge_stop_restart PRIVATE
    ${VIOSPICE_LINKER_GROUP_BEGIN}
    VioCore
    VioSymbols
    VioSchematic
    VioUI
    VioSimulator
    FluxScript
    FluxScriptManager
    ${VIOSPICE_LINKER_GROUP_END}
)

add_executable(test_project_manager_launch
    test_project_manager_launch.cpp
    ${VIORAEDA_CLI_MODULE_OBJECTS}
)

target_link_libraries(test_project_manager_launch PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
vioraeda_configure_cli_target(test_project_manager_launch)
target_link_libraries(test_project_manager_launch PRIVATE
    ${VIOSPICE_LINKER_GROUP_BEGIN}
    VioCore
    VioSymbols
    VioSchematic
    VioUI
    VioSimulator
    FluxScript
    FluxScriptManager
    ${VIOSPICE_LINKER_GROUP_END}
)


add_executable(test_benchmark
    test_benchmark.cpp
    ${VIORAEDA_CLI_MODULE_OBJECTS}
)

target_link_libraries(test_benchmark PRIVATE Qt${QT_VERSION_MAJOR}::Core)
vioraeda_configure_cli_target(test_benchmark)
