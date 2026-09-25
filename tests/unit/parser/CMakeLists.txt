add_executable(test_raw_data_parser
    test_raw_data_parser.cpp
)

target_include_directories(test_raw_data_parser PRIVATE ${CMAKE_SOURCE_DIR}/simulator/core)

vioraeda_configure_cli_target(test_raw_data_parser)
target_link_libraries(test_raw_data_parser PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    VioSimulator
)

add_test(NAME unit_raw_data_parser COMMAND test_raw_data_parser)
set_tests_properties(unit_raw_data_parser PROPERTIES LABELS "unit;simulator")
