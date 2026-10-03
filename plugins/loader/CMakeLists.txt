# PluginLabLoader: the plugin that will load other plugins (W3). W1: a pass-through effect (VST3), so that building,
# pluginval and the CI steps for plugins exist from the start.
juce_add_plugin(PluginLabLoader
    COMPANY_NAME "Jade_Hochschule"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES "Fx" "Tools"
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE IHAJ
    PLUGIN_CODE Plbl
    FORMATS VST3
    PRODUCT_NAME "PluginLabLoader"
    VERSION "${PROJECT_VERSION}")

target_sources(PluginLabLoader
    PRIVATE
        LoaderProcessor.cpp)

target_compile_definitions(PluginLabLoader
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PluginLabLoader
    PRIVATE
        pluginlab_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
