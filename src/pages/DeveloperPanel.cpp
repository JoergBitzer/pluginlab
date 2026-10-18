#include "DeveloperPanel.h"

#include "PageServices.h"
#include "pluginlab/engine/Mapping.h"

#include "pluginlab/engine/OfflineRenderer.h"
#include "pluginlab/engine/ReportText.h"
#include "pluginlab/hosting/PluginCatalog.h"
#include "pluginlab/hosting/PluginDisplayName.h"

namespace pluginlab::host
{
namespace
{
constexpr int kMargin = 8;
constexpr int kRowHeight = 28;
constexpr int kHeaderHeight = 24;
constexpr int kInfoHeight = 96; // six lines: what the jobs do and where their results are kept
constexpr int kProgressHeight = 22; // the strip with the running job
constexpr int kColumnNumber = 1;
constexpr int kColumnName = 2;
constexpr int kColumnFormat = 3;
constexpr int kColumnStatus = 4;
constexpr int kColumnDate = 5;
constexpr int kColumnActions = 6;
constexpr int kColumnGui = 7;
constexpr int kColumnMapping = 8;
constexpr int kColumnCalibration = 9;
constexpr int kFirstSummaryColumn = 10; // the columns of the single results follow, in the order of kSummaryColumns
constexpr int kWidthSummary = 90;
const juce::Colour kDoubtfulColour(0xffffa040);
constexpr int kWidthNumber = 30;
constexpr int kWidthName = 200;
constexpr int kWidthFormat = 60;
constexpr int kWidthStatus = 240;
constexpr int kWidthDate = 150;
constexpr int kWidthActions = 960;
constexpr int kWidthGui = 220;
constexpr int kWidthMapping = 280;
constexpr int kWidthCalibration = 280;
constexpr int kButtonWidth = 110;
constexpr int kProcessTimeoutMs = 20 * 60 * 1000; // the measurement loads the plugin many times; a big plugin needs minutes
constexpr int kPollMs = 200;
constexpr int kStopTimeoutMs = 10000;
constexpr juce::uint32 kProgressIntervalMs = 1000;           // how often the job reads the child's progress
const juce::Colour kProgressColour(0xff3a6ea5);                  // the done part of a progress bar
const juce::Colour kProgressBackground(0xff2a2a2a);
constexpr int kReportWindowWidth = 1000;
constexpr int kReportWindowHeight = 720;
constexpr float kTextFontHeight = 14.0f;
const juce::String kReportExtension = ".md";
const juce::String kGuiFolderSuffix = "_gui";
const juce::String kGuiReviewFile = "gui_review.md";
const juce::String kGuiWindowSuffix = "#gui";
const juce::String kMappingWindowSuffix = "#map";
const juce::String kMappingFileSuffix = "_mapping";
const juce::String kCalibrationWindowSuffix = "#cal";
const juce::String kCalibrationFileSuffix = "_calibration";
const juce::String kDateFormat = "%Y-%m-%d %H:%M";
}

// The single results shown as columns: the key of the summary item and the column title (the full test name is the tooltip)
struct SummaryColumn
{
    const char* key;
    const char* title;
};
const SummaryColumn kSummaryColumns[] = {{"parameters", "Param. / react"},  {"systemClass", "Class"}, {"latency", "Latency rep. / meas."}, {"delivery", "Delivery"},
                                         {"blockSizes", "Block size indep."}, {"deterministic", "Deterministic"},       {"finite", "Finite"},
                                         {"recovers", "Recovers"},          {"silence", "Silent"},                 {"ownSignal", "Own signal"},
                                         {"zipper", "Zipper noise"}};
constexpr int kNumberOfSummaryColumns = static_cast<int>(sizeof(kSummaryColumns) / sizeof(kSummaryColumns[0]));

juce::File getSummaryFile(const juce::File& reportFile)
{
    return juce::File(reportFile.getFullPathName() + ".json");
}

// Runs "PluginLabHost --fingerprint" (a report), "PluginLabHost --gui-snapshot" (a GUI review) or "PluginLabHost --map" (survey and mapping) for one plugin in a child process and tells the
// panel when it has ended. The result is written next to its place and put there only when it is complete: a half-written result is never shown.
class PanelJob : public juce::Thread
{
public:
    PanelJob(DeveloperPanel& owner, DeveloperPanel::JobKind kind, const juce::File& pluginFile, const juce::String& identifier, const juce::File& target,
             const juce::String& key)
        : juce::Thread("PanelJob"), m_owner(owner), m_kind(kind), m_pluginFile(pluginFile), m_identifier(identifier), m_target(target), m_key(key)
    {
    }

    ~PanelJob() override
    {
        signalThreadShouldExit();
        stopThread(kStopTimeoutMs);
    }

    void run() override
    {
        const juce::File temporary = m_target.withFileExtension(".part");
        temporary.deleteRecursively();
        m_target.getParentDirectory().createDirectory();
        juce::String option = "--fingerprint";
        if (m_kind == DeveloperPanel::JobKind::GuiReview)
        {
            option = "--gui-snapshot";
        }
        else if (m_kind == DeveloperPanel::JobKind::Mapping)
        {
            option = "--map";
        }
        else if (m_kind == DeveloperPanel::JobKind::Calibration)
        {
            option = "--calibrate";
        }
        juce::ChildProcess process;
        const juce::File host = getHelperExecutable(); // the application, or the PluginLabHost in the plugin's bundle
        const juce::StringArray arguments{host.getFullPathName(), option, m_pluginFile.getFullPathName(), temporary.getFullPathName(), m_identifier};
        bool success = false;
        juce::String message;
        if (process.start(arguments, 0))
        {
            const juce::int64 started = juce::Time::getMillisecondCounter();
            juce::uint32 lastProgress = 0;
            while (process.isRunning() && ! threadShouldExit())
            {
                if (juce::Time::getMillisecondCounter() - lastProgress >= kProgressIntervalMs)
                {
                    lastProgress = juce::Time::getMillisecondCounter();
                    postProgress(temporary);
                }
                if (juce::Time::getMillisecondCounter() - started > static_cast<juce::uint32>(kProcessTimeoutMs))
                {
                    break;
                }
                wait(kPollMs);
            }
            if (process.isRunning())
            {
                process.kill();
                message = "stopped (time limit)";
            }
            else
            {
                // (a child that died from a signal reports exit code 0 on POSIX: the result file is what counts)
                success = finish(temporary, message);
            }
        }
        else
        {
            message = "could not start the host process";
        }
        temporary.deleteRecursively();
        getSummaryFile(temporary).deleteFile();
        getProgressFile(temporary).deleteFile();
        const juce::Component::SafePointer<DeveloperPanel> owner(&m_owner);
        const juce::String key = m_key;
        const DeveloperPanel::JobKind kind = m_kind;
        juce::MessageManager::callAsync([owner, key, kind, success, message]
                                        {
                                            if (owner != nullptr)
                                            {
                                                owner->jobFinished(key, kind, success, message);
                                            }
                                        });
    }

private:
    // the child's progress (a report or a mapping: <file>.progress with the fraction; a GUI review: the step in progress.txt) to the panel
    void postProgress(const juce::File& temporary) const
    {
        JobProgress progress;
        if (m_kind == DeveloperPanel::JobKind::GuiReview)
        {
            progress.step = temporary.getChildFile("progress.txt").loadFileAsString().trim();
        }
        else
        {
            progress = readProgressFile(getProgressFile(temporary));
        }
        const juce::Component::SafePointer<DeveloperPanel> owner(&m_owner);
        const juce::String key = m_key;
        const DeveloperPanel::JobKind kind = m_kind;
        juce::MessageManager::callAsync([owner, key, kind, progress]
                                        {
                                            if (owner != nullptr)
                                            {
                                                owner->jobProgress(key, kind, progress.fraction, progress.step);
                                            }
                                        });
    }

