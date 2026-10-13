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
        InteractionTests.cpp
        ValidatorTests.cpp
        LoaderTests.cpp
        MappingTests.cpp
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
        MeasureSpectrogramTests.cpp
        MeasureSystemClassTests.cpp
        PluginMeasurementTests.cpp
        NonlinearReferenceTests.cpp
        OracleTests.cpp
        ParameterSurveyTests.cpp
        BandFitTests.cpp
        CalibrationTests.cpp
        BrowserTests.cpp
        CatalogTests.cpp
        EngineTests.cpp
        EqModelTests.cpp
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
        PLUGINLAB_MAPPING_DIR="${CMAKE_SOURCE_DIR}/mappings"
        PLUGINLAB_SCANNER_EXECUTABLE="$<TARGET_FILE:PluginLabScanner>"
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PluginLabTests,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PluginLabTests,JUCE_VERSION>")

target_link_libraries(PluginLabTests
    PRIVATE
        pluginlab_core
        pluginlab_engine
        pluginlab_eqmodel
        pluginlab_signals
        pluginlab_reference
        pluginlab_measure
        pluginlab_hosting_ui
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

add_test(NAME PluginLabTests COMMAND PluginLabTests --skip "Parameter survey|PluginFingerprint|Real-time fingerprint|Calibration (W8.6)|Interactions (W8.7)")
# a hanging test (a dialog nobody can click, a plugin that never returns) must fail soon, not after the default 25 minutes
set_tests_properties(PluginLabTests PROPERTIES TIMEOUT 600 RESOURCE_LOCK display)
# the fingerprint measures many test plugins (with the system class at A and B since 0.49.0): a test with a limit of its own; not at the same
# time as PluginLabTests (the lock: both are long and render much)
add_test(NAME PluginLabFingerprint COMMAND PluginLabTests --only "PluginFingerprint")
set_tests_properties(PluginLabFingerprint PROPERTIES TIMEOUT 1200 RESOURCE_LOCK display)
# the real-time plugins of the fingerprint (timer, offline flag, wall clock): their checks compare wall-clock timing, so they run alone
add_test(NAME PluginLabFingerprintRealTime COMMAND PluginLabTests --only "Real-time fingerprint")
set_tests_properties(PluginLabFingerprintRealTime PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
# the parameter survey (W8.4) renders some hundred times per plugin: one entry per plugin with its own limit (Debug: 1 ... 9 minutes each), so
# that "ctest -j" runs them side by side
# the interactions between bands (W8.7): one entry per plugin
foreach(COMBINED "Test EQ Two Bands" "Test EQ Parallel" "Reference EQ")
    string(REPLACE " " "" INTERACTION_TEST_NAME "${COMBINED}")
    add_test(NAME PluginLabInteractions_${INTERACTION_TEST_NAME} COMMAND PluginLabTests --only "Interactions (W8.7): ${COMBINED}")
    set_tests_properties(PluginLabInteractions_${INTERACTION_TEST_NAME} PROPERTIES TIMEOUT 1200)
endforeach()
# the calibration (W8.6) measures every band parameter at three sample rates: one entry per plugin
foreach(CALIBRATED "Test EQ" "Test EQ Fs Fault" "Test EQ Knob Faults" "Reference EQ")
    string(REPLACE " " "" CALIBRATION_TEST_NAME "${CALIBRATED}")
    add_test(NAME PluginLabCalibration_${CALIBRATION_TEST_NAME} COMMAND PluginLabTests --only "Calibration (W8.6): ${CALIBRATED}")
    set_tests_properties(PluginLabCalibration_${CALIBRATION_TEST_NAME} PROPERTIES TIMEOUT 1800)
endforeach()
foreach(SURVEYED "Test EQ Drive" "Test EQ Switch" "Reference EQ" "Reference Utility" "Reference Nonlinear")
    string(REPLACE " " "" SURVEY_TEST_NAME "${SURVEYED}")
    add_test(NAME PluginLabParameterSurvey_${SURVEY_TEST_NAME} COMMAND PluginLabTests --only "Parameter survey (W8.4): ${SURVEYED}")
    set_tests_properties(PluginLabParameterSurvey_${SURVEY_TEST_NAME} PROPERTIES TIMEOUT 1800)
endforeach()

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

# The parameter survey and mapping proposal from the command line (W8.5); the settings folder of the build, not the user's
add_test(NAME PluginLabHostMap
    COMMAND ${CMAKE_COMMAND}
        -DHOST_APPLICATION=$<TARGET_FILE:PluginLabHost>
        -DPLUGIN_FILE=${PLUGINLAB_TEST_PLUGIN_DIR}/PluginLabTestEqSwitch.vst3
        -DREPORT_FILE=${CMAKE_CURRENT_BINARY_DIR}/host_map.md
        -DSETTINGS_FOLDER=${CMAKE_CURRENT_BINARY_DIR}/host_map_settings
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckHostMap.cmake)
set_tests_properties(PluginLabHostMap PROPERTIES TIMEOUT 1200)

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
    # W5d.4/W5d.5: deliberately wrong editors (crash, leak, busy paint, a paint that blocks the audio) and a correct one
    add_test(NAME PluginLabHostGuiRobustness
        COMMAND ${CMAKE_COMMAND}
            -DHOST_APPLICATION=$<TARGET_FILE:PluginLabHost>
            -DPLUGIN_FOLDER=${PLUGINLAB_TEST_PLUGIN_DIR}
            -DFOLDER=${CMAKE_CURRENT_BINARY_DIR}/gui_review_robustness
            -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckHostGuiRobustness.cmake)
    # tests that open windows in the virtual display do not run at the same time under "ctest -j" (the captures would overlap)
    set_tests_properties(PluginLabHostGuiSnapshot PluginLabHostGuiScaling PluginLabHostGuiRobustness PROPERTIES RESOURCE_LOCK display)
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
