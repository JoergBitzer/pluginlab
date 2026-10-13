# pluginlab_match: the matcher of W9 (docs/design/W9-plan.md): the general layer (parameter space, probe, objective, Levenberg-Marquardt,
# the match problem with its cache and budget), which knows nothing about EQs; the domains (EQ first) supply probe and objective. No GUI, no
# hosting.
add_library(pluginlab_match STATIC
    Match.cpp)

target_include_directories(pluginlab_match
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(pluginlab_match
    PUBLIC
        pluginlab_measure
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
