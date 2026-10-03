# PluginLabLoader: the plugin that loads other plugins (W3): hosts one VST3/VST2 plugin, passes the audio through it, shows the
# plugin browser, the parameters and the editor of the loaded plugin. It is also copied into ${PLUGINLAB_LOADER_PLUGIN_DIR} after the
# build, where the tests find it.
set(PLUGINLAB_LOADER_PLUGIN_DIR "${CMAKE_BINARY_DIR}/loader_plugin" CACHE INTERNAL "Folder with the built loader plugin")
file(MAKE_DIRECTORY "${PLUGINLAB_LOADER_PLUGIN_DIR}")

# The processor and the editor are a static library: the plugin links it and the tests use LoaderProcessor directly (without the
# plugin wrapper of a host in between).
add_library(pluginlab_loader_core STATIC
    LoaderEditor.cpp
    LoaderProcessor.cpp)

target_include_directories(pluginlab_loader_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(pluginlab_loader_core
    PUBLIC
        pluginlab_core
        pluginlab_hosting_ui
        juce::juce_audio_utils
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

juce_add_plugin(PluginLabLoader
    COMPANY_NAME "Jade_Hochschule"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES "Fx" "Tools"
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "${PLUGINLAB_LOADER_PLUGIN_DIR}"
    PLUGIN_MANUFACTURER_CODE IHAJ
    PLUGIN_CODE Plbl
    FORMATS VST3
    PRODUCT_NAME "PluginLabLoader"
    VERSION "${PROJECT_VERSION}")

target_compile_definitions(PluginLabLoader
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PluginLabLoader
    PRIVATE
        pluginlab_loader_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
