# PluginLabTeachingFigures: writes the figures of the teaching pages (docs/teaching/references) as SVG, computed with pluginlab_reference
# (so that every curve on a page is what the tested library computes). Run: PluginLabTeachingFigures docs/teaching/references/figures
juce_add_console_app(PluginLabTeachingFigures
    PRODUCT_NAME "PluginLabTeachingFigures"
    VERSION "${PROJECT_VERSION}")

target_sources(PluginLabTeachingFigures
    PRIVATE
        Main.cpp
        SvgPlot.cpp)

target_compile_definitions(PluginLabTeachingFigures
    PRIVATE
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PluginLabTeachingFigures,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PluginLabTeachingFigures,JUCE_VERSION>")

target_link_libraries(PluginLabTeachingFigures
    PRIVATE
        pluginlab_reference
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