    // moves a complete result into place; for a GUI review that did not complete, the step it was in (progress.txt) is the message
    bool finish(const juce::File& temporary, juce::String& message) const
    {
        if (m_kind == DeveloperPanel::JobKind::Mapping || m_kind == DeveloperPanel::JobKind::Calibration)
        {
            return temporary.existsAsFile() && temporary.getSize() > 0 && temporary.moveFileTo(m_target);
        }
        if (m_kind == DeveloperPanel::JobKind::Report)
        {
            const bool complete = temporary.existsAsFile() && temporary.getSize() > 0 && temporary.moveFileTo(m_target);
            if (complete)
            {
                getSummaryFile(temporary).moveFileTo(getSummaryFile(m_target));
            }
            return complete;
        }
        const juce::String progress = temporary.getChildFile("progress.txt").loadFileAsString().trim();
        if (! temporary.getChildFile(kGuiReviewFile).existsAsFile() || progress != "done")
        {
            message = "crashed while " + progress;
            if (progress.isEmpty())
            {
                message = "crashed before the review started";
            }
            return false;
        }
        m_target.deleteRecursively();
        return temporary.moveFileTo(m_target);
    }

    DeveloperPanel& m_owner;
    DeveloperPanel::JobKind m_kind;
    juce::File m_pluginFile;
    juce::String m_identifier;
    juce::File m_target;
    juce::String m_key;
};

// The report drawn line by line in a monospaced font: headings bold, results worth a look (**...** in the Markdown) and the findings in the
// colour of the Developer page. (A text editor cannot reliably change font or colour from line to line.) The two "**" marks of a
// highlighted text become spaces after it, so the aligned table columns stay aligned.
class ReportView : public juce::Component
{
public:
    // folder: where the images of the report (lines "![text](file)": the logo, contact sheets) are
    ReportView(const juce::String& markdown, const juce::File& folder)
    {
        bool inFindings = false;
        for (const juce::String& line : juce::StringArray::fromLines(pluginlab::engine::alignMarkdownTables(markdown)))
        {
            Line entry;
            if (line.startsWith("!["))
            {
                const juce::String file = line.fromFirstOccurrenceOf("](", false, false).upToLastOccurrenceOf(")", false, false);
                entry.image = juce::ImageFileFormat::loadFrom(folder.getChildFile(file));
                if (entry.image.isValid())
                {
                    m_lines.push_back(entry);
                    continue;
                }
            }
            if (line.startsWith("# ") || line.startsWith("## ") || line.startsWith("### "))
            {
                entry.text = line.fromFirstOccurrenceOf(" ", false, false);
                entry.heading = true;
                inFindings = entry.text == kFindingsHeading;
                m_lines.push_back(entry);
                continue;
            }
            entry.text = line;
            entry.finding = inFindings && line.startsWith("- ") && ! line.contains(kNothingUnusual);
            m_lines.push_back(entry);
        }
        const juce::Font font = makeFont(true);
        int widest = 0;
        int height = 2 * kViewMargin;
        for (const Line& entry : m_lines)
        {
            if (entry.image.isValid())
            {
                widest = juce::jmax(widest, entry.image.getWidth());
                height += entry.image.getHeight() + kImageGap;
                continue;
            }
            widest = juce::jmax(widest, static_cast<int>(juce::GlyphArrangement::getStringWidth(font, entry.text)));
            height += kLineHeight;
        }
        setSize(widest + 2 * kViewMargin, height);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(findColour(juce::ResizableWindow::backgroundColourId).darker(kBackgroundDarker));
        int y = kViewMargin;
        for (const Line& entry : m_lines)
        {
            if (entry.image.isValid())
            {
                g.drawImageAt(entry.image, kViewMargin, y);
                y += entry.image.getHeight() + kImageGap;
                continue;
            }
            if (entry.heading)
            {
                g.setFont(makeFont(true));
                g.setColour(kHeadingColour);
                g.drawSingleLineText(entry.text, kViewMargin, y + kBaseline);
                y += kLineHeight;
                continue;
            }
            g.setFont(makeFont(false));
            drawLine(g, entry, y);
            y += kLineHeight;
        }
    }

private:
    struct Line
    {
        juce::String text;
        bool heading = false;
        bool finding = false;
        juce::Image image;
    };

