# PluginLabTests: a console app that runs all juce::UnitTest classes; CTest runs it.
juce_add_console_app(PluginLabTests
    PRODUCT_NAME "PluginLabTests")

target_sources(PluginLabTests
    PRIVATE
        TestMain.cpp
        VersionTests.cpp)

target_compile_definitions(PluginLabTests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PluginLabTests,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PluginLabTests,JUCE_VERSION>")

target_link_libraries(PluginLabTests
    PRIVATE
        pluginlab_core
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

add_test(NAME PluginLabTests COMMAND PluginLabTests)

# Smoke test of the stand-alone app: it starts, writes its version into a file and quits; the version is the one of CMake.
add_test(NAME PluginLabHostVersion
    COMMAND ${CMAKE_COMMAND}
        -DHOST_APPLICATION=$<TARGET_FILE:PluginLabHost>
        -DEXPECTED_VERSION=${PROJECT_VERSION}
        -DVERSION_FILE=${CMAKE_CURRENT_BINARY_DIR}/host_version.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckHostVersion.cmake)
