#pragma once

#include "Measurement.h"

#include <array>
#include <cstddef>

enum class AlarmLevel
{
    Normal,
    Warning,
    Alarm
};

enum class AlarmDirection
{
    None,
    Low,
    High
};

struct AlarmConfiguration
{
    double minimumMilliamp = 4.0;
    double maximumMilliamp = 20.0;
    double hysteresisMilliamp = 0.2;
    double warningBandFraction = 0.1;
};

class AlarmEvaluator
{
public:
    AlarmLevel Update(
        std::size_t channel,
        double valueMilliamp,
        const AlarmConfiguration& configuration) noexcept;

    AlarmLevel CurrentLevel(std::size_t channel) const noexcept;
    void Reset(std::size_t channel) noexcept;
    void Reset() noexcept;

private:
    std::array<AlarmLevel, kChannelCount> m_levels{
        AlarmLevel::Normal,
        AlarmLevel::Normal,
        AlarmLevel::Normal,
        AlarmLevel::Normal
    };
    std::array<AlarmDirection, kChannelCount> m_directions{
        AlarmDirection::None,
        AlarmDirection::None,
        AlarmDirection::None,
        AlarmDirection::None
    };
};
