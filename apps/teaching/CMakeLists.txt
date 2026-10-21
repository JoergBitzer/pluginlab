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
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PluginLabTeachingFigures,JUCE_PRODUCT_NAME>")
# the version is not a compile definition: a new version would recompile every file of the target (with its JUCE modules); the programs
# read it from pluginlab/PluginLabVersion.h (one generated file in pluginlab_core)

# no link-time optimization: a development tool, not shipped (LTO only makes its links slow)
target_link_libraries(PluginLabTeachingFigures
    PRIVATE
        pluginlab_reference
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
