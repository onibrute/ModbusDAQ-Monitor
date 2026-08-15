#pragma once

#include "Measurement.h"

#include <cstddef>
#include <cstdint>
#include <vector>

class ISerialTransport;

struct ModbusReadResult
{
    std::vector<std::uint16_t> registers;
    CommunicationError error = CommunicationError::None;
    std::uint8_t exceptionCode = 0;

    bool IsValid() const noexcept
    {
        return error == CommunicationError::None;
    }
};

class ModbusRtuClient final
{
public:
    explicit ModbusRtuClient(ISerialTransport& transport) noexcept;

    ModbusReadResult ReadHoldingRegisters(
        std::uint8_t slaveId,
        std::uint16_t startAddress,
        std::uint16_t registerCount,
        unsigned int timeoutMs = 300);

    static std::uint16_t CalculateCrc(const std::uint8_t* data, std::size_t length) noexcept;

private:
    ISerialTransport& m_transport;
};
