# JadeBenchmark: the plugin product (W10.2, docs/design/W10-plan.md): the pages Plugins, Compare and Matcher inside a DAW, the DAW's track
# through up to four plugins and the dry reference. The processor and the editor are a static library: the plugin links it and the tests use
# BenchmarkProcessor directly. The bundle carries PluginLabScanner (the plugin browser scans with it) and PluginLabHost (the jobs of the
# pages run in it: a crashing plugin must not take the DAW down); the finished bundle is copied to ${PLUGINLAB_BENCHMARK_PLUGIN_DIR}.
set(PLUGINLAB_BENCHMARK_PLUGIN_DIR "${CMAKE_BINARY_DIR}/benchmark_plugin" CACHE INTERNAL "Folder with the built Jade Benchmark plugin")
file(MAKE_DIRECTORY "${PLUGINLAB_BENCHMARK_PLUGIN_DIR}")

add_library(pluginlab_benchmark_core STATIC
    BenchmarkEditor.cpp
    BenchmarkProcessor.cpp)

target_include_directories(pluginlab_benchmark_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(pluginlab_benchmark_core
    PUBLIC
        pluginlab_pages
        juce::juce_audio_utils
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

juce_add_plugin(JadeBenchmark
    COMPANY_NAME "Jade_Hochschule"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES "Fx" "Tools" "Analyzer"
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE IHAJ
    PLUGIN_CODE Jdbm
    FORMATS VST3
    PLUGIN_NAME "Jade Benchmark"
    PRODUCT_NAME "JadeBenchmark"
    VERSION "${PROJECT_VERSION}")

target_sources(JadeBenchmark
    PRIVATE
        BenchmarkEntry.cpp)

target_compile_definitions(JadeBenchmark
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(JadeBenchmark
    PRIVATE
        pluginlab_benchmark_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# the scanner and the host next to the plugin binary in the bundle (Contents/<platform>/), then the bundle to the plugin folder
get_target_property(PLUGINLAB_BENCHMARK_BUNDLE JadeBenchmark_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
add_custom_target(JadeBenchmarkBundle ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:PluginLabScanner> $<TARGET_FILE_DIR:JadeBenchmark_VST3>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:PluginLabHost> $<TARGET_FILE_DIR:JadeBenchmark_VST3>
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${PLUGINLAB_BENCHMARK_PLUGIN_DIR}/JadeBenchmark.vst3"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PLUGINLAB_BENCHMARK_BUNDLE}" "${PLUGINLAB_BENCHMARK_PLUGIN_DIR}/JadeBenchmark.vst3"
    COMMENT "Putting PluginLabScanner and PluginLabHost into the Jade Benchmark bundle"
    VERBATIM)
add_dependencies(JadeBenchmarkBundle JadeBenchmark_VST3 PluginLabScanner PluginLabHost)