    static juce::Font makeFont(bool bold)
    {
        int style = juce::Font::plain;
        if (bold)
        {
            style = juce::Font::bold;
        }
        return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), kTextFontHeight, style));
    }

    // the line in pieces: text between "**" marks in the highlight colour, the marks themselves as spaces
    void drawLine(juce::Graphics& g, const Line& entry, int y) const
    {
        const juce::Font font = makeFont(false);
        juce::Colour normal = kTextColour;
        if (entry.finding)
        {
            normal = kHighlightColour;
        }
        float x = static_cast<float>(kViewMargin);
        bool highlighted = false;
        juce::String rest = entry.text;
        while (rest.isNotEmpty())
        {
            const int mark = rest.indexOf(kHighlightMark);
            juce::String piece = rest;
            if (mark >= 0)
            {
                piece = rest.substring(0, mark);
            }
            juce::Colour colour = normal;
            if (highlighted)
            {
                colour = kHighlightColour;
            }
            g.setColour(colour);
            g.drawSingleLineText(piece, static_cast<int>(x), y + kBaseline);
            x += juce::GlyphArrangement::getStringWidth(font, piece);
            if (mark < 0)
            {
                break;
            }
            if (highlighted)
            {
                x += juce::GlyphArrangement::getStringWidth(font, kMarkReplacement); // both marks after the text: the columns stay aligned
            }
            highlighted = ! highlighted;
            rest = rest.substring(mark + kHighlightMark.length());
        }
    }

    static constexpr int kViewMargin = 10;
    static constexpr int kImageGap = 6;
    static constexpr int kLineHeight = 18;
    static constexpr int kBaseline = 14;
    static constexpr float kBackgroundDarker = 0.3f;
    inline static const juce::String kHighlightMark = "**";
    inline static const juce::String kMarkReplacement = "    ";
    inline static const juce::String kFindingsHeading = "Findings";
    inline static const juce::String kNothingUnusual = "nothing unusual";
    inline static const juce::Colour kTextColour{0xffd0d0d0};
    inline static const juce::Colour kHeadingColour{0xff80c0ff};
    inline static const juce::Colour kHighlightColour{0xffffa040};

    std::vector<Line> m_lines;
};

// A window with the report (ReportView in a scrolling view) and a button that copies the report (Markdown) to the clipboard.
class ReportWindow : public juce::DocumentWindow
{
public:
    // actions: further buttons beside "Copy report" (e.g. "Confirm mapping"), their text and what they do
    using Action = std::pair<juce::String, std::function<void()>>;

    ReportWindow(DeveloperPanel& owner, const juce::String& key, const juce::String& title, const juce::String& markdown, const juce::File& folder,
                 const std::vector<Action>& actions = {})
        : juce::DocumentWindow(title, juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                               juce::DocumentWindow::allButtons),
          m_owner(owner),
          m_key(key),
          m_markdown(markdown),
          m_view(markdown, folder)
    {
        m_viewport.setViewedComponent(&m_view, false);
        m_viewport.setScrollBarsShown(true, true);
        m_copyButton.onClick = [this] { juce::SystemClipboard::copyTextToClipboard(m_markdown); };
        m_content.addAndMakeVisible(m_viewport);
        m_content.addAndMakeVisible(m_copyButton);
        for (const Action& action : actions)
        {
            auto button = std::make_unique<juce::TextButton>(action.first);
            button->onClick = action.second;
            m_content.addAndMakeVisible(*button);
            m_actions.push_back(std::move(button));
        }
        m_content.onResize = [this] { layout(); };
        m_content.setSize(kReportWindowWidth, kReportWindowHeight);
        setUsingNativeTitleBar(true);
        setContentNonOwned(&m_content, true);
        setResizable(true, false);
        centreWithSize(kReportWindowWidth, kReportWindowHeight);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        m_owner.closeWindow(m_key);
    }

private:
    // a plain component that tells when it was resized
    class Content : public juce::Component
    {
    public:
        std::function<void()> onResize;

        void resized() override
        {
            if (onResize)
            {
                onResize();
            }
        }
    };

    void layout()
    {
        juce::Rectangle<int> area = m_content.getLocalBounds();
        juce::Rectangle<int> row = area.removeFromBottom(kCopyRowHeight).reduced(kMargin / 2);
        m_copyButton.setBounds(row.removeFromLeft(kCopyButtonWidth));
        for (const auto& button : m_actions)
        {
            row.removeFromLeft(kMargin);
            button->setBounds(row.removeFromLeft(kCopyButtonWidth));
        }
        m_viewport.setBounds(area);
    }

    static constexpr int kCopyRowHeight = 36;
    static constexpr int kCopyButtonWidth = 200;

    DeveloperPanel& m_owner;
    juce::String m_key;
    juce::String m_markdown;
    ReportView m_view;
    juce::Viewport m_viewport;
    juce::TextButton m_copyButton{"Copy report (Markdown)"};
    std::vector<std::unique_ptr<juce::TextButton>> m_actions;
    Content m_content;
};

// A window with the GUI review: its Markdown with the logo and the contact sheets drawn where the review puts them
class GuiReviewWindow : public juce::DocumentWindow
{
public:
    GuiReviewWindow(DeveloperPanel& owner, const juce::String& key, const juce::String& title, const juce::File& folder)
        : juce::DocumentWindow(title, juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                               juce::DocumentWindow::allButtons),
          m_owner(owner),
          m_key(key),
          m_view(folder.getChildFile(kGuiReviewFile).loadFileAsString(), folder)
    {
        m_viewport.setViewedComponent(&m_view, false);
        m_viewport.setScrollBarsShown(true, true);
        m_viewport.setSize(kReportWindowWidth, kReportWindowHeight);
        setUsingNativeTitleBar(true);
        setContentNonOwned(&m_viewport, true);
        setResizable(true, false);
        centreWithSize(kReportWindowWidth, kReportWindowHeight);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        m_owner.closeWindow(m_key);
    }

private:
    DeveloperPanel& m_owner;
    juce::String m_key;
    ReportView m_view;
    juce::Viewport m_viewport;
};

// The cell with the buttons of a row: the report and the GUI review
class ActionCell : public juce::Component
{
public:
    ActionCell(DeveloperPanel& owner)
        : m_owner(owner)
    {
        m_generate.onClick = [this] { m_owner.generateReport(m_row); };
        m_view.onClick = [this] { m_owner.viewReport(m_row); };
        m_review.onClick = [this] { m_owner.generateGuiReview(m_row); };
        m_viewGui.onClick = [this] { m_owner.viewGuiReview(m_row); };
        m_map.onClick = [this] { m_owner.generateMapping(m_row); };
        m_viewMap.onClick = [this] { m_owner.viewMapping(m_row); };
        m_calibrate.onClick = [this] { m_owner.generateCalibration(m_row); };
        m_viewCalibration.onClick = [this] { m_owner.viewCalibration(m_row); };
        m_map.setTooltip("Parameter survey and mapping proposal (W8.4, W8.5): every parameter alone over its range; minutes per plugin");
        m_calibrate.setTooltip("Calibration (W8.6) and interactions between bands (W8.7) with the plugin's mapping (the survey first if there is none)");
        for (juce::TextButton* button : {&m_generate, &m_view, &m_review, &m_viewGui, &m_map, &m_viewMap, &m_calibrate, &m_viewCalibration})
        {
            addAndMakeVisible(button);
        }
    }

