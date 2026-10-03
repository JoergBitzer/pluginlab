/*
    vst2_shim/aeffectx.h
    Adapter between JUCE's VST2 wrapper and the free FST headers (GPL v3 or later, external/FST).

    JUCE includes <pluginterfaces/vst2.x/aeffectx.h> inside "namespace Vst2". The FST headers do not define
    everything that the wrapper uses. This file includes the FST header (copied to aeffectx_fst.h at configure
    time) and adds the four missing names. FST is a clean-room header: values that FST does not know are not
    taken from anywhere else. They get a placeholder value like FST's own "unknown" opcodes (100000 + line number),
    which no host ever sends; the corresponding feature (MIDI key names) is therefore not available in the VST2 plugin.
*/
#pragma once

#include "aeffectx_fst.h"

// FST names the type of the host opcodes t_fstHostOpcode; JUCE uses this name for the opcodes it sends to the host
typedef t_fstHostOpcode AudioMasterOpcodesX;

// length of name strings (FST: effect, vendor and product names are char[64])
enum { kVstMaxNameLen = 64 };

// opcode effGetMidiKeyName: the value is not known to FST (placeholder, see above)
enum { effGetMidiKeyName = 100000 + __LINE__ };

// argument of effGetMidiKeyName (never delivered, because no host sends the placeholder opcode)
struct MidiKeyName
{
    int thisProgramIndex;
    int thisKeyNumber;
    char keyName[kVstMaxNameLen];
    int reserved;
    int flags;
};

// ---- names used by JUCE's VST2 *hosting* code (not by the plugin wrapper) ----

// the type of the SMPTE frame rate in the time information (FST: the field is an int, the values are unknown placeholders)
typedef int VstSmpteFrameRate;

// the number of plugin categories (marks the end of the enumeration); placeholder value like the other unknown constants
enum { kPlugCategMaxCount = 100000 + __LINE__ };
