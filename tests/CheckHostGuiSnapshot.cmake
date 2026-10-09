# Runs "PluginLabHost --gui-snapshot <plugin file> <folder>" on the Reference EQ (W5d) and checks the review: the X window capture has content
# (JUCE's component snapshot of a hosted editor is empty on Linux), the vision contact sheet exists. Arguments: HOST_APPLICATION, PLUGIN_FILE, FOLDER
file(REMOVE_RECURSE "${FOLDER}")

execute_process(
    COMMAND "${HOST_APPLICATION}" --gui-snapshot "${PLUGIN_FILE}" "${FOLDER}"
    RESULT_VARIABLE HOST_RESULT
    TIMEOUT 240)

if(NOT HOST_RESULT EQUAL 0)
    message(FATAL_ERROR "PluginLabHost exited with '${HOST_RESULT}'")
endif()
foreach(EXPECTED_FILE "gui_review.md" "contact_sheet_vision.png" "contact_sheet_sizes.png" "scale_1_window.png")
    if(NOT EXISTS "${FOLDER}/${EXPECTED_FILE}")
        message(FATAL_ERROR "PluginLabHost did not write ${EXPECTED_FILE}")
    endif()
endforeach()
file(READ "${FOLDER}/gui_review.md" REVIEW)
foreach(EXPECTED "# GUI review: PluginLab Reference EQ" "| scale_1 | 640 x 400 |" "X window |" "it reacts to the host's scale factor")
    string(FIND "${REVIEW}" "${EXPECTED}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "The review does not contain '${EXPECTED}'")
    endif()
endforeach()
