#include "DeveloperPanel.h"

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
constexpr int kInfoHeight = 48;
constexpr int kColumnNumber = 1;
constexpr int kColumnName = 2;
constexpr int kColumnFormat = 3;
constexpr int kColumnStatus = 4;
constexpr int kColumnDate = 5;
constexpr int kColumnActions = 6;
constexpr int kColumnGui = 7;
constexpr int kFirstSummaryColumn = 10; // the columns of the single results follow, in the order of kSummaryColumns
constexpr int kWidthSummary = 90;
const juce::Colour kDoubtfulColour(0xffffa040);
constexpr int kWidthNumber = 30;
constexpr int kWidthName = 200;
constexpr int kWidthFormat = 60;
constexpr int kWidthStatus = 180;
constexpr int kWidthDate = 150;
constexpr int kWidthActions = 480;
constexpr int kWidthGui = 220;
constexpr int kButtonWidth = 110;
constexpr int kProcessTimeoutMs = 20 * 60 * 1000; // the measurement loads the plugin many times; a big plugin needs minutes
constexpr int kPollMs = 200;
constexpr int kStopTimeoutMs = 10000;
constexpr int kReportWindowWidth = 1000;
constexpr int kReportWindowHeight = 720;
constexpr float kTextFontHeight = 14.0f;
const juce::String kReportExtension = ".md";
const juce::String kGuiFolderSuffix = "_gui";
const juce::String kGuiReviewFile = "gui_review.md";
const juce::String kGuiWindowSuffix = "#gui";
const juce::String kDateFormat = "%Y-%m-%d %H:%M";
}

// The single results shown as columns: the key of the summary item and the column title (the full test name is the tooltip)
struct SummaryColumn
{
    const char* key;
    const char* title;
};
const SummaryColumn kSummaryColumns[] = {{"parameters", "Param. / react"},  {"latency", "Latency rep. / meas."}, {"delivery", "Delivery"},
                                         {"blockSizes", "Block size indep."}, {"deterministic", "Deterministic"},       {"finite", "Finite"},
                                         {"recovers", "Recovers"},          {"silence", "Silent"},                 {"ownSignal", "Own signal"}};
constexpr int kNumberOfSummaryColumns = static_cast<int>(sizeof(kSummaryColumns) / sizeof(kSummaryColumns[0]));

juce::File getSummaryFile(const juce::File& reportFile)
{
    return juce::File(reportFile.getFullPathName() + ".json");
}

