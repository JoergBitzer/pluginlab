# Runs "PluginLabHost --fingerprint <plugin file> <report file>" and checks the report of the gain plugin.
# Arguments: HOST_APPLICATION, PLUGIN_FILE, REPORT_FILE
file(REMOVE "${REPORT_FILE}" "${REPORT_FILE}.json")

execute_process(
    COMMAND "${HOST_APPLICATION}" --fingerprint "${PLUGIN_FILE}" "${REPORT_FILE}"
    RESULT_VARIABLE HOST_RESULT
    TIMEOUT 240)

if(NOT HOST_RESULT EQUAL 0)
    message(FATAL_ERROR "PluginLabHost exited with '${HOST_RESULT}'")
endif()
if(NOT EXISTS "${REPORT_FILE}")
    message(FATAL_ERROR "PluginLabHost did not write ${REPORT_FILE}")
endif()

file(READ "${REPORT_FILE}" REPORT)
if(NOT EXISTS "${REPORT_FILE}.json")
    message(FATAL_ERROR "PluginLabHost did not write the summary ${REPORT_FILE}.json")
endif()
file(READ "${REPORT_FILE}.json" SUMMARY)
string(FIND "${SUMMARY}" "\"key\": \"deterministic\"" POSITION)
if(POSITION EQUAL -1)
    message(FATAL_ERROR "The summary does not contain the item 'deterministic'")
endif()

foreach(EXPECTED "# Fingerprint: PluginLab Test Gain" "## Summary" "## Findings" "## Delivery of parameters" "## Block sizes" "## Measurements (AES17)")
    string(FIND "${REPORT}" "${EXPECTED}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "The report does not contain '${EXPECTED}'")
    endif()
endforeach()

# a plugin identifier that is not in the file: no report, an error
file(REMOVE "${REPORT_FILE}")
execute_process(
    COMMAND "${HOST_APPLICATION}" --fingerprint "${PLUGIN_FILE}" "${REPORT_FILE}" "no such plugin"
    RESULT_VARIABLE HOST_RESULT
    TIMEOUT 240)
if(HOST_RESULT EQUAL 0)
    message(FATAL_ERROR "A plugin that is not in the file must give an error")
endif()
if(EXISTS "${REPORT_FILE}")
    message(FATAL_ERROR "No report may be written for a plugin that is not in the file")
endif()
