# pluginlab_measure: the measurement units of W7 (docs/design/W7-plan.md, one document per unit in docs/measurements/): the device under test,
# the analyzer of AES17-2015 (level meter, standard filters) and the units. No hosting, no GUI.
add_library(pluginlab_measure STATIC
    Analyzer.cpp
    Device.cpp
    Gain.cpp)

target_include_directories(pluginlab_measure
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_definitions(pluginlab_measure
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(pluginlab_measure
    PUBLIC
        pluginlab_signals
        pluginlab_reference
        juce::juce_audio_basics
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
