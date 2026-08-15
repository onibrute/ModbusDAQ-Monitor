#include "pch.h"
#include "AcquisitionService.h"

#include <chrono>
#include <sstream>
#include <thread>

AcquisitionService::AcquisitionService()
    : m_modbus(m_serialPort)
{
}

AcquisitionService::~AcquisitionService()
{
    Disconnect();
}

bool AcquisitionService::Connect(
    const std::string& portName,
    std::uint32_t baudRate,
    std::uint8_t slaveId,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (slaveId == 0 || slaveId > 247)
    {
        errorMessage = "Slave ID must be between 1 and 247.";
        return false;
    }

    Disconnect();
    m_slaveId = slaveId;
    if (!m_serialPort.Open(portName, baudRate))
    {
        std::ostringstream message;
        message << "Could not open " << portName
            << " (Win32 error " << m_serialPort.LastErrorCode() << ").";
        errorMessage = message.str();
        return false;
    }
    return true;
}

void AcquisitionService::Disconnect() noexcept
{
    m_serialPort.Close();
}

bool AcquisitionService::IsConnected() const noexcept
{
    return m_serialPort.IsOpen();
}

bool AcquisitionService::SetSlaveId(std::uint8_t slaveId) noexcept
{
    if (slaveId == 0 || slaveId > 247)
    {
        return false;
    }
    m_slaveId = slaveId;
    return true;
}

std::uint8_t AcquisitionService::SlaveId() const noexcept
{
    return m_slaveId;
}

MeasurementBatch AcquisitionService::Poll(
    std::uint16_t startAddress,
    std::uint16_t channelCount)
{
    MeasurementBatch batch;
    batch.timestamp = std::chrono::system_clock::now();

    if (channelCount != kChannelCount)
    {
        batch.error = CommunicationError::InvalidByteCount;
        return batch;
    }

    const auto response = m_modbus.ReadHoldingRegisters(
        m_slaveId,
        startAddress,
        channelCount);
    batch.error = response.error;
    batch.exceptionCode = response.exceptionCode;
    if (!response.IsValid() || response.registers.size() != kChannelCount)
    {
        return batch;
    }

    for (std::size_t channel = 0; channel < kChannelCount; ++channel)
    {
        const double rawMilliamp =
            static_cast<double>(response.registers[channel]) /
            kRegisterUnitsPerMilliamp;
        batch.channels[channel].rawMilliamp = rawMilliamp;
        batch.channels[channel].calibratedMilliamp = rawMilliamp;
        batch.channels[channel].valid = true;
    }
    return batch;
}

AverageReadResult AcquisitionService::ReadAverageRaw(
    std::size_t channel,
    std::size_t sampleCount,
    unsigned int delayBetweenSamplesMs)
{
    AverageReadResult result;
    if (channel >= kChannelCount || sampleCount == 0)
    {
        result.error = CommunicationError::InvalidByteCount;
        return result;
    }

    double sum = 0.0;
    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        const auto batch = Poll();
        if (!batch.IsValid())
        {
            result.error = batch.error;
            result.exceptionCode = batch.exceptionCode;
            return result;
        }

        sum += batch.channels[channel].rawMilliamp;
        if (sample + 1 < sampleCount && delayBetweenSamplesMs > 0)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(delayBetweenSamplesMs));
        }
    }

    result.rawMilliamp = sum / static_cast<double>(sampleCount);
    return result;
}

unsigned long AcquisitionService::LastTransportError() const noexcept
{
    return m_serialPort.LastErrorCode();
}
