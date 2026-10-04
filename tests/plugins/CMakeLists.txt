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
function(pluginlab_add_test_plugin TARGET_NAME DISPLAY_NAME PLUGIN_CODE CRASHES CRASHES_IN_PROCESS AUTO_MANIFEST WITH_VST2 MONO_ONLY)
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
            PLUGINLAB_TEST_PLUGIN_MONO_ONLY=${MONO_ONLY})

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endfunction()

pluginlab_add_test_plugin(PluginLabTestGain "PluginLab Test Gain" Tgn1 0 0 TRUE TRUE 0)
pluginlab_add_test_plugin(PluginLabTestCrash "PluginLab Test Crash" Tcr1 1 0 FALSE FALSE 0)
pluginlab_add_test_plugin(PluginLabTestCrashProcess "PluginLab Test Crash Process" Tcp1 0 1 TRUE FALSE 0)
pluginlab_add_test_plugin(PluginLabTestMono "PluginLab Test Mono" Tmn1 0 0 TRUE FALSE 1)
