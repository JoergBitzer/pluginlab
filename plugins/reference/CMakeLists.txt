# The reference plugins of W6.4 (docs/design/W6-plan.md): PluginLab Reference EQ, Reference Nonlinear and Reference Utility.
# Each is an instance of the AdvancedAudioTemplate (AAT2) in the simple form (no preset bar, no day/night theme): the glue code
# (PluginProcessor.*, PluginEditor.*) comes from the template with the name replaced; tools/ and PluginSettings.h are shared by the
# three. The audio comes from pluginlab_reference (the tested reference processors), the GUI from ReferenceControls.h.
# After the build the VST3 bundles are copied into ${PLUGINLAB_REFERENCE_PLUGIN_DIR} (for the tests); a Release build also installs
# them into the user's VST3 folder (PLUGINLAB_INSTALL_REFERENCE_PLUGINS).
set(PLUGINLAB_REFERENCE_PLUGIN_DIR "${CMAKE_BINARY_DIR}/reference_plugins" CACHE INTERNAL "Folder with the built reference plugins")
file(MAKE_DIRECTORY "${PLUGINLAB_REFERENCE_PLUGIN_DIR}")
configure_file(Versioning.h.in "${CMAKE_CURRENT_BINARY_DIR}/Versioning.h")

# Installing into the user's VST3 folder: only a Release build (a Debug build is slow in a DAW and stops at assertions), not in CI
# and not on Windows (the VST3 folder there needs administrator rights)
set(PLUGINLAB_INSTALL_REFERENCE_DEFAULT OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT DEFINED ENV{CI} AND NOT WIN32)
    set(PLUGINLAB_INSTALL_REFERENCE_DEFAULT ON)
endif()
option(PLUGINLAB_INSTALL_REFERENCE_PLUGINS "Copy the built reference plugins into the user's VST3 folder" ${PLUGINLAB_INSTALL_REFERENCE_DEFAULT})
set(PLUGINLAB_USER_VST3_DIR "$ENV{HOME}/.vst3")
if(APPLE)
    set(PLUGINLAB_USER_VST3_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
endif()

function(pluginlab_add_reference_plugin NAME DISPLAY_NAME PLUGIN_CODE CATEGORIES)
    set(TARGET_NAME PluginLab${NAME})
    juce_add_plugin(${TARGET_NAME}
        COMPANY_NAME "Jade_Hochschule"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        VST3_CATEGORIES ${CATEGORIES}
        AU_MAIN_TYPE "kAudioUnitType_Effect"
        COPY_PLUGIN_AFTER_BUILD TRUE
        VST3_COPY_DIR "${PLUGINLAB_REFERENCE_PLUGIN_DIR}"
        AU_COPY_DIR "${PLUGINLAB_REFERENCE_PLUGIN_DIR}"
        PLUGIN_MANUFACTURER_CODE IHAJ
        PLUGIN_CODE ${PLUGIN_CODE}
        FORMATS VST3 Standalone
        PLUGIN_NAME "${DISPLAY_NAME}"
        PRODUCT_NAME "${TARGET_NAME}")

    juce_generate_juce_header(${TARGET_NAME})

    target_sources(${TARGET_NAME}
        PRIVATE
            ${NAME}/PluginEditor.cpp
            ${NAME}/PluginProcessor.cpp
            ${NAME}/${NAME}.cpp
            tools/MidiModPitchState.cpp
            tools/PresetHandler.cpp
            tools/SynchronBlockProcessor.cpp
            ReferenceControls.cpp)

    target_include_directories(${TARGET_NAME}
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/${NAME}"
            "${CMAKE_CURRENT_BINARY_DIR}")

    target_compile_definitions(${TARGET_NAME}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            pluginlab_reference
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # after JUCE has copied the bundle into PLUGINLAB_REFERENCE_PLUGIN_DIR (its POST_BUILD step comes first)
    if(PLUGINLAB_INSTALL_REFERENCE_PLUGINS)
        add_custom_command(TARGET ${TARGET_NAME}_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGINLAB_USER_VST3_DIR}"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${PLUGINLAB_USER_VST3_DIR}/${TARGET_NAME}.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${PLUGINLAB_REFERENCE_PLUGIN_DIR}/${TARGET_NAME}.vst3" "${PLUGINLAB_USER_VST3_DIR}/${TARGET_NAME}.vst3"
            COMMENT "Installing ${TARGET_NAME}.vst3 into ${PLUGINLAB_USER_VST3_DIR}"
            VERBATIM)
    endif()
endfunction()

pluginlab_add_reference_plugin(ReferenceEq "PluginLab Reference EQ" Rfeq "Fx;EQ")
pluginlab_add_reference_plugin(ReferenceNonlinear "PluginLab Reference Nonlinear" Rfnl "Fx;Distortion")
pluginlab_add_reference_plugin(ReferenceUtility "PluginLab Reference Utility" Rfut "Fx;Tools")
