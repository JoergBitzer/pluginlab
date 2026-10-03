# Runs the host application with --write-version and compares the file content with the expected version.
# Arguments: HOST_APPLICATION, EXPECTED_VERSION, VERSION_FILE
file(REMOVE "${VERSION_FILE}")

execute_process(
    COMMAND "${HOST_APPLICATION}" --write-version "${VERSION_FILE}"
    RESULT_VARIABLE HOST_RESULT
    TIMEOUT 60)

if(NOT HOST_RESULT EQUAL 0)
    message(FATAL_ERROR "PluginLabHost exited with '${HOST_RESULT}'")
endif()

if(NOT EXISTS "${VERSION_FILE}")
    message(FATAL_ERROR "PluginLabHost did not write ${VERSION_FILE}")
endif()

file(READ "${VERSION_FILE}" HOST_VERSION)
string(STRIP "${HOST_VERSION}" HOST_VERSION)
if(NOT HOST_VERSION STREQUAL EXPECTED_VERSION)
    message(FATAL_ERROR "PluginLabHost reports version '${HOST_VERSION}', expected '${EXPECTED_VERSION}'")
endif()
message(STATUS "PluginLabHost reports version ${HOST_VERSION}")
