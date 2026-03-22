# FindNGSPICE.cmake
# Find the ngspice shared library and headers

# First check if we're building for Android and have an ABI
if(ANDROID AND ANDROID_ABI)
    # Map Android ABI to ngspice build directory
    set(_ngspice_search_path "")
    
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(_ngspice_search_path "${NGSPICE_ROOT}/android-arm64")
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(_ngspice_search_path "${NGSPICE_ROOT}/android-arm")
    elseif(ANDROID_ABI STREQUAL "x86")
        set(_ngspice_search_path "${NGSPICE_ROOT}/android-x86")
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(_ngspice_search_path "${NGSPICE_ROOT}/android-x86_64")
    else()
        set(_ngspice_search_path "${NGSPICE_ROOT}/android-arm64")
    endif()
    
    message(STATUS "NGSPICE: Using ${_ngspice_search_path} for ABI ${ANDROID_ABI}")
else()
    set(_ngspice_search_path "${NGSPICE_ROOT}")
endif()

# Check for library - custom build paths first, then system paths
set(_possible_lib_paths
    "${_ngspice_search_path}/shared/src/.libs/libngspice.so"
    "${NGSPICE_ROOT}/android-arm64/shared/src/.libs/libngspice.so"
    "/usr/lib/x86_64-linux-gnu/libngspice.so"
    "/usr/lib/libngspice.so"
    "/usr/local/lib/libngspice.so"
)

foreach(_lib_path IN LISTS _possible_lib_paths)
    if(NOT NGSPICE_LIBRARY AND EXISTS "${_lib_path}")
        set(NGSPICE_LIBRARY "${_lib_path}")
    endif()
endforeach()

# Check for include directory - custom build paths first, then system paths
set(_possible_include_paths
    "${_ngspice_search_path}/../../src/include"
    "${NGSPICE_ROOT}/android-arm64/../../src/include"
    "/usr/include"
    "/usr/local/include"
)

foreach(_inc_path IN LISTS _possible_include_paths)
    if(NOT NGSPICE_INCLUDE_DIR AND EXISTS "${_inc_path}/ngspice/sharedspice.h")
        set(NGSPICE_INCLUDE_DIR "${_inc_path}")
    endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NGSPICE DEFAULT_MSG NGSPICE_LIBRARY NGSPICE_INCLUDE_DIR)

if(NGSPICE_FOUND)
    set(NGSPICE_LIBRARIES ${NGSPICE_LIBRARY})
    set(NGSPICE_INCLUDE_DIRS ${NGSPICE_INCLUDE_DIR})
    
    message(STATUS "NGSPICE found!")
    message(STATUS "  Library: ${NGSPICE_LIBRARIES}")
    message(STATUS "  Include: ${NGSPICE_INCLUDE_DIRS}")
    
    if(NOT TARGET NGSPICE::ngspice)
        add_library(NGSPICE::ngspice SHARED IMPORTED)
        set_target_properties(NGSPICE::ngspice PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${NGSPICE_INCLUDE_DIRS}"
            IMPORTED_LOCATION "${NGSPICE_LIBRARIES}"
        )
    endif()
endif()

mark_as_advanced(NGSPICE_INCLUDE_DIR NGSPICE_LIBRARY)
