#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

inline constexpr std::size_t kChannelCount = 4;

enum class CommunicationError
{
    None,
    Timeout,
    CrcMismatch,
    InvalidSlave,
    InvalidFunction,
    InvalidByteCount,
    ModbusException,
    PortClosed,
    TransportError
};

struct ChannelMeasurement
{
    double rawMilliamp = 0.0;
    double calibratedMilliamp = 0.0;
    bool valid = false;
};

struct MeasurementBatch
{
    std::chrono::system_clock::time_point timestamp{};
    std::array<ChannelMeasurement, kChannelCount> channels{};
    CommunicationError error = CommunicationError::None;
    std::uint8_t exceptionCode = 0;

    bool IsValid() const noexcept
    {
        if (error != CommunicationError::None)
        {
            return false;
        }

        for (const auto& channel : channels)
        {
            if (!channel.valid)
            {
                return false;
            }
        }

        return true;
    }
};
