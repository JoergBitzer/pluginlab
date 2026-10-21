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
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PluginLabScanner,JUCE_PRODUCT_NAME>")
# the version is not a compile definition: a new version would recompile every file of the target (with its JUCE modules); the programs
# read it from pluginlab/PluginLabVersion.h (one generated file in pluginlab_core)

target_link_libraries(PluginLabScanner
    PRIVATE
        pluginlab_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
