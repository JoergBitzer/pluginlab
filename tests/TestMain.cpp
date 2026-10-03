#include <juce_core/juce_core.h>

// Runs all registered juce::UnitTest classes; the exit code is 0 only if all tests passed.
int main(int argc, char* argv[])
{
    juce::ignoreUnused(argc, argv);

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
