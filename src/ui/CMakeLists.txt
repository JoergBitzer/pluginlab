# pluginlab_hosting_ui: GUI components shared by the host application and the loader plugin (plugin browser with scanning and
# validation, parameter table, formats with editor support), the image variants of the GUI review (W5d) and the spectrogram picture of the reports.
add_library(pluginlab_hosting_ui STATIC
    GuiFormats.cpp
    GuiReview.cpp
    Logo.cpp
    PluginEditorFactory.cpp
    ParameterTableComponent.cpp
    PluginEditorWindow.cpp
    PluginBrowserComponent.cpp
    ResponsePlotImage.cpp
    SpectrogramImage.cpp)

target_include_directories(pluginlab_hosting_ui
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(pluginlab_hosting_ui
    PUBLIC
        pluginlab_core
        pluginlab_measure
        juce::juce_gui_basics
        juce::juce_audio_processors
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
