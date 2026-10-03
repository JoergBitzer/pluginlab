# PluginLabHost: the stand-alone host application (W1: an empty window with the version).
juce_add_gui_app(PluginLabHost
    PRODUCT_NAME "PluginLabHost"
    COMPANY_NAME "Jade_Hochschule"
    VERSION "${PROJECT_VERSION}")

target_sources(PluginLabHost
    PRIVATE
        Main.cpp)

target_compile_definitions(PluginLabHost
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PluginLabHost,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PluginLabHost,JUCE_VERSION>")

target_link_libraries(PluginLabHost
    PRIVATE
        pluginlab_core
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
