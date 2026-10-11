#include "pluginlab/engine/ReportText.h"

#include "pluginlab/PluginLabVersion.h"

#include <algorithm>
#include <vector>

namespace pluginlab::engine
{
namespace
{
const juce::String kCellSeparator = "|";
const juce::String kColumnGap = "  ";

bool isTableLine(const juce::String& line)
{
    return line.trimStart().startsWith(kCellSeparator);
}

bool isSeparatorLine(const juce::String& line)
{
    return isTableLine(line) && line.containsOnly("|-: ");
}

std::vector<juce::String> splitCells(const juce::String& line)
{
    juce::String inner = line.trim();
    if (inner.startsWith(kCellSeparator))
    {
        inner = inner.substring(1);
    }
    if (inner.endsWith(kCellSeparator))
    {
        inner = inner.dropLastCharacters(1);
    }
    std::vector<juce::String> cells;
    for (const juce::String& cell : juce::StringArray::fromTokens(inner, kCellSeparator, ""))
    {
        cells.push_back(cell.trim());
    }
    return cells;
}

juce::String formatTable(const std::vector<juce::String>& lines)
{
    std::vector<std::vector<juce::String>> rows;
    std::vector<size_t> separatorRows;
    for (const juce::String& line : lines)
    {
        if (isSeparatorLine(line))
        {
            separatorRows.push_back(rows.size());
            rows.emplace_back();
            continue;
        }
        rows.push_back(splitCells(line));
    }
    std::vector<int> widths;
    for (const std::vector<juce::String>& row : rows)
    {
        for (size_t column = 0; column < row.size(); ++column)
        {
            if (widths.size() <= column)
            {
                widths.push_back(0);
            }
            widths[column] = std::max(widths[column], row[column].length());
        }
    }
    juce::String text;
    for (size_t index = 0; index < rows.size(); ++index)
    {
        const bool isSeparator = std::find(separatorRows.begin(), separatorRows.end(), index) != separatorRows.end();
        for (size_t column = 0; column < widths.size(); ++column)
        {
            if (isSeparator)
            {
                text += juce::String::repeatedString("-", widths[column]);
            }
            else if (column < rows[index].size())
            {
                text += rows[index][column].paddedRight(' ', widths[column]);
            }
            else
            {
                text += juce::String::repeatedString(" ", widths[column]);
            }
            if (column + 1 < widths.size())
            {
                text += kColumnGap;
            }
        }
        text = text.trimEnd() + "\n";
    }
    return text;
}
}

juce::String alignMarkdownTables(const juce::String& markdown)
{
    juce::String result;
    std::vector<juce::String> table;
    const auto flushTable = [&]
    {
        if (! table.empty())
        {
            result += formatTable(table);
            table.clear();
        }
    };
    for (const juce::String& line : juce::StringArray::fromLines(markdown))
    {
        if (isTableLine(line))
        {
            table.push_back(line);
            continue;
        }
        flushTable();
        result += line + "\n";
    }
    flushTable();
    return result;
}

juce::String wrapText(const juce::String& text, int width)
{
    juce::StringArray lines;
    juce::String line;
    for (const juce::String& word : juce::StringArray::fromTokens(text, " ", ""))
    {
        if (word.isEmpty())
        {
            continue;
        }
        if (line.isNotEmpty() && line.length() + 1 + word.length() > width)
        {
            lines.add(line);
            line.clear();
        }
        if (line.isNotEmpty())
        {
            line << " ";
        }
        line << word;
    }
    if (line.isNotEmpty())
    {
        lines.add(line);
    }
    return lines.joinIntoString("\n");
}

juce::String getReviewSignature(const juce::String& program)
{
    return "Jade Benchmark, pluginlab " + juce::String(pluginlab::getVersionString()) + " (" + program + "; " + juce::SystemStats::getJUCEVersion() + "; "
           + juce::SystemStats::getOperatingSystemName() + "), " + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M");
}

juce::String makeReportHead(const juce::String& title, const juce::String& program)
{
    return "![Jade Benchmark](" + juce::String(kLogoFileName) + ")\n\n# " + title + "\n\nReviewed by " + getReviewSignature(program) + "\n";
}

juce::String wrapMarkdownProse(const juce::String& markdown, int width)
{
    juce::StringArray result;
    bool inCode = false;
    for (const juce::String& line : juce::StringArray::fromLines(markdown))
    {
        const juce::String trimmed = line.trimStart();
        if (trimmed.startsWith("```"))
        {
            inCode = ! inCode;
            result.add(line);
            continue;
        }
        const bool keep = inCode || line.length() <= width || trimmed.startsWith("|") || trimmed.startsWith("#") || trimmed.startsWith("![")
                          || trimmed.isEmpty();
        if (keep)
        {
            result.add(line);
            continue;
        }
        // a list item or a quote keeps its marker; its continuation lines are indented to the text
        const int indent = line.length() - trimmed.length();
        juce::String marker;
        if (trimmed.startsWith("- ") || trimmed.startsWith("* ") || trimmed.startsWith("> "))
        {
            marker = trimmed.substring(0, 2);
        }
        const juce::String lead = juce::String::repeatedString(" ", indent);
        const juce::String text = trimmed.substring(marker.length());
        const juce::StringArray wrapped = juce::StringArray::fromLines(wrapText(text, width - indent - marker.length()));
        for (int index = 0; index < wrapped.size(); ++index)
        {
            juce::String prefix = lead + juce::String::repeatedString(" ", marker.length());
            if (index == 0)
            {
                prefix = lead + marker;
            }
            result.add(prefix + wrapped[index]);
        }
    }
    return result.joinIntoString("\n");
}

juce::String signReport(const juce::String& markdown, const juce::String& program)
{
    juce::StringArray lines = juce::StringArray::fromLines(markdown);
    for (int index = 0; index < lines.size(); ++index)
    {
        if (lines[index].startsWith("# "))
        {
            lines.insert(index + 1, "");
            lines.insert(index + 2, "Reviewed by " + getReviewSignature(program));
            lines.insert(index, "");
            lines.insert(index, "![Jade Benchmark](" + juce::String(kLogoFileName) + ")");
            break;
        }
    }
    return wrapMarkdownProse(lines.joinIntoString("\n"));
}
}
