# Runs "PluginLabHost --gui-snapshot" on the three test editors of W5d.3 and checks the judgement of the scale factor.
# Arguments: HOST_APPLICATION, PLUGIN_FOLDER, FOLDER
foreach(CASE "EditorScales|follows (size and content)" "EditorIgnoresScale|ignores the scale factor" "EditorSizeOnly|size only (the content does not scale uniformly)")
    string(REPLACE "|" ";" PARTS "${CASE}")
    list(GET PARTS 0 NAME)
    list(GET PARTS 1 EXPECTED)
    file(REMOVE_RECURSE "${FOLDER}/${NAME}")
    execute_process(
        COMMAND "${HOST_APPLICATION}" --gui-snapshot "${PLUGIN_FOLDER}/PluginLabTest${NAME}.vst3" "${FOLDER}/${NAME}"
        RESULT_VARIABLE HOST_RESULT
        TIMEOUT 240)
    if(NOT HOST_RESULT EQUAL 0)
        message(FATAL_ERROR "PluginLabHost exited with '${HOST_RESULT}' for ${NAME}")
    endif()
    file(READ "${FOLDER}/${NAME}/gui_review.md" REVIEW)
    string(FIND "${REVIEW}" "| 2 |" ROW)
    string(SUBSTRING "${REVIEW}" ${ROW} 200 ROW_TEXT)
    string(FIND "${ROW_TEXT}" "${EXPECTED}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "${NAME}: the scale factor 2 is not judged '${EXPECTED}': ${ROW_TEXT}")
    endif()
endforeach()
