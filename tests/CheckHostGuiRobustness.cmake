# Runs "PluginLabHost --gui-snapshot" on the deliberately wrong editors of W5d.4/W5d.5 and checks that the review finds what is wrong.
# Arguments: HOST_APPLICATION, PLUGIN_FOLDER, FOLDER
function(run_review NAME)
    file(REMOVE_RECURSE "${FOLDER}/${NAME}")
    execute_process(
        COMMAND "${HOST_APPLICATION}" --gui-snapshot "${PLUGIN_FOLDER}/PluginLabTest${NAME}.vst3" "${FOLDER}/${NAME}"
        RESULT_VARIABLE HOST_RESULT
        OUTPUT_QUIET ERROR_QUIET
        TIMEOUT 300)
    set(HOST_RESULT "${HOST_RESULT}" PARENT_SCOPE)
endfunction()

function(read_number NAME KEY OUT)
    file(READ "${FOLDER}/${NAME}/gui_robustness.json" JSON_TEXT)
    string(JSON VALUE GET "${JSON_TEXT}" "${KEY}")
    set(${OUT} "${VALUE}" PARENT_SCOPE)
endfunction()

# a crashing editor ends the process; the progress file names the step (the Developer page shows it, the host survives because it runs this in a child process)
run_review(EditorCrashes)
if(HOST_RESULT EQUAL 0)
    message(FATAL_ERROR "EditorCrashes: the review must not succeed")
endif()
file(READ "${FOLDER}/EditorCrashes/progress.txt" PROGRESS)
if(NOT PROGRESS STREQUAL "opening the editor")
    message(FATAL_ERROR "EditorCrashes: progress is '${PROGRESS}'")
endif()

run_review(EditorLeaks)
read_number(EditorLeaks probableLeak LEAK)
if(NOT LEAK STREQUAL "ON" AND NOT LEAK STREQUAL "true")
    message(FATAL_ERROR "EditorLeaks: no leak found (${LEAK})")
endif()

run_review(EditorBusy)
read_number(EditorBusy idleLoadPercent LOAD)
if(LOAD LESS 30)
    message(FATAL_ERROR "EditorBusy: idle load only ${LOAD} %")
endif()

run_review(EditorBlocksAudio)
read_number(EditorBlocksAudio lateBlocksWithEditor LATE)
if(LATE LESS 5)
    message(FATAL_ERROR "EditorBlocksAudio: only ${LATE} late blocks")
endif()

# a correct editor: no leak, no late block, little load
run_review(EditorScales)
read_number(EditorScales probableLeak LEAK)
read_number(EditorScales lateBlocksWithEditor LATE)
read_number(EditorScales idleLoadPercent LOAD)
if(LEAK STREQUAL "ON" OR LEAK STREQUAL "true" OR LATE GREATER 2 OR LOAD GREATER 15)
    message(FATAL_ERROR "EditorScales: leak ${LEAK}, late blocks ${LATE}, load ${LOAD} %")
endif()
