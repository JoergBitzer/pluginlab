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
int main(int argc, char* argv[])
{
    juce::ignoreUnused(argc, argv);

    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    StdoutLogger logger;
    juce::Logger::setCurrentLogger(&logger);

    juce::UnitTestRunner runner;
    runner.runAllTests();

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
