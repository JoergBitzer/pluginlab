# pluginlab_core: the code shared by the host application, the scanner, the loader plugin and the tests.
# No GUI code in this library (juce_audio_processors_headless, not juce_audio_processors).
configure_file(Versioning.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/pluginlab/Versioning.h")

add_library(pluginlab_core STATIC
    PluginLabVersion.cpp
    hosting/FormatManager.cpp
    hosting/HostedPlugin.cpp
    hosting/PluginDisplayName.cpp
    hosting/PluginScanner.cpp
    hosting/PluginScanXml.cpp)

target_include_directories(pluginlab_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated")

# JUCE switches that every target using the core must share (curl and the web browser are not needed; without
# JUCE_USE_CURL=0 juce_core wants libcurl headers, which CI runners do not have). JUCE_PLUGINHOST_VST3: VST3 hosting.
target_compile_definitions(pluginlab_core
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1)

# VST2 hosting (JUCE's VSTPluginFormat) needs the VST2 headers (cmake/Vst2Sdk.cmake)
if(PLUGINLAB_VST2)
    target_compile_definitions(pluginlab_core PUBLIC JUCE_PLUGINHOST_VST=1 PLUGINLAB_WITH_VST2=1)
    target_link_libraries(pluginlab_core PUBLIC juce_vst2_sdk)
endif()

target_link_libraries(pluginlab_core
    PUBLIC
        juce::juce_core
        juce::juce_audio_processors_headless
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