    void update(int row, bool ready, bool busy, bool guiReady, bool guiBusy, bool mapReady, bool mapBusy, bool calibrationReady, bool calibrationBusy)
    {
        m_viewCalibration.setEnabled(calibrationReady);
        m_calibrate.setEnabled(! calibrationBusy);
        m_viewMap.setEnabled(mapReady);
        m_map.setEnabled(! mapBusy);
        m_row = row;
        m_view.setEnabled(ready);
        m_generate.setEnabled(! busy);
        m_viewGui.setEnabled(guiReady);
        m_review.setEnabled(! guiBusy);
    }

    void resized() override
    {
        juce::Rectangle<int> area = getLocalBounds().reduced(2);
        for (juce::TextButton* button : {&m_generate, &m_view, &m_review, &m_viewGui, &m_map, &m_viewMap, &m_calibrate, &m_viewCalibration})
        {
            button->setBounds(area.removeFromLeft(kButtonWidth));
            area.removeFromLeft(kMargin);
        }
    }

private:
    DeveloperPanel& m_owner;
    int m_row = 0;
    juce::TextButton m_generate{"Generate report"};
    juce::TextButton m_view{"View report"};
    juce::TextButton m_review{"GUI review"};
    juce::TextButton m_viewGui{"View GUI review"};
    juce::TextButton m_map{"Survey + mapping"};
    juce::TextButton m_viewMap{"View mapping"};
    juce::TextButton m_calibrate{"Calibrate"};
    juce::TextButton m_viewCalibration{"View calibration"};
};

DeveloperPanel::DeveloperPanel(pluginlab::engine::MeasurementEngine& engine, HostSettings& settings)
    : m_engine(engine), m_settings(settings)
{
    m_table.setModel(this);
    m_table.setRowHeight(kRowHeight);
    m_table.setHeaderHeight(kHeaderHeight);
    // the buttons and the state of the report first (visible without scrolling), then the single results, then the date
    m_table.getHeader().addColumn("#", kColumnNumber, kWidthNumber);
    m_table.getHeader().addColumn("Plugin", kColumnName, kWidthName);
    m_table.getHeader().addColumn("", kColumnActions, kWidthActions);
    m_table.getHeader().addColumn("Report", kColumnStatus, kWidthStatus);
    m_table.getHeader().addColumn("GUI review", kColumnGui, kWidthGui);
    m_table.getHeader().addColumn("Mapping", kColumnMapping, kWidthMapping);
    m_table.getHeader().addColumn("Calibration", kColumnCalibration, kWidthCalibration);
    for (int index = 0; index < kNumberOfSummaryColumns; ++index)
    {
        m_table.getHeader().addColumn(kSummaryColumns[index].title, kFirstSummaryColumn + index, kWidthSummary);
    }
    m_table.getHeader().addColumn("Format", kColumnFormat, kWidthFormat);
    m_table.getHeader().addColumn("Date", kColumnDate, kWidthDate);
    m_infoLabel.setText("The plugins loaded on the Plugins page. The columns show the single results of the last report (orange: worth a look; the tooltip "
                        "gives the number). A report tests a plugin technically (parameters, latency at three sample rates, block sizes, "
                        "delivery of parameters, robustness, AES17 measurements) in a process of its own; it can take a minute. The GUI review opens the plugin's "
                        "editor in windows of its own (captures, colour-vision variants, scale factors, robustness, load). Survey + mapping moves every parameter "
                        "alone and proposes a mapping (minutes per plugin); confirm it in its window. Calibrate compares what the knobs show with what the plugin does at "
                        "44.1, 48 and 96 kHz and whether its bands add. Reports are kept in "
                        + m_settings.getFingerprintFolder().getFullPathName() + ", proposed mappings in " + m_settings.getMappingFolder().getFullPathName() + ".",
                        juce::dontSendNotification);
    m_infoLabel.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(m_table);
    addAndMakeVisible(m_infoLabel);
    refresh();
}

DeveloperPanel::~DeveloperPanel()
{
    m_table.setModel(nullptr);
    m_windows.clear();
    m_job.reset();
}

void DeveloperPanel::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(kMargin);
    m_infoLabel.setBounds(area.removeFromTop(kInfoHeight));
    area.removeFromTop(kMargin);
    m_progressArea = area.removeFromTop(kProgressHeight);
    area.removeFromTop(kMargin);
    m_table.setBounds(area);
}

void DeveloperPanel::paint(juce::Graphics& g)
{
    if (m_job == nullptr)
    {
        return;
    }
    const Row* row = nullptr;
    for (const Row& entry : m_rows)
    {
        if (entry.key == m_running.key)
        {
            row = &entry;
        }
    }
    if (row == nullptr)
    {
        return;
    }
    const juce::Rectangle<float> bar = m_progressArea.toFloat();
    g.setColour(kProgressBackground);
    g.fillRoundedRectangle(bar, 4.0f);
    if (m_running.fraction >= 0.0)
    {
        g.setColour(kProgressColour);
        g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * static_cast<float>(juce::jlimit(0.0, 1.0, m_running.fraction))), 4.0f);
    }
    const juce::String job = m_running.kind == JobKind::Mapping       ? "Survey + mapping"
                             : m_running.kind == JobKind::Calibration ? "Calibration"
                             : m_running.kind == JobKind::GuiReview   ? "GUI review"
                                                                      : "Report";
    juce::String text = job + " of " + hosting::getDisplayName(row->description) + ": " + getRunningText(*row, m_running.kind);
    if (! m_queue.empty())
    {
        text << " (" << static_cast<int>(m_queue.size()) << " more waiting)";
    }
    g.setColour(juce::Colours::white);
    g.drawText(text, m_progressArea.reduced(kMargin / 2, 0), juce::Justification::centredLeft, true);
}

juce::File DeveloperPanel::makeReportFile(const juce::PluginDescription& description) const
{
    const juce::String name = pluginlab::engine::makeFileNamePart(pluginlab::hosting::getDisplayName(description));
    const juce::String hash = juce::String::toHexString(description.createIdentifierString().hashCode64());
    return m_settings.getFingerprintFolder().getChildFile(name + "_" + hash + kReportExtension);
}

