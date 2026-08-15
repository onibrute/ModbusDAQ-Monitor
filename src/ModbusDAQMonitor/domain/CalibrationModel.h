#pragma once

#include "Measurement.h"

#include <array>
#include <cstddef>

class CalibrationModel
{
public:
    bool SetFirstPoint(
        std::size_t channel,
        double rawMilliamp,
        double referenceMilliamp) noexcept;

    bool SetSecondPoint(
        std::size_t channel,
        double rawMilliamp,
        double referenceMilliamp) noexcept;

    bool Calculate(std::size_t channel) noexcept;

    double Apply(
        std::size_t channel,
        double rawMilliamp) const noexcept;

    bool HasFirstPoint(std::size_t channel) const noexcept;
    bool HasSecondPoint(std::size_t channel) const noexcept;
    bool IsCalibrated(std::size_t channel) const noexcept;
    double Slope(std::size_t channel) const noexcept;
    double Offset(std::size_t channel) const noexcept;

    void Reset(std::size_t channel) noexcept;
    void Reset() noexcept;

private:
    struct ChannelCalibration
    {
        double rawPoint1Milliamp = 0.0;
        double referencePoint1Milliamp = 0.0;
        double rawPoint2Milliamp = 0.0;
        double referencePoint2Milliamp = 0.0;
        double slope = 1.0;
        double offset = 0.0;
        bool hasFirstPoint = false;
        bool hasSecondPoint = false;
        bool calibrated = false;
    };

    static bool IsValidPoint(
        std::size_t channel,
        double rawMilliamp,
        double referenceMilliamp) noexcept;

    std::array<ChannelCalibration, kChannelCount> m_channels{};
};
