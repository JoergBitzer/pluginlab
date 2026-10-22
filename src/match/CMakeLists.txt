# pluginlab_match: the matcher of W9 (docs/design/W9-plan.md): the general layer (parameter space, probe, objective, Levenberg-Marquardt,
# the match problem with its cache and budget), which knows nothing about EQs; the domains (the EQ of W9.2 first) supply probe and objective. No GUI, no
# hosting.
add_library(pluginlab_match STATIC
    DynamicsDomain.cpp
    EqDomain.cpp
    Match.cpp
    ReverbDomain.cpp)

target_include_directories(pluginlab_match
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(pluginlab_match
    PUBLIC
        pluginlab_measure
        pluginlab_eqmodel
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
