# VST2 support. JUCE no longer ships the VST2 SDK. The free, reverse-engineered header files of the FST project (GPL v3 or
# later, https://git.iem.at/zmoelnig/FST, fork https://github.com/pierreguillot/FTS) are a git submodule in external/FST.
# JUCE wants them as <sdk>/pluginterfaces/vst2.x/aeffect.h and aeffectx.h: they are copied there at configure time, with the
# adapter vst2_shim/aeffectx.h and three patches (found with the plugin SimplePeakEQ and its Windows CI):
#   1. four names that JUCE uses and FST does not define (vst2_shim/aeffectx.h); FST does not know every opcode, so unknown
#      ones get placeholder values (no host sends them): MIDI key names and a few others do not work with VST2
#   2. FST chooses its 64 bit pointer-sized integer with "defined(__x86_64__)", which MSVC never defines: on Windows the
#      AEffect structure and the dispatcher were wrong (no processing, heap corruption); patched to _WIN64
#   3. the arrays speakers[] and events[] have zero size in FST, JUCE sizes its buffers assuming 8 speakers and 2 events
#      (heap corruption); given their usual length
#   5. fstPinProperties has no padding behind shortLabel, JUCE's wrapper writes one byte more than the arrays hold (stack
#      protector abort in the host on macOS and Windows): 48 bytes of padding added
#   4. JUCE's VST2 hosting code forward-declares "struct AEffect;": FST's structure is called _fstEffect (AEffect is a
#      typedef): the structure tag is renamed to AEffect
# A build error stops the configuration if FST changes in a way that breaks a patch.
# Result: PLUGINLAB_VST2 is ON when the headers were found and juce_set_vst2_sdk_path was called, else OFF.
# Binaries with VST2 are GPL v3 (compatible with the AGPL v3 of JUCE).
set(PLUGINLAB_VST2 OFF)

set(FST_HEADER_DIR "${CMAKE_SOURCE_DIR}/external/FST/fst")
if(EXISTS "${FST_HEADER_DIR}/aeffect.h" AND EXISTS "${FST_HEADER_DIR}/aeffectx.h")
    set(VST2_SDK_DIR "${CMAKE_BINARY_DIR}/vst2_sdk")
    set(VST2_SDK_HEADER_DIR "${VST2_SDK_DIR}/pluginterfaces/vst2.x")
    file(MAKE_DIRECTORY "${VST2_SDK_HEADER_DIR}")

    configure_file("${FST_HEADER_DIR}/aeffect.h" "${VST2_SDK_HEADER_DIR}/aeffect.h" COPYONLY)
    configure_file("${FST_HEADER_DIR}/aeffectx.h" "${VST2_SDK_HEADER_DIR}/aeffectx_fst.h" COPYONLY)
    configure_file("${CMAKE_SOURCE_DIR}/vst2_shim/aeffectx.h" "${VST2_SDK_HEADER_DIR}/aeffectx.h" COPYONLY)

    file(READ "${CMAKE_SOURCE_DIR}/vst2_shim/fst_prefix.h" FST_PREFIX_TEXT)
    file(READ "${FST_HEADER_DIR}/fst.h" FST_ORIGINAL_TEXT)

    string(REPLACE "t_fstSpeakerProperties speakers[];" "t_fstSpeakerProperties speakers[8];" FST_PATCHED_TEXT "${FST_ORIGINAL_TEXT}")
    string(REPLACE "t_fstEvent*events[];" "t_fstEvent*events[2];" FST_PATCHED_TEXT "${FST_PATCHED_TEXT}")
    if(FST_PATCHED_TEXT STREQUAL FST_ORIGINAL_TEXT)
        message(FATAL_ERROR "FST header changed: the zero-size arrays of fstSpeakerArrangement and fstEvents were not found (cmake/Vst2Sdk.cmake)")
    endif()

    string(FIND "${FST_PATCHED_TEXT}" "#if defined(_WIN32) && defined(__x86_64__)" FST_WIN64_CONDITION_POSITION)
    if(FST_WIN64_CONDITION_POSITION EQUAL -1)
        message(FATAL_ERROR "FST header changed: the 64 bit pointer type condition was not found (cmake/Vst2Sdk.cmake)")
    endif()
    string(REPLACE "#if defined(_WIN32) && defined(__x86_64__)" "#if defined(_WIN64) || (defined(_WIN32) && defined(__x86_64__))"
        FST_PATCHED_TEXT "${FST_PATCHED_TEXT}")

    # JUCE's VST2 *hosting* code forward-declares "struct AEffect;" (in namespace Vst2): FST names the structure _fstEffect and
    # makes AEffect a typedef, which conflicts with the forward declaration. Make AEffect the real structure tag.
    string(FIND "${FST_PATCHED_TEXT}" "struct _fstEffect" FST_EFFECT_TAG_POSITION)
    if(FST_EFFECT_TAG_POSITION EQUAL -1)
        message(FATAL_ERROR "FST header changed: 'struct _fstEffect' was not found (cmake/Vst2Sdk.cmake)")
    endif()
    string(REPLACE "struct _fstEffect" "struct AEffect" FST_PATCHED_TEXT "${FST_PATCHED_TEXT}")

    # JUCE's VST2 wrapper writes label.copyToUTF8(properties.label, kVstMaxLabelLen + 1) and the same for shortLabel into the
    # structure that the host passes for effGetInputProperties/effGetOutputProperties: one byte more than the arrays hold, which is
    # harmless only because the official structure has 48 bytes of padding ("future") behind shortLabel. FST's structure has none:
    # the byte lands behind the host's variable (stack protector abort on macOS and Windows, found with the debug workflow).
    string(FIND "${FST_PATCHED_TEXT}" "  char shortLabel[8];\n} FST_UNKNOWN(t_fstPinProperties);" FST_PIN_PROPERTIES_POSITION)
    if(FST_PIN_PROPERTIES_POSITION EQUAL -1)
        message(FATAL_ERROR "FST header changed: the end of fstPinProperties was not found (cmake/Vst2Sdk.cmake)")
    endif()
    string(REPLACE "  char shortLabel[8];\n} FST_UNKNOWN(t_fstPinProperties);"
        "  char shortLabel[8];\n  char future[48]; /* padding, added by pluginlab (cmake/Vst2Sdk.cmake) */\n} FST_UNKNOWN(t_fstPinProperties);"
        FST_PATCHED_TEXT "${FST_PATCHED_TEXT}")

    file(WRITE "${VST2_SDK_HEADER_DIR}/fst.h" "${FST_PREFIX_TEXT}\n${FST_PATCHED_TEXT}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${FST_HEADER_DIR}/fst.h" "${FST_HEADER_DIR}/aeffect.h" "${FST_HEADER_DIR}/aeffectx.h"
        "${CMAKE_SOURCE_DIR}/vst2_shim/aeffectx.h" "${CMAKE_SOURCE_DIR}/vst2_shim/fst_prefix.h")

    juce_set_vst2_sdk_path("${VST2_SDK_DIR}")
    set(PLUGINLAB_VST2 ON)
    message(STATUS "VST2 support: ON (FST headers)")
else()
    message(STATUS "VST2 support: OFF (FST headers not found: git submodule update --init)")
endif()
