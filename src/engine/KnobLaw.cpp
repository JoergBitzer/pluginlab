#include "pluginlab/engine/KnobLaw.h"

#include <algorithm>
#include <cmath>

namespace pluginlab::engine
{
namespace
{
constexpr double kSimplerWins = 1.1;          // a simpler law within 10 % of the best error wins
constexpr double kEqualError = 1.0e-4;        // errors below this share of the range are equal (three points fit a power law exactly)
constexpr double kLowestSkew = 0.05;
constexpr double kHighestSkew = 20.0;
constexpr int kSkewSteps = 400;
constexpr double kLinearSkewTolerance = 0.02; // a power law this close to skew 1 is linear
constexpr int kRefineIterations = 60;

double getRange(const std::vector<LawPoint>& points)
{
    double low = points.front().value;
    double high = points.front().value;
    for (const LawPoint& point : points)
    {
        low = std::min(low, point.value);
        high = std::max(high, point.value);
    }
    return high - low;
}

double getRmsError(const KnobLaw& law, const std::vector<LawPoint>& points, double range)
{
    double squares = 0.0;
    for (const LawPoint& point : points)
    {
        const double error = law.toValue(point.normalised) - point.value;
        squares += error * error;
    }
    return std::sqrt(squares / static_cast<double>(points.size())) / range;
}

// least squares x = a + b g(v) for a basis g
template <typename Basis>
bool fitAffine(const std::vector<LawPoint>& points, Basis basis, double& a, double& b)
{
    double sumG = 0.0, sumX = 0.0, sumGG = 0.0, sumGX = 0.0;
    for (const LawPoint& point : points)
    {
        const double g = basis(point.normalised);
        sumG += g;
        sumX += point.value;
        sumGG += g * g;
        sumGX += g * point.value;
    }
    const double n = static_cast<double>(points.size());
    const double determinant = n * sumGG - sumG * sumG;
    if (std::abs(determinant) < 1.0e-300)
    {
        return false;
    }
    b = (n * sumGX - sumG * sumX) / determinant;
    a = (sumX - b * sumG) / n;
    return true;
}
}

const char* getLawKindName(LawKind kind)
{
    switch (kind)
    {
        case LawKind::Linear: return "linear";
        case LawKind::Logarithmic: return "log";
        case LawKind::Power: return "power";
        case LawKind::Table: return "table";
    }
    return "";
}

double KnobLaw::toValue(double normalised) const
{
    const double v = std::clamp(normalised, 0.0, 1.0);
    switch (kind)
    {
        case LawKind::Linear:
            return minimum + (maximum - minimum) * v;
        case LawKind::Logarithmic:
            return minimum * std::pow(maximum / minimum, v);
        case LawKind::Power:
            return minimum + (maximum - minimum) * std::pow(v, 1.0 / skew);
        case LawKind::Table:
        {
            if (table.empty())
            {
                return 0.0;
            }
            if (table.size() == 1)
            {
                return table.front();
            }
            const auto step = static_cast<size_t>(std::lround(v * static_cast<double>(table.size() - 1)));
            return table[step];
        }
    }
    return 0.0;
}

double KnobLaw::toNormalised(double value) const
{
    double v = 0.0;
    switch (kind)
    {
        case LawKind::Linear:
            v = (value - minimum) / (maximum - minimum);
            break;
        case LawKind::Logarithmic:
            v = std::log(value / minimum) / std::log(maximum / minimum);
            break;
        case LawKind::Power:
        {
            const double share = std::clamp((value - minimum) / (maximum - minimum), 0.0, 1.0);
            v = std::pow(share, skew);
            break;
        }
        case LawKind::Table:
        {
            if (table.size() < 2)
            {
                return 0.0;
            }
            size_t best = 0;
            for (size_t step = 1; step < table.size(); ++step)
            {
                if (std::abs(table[step] - value) < std::abs(table[best] - value))
                {
                    best = step;
                }
            }
            return static_cast<double>(best) / static_cast<double>(table.size() - 1);
        }
    }
    return std::clamp(v, 0.0, 1.0);
}

juce::String KnobLaw::describe(const juce::String& unit) const
{
    auto number = [](double x) {
        const double magnitude = std::abs(x);
        const int decimals = magnitude >= 100.0 ? 0 : (magnitude >= 10.0 ? 1 : (magnitude >= 1.0 ? 2 : 3));
        return magnitude < 0.5 * std::pow(10.0, -decimals) ? juce::String("0") : juce::String(x, decimals);
    };
    const juce::String suffix = unit.isNotEmpty() ? " " + unit : juce::String();
    if (kind == LawKind::Table)
    {
        juce::StringArray values;
        for (const double x : table)
        {
            values.add(number(x));
        }
        return "table " + values.joinIntoString(", ") + suffix;
    }
    juce::String text = juce::String(getLawKindName(kind));
    if (kind == LawKind::Power)
    {
        text << " (skew " << juce::String(skew, 3) << ")";
    }
    return text + ", " + number(minimum) + " ... " + number(maximum) + suffix;
}

juce::var KnobLaw::toVar() const
{
    auto* object = new juce::DynamicObject();
    object->setProperty("kind", getLawKindName(kind));
    if (kind == LawKind::Table)
    {
        juce::Array<juce::var> values;
        for (const double x : table)
        {
            values.add(x);
        }
        object->setProperty("table", values);
    }
    else
    {
        object->setProperty("min", minimum);
        object->setProperty("max", maximum);
        if (kind == LawKind::Power)
        {
            object->setProperty("skew", skew);
        }
    }
    object->setProperty("rmsError", rmsError);
    object->setProperty("points", points);
    return juce::var(object);
}

std::optional<KnobLaw> KnobLaw::fromVar(const juce::var& value)
{
    if (! value.isObject())
    {
        return std::nullopt;
    }
    KnobLaw law;
    const juce::String kind = value["kind"].toString();
    if (kind == "linear")
    {
        law.kind = LawKind::Linear;
    }
    else if (kind == "log")
    {
        law.kind = LawKind::Logarithmic;
    }
    else if (kind == "power")
    {
        law.kind = LawKind::Power;
    }
    else if (kind == "table")
    {
        law.kind = LawKind::Table;
    }
    else
    {
        return std::nullopt;
    }
    if (law.kind == LawKind::Table)
    {
        if (const juce::Array<juce::var>* values = value["table"].getArray())
        {
            for (const juce::var& x : *values)
            {
                law.table.push_back(static_cast<double>(x));
            }
        }
    }
    else
    {
        law.minimum = static_cast<double>(value["min"]);
        law.maximum = static_cast<double>(value["max"]);
        if (law.kind == LawKind::Power)
        {
            law.skew = static_cast<double>(value.getProperty("skew", 1.0));
        }
    }
    law.rmsError = static_cast<double>(value.getProperty("rmsError", 0.0));
    law.points = static_cast<int>(value.getProperty("points", 0));
    return law;
}

std::optional<KnobLaw> fitKnobLaw(const std::vector<LawPoint>& points)
{
    if (points.size() < 3)
    {
        return std::nullopt;
    }
    const double range = getRange(points);
    if (range <= 0.0)
    {
        return std::nullopt;
    }
    std::vector<KnobLaw> candidates;
    double a = 0.0;
    double b = 0.0;
    if (fitAffine(points, [](double v) { return v; }, a, b))
    {
        KnobLaw linear;
        linear.kind = LawKind::Linear;
        linear.minimum = a;
        linear.maximum = a + b;
        candidates.push_back(linear);
    }
    const bool positive = std::all_of(points.begin(), points.end(), [](const LawPoint& point) { return point.value > 0.0; });
    if (positive)
    {
        std::vector<LawPoint> logarithms;
        for (const LawPoint& point : points)
        {
            logarithms.push_back({point.normalised, std::log(point.value)});
        }
        if (fitAffine(logarithms, [](double v) { return v; }, a, b))
        {
            KnobLaw logarithmic;
            logarithmic.kind = LawKind::Logarithmic;
            logarithmic.minimum = std::exp(a);
            logarithmic.maximum = std::exp(a + b);
            candidates.push_back(logarithmic);
        }
    }
    // the power law: the skew on a log grid, the end values by least squares for each skew
    KnobLaw bestPower;
    double bestPowerError = 1.0e300;
    for (int step = 0; step <= kSkewSteps; ++step)
    {
        const double skew = kLowestSkew * std::pow(kHighestSkew / kLowestSkew, static_cast<double>(step) / kSkewSteps);
        if (! fitAffine(points, [skew](double v) { return std::pow(v, 1.0 / skew); }, a, b))
        {
            continue;
        }
        KnobLaw power;
        power.kind = LawKind::Power;
        power.minimum = a;
        power.maximum = a + b;
        power.skew = skew;
        const double error = getRmsError(power, points, range);
        if (error < bestPowerError)
        {
            bestPowerError = error;
            bestPower = power;
        }
    }
    // refine the skew between the neighbouring grid points (golden section on the log of the skew)
    if (bestPowerError < 1.0e300)
    {
        const double ratio = std::pow(kHighestSkew / kLowestSkew, 1.0 / kSkewSteps);
        double low = std::log(bestPower.skew / ratio);
        double high = std::log(bestPower.skew * ratio);
        auto evaluate = [&](double logSkew, KnobLaw& law) {
            const double skew = std::exp(logSkew);
            double offset = 0.0;
            double scale = 0.0;
            if (! fitAffine(points, [skew](double v) { return std::pow(v, 1.0 / skew); }, offset, scale))
            {
                return 1.0e300;
            }
            law.kind = LawKind::Power;
            law.minimum = offset;
            law.maximum = offset + scale;
            law.skew = skew;
            return getRmsError(law, points, range);
        };
        constexpr double kGolden = 0.6180339887498949;
        for (int iteration = 0; iteration < kRefineIterations; ++iteration)
        {
            const double left = high - kGolden * (high - low);
            const double right = low + kGolden * (high - low);
            KnobLaw leftLaw;
            KnobLaw rightLaw;
            if (evaluate(left, leftLaw) < evaluate(right, rightLaw))
            {
                high = right;
            }
            else
            {
                low = left;
            }
        }
        KnobLaw refined;
        const double refinedError = evaluate(0.5 * (low + high), refined);
        if (refinedError < bestPowerError)
        {
            bestPowerError = refinedError;
            bestPower = refined;
        }
    }
    if (bestPowerError < 1.0e300 && std::abs(std::log(bestPower.skew)) > kLinearSkewTolerance)
    {
        candidates.push_back(bestPower);
    }
    if (candidates.empty())
    {
        return std::nullopt;
    }
    for (KnobLaw& law : candidates)
    {
        law.rmsError = getRmsError(law, points, range);
        law.points = static_cast<int>(points.size());
    }
    // the best error; then the simplest law (linear, log, power: in this order in the list) within kSimplerWins of it
    double best = 1.0e300;
    for (const KnobLaw& law : candidates)
    {
        best = std::min(best, law.rmsError);
    }
    for (const KnobLaw& law : candidates)
    {
        if (law.rmsError <= best * kSimplerWins + kEqualError)
        {
            return law;
        }
    }
    return candidates.front();
}

KnobLaw makeTableLaw(const std::vector<LawPoint>& points)
{
    KnobLaw law;
    law.kind = LawKind::Table;
    std::vector<LawPoint> sorted = points;
    std::sort(sorted.begin(), sorted.end(), [](const LawPoint& p, const LawPoint& q) { return p.normalised < q.normalised; });
    for (const LawPoint& point : sorted)
    {
        law.table.push_back(point.value);
    }
    if (! law.table.empty())
    {
        law.minimum = law.table.front();
        law.maximum = law.table.back();
    }
    law.points = static_cast<int>(points.size());
    return law;
}

std::optional<double> parseDisplayedNumber(const juce::String& text)
{
    const juce::String trimmed = text.trim();
    int start = -1;
    for (int index = 0; index < trimmed.length(); ++index)
    {
        const juce::juce_wchar c = trimmed[index];
        const bool sign = (c == '-' || c == '+') && index + 1 < trimmed.length() && (juce::CharacterFunctions::isDigit(trimmed[index + 1]) || trimmed[index + 1] == '.');
        if (juce::CharacterFunctions::isDigit(c) || sign || (c == '.' && index + 1 < trimmed.length() && juce::CharacterFunctions::isDigit(trimmed[index + 1])))
        {
            start = index;
            break;
        }
    }
    if (start < 0)
    {
        return std::nullopt;
    }
    int end = start + 1;
    while (end < trimmed.length() && (juce::CharacterFunctions::isDigit(trimmed[end]) || trimmed[end] == '.'))
    {
        ++end;
    }
    double value = trimmed.substring(start, end).getDoubleValue();
    const juce::String rest = trimmed.substring(end).trimStart();
    if (rest.startsWithChar('k') || rest.startsWithChar('K'))
    {
        value *= 1000.0; // "1.2 kHz", "1k"
    }
    return value;
}
}
