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
# An optional 17th argument EDITOR_MODE (W5d): 0 the generic editor, 1 a drawn editor that follows the host's scale factor, 2 one that ignores it,
# 3 one that changes only its size (the drawing keeps its size)
function(pluginlab_add_test_plugin TARGET_NAME DISPLAY_NAME PLUGIN_CODE CRASHES CRASHES_IN_PROCESS AUTO_MANIFEST WITH_VST2 MONO_ONLY DELAY_SAMPLES REPORTED_LATENCY FIR_TAPS BLOCK_MODE STEREO_MODE SIDECHAIN LFO TIME_MODE)
    set(EDITOR_MODE 0)
    if(ARGC GREATER 16)
        set(EDITOR_MODE ${ARGV16})
    endif()
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
            PLUGINLAB_TEST_PLUGIN_BLOCK_MODE=${BLOCK_MODE}
            PLUGINLAB_TEST_PLUGIN_STEREO_MODE=${STEREO_MODE}
            PLUGINLAB_TEST_PLUGIN_SIDECHAIN=${SIDECHAIN}
            PLUGINLAB_TEST_PLUGIN_LFO=${LFO}
            PLUGINLAB_TEST_PLUGIN_TIME_MODE=${TIME_MODE}
            PLUGINLAB_TEST_PLUGIN_EDITOR_MODE=${EDITOR_MODE})

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endfunction()

pluginlab_add_test_plugin(PluginLabTestGain "PluginLab Test Gain" Tgn1 0 0 TRUE TRUE 0 0 0 0 0 0 0 0 0)
pluginlab_add_test_plugin(PluginLabTestCrash "PluginLab Test Crash" Tcr1 1 0 FALSE FALSE 0 0 0 0 0 0 0 0 0)
pluginlab_add_test_plugin(PluginLabTestCrashProcess "PluginLab Test Crash Process" Tcp1 0 1 TRUE FALSE 0 0 0 0 0 0 0 0 0)
pluginlab_add_test_plugin(PluginLabTestMono "PluginLab Test Mono" Tmn1 0 0 TRUE FALSE 1 0 0 0 0 0 0 0 0)
# a plugin that delays the audio by 64 samples and says so, and one that delays by 100 samples and reports 0 (the latency must be measured)
pluginlab_add_test_plugin(PluginLabTestLatency "PluginLab Test Latency" Tlt1 0 0 TRUE FALSE 0 64 64 0 0 0 0 0 0)
pluginlab_add_test_plugin(PluginLabTestLatencyLie "PluginLab Test Latency Lie" Tll1 0 0 TRUE FALSE 0 100 0 0 0 0 0 0 0)
# a linear-phase FIR of 255 taps (latency 127, reported): pre-ringing before the peak
pluginlab_add_test_plugin(PluginLabTestLinearPhase "PluginLab Test Linear Phase" Tlp1 0 0 TRUE FALSE 0 0 127 255 0 0 0 0 0)
# the gain smoothed once per block (steady state independent of the block size) and a low-pass reset per block (a fault)
pluginlab_add_test_plugin(PluginLabTestBlockSmoothing "PluginLab Test Block Smoothing" Tbs1 0 0 TRUE FALSE 0 0 0 0 1 0 0 0 0)
pluginlab_add_test_plugin(PluginLabTestBlockFault "PluginLab Test Block Fault" Tbf1 0 0 TRUE FALSE 0 0 0 0 2 0 0 0 0)
# stereo behaviour: cross feed (R += L / 2) and a width control (the gain acts on L - R only), and a plugin with a side-chain input
pluginlab_add_test_plugin(PluginLabTestCrossFeed "PluginLab Test Cross Feed" Tcf1 0 0 TRUE FALSE 0 0 0 0 0 1 0 0 0)
pluginlab_add_test_plugin(PluginLabTestWidth "PluginLab Test Width" Twd1 0 0 TRUE FALSE 0 0 0 0 0 2 0 0 0)
pluginlab_add_test_plugin(PluginLabTestSideChain "PluginLab Test Side Chain" Tsc1 0 0 TRUE FALSE 0 0 0 0 0 0 1 0 0)
# a tremolo with a free-running LFO: time-varying
pluginlab_add_test_plugin(PluginLabTestTremolo "PluginLab Test Tremolo" Ttr1 0 0 TRUE FALSE 0 0 0 0 0 0 0 1 0)
# real-time behaviour (W5c): the gain reaches the audio only through a 100 ms timer on the message thread; +6 dB when rendering offline;
# a tremolo on the system clock
pluginlab_add_test_plugin(PluginLabTestTimerGain "PluginLab Test Timer Gain" Ttg1 0 0 TRUE FALSE 0 0 0 0 0 0 0 0 1)
pluginlab_add_test_plugin(PluginLabTestOfflineSwitch "PluginLab Test Offline Switch" Tos1 0 0 TRUE FALSE 0 0 0 0 0 0 0 0 2)
pluginlab_add_test_plugin(PluginLabTestWallClock "PluginLab Test Wall Clock" Twc1 0 0 TRUE FALSE 0 0 0 0 0 0 0 0 3)
# editors for the GUI review (W5d.3): follows the scale factor, ignores it, changes only its size
pluginlab_add_test_plugin(PluginLabTestEditorScales "PluginLab Test Editor Scales" Tes1 0 0 TRUE FALSE 0 0 0 0 0 0 0 0 0 1)
pluginlab_add_test_plugin(PluginLabTestEditorIgnoresScale "PluginLab Test Editor Ignores Scale" Tei1 0 0 TRUE FALSE 0 0 0 0 0 0 0 0 0 2)
pluginlab_add_test_plugin(PluginLabTestEditorSizeOnly "PluginLab Test Editor Size Only" Tez1 0 0 TRUE FALSE 0 0 0 0 0 0 0 0 0 3)

# An RBJ peaking EQ for the fingerprint tests: correct, with the 44.1 kHz fault, with the prepareToPlay fault
function(pluginlab_add_test_eq_plugin TARGET_NAME DISPLAY_NAME PLUGIN_CODE FS_BUG PREPARE_BUG ENABLE_SWITCH)
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
            PLUGINLAB_TEST_EQ_PREPARE_BUG=${PREPARE_BUG}
            PLUGINLAB_TEST_EQ_ENABLE_SWITCH=${ENABLE_SWITCH})

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endfunction()

pluginlab_add_test_eq_plugin(PluginLabTestEq "PluginLab Test EQ" Teq1 0 0 0)
pluginlab_add_test_eq_plugin(PluginLabTestEqFs "PluginLab Test EQ Fs Fault" Teq2 1 0 0)
pluginlab_add_test_eq_plugin(PluginLabTestEqPrepare "PluginLab Test EQ Prepare Fault" Teq3 0 1 0)
# a band that is off by default ("Enabled"): the gain changes the audio only together with the switch
pluginlab_add_test_eq_plugin(PluginLabTestEqSwitch "PluginLab Test EQ Switch" Teq4 0 0 1)
