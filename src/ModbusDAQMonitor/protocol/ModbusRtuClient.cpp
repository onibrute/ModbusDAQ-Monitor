#include "pch.h"
#include "ModbusRtuClient.h"

#include "ISerialTransport.h"

#include <cstddef>

namespace
{
constexpr std::uint8_t kReadHoldingRegisters = 0x03;
constexpr std::uint16_t kMaximumRegisterCount = 125;
constexpr unsigned long kWin32WaitTimeout = 258UL;

CommunicationError TransportFailure(const ISerialTransport& transport) noexcept
{
    return transport.IsOpen()
        ? CommunicationError::TransportError
        : CommunicationError::PortClosed;
}

CommunicationError ReadFailure(const ISerialTransport& transport) noexcept
{
    if (!transport.IsOpen())
    {
        return CommunicationError::PortClosed;
    }

    const auto errorCode = transport.LastErrorCode();
    return errorCode == 0UL || errorCode == kWin32WaitTimeout
        ? CommunicationError::Timeout
        : CommunicationError::TransportError;
}

bool HasValidCrc(const std::vector<std::uint8_t>& frame) noexcept
{
    if (frame.size() < 4)
    {
        return false;
    }

    const auto calculated = ModbusRtuClient::CalculateCrc(frame.data(), frame.size() - 2);
    const auto received = static_cast<std::uint16_t>(frame[frame.size() - 2]) |
        (static_cast<std::uint16_t>(frame.back()) << 8);
    return calculated == received;
}
}

ModbusRtuClient::ModbusRtuClient(ISerialTransport& transport) noexcept
    : m_transport(transport)
{
}

ModbusReadResult ModbusRtuClient::ReadHoldingRegisters(
    std::uint8_t slaveId,
    std::uint16_t startAddress,
    std::uint16_t registerCount,
    unsigned int timeoutMs)
{
    ModbusReadResult result;
    if (!m_transport.IsOpen())
    {
        result.error = CommunicationError::PortClosed;
        return result;
    }
    if (slaveId == 0 || slaveId > 247 || registerCount == 0 ||
        registerCount > kMaximumRegisterCount)
    {
        result.error = CommunicationError::TransportError;
        return result;
    }

    std::vector<std::uint8_t> request(8);
    request[0] = slaveId;
    request[1] = kReadHoldingRegisters;
    request[2] = static_cast<std::uint8_t>(startAddress >> 8);
    request[3] = static_cast<std::uint8_t>(startAddress & 0xFF);
    request[4] = static_cast<std::uint8_t>(registerCount >> 8);
    request[5] = static_cast<std::uint8_t>(registerCount & 0xFF);
    const auto requestCrc = CalculateCrc(request.data(), 6);
    request[6] = static_cast<std::uint8_t>(requestCrc & 0xFF);
    request[7] = static_cast<std::uint8_t>(requestCrc >> 8);

    if (!m_transport.Purge() || !m_transport.Write(request))
    {
        result.error = TransportFailure(m_transport);
        return result;
    }

    std::vector<std::uint8_t> header;
    if (!m_transport.ReadExact(header, 3, timeoutMs))
    {
        result.error = ReadFailure(m_transport);
        return result;
    }

    const bool isException = (header[1] & 0x80U) != 0;
    if (!isException && header[2] > 250U)
    {
        result.error = CommunicationError::InvalidByteCount;
        return result;
    }
    std::vector<std::uint8_t> tail;
    const std::size_t tailSize = isException
        ? 2U
        : static_cast<std::size_t>(header[2]) + 2U;

    if (!m_transport.ReadExact(tail, tailSize, timeoutMs))
    {
        result.error = ReadFailure(m_transport);
        return result;
    }

    std::vector<std::uint8_t> response = header;
    response.insert(response.end(), tail.begin(), tail.end());

    if (!HasValidCrc(response))
    {
        result.error = CommunicationError::CrcMismatch;
        return result;
    }
    if (header[0] != slaveId)
    {
        result.error = CommunicationError::InvalidSlave;
        return result;
    }
    if (isException)
    {
        if ((header[1] & 0x7FU) != kReadHoldingRegisters)
        {
            result.error = CommunicationError::InvalidFunction;
            return result;
        }
        result.error = CommunicationError::ModbusException;
        result.exceptionCode = header[2];
        return result;
    }
    if (header[1] != kReadHoldingRegisters)
    {
        result.error = CommunicationError::InvalidFunction;
        return result;
    }

    const auto expectedByteCount = static_cast<std::uint8_t>(registerCount * 2U);
    if (header[2] != expectedByteCount)
    {
        result.error = CommunicationError::InvalidByteCount;
        return result;
    }

    result.registers.resize(registerCount);
    for (std::size_t index = 0; index < registerCount; ++index)
    {
        const auto dataOffset = 3U + index * 2U;
        result.registers[index] =
            (static_cast<std::uint16_t>(response[dataOffset]) << 8) |
            static_cast<std::uint16_t>(response[dataOffset + 1]);
    }
    return result;
}

std::uint16_t ModbusRtuClient::CalculateCrc(
    const std::uint8_t* data,
    std::size_t length) noexcept
{
    std::uint16_t crc = 0xFFFF;
    for (std::size_t index = 0; index < length; ++index)
    {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc & 1U) != 0U
                ? static_cast<std::uint16_t>((crc >> 1) ^ 0xA001U)
                : static_cast<std::uint16_t>(crc >> 1);
        }
    }
    return crc;
}
