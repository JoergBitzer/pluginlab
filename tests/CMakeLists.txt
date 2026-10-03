# PluginLabTests: a console app that runs all juce::UnitTest classes; CTest runs it.
add_subdirectory(plugins) # the test plugins (PluginLabTestGain, PluginLabTestCrash) and PLUGINLAB_TEST_PLUGIN_DIR

juce_add_console_app(PluginLabTests
    PRODUCT_NAME "PluginLabTests")

target_sources(PluginLabTests
    PRIVATE
        TestMain.cpp
        VersionTests.cpp
        ScannerTests.cpp
        HostedPluginTests.cpp)

target_compile_definitions(PluginLabTests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        PLUGINLAB_TEST_PLUGIN_DIR="${PLUGINLAB_TEST_PLUGIN_DIR}"
        PLUGINLAB_SCANNER_EXECUTABLE="$<TARGET_FILE:PluginLabScanner>"
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PluginLabTests,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PluginLabTests,JUCE_VERSION>")

target_link_libraries(PluginLabTests
    PRIVATE
        pluginlab_core
        juce::juce_core
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# the tests need the scanner and the test plugins built first
add_dependencies(PluginLabTests PluginLabScanner PluginLabTestGain_VST3 PluginLabTestCrash_VST3 PluginLabHost)

add_test(NAME PluginLabTests COMMAND PluginLabTests)

# Smoke test of the stand-alone app: it starts, writes its version into a file and quits; the version is the one of CMake.
add_test(NAME PluginLabHostVersion
    COMMAND ${CMAKE_COMMAND}
        -DHOST_APPLICATION=$<TARGET_FILE:PluginLabHost>
        -DEXPECTED_VERSION=${PROJECT_VERSION}
        -DVERSION_FILE=${CMAKE_CURRENT_BINARY_DIR}/host_version.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckHostVersion.cmake)

# End to end: the host application scans a folder (scanner processes), loads the plugins and writes a report.
add_test(NAME PluginLabHostReport
    COMMAND ${CMAKE_COMMAND}
        -DHOST_APPLICATION=$<TARGET_FILE:PluginLabHost>
        -DPLUGIN_FOLDER=${PLUGINLAB_TEST_PLUGIN_DIR}
        -DREPORT_FILE=${CMAKE_CURRENT_BINARY_DIR}/host_report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckHostReport.cmake)
