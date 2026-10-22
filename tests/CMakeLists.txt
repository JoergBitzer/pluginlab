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
        LiveResponseTests.cpp
        LoaderTests.cpp
        ZipperNoiseTests.cpp
        MappingTests.cpp
        MatchEqTests.cpp
        MatchPluginTests.cpp
        MatchTests.cpp
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
        PresetTests.cpp
        NonlinearReferenceTests.cpp
        OracleTests.cpp
        ParameterSurveyTests.cpp
        PluginMatchTests.cpp
        BandFitTests.cpp
        BenchmarkTests.cpp
        CalibrationTests.cpp
        CrashProtectionTests.cpp
        BrowserTests.cpp
        CatalogTests.cpp
        DelayTrajectoryTests.cpp
        DynamicsTests.cpp
        EasyTests.cpp
        EchoResponseTests.cpp
        EngineTests.cpp
        ExpertTests.cpp
        EqModelTests.cpp
        FingerprintTests.cpp
        GuiReviewTests.cpp
        ProbeTests.cpp
        RealWorldTests.cpp
        ReferencePluginTests.cpp
        ReferenceTests.cpp
        ReverbTests.cpp
        SignalsTests.cpp
        TempoTests.cpp)

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
        PLUGINLAB_HOST_EXECUTABLE="$<TARGET_FILE:PluginLabHost>"
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PluginLabTests,JUCE_PRODUCT_NAME>")
# the version is not a compile definition: a new version would recompile every file of the target (with its JUCE modules); the programs
# read it from pluginlab/PluginLabVersion.h (one generated file in pluginlab_core)

