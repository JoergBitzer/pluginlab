# pluginlab_engine: the audio engine of the host (W4): audio files, plugin slots that run in parallel on the same input, latency
# measurement and compensation, gapless switching, offline rendering. No GUI code, no audio device.
add_library(pluginlab_engine STATIC
    AudioFileSource.cpp
    ChannelAdapter.cpp
    Fingerprint.cpp
    LatencyMeasurer.cpp
    MeasurementEngine.cpp
    OfflineRenderer.cpp
    ReportText.cpp
    RenderSlot.cpp
    SessionFiles.cpp)

target_include_directories(pluginlab_engine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(pluginlab_engine
    PUBLIC
        pluginlab_core
        juce::juce_audio_formats
        juce::juce_dsp
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
