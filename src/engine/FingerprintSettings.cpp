#include "pluginlab/engine/FingerprintSettings.h"

namespace pluginlab::engine
{
namespace
{
const juce::String kEnvironmentVariable = "PLUGINLAB_FINGERPRINT_SETTINGS";
const juce::String kFolderName = "pluginlab";
const juce::String kFileName = "fingerprint_settings.json";

// the keys of the file, one per field
const juce::Identifier kReactsAboveDb("reactsAboveDb");
const juce::Identifier kDifferentAboveDb("differentAboveDb");
const juce::Identifier kSameBelowDb("sameBelowDb");
const juce::Identifier kBlockIndependentBelowDb("blockIndependentBelowDb");
const juce::Identifier kSilentReferenceDbfs("silentReferenceDbfs");
const juce::Identifier kCouplingBelowDb("couplingBelowDb");
const juce::Identifier kSilenceBelowDbfs("silenceBelowDbfs");
const juce::Identifier kTimeInvarianceGapSeconds("timeInvarianceGapSeconds");
const juce::Identifier kMaximumPairs("maximumPairs");
const juce::Identifier kLongSettleSeconds("longSettleSeconds");
const juce::Identifier kNoiseLevel("noiseLevel");
const juce::Identifier kSettleSeconds("settleSeconds");
const juce::Identifier kImpulsePreDelaySamples("impulsePreDelaySamples");
const juce::Identifier kLatencyObserveSeconds("latencyObserveSeconds");
const juce::Identifier kLatencyMinimumPeak("latencyMinimumPeak");
const juce::Identifier kBlockRenderSeconds("blockRenderSeconds");
const juce::Identifier kBlockCompareSeconds("blockCompareSeconds");
const juce::Identifier kBlockSizes("blockSizes");
const juce::Identifier kLowSetting("lowSetting");
const juce::Identifier kHighSetting("highSetting");
const juce::Identifier kPokeDistance("pokeDistance");
const juce::Identifier kMaximumParameters("maximumParameters");
const juce::Identifier kMaximumJumpedParameters("maximumJumpedParameters");

void readDouble(const juce::DynamicObject& object, const juce::Identifier& key, double& value)
{
    if (object.hasProperty(key))
    {
        value = static_cast<double>(object.getProperty(key));
    }
}

void readInt(const juce::DynamicObject& object, const juce::Identifier& key, int& value)
{
    if (object.hasProperty(key))
    {
        value = static_cast<int>(object.getProperty(key));
    }
}
}

juce::File FingerprintSettings::getDefaultFile()
{
    const juce::String fromEnvironment = juce::SystemStats::getEnvironmentVariable(kEnvironmentVariable, {});
    if (fromEnvironment.isNotEmpty())
    {
        return juce::File(fromEnvironment);
    }
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile(kFolderName).getChildFile(kFileName);
}

FingerprintSettings FingerprintSettings::loadOrCreate(const juce::File& file, juce::String& warning)
{
    FingerprintSettings settings;
    if (! file.existsAsFile())
    {
        if (! settings.save(file))
        {
            warning = "the settings file " + file.getFullPathName() + " could not be written: defaults used";
        }
        return settings;
    }
    const juce::var parsed = juce::JSON::parse(file.loadFileAsString());
    const juce::DynamicObject* object = parsed.getDynamicObject();
    if (object == nullptr)
    {
        warning = "the settings file " + file.getFullPathName() + " is not valid JSON: defaults used";
        return settings;
    }
    readDouble(*object, kReactsAboveDb, settings.reactsAboveDb);
    readDouble(*object, kDifferentAboveDb, settings.differentAboveDb);
    readDouble(*object, kSameBelowDb, settings.sameBelowDb);
    readDouble(*object, kBlockIndependentBelowDb, settings.blockIndependentBelowDb);
    readDouble(*object, kSilentReferenceDbfs, settings.silentReferenceDbfs);
    readDouble(*object, kCouplingBelowDb, settings.couplingBelowDb);
    readDouble(*object, kSilenceBelowDbfs, settings.silenceBelowDbfs);
    readDouble(*object, kTimeInvarianceGapSeconds, settings.timeInvarianceGapSeconds);
    readInt(*object, kMaximumPairs, settings.maximumPairs);
    readDouble(*object, kLongSettleSeconds, settings.longSettleSeconds);
    readDouble(*object, kNoiseLevel, settings.noiseLevel);
    readDouble(*object, kSettleSeconds, settings.settleSeconds);
    readInt(*object, kImpulsePreDelaySamples, settings.impulsePreDelaySamples);
    readDouble(*object, kLatencyObserveSeconds, settings.latencyObserveSeconds);
    readDouble(*object, kLatencyMinimumPeak, settings.latencyMinimumPeak);
    readDouble(*object, kBlockRenderSeconds, settings.blockRenderSeconds);
    readDouble(*object, kBlockCompareSeconds, settings.blockCompareSeconds);
    readDouble(*object, kLowSetting, settings.lowSetting);
    readDouble(*object, kHighSetting, settings.highSetting);
    readDouble(*object, kPokeDistance, settings.pokeDistance);
    readInt(*object, kMaximumParameters, settings.maximumParameters);
    readInt(*object, kMaximumJumpedParameters, settings.maximumJumpedParameters);
    const juce::var blockSizes = object->getProperty(kBlockSizes);
    if (const juce::Array<juce::var>* sizes = blockSizes.getArray())
    {
        settings.blockSizes.clear();
        for (const juce::var& size : *sizes)
        {
            settings.blockSizes.push_back(static_cast<int>(size));
        }
    }
    return settings;
}

juce::String FingerprintSettings::toJson() const
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty(kReactsAboveDb, reactsAboveDb);
    object->setProperty(kDifferentAboveDb, differentAboveDb);
    object->setProperty(kSameBelowDb, sameBelowDb);
    object->setProperty(kBlockIndependentBelowDb, blockIndependentBelowDb);
    object->setProperty(kSilentReferenceDbfs, silentReferenceDbfs);
    object->setProperty(kCouplingBelowDb, couplingBelowDb);
    object->setProperty(kSilenceBelowDbfs, silenceBelowDbfs);
    object->setProperty(kTimeInvarianceGapSeconds, timeInvarianceGapSeconds);
    object->setProperty(kMaximumPairs, maximumPairs);
    object->setProperty(kLongSettleSeconds, longSettleSeconds);
    object->setProperty(kNoiseLevel, noiseLevel);
    object->setProperty(kSettleSeconds, settleSeconds);
    object->setProperty(kImpulsePreDelaySamples, impulsePreDelaySamples);
    object->setProperty(kLatencyObserveSeconds, latencyObserveSeconds);
    object->setProperty(kLatencyMinimumPeak, latencyMinimumPeak);
    object->setProperty(kBlockRenderSeconds, blockRenderSeconds);
    object->setProperty(kBlockCompareSeconds, blockCompareSeconds);
    juce::Array<juce::var> sizes;
    for (const int size : blockSizes)
    {
        sizes.add(size);
    }
    object->setProperty(kBlockSizes, sizes);
    object->setProperty(kLowSetting, lowSetting);
    object->setProperty(kHighSetting, highSetting);
    object->setProperty(kPokeDistance, pokeDistance);
    object->setProperty(kMaximumParameters, maximumParameters);
    object->setProperty(kMaximumJumpedParameters, maximumJumpedParameters);
    return juce::JSON::toString(juce::var(object.release()));
}

bool FingerprintSettings::save(const juce::File& file) const
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText(toJson());
}
}
