# FindNGSPICE.cmake
# Find the ngspice shared library and headers

find_path(NGSPICE_INCLUDE_DIR
    NAMES ngspice/sharedspice.h
    PATHS
        ${NGSPICE_ROOT}/include
        ${NGSPICE_ROOT}/share/ngspice/include
        /usr/local/include
        /usr/include
)

find_library(NGSPICE_LIBRARY
    NAMES ngspice
    PATHS
        ${NGSPICE_ROOT}/lib
        /usr/local/lib
        /usr/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NGSPICE DEFAULT_MSG NGSPICE_LIBRARY NGSPICE_INCLUDE_DIR)

if(NGSPICE_FOUND)
    set(NGSPICE_LIBRARIES ${NGSPICE_LIBRARY})
    set(NGSPICE_INCLUDE_DIRS ${NGSPICE_INCLUDE_DIR})
    
    if(NOT TARGET NGSPICE::ngspice)
        add_library(NGSPICE::ngspice UNKNOWN IMPORTED)
        set_target_properties(NGSPICE::ngspice PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${NGSPICE_INCLUDE_DIRS}"
            IMPORTED_LOCATION "${NGSPICE_LIBRARIES}"
        )
    endif()
endif()

mark_as_advanced(NGSPICE_INCLUDE_DIR NGSPICE_LIBRARY)
