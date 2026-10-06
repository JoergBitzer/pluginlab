# pluginlab_reference: the reference processors of W6.2 and W6.3 (docs/design/W6-plan.md): linear filters with their exact frequency
# response (RBJ, Orfanidis, Zoelzer, state-variable, Butterworth / Linkwitz-Riley, linear-phase FIR, delays) and the analog
# prototypes; nonlinear processors with known harmonics (polynomial, clippers, quantizer) and utilities (gain, channel matrix, DC, hum,
# noise, tremolo). No GUI, no hosting.
add_library(pluginlab_reference STATIC
    Analog.cpp
    Biquad.cpp
    Delays.cpp
    Designs.cpp
    DirectFormFilter.cpp
    LinearPhaseFir.cpp
    LinearProcessor.cpp
    Nonlinear.cpp
    Processor.cpp
    StateVariableFilter.cpp
    Utility.cpp)

target_include_directories(pluginlab_reference
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_definitions(pluginlab_reference
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(pluginlab_reference
    PUBLIC
        pluginlab_signals
        juce::juce_audio_basics
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
