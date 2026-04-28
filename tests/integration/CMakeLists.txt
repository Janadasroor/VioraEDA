# test_integration_schematic_simulator removed (Internal solver deleted)

add_executable(test_integration_ngspice_shared
    test_ngspice_shared_lib.cpp
    ${VIORAEDA_CLI_MODULE_OBJECTS}
)

target_link_libraries(test_integration_ngspice_shared PRIVATE Qt${QT_VERSION_MAJOR}::Test)
vioraeda_configure_cli_target(test_integration_ngspice_shared)
# Ensure circular static lib deps resolve (Gemini/Python symbols).
target_link_libraries(test_integration_ngspice_shared PRIVATE
    -Wl,--start-group
    VioCore
    VioSymbols
    VioSchematic
    VioUI
    VioSimulator
    FluxScript
    FluxScriptManager
    -Wl,--end-group
)

add_test(
    NAME integration.ngspice_shared
    COMMAND test_integration_ngspice_shared
)
set_tests_properties(integration.ngspice_shared PROPERTIES LABELS "fast;integration;simulator;ngspice")

add_executable(test_benchmark
    test_benchmark.cpp
    ${VIORAEDA_CLI_MODULE_OBJECTS}
)

target_link_libraries(test_benchmark PRIVATE Qt${QT_VERSION_MAJOR}::Core)
vioraeda_configure_cli_target(test_benchmark)
target_link_libraries(test_benchmark PRIVATE
    -Wl,--start-group
    VioCore
    VioSymbols
    VioSchematic
    VioUI
    VioSimulator
    FluxScript
    FluxScriptManager
    -Wl,--end-group
)

add_executable(test_slider_realtime
    test_slider_realtime.cpp
    ${VIORAEDA_CLI_MODULE_OBJECTS}
)

target_link_libraries(test_slider_realtime PRIVATE Qt${QT_VERSION_MAJOR}::Core)
vioraeda_configure_cli_target(test_slider_realtime)
target_link_libraries(test_slider_realtime PRIVATE
    -Wl,--start-group
    VioCore
    VioSymbols
    VioSchematic
    VioUI
    VioSimulator
    FluxScript
    FluxScriptManager
    -Wl,--end-group
)

add_test(
    NAME integration.slider_realtime
    COMMAND test_slider_realtime
)
set_tests_properties(integration.slider_realtime PROPERTIES LABELS "fast;integration;simulator")
