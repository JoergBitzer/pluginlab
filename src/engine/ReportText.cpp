#include "pluginlab/engine/ReportText.h"

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
}
