# PluginLabHost: the stand-alone host application. W2: plugin list (scanned by PluginLabScanner processes), load/unload,
# parameter table, editor windows; command line modes --write-version and --report for CI.
juce_add_gui_app(PluginLabHost
    PRODUCT_NAME "PluginLabHost"
    COMPANY_NAME "Jade_Hochschule"
    VERSION "${PROJECT_VERSION}")

target_sources(PluginLabHost
    PRIVATE
        Main.cpp
        ComparePanel.cpp
        DeveloperPanel.cpp
        HostMainComponent.cpp
        HostSettings.cpp
        HostShell.cpp
        HostFingerprint.cpp
        HostGuiSnapshot.cpp
        HostReport.cpp)

target_compile_definitions(PluginLabHost
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PluginLabHost,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PluginLabHost,JUCE_VERSION>")

# the scanner is started by the host and is expected in the folder of the host executable (on macOS inside the app bundle):
# build it first and copy it there after the host is built
add_dependencies(PluginLabHost PluginLabScanner)
add_custom_command(TARGET PluginLabHost POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:PluginLabScanner> $<TARGET_FILE_DIR:PluginLabHost>
    COMMENT "Copying PluginLabScanner next to PluginLabHost")

target_link_libraries(PluginLabHost
    PRIVATE
        pluginlab_core
        pluginlab_engine
        pluginlab_hosting_ui
        juce::juce_gui_basics
        juce::juce_audio_processors
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
