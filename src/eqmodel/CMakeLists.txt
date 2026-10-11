# pluginlab_eqmodel: the EQ band model of W8.2 (docs/eqmodel/band-model.md): bands (type, frequency, gain, Q, order, design family), their exact
# responses from the designs of pluginlab_reference, models of several bands, Q / bandwidth / shelf-slope conversions. Used by the Reference EQ
# plugin and (W8.3) by the band identification. No GUI, no hosting.
add_library(pluginlab_eqmodel STATIC
    EqModel.cpp)

target_include_directories(pluginlab_eqmodel
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(pluginlab_eqmodel
    PUBLIC
        pluginlab_reference
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
