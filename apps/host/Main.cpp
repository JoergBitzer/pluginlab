#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "pluginlab/PluginLabVersion.h"

namespace
{
constexpr int kWindowWidth = 600;
constexpr int kWindowHeight = 300;
constexpr float kLabelFontHeight = 20.0f;

// Command line: "--write-version <file>" writes the version into the file and quits without opening a window.
// (A file, because a GUI program on Windows has no console; CI checks the file.)
const juce::String kWriteVersionOption = "--write-version";
}

// The content of the window: W1 shows only the name and the version.
class MainContent : public juce::Component
{
public:
    MainContent()
    {
        m_label.setText("pluginlab host, version " + juce::String(pluginlab::getVersionString()), juce::dontSendNotification);
        m_label.setJustificationType(juce::Justification::centred);
        m_label.setFont(juce::FontOptions(kLabelFontHeight));
        addAndMakeVisible(m_label);
        setSize(kWindowWidth, kWindowHeight);
    }

    void resized() override
    {
        m_label.setBounds(getLocalBounds());
    }

private:
    juce::Label m_label;
};

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow(const juce::String& name)
        : juce::DocumentWindow(name,
                               juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                               juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainContent(), true);
        setResizable(true, true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class PluginLabHostApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override
    {
        return JUCE_APPLICATION_NAME_STRING;
    }

    const juce::String getApplicationVersion() override
    {
        return juce::String(pluginlab::getVersionString());
    }

    bool moreThanOneInstanceAllowed() override
    {
        return true;
    }

    void initialise(const juce::String& commandLine) override
    {
        const juce::StringArray arguments = juce::StringArray::fromTokens(commandLine, true);
        const int optionIndex = arguments.indexOf(kWriteVersionOption);
        const bool writeVersionOnly = optionIndex >= 0 && optionIndex + 1 < arguments.size();
        if (writeVersionOnly)
        {
            const juce::File versionFile(arguments[optionIndex + 1].unquoted());
            versionFile.replaceWithText(juce::String(pluginlab::getVersionString()));
            quit();
            return;
        }

        m_mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        m_mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    std::unique_ptr<MainWindow> m_mainWindow;
};

START_JUCE_APPLICATION(PluginLabHostApplication)
