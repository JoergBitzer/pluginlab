# Runs "PluginLabHost --map <plugin file> <report file>" on the test EQ with an enable switch (W8.5) with the settings folder of the build, and
# checks the report and the proposal written into its mapping folder.
# Arguments: HOST_APPLICATION, PLUGIN_FILE, REPORT_FILE, SETTINGS_FOLDER
file(REMOVE "${REPORT_FILE}")
file(REMOVE_RECURSE "${SETTINGS_FOLDER}")
set(PROPOSAL "${SETTINGS_FOLDER}/mappings/pluginlab_test_eq_switch.json")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env "PLUGINLAB_SETTINGS_FOLDER=${SETTINGS_FOLDER}" "${HOST_APPLICATION}" --map "${PLUGIN_FILE}" "${REPORT_FILE}"
    RESULT_VARIABLE HOST_RESULT
    TIMEOUT 900)

if(NOT HOST_RESULT EQUAL 0)
    message(FATAL_ERROR "PluginLabHost exited with '${HOST_RESULT}'")
endif()
foreach(WRITTEN "${REPORT_FILE}" "${PROPOSAL}")
    if(NOT EXISTS "${WRITTEN}")
        message(FATAL_ERROR "PluginLabHost did not write ${WRITTEN}")
    endif()
endforeach()

file(READ "${REPORT_FILE}" REPORT)
foreach(EXPECTED "# Parameter survey and mapping: PluginLab Test EQ Switch" "Reviewed by Jade Benchmark, pluginlab" "## Mapping: PluginLab Test EQ Switch"
                 "**Proposal** (not confirmed)" "Enabled = On")
    string(FIND "${REPORT}" "${EXPECTED}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "The report does not contain '${EXPECTED}'")
    endif()
endforeach()
file(READ "${PROPOSAL}" JSON)
foreach(EXPECTED "\"quantity\": \"frequency\"" "\"quantity\": \"band on/off\"")
    string(FIND "${JSON}" "${EXPECTED}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "The proposal does not contain '${EXPECTED}'")
    endif()
endforeach()
