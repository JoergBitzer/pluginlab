#include "ReferenceControls.h"

#include <algorithm>

#include "PluginSettings.h"

namespace pluginlab::refplugins
{
namespace
{
constexpr int kColumns = 5;
constexpr int kLabelHeight = 18;
constexpr int kCellHeight = 110;
constexpr int kComboHeight = 26;
constexpr int kMargin = 6;
constexpr float kDisabledAlpha = 0.35f;
constexpr float kGroupBrightness = 0.12f;
constexpr float kGroupCorner = 6.0f;
// the signal flow, in design units (multiplied by the scale)
constexpr float kFlowTitle = 18.0f;        // the title line of a box
constexpr float kFlowPadding = 4.0f;       // inside a box
constexpr float kFlowArrowGap = 30.0f;     // between two boxes of a row
constexpr float kFlowEnd = 44.0f;          // room for "in" and "out"
constexpr float kFlowRowGap = 36.0f;       // between the rows (the arrow to the next row)
constexpr float kFlowBoxHeight = 150.0f;
constexpr float kFlowArrowThickness = 1.5f;
}

std::unique_ptr<juce::AudioParameterChoice> makeChoiceParameter(const ChoiceSpec& spec)
{
    jade::parameterHelpTexts()[juce::String(spec.ID)] = juce::String(spec.help);
    return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(juce::String(spec.ID), 1), juce::String(spec.name), spec.choices, spec.defaultIndex);
}

juce::String choiceHelpText(const ChoiceSpec& spec)
{
    return juce::String(spec.name) + " (" + spec.choices.joinIntoString(", ") + "; default " + spec.choices[spec.defaultIndex] + "): " + juce::String(spec.help);
}

int getChoiceIndex(const std::atomic<float>* rawValue)
{
    return juce::roundToInt(rawValue->load());
}

ReferenceControls::ReferenceControls(juce::AudioProcessorValueTreeState& state)
    : m_state(state)
{
}

void ReferenceControls::addChoice(const ChoiceSpec& spec)
{
    auto box = std::make_unique<juce::ComboBox>();
    box->addItemList(spec.choices, 1);
    box->setTooltip(choiceHelpText(spec));
    m_comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(m_state, juce::String(spec.ID), *box));
    m_comboBoxes[juce::String(spec.ID)] = box.get();
    addCell(juce::String(spec.ID), juce::String(spec.name), std::move(box));
}

juce::ComboBox* ReferenceControls::getComboBox(const juce::String& parameterId) const
{
    const auto found = m_comboBoxes.find(parameterId);
    if (found == m_comboBoxes.end())
    {
        return nullptr;
    }
    return found->second;
}

void ReferenceControls::setControlEnabled(const juce::String& parameterId, bool enabled)
{
    for (size_t index = 0; index < m_ids.size(); ++index)
    {
        if (m_ids[index] == parameterId && m_controls[index]->isEnabled() != enabled)
        {
            m_controls[index]->setEnabled(enabled);
            m_controls[index]->setAlpha(enabled ? 1.0f : kDisabledAlpha);
            m_labels[index]->setAlpha(enabled ? 1.0f : kDisabledAlpha);
        }
    }
}

void ReferenceControls::placeCell(const juce::String& parameterId, juce::Rectangle<int> bounds)
{
    m_places[parameterId] = bounds;
    for (size_t index = 0; index < m_ids.size(); ++index)
    {
        if (m_ids[index] == parameterId)
        {
            layoutCell(index, bounds);
        }
    }
}

void ReferenceControls::addCell(const juce::String& parameterId, const juce::String& name, std::unique_ptr<juce::Component> control)
{
    auto label = std::make_unique<juce::Label>(juce::String(), name);
    label->setJustificationType(juce::Justification::centred);
    label->setInterceptsMouseClicks(false, false);
    addAndMakeVisible(*label);
    addAndMakeVisible(*control);
    m_ids.push_back(parameterId);
    m_labels.push_back(std::move(label));
    m_controls.push_back(std::move(control));
}

void ReferenceControls::layoutCell(size_t index, juce::Rectangle<int> cell)
{
    m_labels[index]->setBounds(cell.removeFromTop(kLabelHeight));
    if (dynamic_cast<juce::ComboBox*>(m_controls[index].get()) != nullptr)
    {
        cell = cell.withSizeKeepingCentre(cell.getWidth(), std::min(kComboHeight, cell.getHeight()));
    }
    m_controls[index]->setBounds(cell);
}

void ReferenceControls::resized()
{
    if (! m_places.empty())
    {
        for (size_t index = 0; index < m_ids.size(); ++index)
        {
            const auto place = m_places.find(m_ids[index]);
            if (place != m_places.end())
            {
                layoutCell(index, place->second);
            }
        }
        return;
    }
    const int cellWidth = getWidth() / kColumns;
    for (size_t index = 0; index < m_controls.size(); ++index)
    {
        const int column = static_cast<int>(index) % kColumns;
        const int row = static_cast<int>(index) / kColumns;
        juce::Rectangle<int> cell(column * cellWidth, row * kCellHeight, cellWidth, kCellHeight);
        layoutCell(index, cell.reduced(kMargin));
    }
}

