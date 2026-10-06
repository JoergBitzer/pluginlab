# pluginlab_reference: the reference processors of W6.2 (docs/design/W6-plan.md): linear filters with their exact frequency
# response (RBJ, Orfanidis, Zoelzer, state-variable, Butterworth / Linkwitz-Riley, linear-phase FIR, delays) and the analog
# prototypes. No GUI, no hosting.
add_library(pluginlab_reference STATIC
    Analog.cpp
    Biquad.cpp
    Delays.cpp
    Designs.cpp
    DirectFormFilter.cpp
    LinearPhaseFir.cpp
    LinearProcessor.cpp
    StateVariableFilter.cpp)

target_include_directories(pluginlab_reference
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_definitions(pluginlab_reference
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(pluginlab_reference
    PUBLIC
        juce::juce_audio_basics
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
