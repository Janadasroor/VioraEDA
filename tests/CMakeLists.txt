# Root test aggregation entrypoint.
if(VIOSPICE_ENABLE_TESTS)
    # Stage the VioMATRIXC code models (.cm) where the integration tests can find
    # them: the simulator resolves model files relative to the test binary's
    # directory (build/tests/<sub>/, which walks up to build/tests/cm).
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests/cm")
    file(GLOB _test_cm_files "${CMAKE_BINARY_DIR}/cm/*.cm")
    if(_test_cm_files)
        file(COPY ${_test_cm_files} DESTINATION "${CMAKE_BINARY_DIR}/tests/cm")
    endif()
    unset(_test_cm_files)
    vioraeda_add_test(test_design_rule
        SOURCES test_design_rule.cpp test_design_rule.h
        LINK_LIBS VioCore VioSymbols VioSimulator FluxScript FluxScriptManager VioUI
    )
    vioraeda_add_test(test_serialization_and_cloning
        SOURCES test_serialization_and_cloning.cpp test_serialization_and_cloning.h
        LINK_LIBS VioUI VioVioraIDE VioSchematic VioraPCBCore VioFootprints VioCore VioSymbols VioSimulator FluxScriptManager FluxScript
    )
    vioraeda_add_test(test_library_performance
        SOURCES test_library_performance.cpp
        LINK_LIBS VioVioraIDE VioSchematic VioUI VioraPCBCore VioFootprints VioCore VioSymbols VioSimulator FluxScriptManager FluxScript
    )
    vioraeda_add_test(test_symbol_stub_resolution
        SOURCES test_symbol_stub_resolution.cpp
        LINK_LIBS ${VIOSPICE_LINKER_GROUP_BEGIN} VioVioraIDE VioSchematic VioUI VioraPCBCore VioFootprints VioCore VioSymbols VioSimulator FluxScriptManager FluxScript ${VIOSPICE_LINKER_GROUP_END}
    )
    vioraeda_add_test(test_pcb_system
        SOURCES test_pcb_system.cpp test_pcb_system.h
        LINK_LIBS VioraPCBCore VioFootprints VioCore VioUI VioVioraIDE VioSchematic VioSymbols VioSimulator FluxScriptManager FluxScript
    )
    vioraeda_add_test(test_viora_libs_simulation
        SOURCES test_viora_libs_simulation.cpp test_viora_libs_simulation.h
        LINK_LIBS VioSimulator VioSymbols VioFootprints VioCore VioSchematic VioVioraIDE VioUI FluxScriptManager FluxScript
    )
endif()

# CLI-based regression tests (no Python needed)
if(VIOSPICE_ENABLE_TESTS AND BUILD_VIOSPICE_CLI)
    add_executable(test_sim_golden
        regression/test_sim_golden.cpp
    )
    target_compile_definitions(test_sim_golden PRIVATE VIO_CMD_PATH=\"$<TARGET_FILE:viora>\")
    target_link_libraries(test_sim_golden PRIVATE Qt${QT_VERSION_MAJOR}::Core)
    add_test(NAME regression.sim_golden COMMAND test_sim_golden ${CMAKE_SOURCE_DIR}/tests/circuits)
    set_tests_properties(regression.sim_golden PROPERTIES LABELS "fast;regression;simulator")

    # Production validation suite — auto-discovers 200 test circuits
    add_executable(test_production_validation
        regression/test_production_validation.cpp
    )
    target_compile_definitions(test_production_validation PRIVATE VIO_CMD_PATH=\"$<TARGET_FILE:viora>\")
    target_link_libraries(test_production_validation PRIVATE Qt${QT_VERSION_MAJOR}::Core)
    add_test(NAME regression.production_validation
        COMMAND test_production_validation ${CMAKE_SOURCE_DIR}/tests/production_validation
    )
    set_tests_properties(regression.production_validation PROPERTIES
        LABELS "slow;regression;simulator"
        TIMEOUT 600
    )
endif()

# Python-dependent test suites
if(VIOSPICE_ENABLE_TESTS AND VIOSPICE_ENABLE_PYTHON)
    add_subdirectory(unit/ux)
    add_subdirectory(unit/parser)
    add_subdirectory(integration)
    add_subdirectory(regression)
endif()
# add_subdirectory(benchmarks)
