#include "pluginlab/PluginLabVersion.h"

#include "pluginlab/Versioning.h"

namespace pluginlab
{
VersionNumber getVersion()
{
    VersionNumber version;
    version.major = generated::kVersionMajor;
    version.minor = generated::kVersionMinor;
    version.patch = generated::kVersionPatch;
    return version;
}

std::string getVersionString()
{
    const VersionNumber version = getVersion();
    const std::string separator = ".";
    return std::to_string(version.major) + separator + std::to_string(version.minor) + separator
         + std::to_string(version.patch);
}
}
