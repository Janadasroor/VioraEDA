add_executable(test_regression_io_golden
    test_io_golden.cpp
    ${VIORAEDA_CLI_MODULE_OBJECTS}
)

vioraeda_configure_cli_target(test_regression_io_golden)

add_test(
    NAME regression.io_golden
    COMMAND test_regression_io_golden ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
)
set_tests_properties(regression.io_golden PROPERTIES LABELS "fast;regression;io")

if(BUILD_VIOSPICE_CLI)
    add_executable(test_regression_cli
        test_cli_regression.cpp
        ${VIORAEDA_CLI_MODULE_OBJECTS}
    )

    vioraeda_configure_cli_target(test_regression_cli)
    target_compile_definitions(test_regression_cli PRIVATE VIO_CMD_PATH=\"$<TARGET_FILE:viora>\")

    add_test(
        NAME regression.cli
        COMMAND test_regression_cli ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
    )
    set_tests_properties(regression.cli PROPERTIES LABELS "fast;regression;cli")

    add_executable(test_interactive_hang
        test_interactive_hang.cpp
        ${VIORAEDA_CLI_MODULE_OBJECTS}
    )
    vioraeda_configure_cli_target(test_interactive_hang)
    target_link_libraries(test_interactive_hang PRIVATE Qt6::Test)
    
    add_test(
        NAME regression.interactive_hang
        COMMAND test_interactive_hang
    )
    set_tests_properties(regression.interactive_hang PROPERTIES LABELS "regression;interactive")

    add_test(
        NAME regression.cli.py
        COMMAND ${Python_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/test_cli.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(regression.cli.py PROPERTIES LABELS "regression;cli;python")
endif()
