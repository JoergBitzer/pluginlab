#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace pluginlab::ui
{
// A table that shows text, taken from functions of its owner.
class TextTableModel : public juce::TableListBoxModel
{
public:
    using CountFunction = std::function<int()>;
    using TextFunction = std::function<juce::String(int row, int columnId)>;
    using SelectionFunction = std::function<void()>;
    using SortFunction = std::function<void(int columnId, bool forwards)>;

    TextTableModel(CountFunction count, TextFunction text, SelectionFunction onSelectionChanged = {})
        : m_count(std::move(count)), m_text(std::move(text)), m_onSelectionChanged(std::move(onSelectionChanged))
    {
    }

    int getNumRows() override
    {
        return m_count();
    }

    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override
    {
        juce::ignoreUnused(rowNumber, width, height);
        if (rowIsSelected)
        {
            g.fillAll(juce::Colour(0xff4060a0));
        }
    }

    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override
    {
        g.setColour(juce::Colour(0xffd0d0d0));
        if (rowIsSelected)
        {
            g.setColour(juce::Colours::white);
        }
        g.drawText(m_text(rowNumber, columnId), kCellMargin, 0, width - 2 * kCellMargin, height, juce::Justification::centredLeft, true);
    }

    // called when the user clicks a column header
    void setSortFunction(SortFunction onSortChanged)
    {
        m_onSortChanged = std::move(onSortChanged);
    }

    void sortOrderChanged(int newSortColumnId, bool isForwards) override
    {
        if (m_onSortChanged)
        {
            m_onSortChanged(newSortColumnId, isForwards);
        }
    }

    void selectedRowsChanged(int lastRowSelected) override
    {
        juce::ignoreUnused(lastRowSelected);
        if (m_onSelectionChanged)
        {
            m_onSelectionChanged();
        }
    }

private:
    static constexpr int kCellMargin = 4;

    CountFunction m_count;
    TextFunction m_text;
    SelectionFunction m_onSelectionChanged;
    SortFunction m_onSortChanged;
};
}
