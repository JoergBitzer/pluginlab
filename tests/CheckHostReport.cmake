# Runs "PluginLabHost --report <folder> <file>" and checks the report: the good test plugin is listed with its four parameters
# and the plugin that crashes is reported as Crashed, without taking the host down.
# Arguments: HOST_APPLICATION, PLUGIN_FOLDER, REPORT_FILE
file(REMOVE "${REPORT_FILE}")

execute_process(
    COMMAND "${HOST_APPLICATION}" --report "${PLUGIN_FOLDER}" "${REPORT_FILE}"
    RESULT_VARIABLE HOST_RESULT
    TIMEOUT 240)

if(NOT HOST_RESULT EQUAL 0)
    message(FATAL_ERROR "PluginLabHost exited with '${HOST_RESULT}'")
endif()
if(NOT EXISTS "${REPORT_FILE}")
    message(FATAL_ERROR "PluginLabHost did not write ${REPORT_FILE}")
endif()

file(READ "${REPORT_FILE}" REPORT)
message(STATUS "Report:\n${REPORT}")

if(WITH_VST2)
    list(APPEND EXPECTED_VST2 "PLUGIN PluginLab Test Gain (VST) | parameters 4")
endif()

foreach(EXPECTED ${EXPECTED_VST2}
        "PLUGIN PluginLab Test Gain (VST3) | parameters 4"
        "PARAMETER 0 Gain = 0.0 dB"
        "FILE PluginLabTestCrash.vst3 | Crashed")
    string(FIND "${REPORT}" "${EXPECTED}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "The report does not contain '${EXPECTED}'")
    endif()
endforeach()