void DeveloperPanel::updateRowFromDisk(Row& row) const
{
    row.outdated = false;
    row.summary.clear();
    if (! row.reportFile.existsAsFile() || row.reportFile.getSize() == 0)
    {
        row.state = State::NoReport;
        return;
    }
    row.state = State::Ready;
    row.reportTime = row.reportFile.getLastModificationTime();
    row.summary = pluginlab::engine::parseSummaryJson(getSummaryFile(row.reportFile).loadFileAsString());
    row.outdated = pluginlab::hosting::getNewestModificationTime(row.pluginFile) > row.reportTime;
}

// The rows are the plugins of the engine. A row keeps its state while the plugin stays (a report that is being made goes on).
void DeveloperPanel::refresh()
{
    std::vector<Row> rows;
    for (int index = 0; index < m_engine.getNumSlots(); ++index)
    {
        hosting::HostedPlugin* plugin = m_engine.getPlugin(index);
        if (plugin == nullptr)
        {
            continue;
        }
        Row row;
        row.description = plugin->getDescription();
        row.pluginFile = juce::File(row.description.fileOrIdentifier);
        row.key = row.description.createIdentifierString();
        row.reportFile = makeReportFile(row.description);
        row.guiFolder = row.reportFile.getSiblingFile(row.reportFile.getFileNameWithoutExtension() + kGuiFolderSuffix);
        row.mappingFile = row.reportFile.getSiblingFile(row.reportFile.getFileNameWithoutExtension() + kMappingFileSuffix + kReportExtension);
        const Row* known = findRow(row.key);
        row.mappingState = row.mappingFile.existsAsFile() ? State::Ready : State::NoReport;
        if (known != nullptr && (known->mappingState == State::Waiting || known->mappingState == State::Running || known->mappingState == State::Failed))
        {
            row.mappingState = known->mappingState;
        }
        updateMappingConfirmation(row);
        row.calibrationFile = row.reportFile.getSiblingFile(row.reportFile.getFileNameWithoutExtension() + kCalibrationFileSuffix + kReportExtension);
        row.calibrationState = row.calibrationFile.existsAsFile() ? State::Ready : State::NoReport;
        if (known != nullptr && (known->calibrationState == State::Waiting || known->calibrationState == State::Running || known->calibrationState == State::Failed))
        {
            row.calibrationState = known->calibrationState;
        }
        row.guiState = State::NoReport;
        if (row.guiFolder.getChildFile(kGuiReviewFile).existsAsFile())
        {
            row.guiState = State::Ready;
        }
        if (known != nullptr && (known->guiState == State::Waiting || known->guiState == State::Running || known->guiState == State::Failed))
        {
            row.guiState = known->guiState;
            row.guiMessage = known->guiMessage;
        }
        if (known != nullptr && (known->state == State::Waiting || known->state == State::Running || known->state == State::Failed))
        {
            row.state = known->state;
        }
        else
        {
            updateRowFromDisk(row);
        }
        rows.push_back(row);
    }
    m_rows = rows;
    // reports of plugins that were unloaded are not shown any more
    for (auto window = m_windows.begin(); window != m_windows.end();)
    {
        if (findRow(window->first.upToFirstOccurrenceOf("#", false, false)) == nullptr) // the key without a window suffix
        {
            window = m_windows.erase(window);
            continue;
        }
        ++window;
    }
    m_table.updateContent();
    m_table.repaint();
}

DeveloperPanel::Row* DeveloperPanel::findRow(const juce::String& key)
{
    for (Row& row : m_rows)
    {
        if (row.key == key)
        {
            return &row;
        }
    }
    return nullptr;
}

int DeveloperPanel::getNumRows()
{
    return static_cast<int>(m_rows.size());
}

juce::String DeveloperPanel::getStatusText(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
    {
        return {};
    }
    const Row& entry = m_rows[static_cast<size_t>(row)];
    switch (entry.state)
    {
        case State::NoReport:
            return "no report yet";
        case State::Waiting:
            return "waiting ...";
        case State::Running:
            return getRunningText(entry, JobKind::Report);
        case State::Failed:
            return "failed (crash?)";
        case State::Ready:
            if (entry.outdated)
            {
                return "ready (plugin is newer)";
            }
            return "ready";
    }
    return {};
}

bool DeveloperPanel::isReportReady(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
    {
        return false;
    }
    const Row& entry = m_rows[static_cast<size_t>(row)];
    return entry.reportFile.existsAsFile() && entry.state != State::Running && entry.state != State::Waiting;
}

void DeveloperPanel::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    juce::ignoreUnused(rowNumber, width, height);
    if (rowIsSelected)
    {
        g.fillAll(juce::Colour(0xff4060a0));
    }
}

void DeveloperPanel::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(m_rows.size()))
    {
        return;
    }
    const Row& row = m_rows[static_cast<size_t>(rowNumber)];
    juce::String text;
    if (columnId == kColumnNumber)
    {
        text = juce::String(rowNumber + 1);
    }
    else if (columnId == kColumnName)
    {
        text = hosting::getDisplayName(row.description);
    }
    else if (columnId == kColumnFormat)
    {
        text = row.description.pluginFormatName;
    }
    else if (columnId == kColumnStatus)
    {
        text = getStatusText(rowNumber);
    }
    else if (columnId == kColumnGui)
    {
        text = getGuiStatusText(rowNumber);
    }
    else if (columnId == kColumnMapping)
    {
        text = getMappingStatusText(rowNumber);
    }
    else if (columnId == kColumnCalibration)
    {
        text = getCalibrationStatusText(rowNumber);
    }
    else if (columnId == kColumnDate && row.reportFile.existsAsFile())
    {
        text = row.reportTime.formatted(kDateFormat);
    }
    juce::Colour colour(0xffd0d0d0);
    const pluginlab::engine::SummaryItem* item = findSummaryItem(row, columnId);
    if (item != nullptr)
    {
        text = item->result;
        if (! item->good)
        {
            colour = kDoubtfulColour;
        }
    }
    const bool doubtful = item != nullptr && ! item->good;
    if (rowIsSelected && ! doubtful)
    {
        colour = juce::Colours::white;
    }
    if (isRunningCell(row, columnId))
    {
        // a progress bar behind the text: the fraction the child process reported (none yet: only the text)
        const juce::Rectangle<float> bar(static_cast<float>(kMargin) / 4.0f, 3.0f, static_cast<float>(width - kMargin / 2), static_cast<float>(height - 6));
        g.setColour(kProgressBackground);
        g.fillRoundedRectangle(bar, 3.0f);
        if (m_running.fraction >= 0.0)
        {
            g.setColour(kProgressColour);
            g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * static_cast<float>(juce::jlimit(0.0, 1.0, m_running.fraction))), 3.0f);
        }
        colour = juce::Colours::white;
    }
    g.setColour(colour);
    g.drawText(text, kMargin / 2, 0, width - kMargin, height, juce::Justification::centredLeft, true);
}

