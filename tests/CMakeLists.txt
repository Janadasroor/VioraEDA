# Root test aggregation entrypoint.
option(VIOSPICE_ENABLE_TESTS "Enable tests" ON)
if(VIOSPICE_ENABLE_TESTS AND VIOSPICE_ENABLE_PYTHON)
    add_subdirectory(unit/ux)
    add_subdirectory(integration)
    add_subdirectory(regression)
endif()
# add_subdirectory(benchmarks)