target_link_libraries(PluginLabTests
    PRIVATE
        pluginlab_core
        pluginlab_engine
        pluginlab_eqmodel
        pluginlab_match
        pluginlab_signals
        pluginlab_reference
        pluginlab_measure
        pluginlab_hosting_ui
        pluginlab_loader_core
        pluginlab_benchmark_core
        pluginlab_pages
        juce::juce_core
        juce::juce_dsp
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# the tests need the scanner and the test plugins built first
add_dependencies(PluginLabTests PluginLabScanner PluginLabSignals PluginLabTestGain_VST3 PluginLabTestGainRamp_VST3 PluginLabTestCrash_VST3 PluginLabTestCrashProcess_VST3 PluginLabHost PluginLabLoaderBundle PluginLabReferenceEq_VST3 PluginLabReferenceNonlinear_VST3 PluginLabReferenceUtility_VST3 PluginLabReferenceDelay_VST3 PluginLabReferenceReverb_VST3)
if(PLUGINLAB_VST2)
    add_dependencies(PluginLabTests PluginLabTestGain_VST)
endif()

add_test(NAME PluginLabTests COMMAND PluginLabTests --skip "Parameter survey|PluginFingerprint|Real-time fingerprint|Calibration (W8.6)|Interactions (W8.7)|Fast render path (W9.3)|Plugin matching (W9.4)|Match attribution (W9.5)|Easy page (W10.4)|Expert page (W10.5)|Measurement presets (W10.6)|Crash protection (W10.7)|Jade Benchmark (W10.2)|EQ band identification|Matcher, EQ domain (W9.2)|Measure: system class|Measure: plugins through the host|Measure: null test|Live transfer function display (W10.3)|Echo response (W11.1)|Reverb (W11.2)|Dynamics (W11.3)")
# a hanging test (a dialog nobody can click, a plugin that never returns) must fail soon, not after the default 25 minutes
set_tests_properties(PluginLabTests PROPERTIES TIMEOUT 1200 RESOURCE_LOCK display) # about 100 s in Debug since the split of 2026-10-21 (was 580 s)

# the longest groups of PluginLabTests as tests of their own, so that "ctest -j" runs them side by side (PluginLabTests prints the time of each
# group at its end: split again when one grows); times in Debug on the development machine, 2026-10-21
foreach(GROUP "Jade Benchmark (W10.2)" "EQ band identification" "Matcher, EQ domain (W9.2)" "Measure: system class"
              "Measure: plugins through the host" "Measure: null test" "Echo response (W11.1)" "Reverb (W11.2)" "Dynamics (W11.3)")
    string(REGEX REPLACE "[^A-Za-z0-9]" "" GROUP_TEST_NAME "${GROUP}")
    add_test(NAME PluginLab_${GROUP_TEST_NAME} COMMAND PluginLabTests --only "${GROUP}") # 150, 109, 63, 50, 46, 39, 38 s
    set_tests_properties(PluginLab_${GROUP_TEST_NAME} PROPERTIES TIMEOUT 900)
endforeach()
# the live transfer functions (W10.3) check how fast a curve follows a knob (0.3 s): alone, so that other tests do not slow them down
add_test(NAME PluginLabLiveResponse COMMAND PluginLabTests --only "Live transfer function display (W10.3)")
set_tests_properties(PluginLabLiveResponse PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
# the fingerprint measures many test plugins (with the system class at A and B since 0.49.0): a test with a limit of its own; not at the same
# time as PluginLabTests (the lock: both are long and render much)
add_test(NAME PluginLabFingerprint COMMAND PluginLabTests --only "PluginFingerprint")
set_tests_properties(PluginLabFingerprint PROPERTIES TIMEOUT 1200 RESOURCE_LOCK display)
# the real-time plugins of the fingerprint (timer, offline flag, wall clock): their checks compare wall-clock timing, so they run alone
add_test(NAME PluginLabFingerprintRealTime COMMAND PluginLabTests --only "Real-time fingerprint")
set_tests_properties(PluginLabFingerprintRealTime PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
# the parameter survey (W8.4) renders some hundred times per plugin: one entry per plugin with its own limit (Debug: 1 ... 9 minutes each), so
# that "ctest -j" runs them side by side
# the fast render path of the matcher (W9.3): plugins against the careful delivery
add_test(NAME PluginLabFastRenderPath COMMAND PluginLabTests --only "Fast render path (W9.3)")
set_tests_properties(PluginLabFastRenderPath PROPERTIES TIMEOUT 1200 RUN_SERIAL TRUE) # it checks the time per render: alone
# matching plugins (W9.4): the test pairs of the plan
add_test(NAME PluginLabPluginMatching COMMAND PluginLabTests --only "Plugin matching (W9.4)")
set_tests_properties(PluginLabPluginMatching PROPERTIES TIMEOUT 1800)
# why a match misses the tolerance (W9.5): a knob at its end, another family, a band missing, interacting bands
add_test(NAME PluginLabMatchAttribution COMMAND PluginLabTests --only "Match attribution (W9.5)")
set_tests_properties(PluginLabMatchAttribution PROPERTIES TIMEOUT 1800)

# the pages of W10 that start PluginLabHost in child processes (match, expert measurements, the live worker): minutes each
add_test(NAME PluginLabEasyPage COMMAND PluginLabTests --only "Easy page (W10.4)")
set_tests_properties(PluginLabEasyPage PROPERTIES TIMEOUT 1200)
add_test(NAME PluginLabExpertPage COMMAND PluginLabTests --only "Expert page (W10.5)")
set_tests_properties(PluginLabExpertPage PROPERTIES TIMEOUT 1800)
add_test(NAME PluginLabMeasurementPresets COMMAND PluginLabTests --only "Measurement presets (W10.6)")
set_tests_properties(PluginLabMeasurementPresets PROPERTIES TIMEOUT 1200)
add_test(NAME PluginLabCrashProtection COMMAND PluginLabTests --only "Crash protection (W10.7)")
set_tests_properties(PluginLabCrashProtection PROPERTIES TIMEOUT 1200)
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
    set_tests_properties(PluginLabParameterSurvey_${SURVEY_TEST_NAME} PROPERTIES TIMEOUT 1800 LABELS long)
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

# the matcher from the command line (W9.4): a job file, two targets
add_test(NAME PluginLabHostMatch
    COMMAND ${CMAKE_COMMAND}
        -DHOST_APPLICATION=$<TARGET_FILE:PluginLabHost>
        -DPLUGIN_FOLDER=${PLUGINLAB_TEST_PLUGIN_DIR}
        -DREPORT_FILE=${CMAKE_CURRENT_BINARY_DIR}/host_match.md
        -DSETTINGS_FOLDER=${CMAKE_CURRENT_BINARY_DIR}/host_match_settings
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckHostMatch.cmake)
set_tests_properties(PluginLabHostMatch PROPERTIES TIMEOUT 900)

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
    set_tests_properties(PluginLabHostGuiScaling PluginLabHostGuiRobustness PROPERTIES LABELS long)
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

# Two levels (2026-10-21): the quick run for everyday work leaves out the tests labelled "long" (tools/ctest_offscreen.sh -LE long, a few
# minutes); the full run (all tests) before a work package is committed, and in CI
set_tests_properties(PluginLabFingerprint PluginLabFingerprintRealTime PluginLabPluginMatching PluginLabMatchAttribution PluginLabHostMap
                     PluginLabHostFingerprint PROPERTIES LABELS long)
foreach(COMBINED "Test EQ Two Bands" "Test EQ Parallel" "Reference EQ")
    string(REPLACE " " "" INTERACTION_TEST_NAME "${COMBINED}")
    set_tests_properties(PluginLabInteractions_${INTERACTION_TEST_NAME} PROPERTIES LABELS long)
endforeach()
foreach(CALIBRATED "Test EQ" "Test EQ Fs Fault" "Test EQ Knob Faults" "Reference EQ")
    string(REPLACE " " "" CALIBRATION_TEST_NAME "${CALIBRATED}")
    set_tests_properties(PluginLabCalibration_${CALIBRATION_TEST_NAME} PROPERTIES LABELS long)
endforeach()
