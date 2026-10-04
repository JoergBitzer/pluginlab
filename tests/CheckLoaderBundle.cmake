# Checks that the scanner is inside the plugin bundle of the loader (in the platform folder next to the plugin binary).
# Arguments: BUNDLE_FOLDER, SCANNER_NAME
file(GLOB_RECURSE FOUND "${BUNDLE_FOLDER}/Contents/*/${SCANNER_NAME}")
if(NOT FOUND)
    message(FATAL_ERROR "${SCANNER_NAME} is not inside ${BUNDLE_FOLDER}/Contents/<platform>/")
endif()
message(STATUS "Scanner in the bundle: ${FOUND}")
