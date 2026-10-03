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

target_link_libraries(pluginlab_core
    PUBLIC
        juce::juce_core
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
