# pluginlab: measure, compare and match audio plugins (see planning.md).
# Build:   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
# Test:    ctest --test-dir build -C Release --output-on-failure
cmake_minimum_required(VERSION 3.22)

# The version of the whole project: a new feature raises the second number and sets the third to zero,
# a fix or other change raises the third (see CLAUDE.md).
project(pluginlab VERSION 0.5.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # JUCE code is linked into a static library and into plugins

option(PLUGINLAB_BUILD_TESTS "Build the tests (PluginLabTests) and register them with CTest" ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found: run 'git submodule update --init'")
endif()
add_subdirectory(JUCE)
include(cmake/Vst2Sdk.cmake) # sets PLUGINLAB_VST2: VST2 hosting and VST2 test plugins

if(PLUGINLAB_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(src/core)
add_subdirectory(src/ui)
add_subdirectory(apps/scanner)
add_subdirectory(apps/host)
add_subdirectory(plugins/loader)

if(PLUGINLAB_BUILD_TESTS)
    add_subdirectory(tests)
endif()
