#include <iostream>

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
// Usage: PluginLabTests [--only <part of a test name>]   (without arguments all tests run, as CTest does)
int main(int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    StdoutLogger logger;
    juce::Logger::setCurrentLogger(&logger);

    const juce::StringArray arguments(argv + 1, argc - 1);
    const int onlyIndex = arguments.indexOf("--only");
    juce::UnitTestRunner runner;
    if (onlyIndex >= 0 && onlyIndex + 1 < arguments.size())
    {
        juce::Array<juce::UnitTest*> selected;
        for (juce::UnitTest* test : juce::UnitTest::getAllTests())
        {
            if (test->getName().containsIgnoreCase(arguments[onlyIndex + 1]))
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
    for (int resultIndex = 0; resultIndex < runner.getNumResults(); ++resultIndex)
    {
        numberOfFailures += runner.getResult(resultIndex)->failures;
    }

    juce::Logger::setCurrentLogger(nullptr);
    if (numberOfFailures > 0)
    {
        return 1;
    }
    return 0;
}