void layoutFlow(ReferenceControls& controls, std::vector<FlowGroup>& groups, juce::Rectangle<float> area, float scale)
{
    // the widest row decides the width of a cell: room for "in" (first row) or "out" (last row), the gaps and the cells
    int rows = 0;
    for (const FlowGroup& group : groups)
    {
        rows = std::max(rows, group.row + 1);
    }
    float cellWidth = area.getWidth();
    for (int row = 0; row < rows; ++row)
    {
        int cells = 0;
        int boxes = 0;
        for (const FlowGroup& group : groups)
        {
            if (group.row == row)
            {
                cells += group.parameterIds.size();
                ++boxes;
            }
        }
        if (cells > 0)
        {
            const float fixed = (kFlowEnd + kFlowArrowGap * static_cast<float>(boxes - 1) + 2.0f * kFlowPadding * static_cast<float>(boxes)) * scale;
            cellWidth = std::min(cellWidth, (area.getWidth() - fixed) / static_cast<float>(cells));
        }
    }
    for (int row = 0; row < rows; ++row)
    {
        float x = area.getX() + (row == 0 ? kFlowEnd * scale : 0.0f);
        const float y = area.getY() + static_cast<float>(row) * (kFlowBoxHeight + kFlowRowGap) * scale;
        for (FlowGroup& group : groups)
        {
            if (group.row != row)
            {
                continue;
            }
            const float width = cellWidth * static_cast<float>(group.parameterIds.size()) + 2.0f * kFlowPadding * scale;
            group.box = {x, y, width, kFlowBoxHeight * scale};
            juce::Rectangle<float> inner = group.box.reduced(kFlowPadding * scale).withTrimmedTop(kFlowTitle * scale);
            for (const juce::String& id : group.parameterIds)
            {
                controls.placeCell(id, inner.removeFromLeft(cellWidth).reduced(kFlowPadding * scale, 0.0f).toNearestInt());
            }
            x += width + kFlowArrowGap * scale;
        }
    }
}

void drawFlow(juce::Graphics& g, const std::vector<FlowGroup>& groups, float scale, juce::Colour background, juce::Colour text)
{
    if (groups.empty())
    {
        return;
    }
    const juce::Colour arrow = text.withAlpha(0.7f);
    const float thickness = kFlowArrowThickness * std::max(scale, 1.0f);
    g.setFont(juce::FontOptions(kFlowTitle * 0.8f * scale));
    // in
    const juce::Rectangle<float> first = groups.front().box;
    g.setColour(text);
    g.drawText("in", juce::Rectangle<float>(first.getX() - kFlowEnd * scale, first.getCentreY() - kFlowTitle * scale, kFlowEnd * scale * 0.6f, kFlowTitle * scale),
               juce::Justification::centredLeft);
    drawArrow(g, {first.getX() - kFlowEnd * scale, first.getCentreY()}, {first.getX(), first.getCentreY()}, thickness, arrow);
    for (size_t index = 0; index < groups.size(); ++index)
    {
        drawGroup(g, groups[index].box, groups[index].title, kFlowTitle * scale, background, text);
        if (index + 1 < groups.size())
        {
            const juce::Rectangle<float> from = groups[index].box;
            const juce::Rectangle<float> to = groups[index + 1].box;
            if (groups[index + 1].row == groups[index].row)
            {
                drawArrow(g, {from.getRight(), from.getCentreY()}, {to.getX(), to.getCentreY()}, thickness, arrow);
            }
            else
            {
                drawArrow(g, {from.getCentreX(), from.getBottom()}, {to.getCentreX(), to.getY()}, thickness, arrow); // on to the next row
            }
        }
    }
    // out
    const juce::Rectangle<float> last = groups.back().box;
    drawArrow(g, {last.getRight(), last.getCentreY()}, {last.getRight() + kFlowEnd * scale * 0.8f, last.getCentreY()}, thickness, arrow);
    g.setColour(text);
    g.drawText("out", juce::Rectangle<float>(last.getRight() + kFlowEnd * scale * 0.2f, last.getCentreY(), kFlowEnd * scale, kFlowTitle * scale),
               juce::Justification::centredLeft);
}

void drawGroup(juce::Graphics& g, juce::Rectangle<float> box, const juce::String& title, float titleHeight, juce::Colour background, juce::Colour text)
{
    g.setColour(background.brighter(kGroupBrightness));
    g.fillRoundedRectangle(box, kGroupCorner);
    g.setColour(background.brighter(2.0f * kGroupBrightness));
    g.drawRoundedRectangle(box, kGroupCorner, 1.0f);
    g.setColour(text);
    g.setFont(juce::FontOptions(titleHeight * 0.8f, juce::Font::bold));
    g.drawText(title, box.reduced(kGroupCorner, 0.0f).removeFromTop(titleHeight), juce::Justification::centredLeft, true);
}

void drawArrow(juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, float thickness, juce::Colour colour)
{
    juce::Path line;
    line.startNewSubPath(from);
    juce::Point<float> last = from;
    if (std::abs(from.y - to.y) > 0.5f && std::abs(from.x - to.x) > 0.5f)
    {
        // down (or up) first, then across: the bend of a flow that continues in the next row
        const float middle = 0.5f * (from.y + to.y);
        line.lineTo(from.x, middle);
        line.lineTo(to.x, middle);
        last = {to.x, middle};
    }
    const float head = 3.0f * thickness + 4.0f;
    const juce::Point<float> direction = (to - last) / std::max(to.getDistanceFrom(last), 1.0e-3f);
    const juce::Point<float> base = to - direction * head;
    line.lineTo(base);
    g.setColour(colour);
    g.strokePath(line, juce::PathStrokeType(thickness));
    juce::Path tip;
    const juce::Point<float> normal(-direction.y, direction.x);
    tip.addTriangle(to, base + normal * (0.5f * head), base - normal * (0.5f * head));
    g.fillPath(tip);
}

juce::String getCaption(const juce::String& pluginName)
{
    return pluginName + " V " + juce::String(PLUGIN_VERSION_MAJOR) + "." + juce::String(PLUGIN_VERSION_MINOR) + "." + juce::String(PLUGIN_VERSION_PATCH)
           + "  -  pluginlab reference: the exact algorithm of pluginlab_reference, no smoothing, latency 0";
}
}
