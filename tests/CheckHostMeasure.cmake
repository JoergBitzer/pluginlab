# Runs "PluginLabHost --measure <plugin file> <report file>" and checks the measurement report of the gain plugin (W7.11).
# Arguments: HOST_APPLICATION, PLUGIN_FILE, REPORT_FILE
file(REMOVE "${REPORT_FILE}")

execute_process(
    COMMAND "${HOST_APPLICATION}" --measure "${PLUGIN_FILE}" "${REPORT_FILE}"
    RESULT_VARIABLE HOST_RESULT
    TIMEOUT 240)

if(NOT HOST_RESULT EQUAL 0)
    message(FATAL_ERROR "PluginLabHost exited with '${HOST_RESULT}'")
endif()
if(NOT EXISTS "${REPORT_FILE}")
    message(FATAL_ERROR "PluginLabHost did not write ${REPORT_FILE}")
endif()

file(READ "${REPORT_FILE}" REPORT)
foreach(EXPECTED "# PluginLab Test Gain" "## Measurements (AES17)" "AES17 6.2.2" "AES17 6.4.1" "maximum input level")
    string(FIND "${REPORT}" "${EXPECTED}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "The report does not contain '${EXPECTED}'")
    endif()
endforeach()
