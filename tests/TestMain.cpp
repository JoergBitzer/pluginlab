#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

// Runs all registered juce::UnitTest classes; the exit code is 0 only if all tests passed.
// The message manager is needed because the tests load plugins (VST3 hosting runs on the message thread).
int main(int argc, char* argv[])
{
    juce::ignoreUnused(argc, argv);

    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    juce::UnitTestRunner runner;
    runner.runAllTests();

    int numberOfFailures = 0;
    for (int resultIndex = 0; resultIndex < runner.getNumResults(); ++resultIndex)
    {
        numberOfFailures += runner.getResult(resultIndex)->failures;
    }

    if (numberOfFailures > 0)
    {
        return 1;
    }
    return 0;
}
