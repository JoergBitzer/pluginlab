# pluginlab_pages: the pages of the two products (W10.1, docs/design/W10-plan.md 3.1): Plugins, Compare, Matcher and Developer, with the
# settings folder they share and what they need from the shell (PageServices: the program that runs their jobs). The stand-alone application
# shows all of them; the plugin (W10.2) all but the Developer page. The pages know nothing about being in an application or a plugin.
add_library(pluginlab_pages STATIC
    ComparePanel.cpp
    DeveloperPanel.cpp
    HostMainComponent.cpp
    HostSettings.cpp
    LiveResponseView.cpp
    MatcherPanel.cpp
    PageServices.cpp)

target_include_directories(pluginlab_pages
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(pluginlab_pages
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(pluginlab_pages
    PUBLIC
        pluginlab_core
        pluginlab_engine
        pluginlab_hosting_ui
        pluginlab_match
        juce::juce_gui_basics
        juce::juce_audio_processors
        juce::juce_audio_utils
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
