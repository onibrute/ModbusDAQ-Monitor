#include "pch.h"
#include "AlarmEvaluator.h"

#include <algorithm>
#include <cmath>

AlarmLevel AlarmEvaluator::Update(
    const std::size_t channel,
    const double valueMilliamp,
    const AlarmConfiguration& configuration) noexcept
{
    if (channel >= kChannelCount ||
        !std::isfinite(valueMilliamp) ||
        !std::isfinite(configuration.minimumMilliamp) ||
        !std::isfinite(configuration.maximumMilliamp) ||
        !std::isfinite(configuration.hysteresisMilliamp) ||
        !std::isfinite(configuration.warningBandFraction) ||
        configuration.minimumMilliamp >= configuration.maximumMilliamp ||
        configuration.hysteresisMilliamp < 0.0)
    {
        return CurrentLevel(channel);
    }

    const double range =
        configuration.maximumMilliamp - configuration.minimumMilliamp;
    const double hysteresis = std::min(
        configuration.hysteresisMilliamp,
        range * 0.49);
    const double warningFraction = std::clamp(
        configuration.warningBandFraction,
        0.0,
        0.49);
    const double warningBand = range * warningFraction;
    const double warningLow =
        configuration.minimumMilliamp + warningBand;
    const double warningHigh =
        configuration.maximumMilliamp - warningBand;

    const AlarmLevel previousLevel = m_levels[channel];
    const AlarmDirection previousDirection = m_directions[channel];

    if (previousLevel == AlarmLevel::Alarm &&
        ((previousDirection == AlarmDirection::Low &&
          valueMilliamp <= configuration.minimumMilliamp + hysteresis) ||
         (previousDirection == AlarmDirection::High &&
          valueMilliamp >= configuration.maximumMilliamp - hysteresis)))
    {
        return AlarmLevel::Alarm;
    }

    if (valueMilliamp <= configuration.minimumMilliamp)
    {
        m_levels[channel] = AlarmLevel::Alarm;
        m_directions[channel] = AlarmDirection::Low;
        return m_levels[channel];
    }
    if (valueMilliamp >= configuration.maximumMilliamp)
    {
        m_levels[channel] = AlarmLevel::Alarm;
        m_directions[channel] = AlarmDirection::High;
        return m_levels[channel];
    }

    if (previousLevel == AlarmLevel::Warning &&
        ((previousDirection == AlarmDirection::Low &&
          valueMilliamp <= warningLow + hysteresis) ||
         (previousDirection == AlarmDirection::High &&
          valueMilliamp >= warningHigh - hysteresis)))
    {
        return AlarmLevel::Warning;
    }

    if (valueMilliamp <= warningLow)
    {
        m_levels[channel] = AlarmLevel::Warning;
        m_directions[channel] = AlarmDirection::Low;
        return m_levels[channel];
    }
    if (valueMilliamp >= warningHigh)
    {
        m_levels[channel] = AlarmLevel::Warning;
        m_directions[channel] = AlarmDirection::High;
        return m_levels[channel];
    }

    m_levels[channel] = AlarmLevel::Normal;
    m_directions[channel] = AlarmDirection::None;
    return m_levels[channel];
}

AlarmLevel AlarmEvaluator::CurrentLevel(
    const std::size_t channel) const noexcept
{
    return channel < kChannelCount
        ? m_levels[channel]
        : AlarmLevel::Normal;
}

void AlarmEvaluator::Reset(const std::size_t channel) noexcept
{
    if (channel < kChannelCount)
    {
        m_levels[channel] = AlarmLevel::Normal;
        m_directions[channel] = AlarmDirection::None;
    }
}

void AlarmEvaluator::Reset() noexcept
{
    m_levels.fill(AlarmLevel::Normal);
    m_directions.fill(AlarmDirection::None);
}
