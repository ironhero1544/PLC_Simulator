if(NOT MSVC)
    message(FATAL_ERROR "haptics_bridge must be built with MSVC.")
endif()

add_library(haptics_bridge SHARED
        src/haptics_bridge.cpp
        ${CMAKE_SOURCE_DIR}/include/plc_emulator/core/haptics_bridge_api.h
)

target_compile_features(haptics_bridge PRIVATE cxx_std_20)

target_include_directories(haptics_bridge PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(haptics_bridge PRIVATE
        HAPTICS_BRIDGE_EXPORTS
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

target_compile_options(haptics_bridge PRIVATE
        /utf-8
        /W4
        /EHsc
)

target_link_libraries(haptics_bridge PRIVATE
        runtimeobject
        windowsapp
)

set_target_properties(haptics_bridge PROPERTIES
        OUTPUT_NAME "haptics_bridge"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)