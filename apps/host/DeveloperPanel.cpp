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
constexpr int kFirstSummaryColumn = 10; // the columns of the single results follow, in the order of kSummaryColumns
constexpr int kWidthSummary = 90;
const juce::Colour kDoubtfulColour(0xffffa040);
constexpr int kWidthNumber = 30;
constexpr int kWidthName = 200;
constexpr int kWidthFormat = 60;
constexpr int kWidthStatus = 180;
constexpr int kWidthDate = 150;
constexpr int kWidthActions = 240;
constexpr int kButtonWidth = 110;
constexpr int kProcessTimeoutMs = 20 * 60 * 1000; // the measurement loads the plugin many times; a big plugin needs minutes
constexpr int kPollMs = 200;
constexpr int kStopTimeoutMs = 10000;
constexpr int kReportWindowWidth = 1000;
constexpr int kReportWindowHeight = 720;
constexpr float kTextFontHeight = 14.0f;
const juce::String kReportExtension = ".md";
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

// Runs "PluginLabHost --fingerprint" for one plugin in a child process and tells the panel when it has ended.
class FingerprintJob : public juce::Thread
{
public:
    FingerprintJob(DeveloperPanel& owner, const juce::File& pluginFile, const juce::String& identifier, const juce::File& reportFile,
                   const juce::String& key)
        : juce::Thread("FingerprintJob"), m_owner(owner), m_pluginFile(pluginFile), m_identifier(identifier), m_reportFile(reportFile), m_key(key)
    {
    }

    ~FingerprintJob() override
    {
        signalThreadShouldExit();
        stopThread(kStopTimeoutMs);
    }

    void run() override
    {
        // written next to the report and put in place when it is complete: a report that is being written is never shown
        const juce::File temporary = m_reportFile.withFileExtension(".part");
        temporary.deleteFile();
        m_reportFile.getParentDirectory().createDirectory();

        juce::ChildProcess process;
        const juce::File host = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        const juce::StringArray arguments{host.getFullPathName(), "--fingerprint", m_pluginFile.getFullPathName(), temporary.getFullPathName(),
                                          m_identifier};
        bool success = false;
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
            }
            else
            {
                // (a child that died from a signal reports exit code 0 on POSIX: the report file is what counts)
                success = temporary.existsAsFile() && temporary.getSize() > 0 && temporary.moveFileTo(m_reportFile);
                if (success)
                {
                    getSummaryFile(temporary).moveFileTo(getSummaryFile(m_reportFile));
                }
            }
        }
        temporary.deleteFile();
        getSummaryFile(temporary).deleteFile();
        const juce::Component::SafePointer<DeveloperPanel> owner(&m_owner);
        const juce::String key = m_key;
        juce::MessageManager::callAsync([owner, key, success]
                                        {
                                            if (owner != nullptr)
                                            {
                                                owner->jobFinished(key, success);
                                            }
                                        });
    }

private:
    DeveloperPanel& m_owner;
    juce::File m_pluginFile;
    juce::String m_identifier;
    juce::File m_reportFile;
    juce::String m_key;
};

// A window with the report: a read-only text with a monospaced font, the tables aligned, the headings larger.
class ReportWindow : public juce::DocumentWindow
{
public:
    ReportWindow(DeveloperPanel& owner, const juce::String& key, const juce::String& title, const juce::String& markdown)
        : juce::DocumentWindow(title, juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                               juce::DocumentWindow::allButtons),
          m_owner(owner),
          m_key(key)
    {
        m_text.setMultiLine(true, false); // no wrapping: the tables are wide, the editor scrolls
        m_text.setReadOnly(true);
        m_text.setScrollbarsShown(true);
        m_text.setCaretVisible(false);

        // one font only (a text editor cannot reliably change the font from line to line); the headings are underlined with characters
        m_text.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), kTextFontHeight, juce::Font::plain)));
        juce::String text;
        for (const juce::String& line : juce::StringArray::fromLines(pluginlab::engine::alignMarkdownTables(markdown)))
        {
            if (line.startsWith("# "))
            {
                const juce::String title = line.substring(2);
                text += title.toUpperCase() + "\n" + juce::String::repeatedString("=", title.length()) + "\n";
                continue;
            }
            if (line.startsWith("## "))
            {
                const juce::String heading = line.substring(3);
                text += heading + "\n" + juce::String::repeatedString("~", heading.length()) + "\n";
                continue;
            }
            text += line + "\n";
        }
        m_text.setText(text, false);
        m_text.moveCaretToTop(false);
        setUsingNativeTitleBar(true);
        setContentNonOwned(&m_text, false);
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
    juce::TextEditor m_text;
};

