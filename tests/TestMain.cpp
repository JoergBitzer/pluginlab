#include <algorithm>
#include <iostream>
#include <map>
#include <utility>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

namespace
{
// JUCE logs only to the debugger on Windows: without this a CI log shows nothing of the tests
class StdoutLogger : public juce::Logger
{
public:
    void logMessage(const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};
}

// Runs all registered juce::UnitTest classes; the exit code is 0 only if all tests passed.
// The message manager is needed because the tests load plugins (VST3 hosting runs on the message thread).
// Usage: PluginLabTests [--only <part of a test name>] [--skip <part of a test name>[|<another part>...]]   (without arguments all tests run)
int main(int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    StdoutLogger logger;
    juce::Logger::setCurrentLogger(&logger);

    const juce::StringArray arguments(argv + 1, argc - 1);
    const int onlyIndex = arguments.indexOf("--only");
    const int skipIndex = arguments.indexOf("--skip");
    juce::UnitTestRunner runner;
    if ((onlyIndex >= 0 && onlyIndex + 1 < arguments.size()) || (skipIndex >= 0 && skipIndex + 1 < arguments.size()))
    {
        const juce::String only = onlyIndex >= 0 ? arguments[onlyIndex + 1] : juce::String();
        juce::StringArray skip;
        if (skipIndex >= 0)
        {
            skip.addTokens(arguments[skipIndex + 1], "|", {});
        }
        juce::Array<juce::UnitTest*> selected;
        for (juce::UnitTest* test : juce::UnitTest::getAllTests())
        {
            const bool wanted = only.isEmpty() || test->getName().containsIgnoreCase(only);
            const bool skipped = std::any_of(skip.begin(), skip.end(), [test](const juce::String& part) { return test->getName().containsIgnoreCase(part); });
            if (wanted && ! skipped)
            {
                selected.add(test);
            }
        }
        runner.runTests(selected);
    }
    else
    {
        runner.runAllTests();
    }

    int numberOfFailures = 0;
    std::map<juce::String, double> secondsPerTest; // the time of each test group: which ones to split off when the suite gets slow
    for (int resultIndex = 0; resultIndex < runner.getNumResults(); ++resultIndex)
    {
        const juce::UnitTestRunner::TestResult* result = runner.getResult(resultIndex);
        numberOfFailures += result->failures;
        secondsPerTest[result->unitTestName] += (result->endTime - result->startTime).inSeconds();
    }
    std::vector<std::pair<juce::String, double>> times(secondsPerTest.begin(), secondsPerTest.end());
    std::sort(times.begin(), times.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::cout << "Time per test (s):" << std::endl;
    for (const auto& [name, seconds] : times)
    {
        std::cout << "  " << juce::String(seconds, 1) << "  " << name << std::endl;
    }

    juce::Logger::setCurrentLogger(nullptr);
    if (numberOfFailures > 0)
    {
        return 1;
    }
    return 0;
}