const pluginlab::engine::SummaryItem* DeveloperPanel::findSummaryItem(const Row& row, int columnId) const
{
    const int index = columnId - kFirstSummaryColumn;
    if (index < 0 || index >= kNumberOfSummaryColumns)
    {
        return nullptr;
    }
    for (const pluginlab::engine::SummaryItem& item : row.summary)
    {
        if (item.key == kSummaryColumns[index].key)
        {
            return &item;
        }
    }
    return nullptr;
}

juce::String DeveloperPanel::getCellTooltip(int rowNumber, int columnId)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(m_rows.size()))
    {
        return {};
    }
    const Row& row = m_rows[static_cast<size_t>(rowNumber)];
    if (isRunningCell(row, columnId))
    {
        return getRunningText(row, m_running.kind);
    }
    const pluginlab::engine::SummaryItem* item = findSummaryItem(row, columnId);
    if (item == nullptr)
    {
        return {};
    }
    juce::String tip = item->test + ": " + item->result;
    if (item->detail.isNotEmpty())
    {
        tip += " (" + item->detail + ")";
    }
    return tip;
}

juce::Component* DeveloperPanel::refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate)
{
    juce::ignoreUnused(isRowSelected);
    if (columnId != kColumnActions || rowNumber < 0 || rowNumber >= static_cast<int>(m_rows.size()))
    {
        delete existingComponentToUpdate;
        return nullptr;
    }
    auto* cell = dynamic_cast<ActionCell*>(existingComponentToUpdate);
    if (cell == nullptr)
    {
        delete existingComponentToUpdate;
        cell = new ActionCell(*this);
    }
    const Row& row = m_rows[static_cast<size_t>(rowNumber)];
    cell->update(rowNumber, isReportReady(rowNumber), row.state == State::Running || row.state == State::Waiting, isGuiReviewReady(rowNumber),
                 row.guiState == State::Running || row.guiState == State::Waiting, isMappingReady(rowNumber),
                 row.mappingState == State::Running || row.mappingState == State::Waiting, isCalibrationReady(rowNumber),
                 row.calibrationState == State::Running || row.calibrationState == State::Waiting);
    return cell;
}

void DeveloperPanel::generateReport(int row)
{
    queueJob(row, JobKind::Report);
}

void DeveloperPanel::generateGuiReview(int row)
{
    queueJob(row, JobKind::GuiReview);
}

void DeveloperPanel::generateMapping(int row)
{
    queueJob(row, JobKind::Mapping);
}

void DeveloperPanel::generateCalibration(int row)
{
    queueJob(row, JobKind::Calibration);
}

void DeveloperPanel::queueJob(int row, JobKind kind)
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
    {
        return;
    }
    Row& entry = m_rows[static_cast<size_t>(row)];
    State* stateOfKind = &entry.state;
    if (kind == JobKind::GuiReview)
    {
        stateOfKind = &entry.guiState;
    }
    else if (kind == JobKind::Mapping)
    {
        stateOfKind = &entry.mappingState;
    }
    else if (kind == JobKind::Calibration)
    {
        stateOfKind = &entry.calibrationState;
    }
    State& target = *stateOfKind;
    if (target == State::Running || target == State::Waiting)
    {
        return;
    }
    target = State::Waiting;
    m_queue.emplace_back(entry.key, kind);
    m_table.updateContent();
    m_table.repaint();
    startNextJob();
}

void DeveloperPanel::startNextJob()
{
    if (m_job != nullptr || m_queue.empty())
    {
        return;
    }
    const auto [key, kind] = m_queue.front();
    m_queue.pop_front();
    Row* row = findRow(key);
    if (row == nullptr)
    {
        startNextJob(); // the plugin was unloaded in the meantime
        return;
    }
    // a file with many plugins (a bundle): only this plugin is measured
    if (kind == JobKind::Report)
    {
        row->state = State::Running;
        m_job = std::make_unique<PanelJob>(*this, kind, row->pluginFile, row->key, row->reportFile, row->key);
    }
    else if (kind == JobKind::Mapping)
    {
        row->mappingState = State::Running;
        m_job = std::make_unique<PanelJob>(*this, kind, row->pluginFile, row->key, row->mappingFile, row->key);
    }
    else if (kind == JobKind::Calibration)
    {
        row->calibrationState = State::Running;
        m_job = std::make_unique<PanelJob>(*this, kind, row->pluginFile, row->key, row->calibrationFile, row->key);
    }
    else
    {
        row->guiState = State::Running;
        m_job = std::make_unique<PanelJob>(*this, kind, row->pluginFile, row->key, row->guiFolder, row->key);
    }
    m_running = RunningJob();
    m_running.key = key;
    m_running.kind = kind;
    m_running.startedMs = juce::Time::getMillisecondCounter();
    m_job->startThread();
    m_table.updateContent();
    m_table.repaint();
}

void DeveloperPanel::jobProgress(const juce::String& key, JobKind kind, double fraction, const juce::String& step)
{
    if (m_job == nullptr || key != m_running.key || kind != m_running.kind)
    {
        return;
    }
    m_running.fraction = fraction;
    m_running.step = step;
    m_table.repaint(); // also when nothing new came: the elapsed time
    repaint(m_progressArea);
}

juce::String DeveloperPanel::getRunningText(const Row& row, JobKind kind) const
{
    if (m_job == nullptr || row.key != m_running.key || kind != m_running.kind)
    {
        return {};
    }
    const int seconds = static_cast<int>((juce::Time::getMillisecondCounter() - m_running.startedMs) / 1000);
    const juce::String elapsed = juce::String(seconds / 60) + ":" + juce::String(seconds % 60).paddedLeft('0', 2);
    juce::String text;
    if (m_running.fraction >= 0.0)
    {
        text << juce::roundToInt(100.0 * m_running.fraction) << " %, ";
    }
    if (m_running.step.isNotEmpty())
    {
        text << m_running.step << ", ";
    }
    else
    {
        text << (kind == JobKind::Mapping ? "surveying" : kind == JobKind::Calibration ? "calibrating" : kind == JobKind::GuiReview ? "reviewing (editor windows open)" : "measuring")
             << ", ";
    }
    return text + elapsed;
}

