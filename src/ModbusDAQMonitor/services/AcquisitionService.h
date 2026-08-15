#pragma once

#include "Measurement.h"
#include "ModbusRtuClient.h"
#include "Win32SerialPort.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

struct AverageReadResult
{
    std::optional<double> rawMilliamp;
    CommunicationError error = CommunicationError::None;
    std::uint8_t exceptionCode = 0;

    bool IsValid() const noexcept
    {
        return rawMilliamp.has_value() && error == CommunicationError::None;
    }
};

class AcquisitionService final
{
public:
    AcquisitionService();
    ~AcquisitionService();

    AcquisitionService(const AcquisitionService&) = delete;
    AcquisitionService& operator=(const AcquisitionService&) = delete;

    bool Connect(
        const std::string& portName,
        std::uint32_t baudRate,
        std::uint8_t slaveId,
        std::string& errorMessage);
    void Disconnect() noexcept;
    bool IsConnected() const noexcept;

    bool SetSlaveId(std::uint8_t slaveId) noexcept;
    std::uint8_t SlaveId() const noexcept;

    MeasurementBatch Poll(
        std::uint16_t startAddress = 0,
        std::uint16_t channelCount = static_cast<std::uint16_t>(kChannelCount));

    AverageReadResult ReadAverageRaw(
        std::size_t channel,
        std::size_t sampleCount = 10,
        unsigned int delayBetweenSamplesMs = 50);

    unsigned long LastTransportError() const noexcept;

private:
    static constexpr double kRegisterUnitsPerMilliamp = 1000.0;

    Win32SerialPort m_serialPort;
    ModbusRtuClient m_modbus;
    std::uint8_t m_slaveId = 1;
};
