# PluginLabSignals: writes the standard test signals of W6.1 (src/signals) as 32-bit float WAV files, with a description of every
# file (signals.txt) and the step lists of the stepped sines (CSV). For listening, teaching, other tools and the oracle.
juce_add_console_app(PluginLabSignals
    PRODUCT_NAME "PluginLabSignals"
    VERSION "${PROJECT_VERSION}")

target_sources(PluginLabSignals
    PRIVATE
        Main.cpp)

target_compile_definitions(PluginLabSignals
    PRIVATE
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PluginLabSignals,JUCE_PRODUCT_NAME>")
# the version is not a compile definition: a new version would recompile every file of the target (with its JUCE modules); the programs
# read it from pluginlab/PluginLabVersion.h (one generated file in pluginlab_core)

# no link-time optimization: a development tool, not shipped (LTO only makes its links slow)
target_link_libraries(PluginLabSignals
    PRIVATE
        pluginlab_signals
        pluginlab_core          # the version (pluginlab/PluginLabVersion.h)
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