bool DeveloperPanel::isRunningCell(const Row& row, int columnId) const
{
    if (m_job == nullptr || row.key != m_running.key)
    {
        return false;
    }
    return (columnId == kColumnStatus && m_running.kind == JobKind::Report) || (columnId == kColumnMapping && m_running.kind == JobKind::Mapping)
           || (columnId == kColumnGui && m_running.kind == JobKind::GuiReview) || (columnId == kColumnCalibration && m_running.kind == JobKind::Calibration);
}

void DeveloperPanel::jobFinished(const juce::String& key, JobKind kind, bool success, const juce::String& message)
{
    m_job.reset();
    m_running = RunningJob();
    repaint(m_progressArea);
    Row* row = findRow(key);
    if (row != nullptr)
    {
        const int index = static_cast<int>(row - m_rows.data());
        if (kind == JobKind::Report)
        {
            if (success)
            {
                updateRowFromDisk(*row);
                if (onReportReady)
                {
                    onReportReady(index);
                }
            }
            else
            {
                row->state = State::Failed;
            }
        }
        else if (kind == JobKind::Calibration)
        {
            row->calibrationState = success ? State::Ready : State::Failed;
            updateMappingConfirmation(*row); // a calibration without a mapping proposes one first
            if (success)
            {
                m_windows.erase(key + kCalibrationWindowSuffix); // an open window shows the old report
                if (onCalibrationReady)
                {
                    onCalibrationReady(index);
                }
            }
        }
        else if (kind == JobKind::Mapping)
        {
            row->mappingState = success ? State::Ready : State::Failed;
            updateMappingConfirmation(*row);
            if (success)
            {
                m_windows.erase(key + kMappingWindowSuffix); // an open window shows the old report
                if (onMappingReady)
                {
                    onMappingReady(index);
                }
            }
        }
        else
        {
            row->guiState = State::Failed;
            row->guiMessage = message;
            if (success)
            {
                row->guiState = State::Ready;
                row->guiMessage.clear();
                m_windows.erase(key + kGuiWindowSuffix); // an open window shows the old review
                if (onGuiReviewReady)
                {
                    onGuiReviewReady(index);
                }
            }
        }
    }
    m_table.updateContent();
    m_table.repaint();
    startNextJob();
}

juce::String DeveloperPanel::getGuiStatusText(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
    {
        return {};
    }
    const Row& entry = m_rows[static_cast<size_t>(row)];
    switch (entry.guiState)
    {
        case State::NoReport:
            return "no review yet";
        case State::Waiting:
            return "waiting ...";
        case State::Running:
            return getRunningText(entry, JobKind::GuiReview);
        case State::Failed:
            return entry.guiMessage;
        case State::Ready:
            return "ready";
    }
    return {};
}

bool DeveloperPanel::isGuiReviewReady(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
    {
        return false;
    }
    const Row& entry = m_rows[static_cast<size_t>(row)];
    return entry.guiFolder.getChildFile(kGuiReviewFile).existsAsFile() && entry.guiState != State::Running && entry.guiState != State::Waiting;
}

void DeveloperPanel::viewGuiReview(int row)
{
    if (! isGuiReviewReady(row))
    {
        return;
    }
    const Row& entry = m_rows[static_cast<size_t>(row)];
    const juce::String windowKey = entry.key + kGuiWindowSuffix;
    const auto existing = m_windows.find(windowKey);
    if (existing != m_windows.end())
    {
        existing->second->toFront(true);
        return;
    }
    const juce::Time time = entry.guiFolder.getChildFile(kGuiReviewFile).getLastModificationTime();
    const juce::String title = "GUI review: " + hosting::getDisplayName(entry.description) + " (" + time.formatted(kDateFormat) + ")";
    m_windows[windowKey] = std::make_unique<GuiReviewWindow>(*this, windowKey, title, entry.guiFolder);
}

void DeveloperPanel::viewReport(int row)
{
    if (! isReportReady(row))
    {
        return;
    }
    const Row& entry = m_rows[static_cast<size_t>(row)];
    const auto existing = m_windows.find(entry.key);
    if (existing != m_windows.end())
    {
        existing->second->toFront(true);
        return;
    }
    const juce::String title = "Report: " + hosting::getDisplayName(entry.description) + " (" + entry.reportTime.formatted(kDateFormat) + ")";
    m_windows[entry.key] = std::make_unique<ReportWindow>(*this, entry.key, title, entry.reportFile.loadFileAsString(), entry.reportFile.getParentDirectory());
}

juce::String DeveloperPanel::getMappingStatusText(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
    {
        return {};
    }
    switch (m_rows[static_cast<size_t>(row)].mappingState)
    {
        case State::NoReport:
            return "no survey yet";
        case State::Waiting:
            return "waiting ...";
        case State::Running:
            return getRunningText(m_rows[static_cast<size_t>(row)], JobKind::Mapping);
        case State::Failed:
            return "failed (crash or time limit)";
        case State::Ready:
        {
            const Row& entry = m_rows[static_cast<size_t>(row)];
            if (entry.mappingConfirmedBy.isNotEmpty())
            {
                return "ready, confirmed by " + entry.mappingConfirmedBy;
            }
            return entry.mappingJsonExists ? "ready, proposal (not confirmed)" : "ready";
        }
    }
    return {};
}

bool DeveloperPanel::isMappingReady(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
    {
        return false;
    }
    const Row& entry = m_rows[static_cast<size_t>(row)];
    return entry.mappingFile.existsAsFile() && entry.mappingState != State::Running && entry.mappingState != State::Waiting;
}

