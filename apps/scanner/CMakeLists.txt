# PluginLabScanner: examines one plugin file and writes the plugin descriptions as XML. It runs as a separate process, so
# that a plugin that crashes when it is loaded cannot take the host down (see docs/design/W2-host-part1.md).
juce_add_console_app(PluginLabScanner
    PRODUCT_NAME "PluginLabScanner"
    VERSION "${PROJECT_VERSION}")

target_sources(PluginLabScanner
    PRIVATE
        Main.cpp)

target_compile_definitions(PluginLabScanner
    PRIVATE
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PluginLabScanner,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PluginLabScanner,JUCE_VERSION>")

target_link_libraries(PluginLabScanner
    PRIVATE
        pluginlab_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
