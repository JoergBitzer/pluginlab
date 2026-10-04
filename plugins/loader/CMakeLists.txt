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
    COPY_PLUGIN_AFTER_BUILD FALSE
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

# The loader scans plugins with PluginLabScanner, which is looked for next to the plugin binary: the scanner goes into the bundle
# (Contents/<platform>/), then the finished bundle is copied to ${PLUGINLAB_LOADER_PLUGIN_DIR} (for the tests and for a quick install by
# hand). JUCE's own copy step is off because it would run before the scanner is in the bundle.
get_target_property(PLUGINLAB_LOADER_BUNDLE PluginLabLoader_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
add_custom_target(PluginLabLoaderBundle ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:PluginLabScanner> $<TARGET_FILE_DIR:PluginLabLoader_VST3>
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${PLUGINLAB_LOADER_PLUGIN_DIR}/PluginLabLoader.vst3"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PLUGINLAB_LOADER_BUNDLE}" "${PLUGINLAB_LOADER_PLUGIN_DIR}/PluginLabLoader.vst3"
    COMMENT "Putting PluginLabScanner into the loader bundle"
    VERBATIM)
add_dependencies(PluginLabLoaderBundle PluginLabLoader_VST3 PluginLabScanner)

# Every build installs the loader where a DAW on this machine finds it (user folder, no administrator rights needed). Off in CI (no DAW
# there) and on Windows (the VST3 folder needs administrator rights): set PLUGINLAB_INSTALL_LOADER to ON/OFF to override.
set(PLUGINLAB_INSTALL_LOADER_DEFAULT ON)
if(DEFINED ENV{CI} OR WIN32)
    set(PLUGINLAB_INSTALL_LOADER_DEFAULT OFF)
endif()
option(PLUGINLAB_INSTALL_LOADER "Copy the built loader into the user's VST3 folder after every build" ${PLUGINLAB_INSTALL_LOADER_DEFAULT})
if(PLUGINLAB_INSTALL_LOADER)
    set(PLUGINLAB_USER_VST3_DIR "$ENV{HOME}/.vst3")
    if(APPLE)
        set(PLUGINLAB_USER_VST3_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
    endif()
    add_custom_command(TARGET PluginLabLoaderBundle POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGINLAB_USER_VST3_DIR}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${PLUGINLAB_USER_VST3_DIR}/PluginLabLoader.vst3"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PLUGINLAB_LOADER_PLUGIN_DIR}/PluginLabLoader.vst3" "${PLUGINLAB_USER_VST3_DIR}/PluginLabLoader.vst3"
        COMMENT "Installing the loader into ${PLUGINLAB_USER_VST3_DIR}"
        VERBATIM)
endif()