// Runs "PluginLabHost --fingerprint" (a report) or "PluginLabHost --gui-snapshot" (a GUI review) for one plugin in a child process and tells the
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
        juce::ChildProcess process;
        const juce::File host = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        const juce::StringArray arguments{host.getFullPathName(), option, m_pluginFile.getFullPathName(), temporary.getFullPathName(), m_identifier};
        bool success = false;
        juce::String message;
        if (process.start(arguments, 0))
        {
            const juce::int64 started = juce::Time::getMillisecondCounter();
            while (process.isRunning() && ! threadShouldExit())
            {
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
    // moves a complete result into place; for a GUI review that did not complete, the step it was in (progress.txt) is the message
    bool finish(const juce::File& temporary, juce::String& message) const
    {
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
    explicit ReportView(const juce::String& markdown)
    {
        bool inFindings = false;
        for (const juce::String& line : juce::StringArray::fromLines(pluginlab::engine::alignMarkdownTables(markdown)))
        {
            Line entry;
            if (line.startsWith("# ") || line.startsWith("## "))
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
        for (const Line& entry : m_lines)
        {
            widest = juce::jmax(widest, static_cast<int>(juce::GlyphArrangement::getStringWidth(font, entry.text)));
        }
        setSize(widest + 2 * kViewMargin, static_cast<int>(m_lines.size()) * kLineHeight + 2 * kViewMargin);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(findColour(juce::ResizableWindow::backgroundColourId).darker(kBackgroundDarker));
        int y = kViewMargin;
        for (const Line& entry : m_lines)
        {
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
    ReportWindow(DeveloperPanel& owner, const juce::String& key, const juce::String& title, const juce::String& markdown)
        : juce::DocumentWindow(title, juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                               juce::DocumentWindow::allButtons),
          m_owner(owner),
          m_key(key),
          m_markdown(markdown),
          m_view(markdown)
    {
        m_viewport.setViewedComponent(&m_view, false);
        m_viewport.setScrollBarsShown(true, true);
        m_copyButton.onClick = [this] { juce::SystemClipboard::copyTextToClipboard(m_markdown); };
        m_content.addAndMakeVisible(m_viewport);
        m_content.addAndMakeVisible(m_copyButton);
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
        m_copyButton.setBounds(area.removeFromBottom(kCopyRowHeight).reduced(kMargin / 2).removeFromLeft(kCopyButtonWidth));
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
    Content m_content;
};

// A window with the GUI review: its summary (as the report) and the two contact sheets (vision variants, sizes and scale factors) below it
class GuiReviewWindow : public juce::DocumentWindow
{
public:
    GuiReviewWindow(DeveloperPanel& owner, const juce::String& key, const juce::String& title, const juce::File& folder)
        : juce::DocumentWindow(title, juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                               juce::DocumentWindow::allButtons),
          m_owner(owner),
          m_key(key),
          m_view(folder.getChildFile(kGuiReviewFile).loadFileAsString())
    {
        m_content.addAndMakeVisible(m_view);
        int width = m_view.getWidth();
        int height = m_view.getHeight();
        for (const char* name : {"contact_sheet_vision.png", "contact_sheet_sizes.png"})
        {
            const juce::Image image = juce::ImageFileFormat::loadFrom(folder.getChildFile(name));
            if (! image.isValid())
            {
                continue;
            }
            auto sheet = std::make_unique<juce::ImageComponent>();
            sheet->setImage(image, juce::RectanglePlacement(juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yTop | juce::RectanglePlacement::doNotResize));
            sheet->setBounds(0, height + kMargin, image.getWidth(), image.getHeight());
            height += image.getHeight() + kMargin;
            width = juce::jmax(width, image.getWidth());
            m_content.addAndMakeVisible(*sheet);
            m_sheets.push_back(std::move(sheet));
        }
        m_content.setSize(width, height);
        m_viewport.setViewedComponent(&m_content, false);
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
    juce::Component m_content;
    std::vector<std::unique_ptr<juce::ImageComponent>> m_sheets;
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
        for (juce::TextButton* button : {&m_generate, &m_view, &m_review, &m_viewGui})
        {
            addAndMakeVisible(button);
        }
    }

    void update(int row, bool ready, bool busy, bool guiReady, bool guiBusy)
    {
        m_row = row;
        m_view.setEnabled(ready);
        m_generate.setEnabled(! busy);
        m_viewGui.setEnabled(guiReady);
        m_review.setEnabled(! guiBusy);
    }

    void resized() override
    {
        juce::Rectangle<int> area = getLocalBounds().reduced(2);
        for (juce::TextButton* button : {&m_generate, &m_view, &m_review, &m_viewGui})
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
    for (int index = 0; index < kNumberOfSummaryColumns; ++index)
    {
        m_table.getHeader().addColumn(kSummaryColumns[index].title, kFirstSummaryColumn + index, kWidthSummary);
    }
    m_table.getHeader().addColumn("Format", kColumnFormat, kWidthFormat);
    m_table.getHeader().addColumn("Date", kColumnDate, kWidthDate);
    m_infoLabel.setText("The plugins loaded on the Plugins page. The columns show the single results of the last report (orange: worth a look; the tooltip "
                        "gives the number). A report tests a plugin technically (parameters, latency at three sample rates, block sizes, "
                        "delivery of parameters, robustness, AES17 measurements) in a process of its own; it can take a minute. The GUI review opens the plugin's "
                        "editor in windows of its own (captures, colour-vision variants, scale factors, robustness, load). Reports are kept in "
                        + m_settings.getFingerprintFolder().getFullPathName() + ".",
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
    m_table.setBounds(area);
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
        const Row* known = findRow(row.key);
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
        if (findRow(window->first.upToFirstOccurrenceOf(kGuiWindowSuffix, false, false)) == nullptr)
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
            return "measuring ...";
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
    const pluginlab::engine::SummaryItem* item = findSummaryItem(m_rows[static_cast<size_t>(rowNumber)], columnId);
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
                 row.guiState == State::Running || row.guiState == State::Waiting);
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
    else
    {
        row->guiState = State::Running;
        m_job = std::make_unique<PanelJob>(*this, kind, row->pluginFile, row->key, row->guiFolder, row->key);
    }
    m_job->startThread();
    m_table.updateContent();
    m_table.repaint();
}

void DeveloperPanel::jobFinished(const juce::String& key, JobKind kind, bool success, const juce::String& message)
{
    m_job.reset();
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
            return "reviewing (editor windows open) ...";
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
    m_windows[entry.key] = std::make_unique<ReportWindow>(*this, entry.key, title, entry.reportFile.loadFileAsString());
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
}
