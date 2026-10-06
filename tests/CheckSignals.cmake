# Smoke test of PluginLabSignals: it writes the standard signals for one length into an empty folder; checks the number of WAV
# files, the step lists and the description file.
# Arguments: SIGNALS_APPLICATION, OUTPUT_FOLDER
file(REMOVE_RECURSE "${OUTPUT_FOLDER}")
execute_process(COMMAND "${SIGNALS_APPLICATION}" "${OUTPUT_FOLDER}" --seconds 5 RESULT_VARIABLE result OUTPUT_VARIABLE output)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "PluginLabSignals failed (${result}): ${output}")
endif()
file(GLOB wavs "${OUTPUT_FOLDER}/*.wav")
file(GLOB steps "${OUTPUT_FOLDER}/*_steps.csv")
list(LENGTH wavs wavCount)
list(LENGTH steps stepsCount)
set(expectedWavs 23)
if(NOT wavCount EQUAL expectedWavs OR NOT stepsCount EQUAL 2 OR NOT EXISTS "${OUTPUT_FOLDER}/signals.txt")
    message(FATAL_ERROR "expected ${expectedWavs} WAV files, 2 step lists and signals.txt, found ${wavCount} WAV files and ${stepsCount} step lists")
endif()
message(STATUS "PluginLabSignals wrote ${wavCount} WAV files")