// The cell with the two buttons of a row
class ActionCell : public juce::Component
{
public:
    ActionCell(DeveloperPanel& owner)
        : m_owner(owner)
    {
        m_generate.onClick = [this] { m_owner.generateReport(m_row); };
        m_view.onClick = [this] { m_owner.viewReport(m_row); };
        addAndMakeVisible(m_generate);
        addAndMakeVisible(m_view);
    }

    void update(int row, bool ready, bool busy)
    {
        m_row = row;
        m_view.setEnabled(ready);
        m_generate.setEnabled(! busy);
    }

    void resized() override
    {
        juce::Rectangle<int> area = getLocalBounds().reduced(2);
        m_generate.setBounds(area.removeFromLeft(kButtonWidth));
        area.removeFromLeft(kMargin);
        m_view.setBounds(area.removeFromLeft(kButtonWidth));
    }

private:
    DeveloperPanel& m_owner;
    int m_row = 0;
    juce::TextButton m_generate{"Generate report"};
    juce::TextButton m_view{"View report"};
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
    for (int index = 0; index < kNumberOfSummaryColumns; ++index)
    {
        m_table.getHeader().addColumn(kSummaryColumns[index].title, kFirstSummaryColumn + index, kWidthSummary);
    }
    m_table.getHeader().addColumn("Format", kColumnFormat, kWidthFormat);
    m_table.getHeader().addColumn("Date", kColumnDate, kWidthDate);
    m_infoLabel.setText("The plugins loaded on the Plugins page. The columns show the single results of the last report (orange: worth a look; the tooltip "
                        "gives the number). A report tests a plugin technically (parameters, latency at three sample rates, block sizes, "
                        "delivery of parameters, robustness) in a process of its own; it can take a minute. Reports are kept in "
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
        const Row* known = findRow(row.key);
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
        if (findRow(window->first) == nullptr)
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
    cell->update(rowNumber, isReportReady(rowNumber), row.state == State::Running || row.state == State::Waiting);
    return cell;
}

void DeveloperPanel::generateReport(int row)
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
    {
        return;
    }
    Row& entry = m_rows[static_cast<size_t>(row)];
    if (entry.state == State::Running || entry.state == State::Waiting)
    {
        return;
    }
    entry.state = State::Waiting;
    m_queue.push_back(entry.key);
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
    const juce::String key = m_queue.front();
    m_queue.pop_front();
    Row* row = findRow(key);
    if (row == nullptr)
    {
        startNextJob(); // the plugin was unloaded in the meantime
        return;
    }
    row->state = State::Running;
    // a file with many plugins (a bundle): only this plugin is measured
    m_job = std::make_unique<FingerprintJob>(*this, row->pluginFile, row->key, row->reportFile, row->key);
    m_job->startThread();
    m_table.updateContent();
    m_table.repaint();
}

void DeveloperPanel::jobFinished(const juce::String& key, bool success)
{
    m_job.reset();
    Row* row = findRow(key);
    if (row != nullptr)
    {
        if (success)
        {
            updateRowFromDisk(*row);
            if (onReportReady)
            {
                onReportReady(static_cast<int>(row - m_rows.data()));
            }
        }
        else
        {
            row->state = State::Failed;
        }
    }
    m_table.updateContent();
    m_table.repaint();
    startNextJob();
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
