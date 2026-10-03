#pragma once

#include <string>

namespace pluginlab
{
struct VersionNumber
{
    int major = 0;
    int minor = 0;
    int patch = 0;
};

// The version of this build, from project(pluginlab VERSION ...) in the top-level CMakeLists.txt.
VersionNumber getVersion();

// "major.minor.patch", e.g. "0.1.0"
std::string getVersionString();
}