void DeveloperPanel::viewMapping(int row)
{
    if (! isMappingReady(row))
    {
        return;
    }
    const Row& entry = m_rows[static_cast<size_t>(row)];
    const juce::String windowKey = entry.key + kMappingWindowSuffix;
    const auto existing = m_windows.find(windowKey);
    if (existing != m_windows.end())
    {
        existing->second->toFront(true);
        return;
    }
    const juce::String title = "Survey and mapping: " + hosting::getDisplayName(entry.description) + " ("
                               + entry.mappingFile.getLastModificationTime().formatted(kDateFormat) + ")";
    const std::vector<ReportWindow::Action> actions = {{"Confirm mapping ...", [this, row] { confirmMapping(row); }},
                                                       {"Open mapping file", [this, row] { openMappingFile(row); }}};
    m_windows[windowKey] = std::make_unique<ReportWindow>(*this, windowKey, title, entry.mappingFile.loadFileAsString(), entry.mappingFile.getParentDirectory(), actions);
}

juce::String DeveloperPanel::getCalibrationStatusText(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
    {
        return {};
    }
    const Row& entry = m_rows[static_cast<size_t>(row)];
    switch (entry.calibrationState)
    {
        case State::NoReport:
            return "no calibration yet";
        case State::Waiting:
            return "waiting ...";
        case State::Running:
            return getRunningText(entry, JobKind::Calibration);
        case State::Failed:
            return "failed (crash or time limit)";
        case State::Ready:
            return "ready (" + entry.calibrationFile.getLastModificationTime().formatted(kDateFormat) + ")";
    }
    return {};
}

bool DeveloperPanel::isCalibrationReady(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
    {
        return false;
    }
    const Row& entry = m_rows[static_cast<size_t>(row)];
    return entry.calibrationFile.existsAsFile() && entry.calibrationState != State::Running && entry.calibrationState != State::Waiting;
}

void DeveloperPanel::viewCalibration(int row)
{
    if (! isCalibrationReady(row))
    {
        return;
    }
    const Row& entry = m_rows[static_cast<size_t>(row)];
    const juce::String windowKey = entry.key + kCalibrationWindowSuffix;
    const auto existing = m_windows.find(windowKey);
    if (existing != m_windows.end())
    {
        existing->second->toFront(true);
        return;
    }
    const juce::String title = "Calibration: " + hosting::getDisplayName(entry.description) + " ("
                               + entry.calibrationFile.getLastModificationTime().formatted(kDateFormat) + ")";
    m_windows[windowKey] =
        std::make_unique<ReportWindow>(*this, windowKey, title, entry.calibrationFile.loadFileAsString(), entry.calibrationFile.getParentDirectory());
}

juce::File DeveloperPanel::getMappingJson(const Row& row) const
{
    return m_settings.getMappingFolder().getChildFile(pluginlab::engine::getMappingFileName(row.description.name));
}

void DeveloperPanel::updateMappingConfirmation(Row& row) const
{
    juce::String error;
    const std::optional<pluginlab::engine::Mapping> mapping = pluginlab::engine::Mapping::read(getMappingJson(row), error);
    row.mappingJsonExists = mapping.has_value();
    row.mappingConfirmedBy = mapping ? mapping->confirmedBy : juce::String();
}

void DeveloperPanel::openMappingFile(int row)
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
    {
        return;
    }
    const Row& entry = m_rows[static_cast<size_t>(row)];
    const juce::File json = getMappingJson(entry);
    const juce::File proposal = json.withFileExtension(".proposal.json");
    const juce::File file = proposal.existsAsFile() ? proposal : json; // a new proposal next to a confirmed mapping is what needs checking
    if (file.existsAsFile())
    {
        file.startAsProcess();
    }
}

void DeveloperPanel::confirmMapping(int row)
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
    {
        return;
    }
    const Row& entry = m_rows[static_cast<size_t>(row)];
    const juce::File json = getMappingJson(entry);
    const juce::File proposal = json.withFileExtension(".proposal.json");
    // a newer proposal next to a confirmed mapping replaces it when confirmed; otherwise the mapping itself is confirmed
    const juce::File source = proposal.existsAsFile() ? proposal : json;
    juce::String error;
    if (! pluginlab::engine::Mapping::read(source, error))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Confirm mapping", "No mapping to confirm: " + error);
        return;
    }
    juce::String name = m_settings.getMappingConfirmer();
    if (name.isEmpty())
    {
        name = juce::SystemStats::getFullUserName().isNotEmpty() ? juce::SystemStats::getFullUserName() : juce::SystemStats::getLogonName();
    }
    auto* window = new juce::AlertWindow("Confirm mapping",
                                         "You have checked every row of the mapping against the survey (and corrected the JSON where it was wrong). "
                                         "Your name goes into confirmedBy, with today's date:\n" + source.getFullPathName(),
                                         juce::MessageBoxIconType::QuestionIcon);
    window->addTextEditor("name", name, "Name:");
    window->addButton("Confirm", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    const juce::Component::SafePointer<DeveloperPanel> self(this);
    const juce::String key = entry.key;
    window->enterModalState(true, juce::ModalCallbackFunction::create([self, window, source, json, proposal, key](int result) {
                                if (self == nullptr || result != 1)
                                {
                                    return;
                                }
                                const juce::String confirmer = window->getTextEditorContents("name").trim();
                                if (confirmer.isEmpty())
                                {
                                    return;
                                }
                                juce::String readError;
                                std::optional<pluginlab::engine::Mapping> mapping = pluginlab::engine::Mapping::read(source, readError);
                                if (! mapping)
                                {
                                    return;
                                }
                                mapping->confirmedBy = confirmer + " (" + juce::Time::getCurrentTime().formatted("%Y-%m-%d") + ")";
                                if (mapping->write(json) && source == proposal)
                                {
                                    proposal.deleteFile();
                                }
                                self->m_settings.setMappingConfirmer(confirmer);
                                if (Row* changed = self->findRow(key))
                                {
                                    self->updateMappingConfirmation(*changed);
                                }
                                self->m_table.repaint();
                            }),
                            true);
}

void DeveloperPanel::closeWindow(const juce::String& key)
{
    const juce::Component::SafePointer<DeveloperPanel> self(this);
    juce::MessageManager::callAsync([self, key]
                                    {
                                        if (self != nullptr)
                                        {
                                            self->m_windows.erase(key);
                                        }
                                    });
}

void DeveloperPanel::showReport(const juce::String& title, const juce::File& report)
{
    const juce::String key = "report#" + report.getFullPathName();
    const auto existing = m_windows.find(key);
    if (existing != m_windows.end())
    {
        existing->second->toFront(true);
        return;
    }
    m_windows[key] = std::make_unique<ReportWindow>(*this, key, title, report.loadFileAsString(), report.getParentDirectory());
}
}

