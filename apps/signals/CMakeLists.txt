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
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PluginLabSignals,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PluginLabSignals,JUCE_VERSION>")

target_link_libraries(PluginLabSignals
    PRIVATE
        pluginlab_signals
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
