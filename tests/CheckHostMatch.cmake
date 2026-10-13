# Runs "PluginLabHost --match <job file> <report file>" (W9.4): a band-model curve (RBJ peak 1 kHz +6 dB Q 2) as master, the test EQ and the
# test EQ with knob faults as targets; checks that both are matched.
# Arguments: HOST_APPLICATION, PLUGIN_FOLDER, REPORT_FILE, SETTINGS_FOLDER
file(REMOVE "${REPORT_FILE}")
file(REMOVE_RECURSE "${SETTINGS_FOLDER}")
set(JOB_FILE "${REPORT_FILE}.job.json")
file(WRITE "${JOB_FILE}" "{
  \"master\": {\"curve\": [{\"family\": \"RBJ cookbook\", \"type\": \"Peak\", \"frequency\": 1000, \"gain\": 6, \"q\": 2}]},
  \"targets\": [
    {\"plugin\": \"${PLUGIN_FOLDER}/PluginLabTestEq.vst3\", \"release\": [\"Gain\", \"Frequency\", \"Q\"], \"start\": {\"Gain\": 0.5625, \"Frequency\": 0.363, \"Q\": 0.141}},
    {\"plugin\": \"${PLUGIN_FOLDER}/PluginLabTestEqKnobs.vst3\", \"release\": [\"Gain\", \"Frequency\", \"Q\"], \"start\": {\"Gain\": 0.5625, \"Frequency\": 0.363}}
  ],
  \"tolerance\": \"strict\",
  \"seconds\": 30
}
")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env "PLUGINLAB_SETTINGS_FOLDER=${SETTINGS_FOLDER}" "${HOST_APPLICATION}" --match "${JOB_FILE}" "${REPORT_FILE}"
    RESULT_VARIABLE HOST_RESULT
    TIMEOUT 600)

if(NOT HOST_RESULT EQUAL 0)
    message(FATAL_ERROR "PluginLabHost exited with '${HOST_RESULT}'")
endif()
if(NOT EXISTS "${REPORT_FILE}")
    message(FATAL_ERROR "PluginLabHost did not write ${REPORT_FILE}")
endif()
file(READ "${REPORT_FILE}" REPORT)
foreach(EXPECTED "# Match" "Reviewed by Jade Benchmark, pluginlab" "tolerance strict (0.1 dB)" "| PluginLab Test EQ " "| PluginLab Test EQ Knob Faults ")
    string(FIND "${REPORT}" "${EXPECTED}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "The report does not contain '${EXPECTED}'")
    endif()
endforeach()
string(REGEX MATCHALL "\\| matched \\|" MATCHED "${REPORT}")
list(LENGTH MATCHED COUNT)
if(NOT COUNT EQUAL 2)
    message(FATAL_ERROR "Expected two matched targets, found ${COUNT}:\n${REPORT}")
endif()
