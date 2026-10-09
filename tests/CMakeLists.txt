# PluginLabTests: a console app that runs all juce::UnitTest classes; CTest runs it.
add_subdirectory(plugins) # the test plugins (PluginLabTestGain, PluginLabTestCrash) and PLUGINLAB_TEST_PLUGIN_DIR

juce_add_console_app(PluginLabTests
    PRODUCT_NAME "PluginLabTests")

target_sources(PluginLabTests
    PRIVATE
        TestMain.cpp
        VersionTests.cpp
        ScannerTests.cpp
        HostedPluginTests.cpp
        ValidatorTests.cpp
        LoaderTests.cpp
        MeasureCrosstalkTests.cpp
        MeasureDelayTests.cpp
        MeasureDistortionTests.cpp
        MeasureGainTests.cpp
        MeasureIntermodulationTests.cpp
        MeasureLinearityTests.cpp
        MeasureNoiseTests.cpp
        MeasureNullTests.cpp
        MeasurePhaseTests.cpp
        MeasureResponseTests.cpp
        PluginMeasurementTests.cpp
        NonlinearReferenceTests.cpp
        OracleTests.cpp
        BrowserTests.cpp
        CatalogTests.cpp
        EngineTests.cpp
        FingerprintTests.cpp
        GuiReviewTests.cpp
        ProbeTests.cpp
        RealWorldTests.cpp
        ReferencePluginTests.cpp
        ReferenceTests.cpp
        SignalsTests.cpp)

target_compile_definitions(PluginLabTests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        PLUGINLAB_TEST_PLUGIN_DIR="${PLUGINLAB_TEST_PLUGIN_DIR}"
        PLUGINLAB_REFERENCE_PLUGIN_DIR="${PLUGINLAB_REFERENCE_PLUGIN_DIR}"
        PLUGINLAB_ORACLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/oracle"
        PLUGINLAB_SCANNER_EXECUTABLE="$<TARGET_FILE:PluginLabScanner>"
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PluginLabTests,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PluginLabTests,JUCE_VERSION>")

target_link_libraries(PluginLabTests
    PRIVATE
        pluginlab_core
        pluginlab_engine
        pluginlab_signals
        pluginlab_reference
        pluginlab_measure
        pluginlab_loader_core
        juce::juce_core
        juce::juce_dsp
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# the tests need the scanner and the test plugins built first
add_dependencies(PluginLabTests PluginLabScanner PluginLabSignals PluginLabTestGain_VST3 PluginLabTestCrash_VST3 PluginLabTestCrashProcess_VST3 PluginLabHost PluginLabLoaderBundle PluginLabReferenceEq_VST3 PluginLabReferenceNonlinear_VST3 PluginLabReferenceUtility_VST3)
if(PLUGINLAB_VST2)
    add_dependencies(PluginLabTests PluginLabTestGain_VST)
endif()

add_test(NAME PluginLabTests COMMAND PluginLabTests)
# a hanging test (a dialog nobody can click, a plugin that never returns) must fail soon, not after the default 25 minutes
set_tests_properties(PluginLabTests PROPERTIES TIMEOUT 600)

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
        -DWITH_VST2=${PLUGINLAB_VST2}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckHostReport.cmake)

# End to end: the fingerprint of one plugin through the command line of the host (the way the Developer page runs it).
add_test(NAME PluginLabHostFingerprint
    COMMAND ${CMAKE_COMMAND}
        -DHOST_APPLICATION=$<TARGET_FILE:PluginLabHost>
        -DPLUGIN_FILE=${PLUGINLAB_TEST_PLUGIN_DIR}/PluginLabTestGain.vst3
        -DREPORT_FILE=${CMAKE_CURRENT_BINARY_DIR}/host_fingerprint.md
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckHostFingerprint.cmake)
# the measurement reads its settings from this file, not from the user's
set_tests_properties(PluginLabHostFingerprint PROPERTIES ENVIRONMENT "PLUGINLAB_FINGERPRINT_SETTINGS=${CMAKE_CURRENT_BINARY_DIR}/fingerprint_settings.json")

# The measurement units of W7 on a hosted plugin from the command line (W7.11)
add_test(NAME PluginLabHostMeasure
    COMMAND ${CMAKE_COMMAND}
        -DHOST_APPLICATION=$<TARGET_FILE:PluginLabHost>
        -DPLUGIN_FILE=${PLUGINLAB_TEST_PLUGIN_DIR}/PluginLabTestGain.vst3
        -DREPORT_FILE=${CMAKE_CURRENT_BINARY_DIR}/host_measure.md
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckHostMeasure.cmake)

# The GUI review (W5d): the capture of a hosted editor needs X (xwd and ImageMagick); Linux only, in the virtual display of tools/ctest_offscreen.sh
if(UNIX AND NOT APPLE)
    add_test(NAME PluginLabHostGuiSnapshot
        COMMAND ${CMAKE_COMMAND}
            -DHOST_APPLICATION=$<TARGET_FILE:PluginLabHost>
            -DPLUGIN_FILE=${PLUGINLAB_REFERENCE_PLUGIN_DIR}/PluginLabReferenceEq.vst3
            -DFOLDER=${CMAKE_CURRENT_BINARY_DIR}/gui_review_reference_eq
            -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckHostGuiSnapshot.cmake)
    # W5d.3: test editors that follow the scale factor, ignore it, or grow without scaling their drawing
    add_test(NAME PluginLabHostGuiScaling
        COMMAND ${CMAKE_COMMAND}
            -DHOST_APPLICATION=$<TARGET_FILE:PluginLabHost>
            -DPLUGIN_FOLDER=${PLUGINLAB_TEST_PLUGIN_DIR}
            -DFOLDER=${CMAKE_CURRENT_BINARY_DIR}/gui_review_scaling
            -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckHostGuiScaling.cmake)
endif()

# The loader plugin is self-contained: its bundle holds the scanner (found next to the plugin binary when the plugin runs in a DAW).
add_test(NAME PluginLabLoaderBundleHasScanner
    COMMAND ${CMAKE_COMMAND}
        -DBUNDLE_FOLDER=${PLUGINLAB_LOADER_PLUGIN_DIR}/PluginLabLoader.vst3
        -DSCANNER_NAME=$<TARGET_FILE_NAME:PluginLabScanner>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckLoaderBundle.cmake)

# Smoke test of PluginLabSignals (the standard test signals as WAV files)
add_test(NAME PluginLabSignalsWrites
    COMMAND ${CMAKE_COMMAND}
        -DSIGNALS_APPLICATION=$<TARGET_FILE:PluginLabSignals>
        -DOUTPUT_FOLDER=${CMAKE_CURRENT_BINARY_DIR}/signals_smoke
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckSignals.cmake)
