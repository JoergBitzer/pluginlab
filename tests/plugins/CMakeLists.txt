# Test plugins with known behavior (VST3), copied into ${PLUGINLAB_TEST_PLUGIN_DIR} after the build:
#   PluginLab Test Gain  : four parameters, multiplies the audio with a gain (VST3 and, if available, VST2)
#   PluginLab Test Crash : crashes when it is created (a plugin that cannot be scanned)
#   PluginLab Test Crash Process : scans and loads, but crashes when it processes audio (found only by validation)
# NotAPlugin.vst3 is a text file with the name of a plugin (a file that contains no plugin).
set(PLUGINLAB_TEST_PLUGIN_DIR "${CMAKE_BINARY_DIR}/test_plugins" CACHE INTERNAL "Folder with the test plugins")
file(MAKE_DIRECTORY "${PLUGINLAB_TEST_PLUGIN_DIR}")
file(WRITE "${PLUGINLAB_TEST_PLUGIN_DIR}/NotAPlugin.vst3" "This file is not a plugin.\n")

# AUTO_MANIFEST FALSE: no moduleinfo.json, so JUCE has to load the binary to scan the plugin (with a manifest it reads the
# description from the file and never runs the plugin code)
function(pluginlab_add_test_plugin TARGET_NAME DISPLAY_NAME PLUGIN_CODE CRASHES CRASHES_IN_PROCESS AUTO_MANIFEST WITH_VST2 MONO_ONLY DELAY_SAMPLES REPORTED_LATENCY FIR_TAPS BLOCK_MODE)
    set(TEST_PLUGIN_FORMATS VST3)
    if(WITH_VST2 AND PLUGINLAB_VST2)
        list(APPEND TEST_PLUGIN_FORMATS VST)
    endif()

    juce_add_plugin(${TARGET_NAME}
        COMPANY_NAME "Jade_Hochschule"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        VST3_CATEGORIES "Fx"
        VST3_AUTO_MANIFEST ${AUTO_MANIFEST}
        COPY_PLUGIN_AFTER_BUILD TRUE
        VST3_COPY_DIR "${PLUGINLAB_TEST_PLUGIN_DIR}"
        VST_COPY_DIR "${PLUGINLAB_TEST_PLUGIN_DIR}"
        PLUGIN_MANUFACTURER_CODE IHAJ
        PLUGIN_CODE ${PLUGIN_CODE}
        FORMATS ${TEST_PLUGIN_FORMATS}
        PLUGIN_NAME "${DISPLAY_NAME}"
        PRODUCT_NAME "${TARGET_NAME}")

    target_sources(${TARGET_NAME} PRIVATE TestPlugin.cpp)

    target_compile_definitions(${TARGET_NAME}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            PLUGINLAB_TEST_PLUGIN_CRASHES=${CRASHES}
            PLUGINLAB_TEST_PLUGIN_CRASHES_IN_PROCESS=${CRASHES_IN_PROCESS}
            PLUGINLAB_TEST_PLUGIN_MONO_ONLY=${MONO_ONLY}
            PLUGINLAB_TEST_PLUGIN_DELAY_SAMPLES=${DELAY_SAMPLES}
            PLUGINLAB_TEST_PLUGIN_REPORTED_LATENCY=${REPORTED_LATENCY}
            PLUGINLAB_TEST_PLUGIN_FIR_TAPS=${FIR_TAPS}
            PLUGINLAB_TEST_PLUGIN_BLOCK_MODE=${BLOCK_MODE})

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endfunction()

pluginlab_add_test_plugin(PluginLabTestGain "PluginLab Test Gain" Tgn1 0 0 TRUE TRUE 0 0 0 0 0)
pluginlab_add_test_plugin(PluginLabTestCrash "PluginLab Test Crash" Tcr1 1 0 FALSE FALSE 0 0 0 0 0)
pluginlab_add_test_plugin(PluginLabTestCrashProcess "PluginLab Test Crash Process" Tcp1 0 1 TRUE FALSE 0 0 0 0 0)
pluginlab_add_test_plugin(PluginLabTestMono "PluginLab Test Mono" Tmn1 0 0 TRUE FALSE 1 0 0 0 0)
# a plugin that delays the audio by 64 samples and says so, and one that delays by 100 samples and reports 0 (the latency must be measured)
pluginlab_add_test_plugin(PluginLabTestLatency "PluginLab Test Latency" Tlt1 0 0 TRUE FALSE 0 64 64 0 0)
pluginlab_add_test_plugin(PluginLabTestLatencyLie "PluginLab Test Latency Lie" Tll1 0 0 TRUE FALSE 0 100 0 0 0)
# a linear-phase FIR of 255 taps (latency 127, reported): pre-ringing before the peak
pluginlab_add_test_plugin(PluginLabTestLinearPhase "PluginLab Test Linear Phase" Tlp1 0 0 TRUE FALSE 0 0 127 255 0)
# the gain smoothed once per block (steady state independent of the block size) and a low-pass reset per block (a fault)
pluginlab_add_test_plugin(PluginLabTestBlockSmoothing "PluginLab Test Block Smoothing" Tbs1 0 0 TRUE FALSE 0 0 0 0 1)
pluginlab_add_test_plugin(PluginLabTestBlockFault "PluginLab Test Block Fault" Tbf1 0 0 TRUE FALSE 0 0 0 0 2)

# An RBJ peaking EQ for the fingerprint tests: correct, with the 44.1 kHz fault, with the prepareToPlay fault
function(pluginlab_add_test_eq_plugin TARGET_NAME DISPLAY_NAME PLUGIN_CODE FS_BUG PREPARE_BUG)
    juce_add_plugin(${TARGET_NAME}
        COMPANY_NAME "Jade_Hochschule"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        VST3_CATEGORIES "Fx" "EQ"
        VST3_AUTO_MANIFEST TRUE
        COPY_PLUGIN_AFTER_BUILD TRUE
        VST3_COPY_DIR "${PLUGINLAB_TEST_PLUGIN_DIR}"
        PLUGIN_MANUFACTURER_CODE IHAJ
        PLUGIN_CODE ${PLUGIN_CODE}
        FORMATS VST3
        PLUGIN_NAME "${DISPLAY_NAME}"
        PRODUCT_NAME "${TARGET_NAME}")

    target_sources(${TARGET_NAME} PRIVATE TestEqPlugin.cpp)

    target_compile_definitions(${TARGET_NAME}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            PLUGINLAB_TEST_EQ_FS_BUG=${FS_BUG}
            PLUGINLAB_TEST_EQ_PREPARE_BUG=${PREPARE_BUG})

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endfunction()

pluginlab_add_test_eq_plugin(PluginLabTestEq "PluginLab Test EQ" Teq1 0 0)
pluginlab_add_test_eq_plugin(PluginLabTestEqFs "PluginLab Test EQ Fs Fault" Teq2 1 0)
pluginlab_add_test_eq_plugin(PluginLabTestEqPrepare "PluginLab Test EQ Prepare Fault" Teq3 0 1)
