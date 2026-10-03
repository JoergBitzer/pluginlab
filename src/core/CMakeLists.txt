# pluginlab_core: the code shared by the host application, the loader plugin and the tests.
# No GUI code in this library; JUCE modules are added when a work package needs them.
configure_file(Versioning.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/pluginlab/Versioning.h")

add_library(pluginlab_core STATIC
    PluginLabVersion.cpp)

target_include_directories(pluginlab_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated")

# JUCE switches that every target using the core must share (curl and the web browser are not needed; without
# JUCE_USE_CURL=0 juce_core wants libcurl headers, which CI runners do not have)
target_compile_definitions(pluginlab_core
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(pluginlab_core
    PUBLIC
        juce::juce_core
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
