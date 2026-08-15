#include "pch.h"
#include "CalibrationModel.h"

#include <cmath>

namespace
{
constexpr double kMinimumRawSpanMilliamp = 0.1;
}

bool CalibrationModel::SetFirstPoint(
    const std::size_t channel,
    const double rawMilliamp,
    const double referenceMilliamp) noexcept
{
    if (!IsValidPoint(channel, rawMilliamp, referenceMilliamp))
    {
        return false;
    }

    auto& calibration = m_channels[channel];
    calibration.rawPoint1Milliamp = rawMilliamp;
    calibration.referencePoint1Milliamp = referenceMilliamp;
    calibration.hasFirstPoint = true;
    calibration.calibrated = false;
    return true;
}

bool CalibrationModel::SetSecondPoint(
    const std::size_t channel,
    const double rawMilliamp,
    const double referenceMilliamp) noexcept
{
    if (!IsValidPoint(channel, rawMilliamp, referenceMilliamp))
    {
        return false;
    }

    auto& calibration = m_channels[channel];
    calibration.rawPoint2Milliamp = rawMilliamp;
    calibration.referencePoint2Milliamp = referenceMilliamp;
    calibration.hasSecondPoint = true;
    calibration.calibrated = false;
    return true;
}

bool CalibrationModel::Calculate(const std::size_t channel) noexcept
{
    if (channel >= kChannelCount)
    {
        return false;
    }

    auto& calibration = m_channels[channel];
    calibration.slope = 1.0;
    calibration.offset = 0.0;
    calibration.calibrated = false;

    if (!calibration.hasFirstPoint || !calibration.hasSecondPoint)
    {
        return false;
    }

    const double rawSpan =
        calibration.rawPoint2Milliamp - calibration.rawPoint1Milliamp;

    if (std::abs(rawSpan) <= kMinimumRawSpanMilliamp)
    {
        return false;
    }

    const double referenceSpan =
        calibration.referencePoint2Milliamp -
        calibration.referencePoint1Milliamp;

    const double slope = referenceSpan / rawSpan;
    const double offset =
        calibration.referencePoint1Milliamp -
        slope * calibration.rawPoint1Milliamp;

    if (!std::isfinite(slope) || !std::isfinite(offset) || slope <= 0.0)
    {
        return false;
    }

    calibration.slope = slope;
    calibration.offset = offset;
    calibration.calibrated = true;
    return true;
}

double CalibrationModel::Apply(
    const std::size_t channel,
    const double rawMilliamp) const noexcept
{
    if (channel >= kChannelCount || !m_channels[channel].calibrated)
    {
        return rawMilliamp;
    }

    const auto& calibration = m_channels[channel];
    return calibration.slope * rawMilliamp + calibration.offset;
}

bool CalibrationModel::HasFirstPoint(const std::size_t channel) const noexcept
{
    return channel < kChannelCount && m_channels[channel].hasFirstPoint;
}

bool CalibrationModel::HasSecondPoint(const std::size_t channel) const noexcept
{
    return channel < kChannelCount && m_channels[channel].hasSecondPoint;
}

bool CalibrationModel::IsCalibrated(const std::size_t channel) const noexcept
{
    return channel < kChannelCount && m_channels[channel].calibrated;
}

double CalibrationModel::Slope(const std::size_t channel) const noexcept
{
    return channel < kChannelCount ? m_channels[channel].slope : 1.0;
}

double CalibrationModel::Offset(const std::size_t channel) const noexcept
{
    return channel < kChannelCount ? m_channels[channel].offset : 0.0;
}

void CalibrationModel::Reset(const std::size_t channel) noexcept
{
    if (channel < kChannelCount)
    {
        m_channels[channel] = ChannelCalibration{};
    }
}

void CalibrationModel::Reset() noexcept
{
    m_channels = {};
}

bool CalibrationModel::IsValidPoint(
    const std::size_t channel,
    const double rawMilliamp,
    const double referenceMilliamp) noexcept
{
    return channel < kChannelCount &&
        std::isfinite(rawMilliamp) &&
        std::isfinite(referenceMilliamp);
}
