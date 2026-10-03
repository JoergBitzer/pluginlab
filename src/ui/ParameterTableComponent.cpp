#include "pluginlab/ui/ParameterTableComponent.h"

#include "pluginlab/ui/TextTableModel.h"

namespace pluginlab::ui
{
namespace
{
constexpr int kRowHeight = 24;
constexpr int kHeaderHeight = 24;
constexpr int kRefreshHz = 5;
constexpr int kColumnIndex = 1;
constexpr int kColumnName = 2;
constexpr int kColumnValue = 3;
constexpr int kColumnSlider = 4;
constexpr int kWidthIndex = 40;
constexpr int kWidthName = 200;
constexpr int kWidthValue = 140;
constexpr int kWidthSlider = 300;
}

// A text table with a slider in the last column.
class ParameterTableComponent::SliderModel : public TextTableModel
{
public:
    using ValueFunction = std::function<float(int row)>;
    using ChangeFunction = std::function<void(int row, float normalisedValue)>;

    SliderModel(CountFunction count, TextFunction text, ValueFunction value, ChangeFunction change)
        : TextTableModel(std::move(count), std::move(text)), m_value(std::move(value)), m_change(std::move(change))
    {
    }

    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existing) override
    {
        juce::ignoreUnused(isRowSelected);
        if (columnId != kColumnSlider || rowNumber >= getNumRows())
        {
            delete existing;
            return nullptr;
        }

        auto* slider = dynamic_cast<juce::Slider*>(existing);
        if (slider == nullptr)
        {
            slider = new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            slider->setRange(0.0, 1.0);
        }
        slider->onValueChange = [this, slider, rowNumber]
        {
            m_change(rowNumber, static_cast<float>(slider->getValue()));
        };
        if (! slider->isMouseButtonDown())
        {
            slider->setValue(static_cast<double>(m_value(rowNumber)), juce::dontSendNotification);
        }
        return slider;
    }

private:
    ValueFunction m_value;
    ChangeFunction m_change;
};

ParameterTableComponent::ParameterTableComponent()
{
    m_model = std::make_unique<SliderModel>(
        [this] { return static_cast<int>(m_rows.size()); },
        [this](int row, int column)
        {
            const hosting::ParameterInfo& parameter = m_rows[static_cast<size_t>(row)];
            if (column == kColumnIndex)
            {
                return juce::String(parameter.index);
            }
            if (column == kColumnName)
            {
                return parameter.name;
            }
            if (column == kColumnValue)
            {
                return (parameter.valueText + " " + parameter.label).trim();
            }
            return juce::String();
        },
        [this](int row) { return m_rows[static_cast<size_t>(row)].normalisedValue; },
        [this](int row, float value)
        {
            if (m_plugin != nullptr && row < static_cast<int>(m_rows.size()))
            {
                m_plugin->setParameterNormalised(m_rows[static_cast<size_t>(row)].index, value);
            }
        });

    m_table.setModel(m_model.get());
    m_table.setRowHeight(kRowHeight);
    m_table.setHeaderHeight(kHeaderHeight);
    m_table.getHeader().addColumn("#", kColumnIndex, kWidthIndex);
    m_table.getHeader().addColumn("Parameter", kColumnName, kWidthName);
    m_table.getHeader().addColumn("Value", kColumnValue, kWidthValue);
    m_table.getHeader().addColumn("", kColumnSlider, kWidthSlider);
    addAndMakeVisible(m_table);
    startTimerHz(kRefreshHz);
}

ParameterTableComponent::~ParameterTableComponent()
{
    stopTimer();
    m_table.setModel(nullptr);
}

void ParameterTableComponent::setPlugin(hosting::HostedPlugin* plugin)
{
    m_plugin = plugin;
    refresh();
}

void ParameterTableComponent::resized()
{
    m_table.setBounds(getLocalBounds());
}

void ParameterTableComponent::timerCallback()
{
    refresh();
}

void ParameterTableComponent::refresh()
{
    m_rows.clear();
    if (m_plugin != nullptr)
    {
        m_rows = m_plugin->getParameters();
    }
    m_table.updateContent();
    m_table.repaint();
}
}
