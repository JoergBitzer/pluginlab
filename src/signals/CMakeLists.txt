# pluginlab_signals: the test signals of W6.1 (docs/design/W6-plan.md): sines, the synchronized swept sine with its analytic inverse
# filter, the stepped sine, two-tone, multitone, noise, bursts, WAV export. No GUI, no hosting.
add_library(pluginlab_signals STATIC
    Signals.cpp
    SteppedSine.cpp
    SweptSine.cpp)

target_include_directories(pluginlab_signals
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_definitions(pluginlab_signals
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(pluginlab_signals
    PUBLIC
        juce::juce_audio_basics
        juce::juce_audio_formats
    PRIVATE
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
