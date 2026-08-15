#include "pch.h"
#include "SignalSimulator.h"

#include <array>
#include <chrono>
#include <cmath>

MeasurementBatch SignalSimulator::Generate()
{
    MeasurementBatch batch;
    batch.timestamp = std::chrono::system_clock::now();
    batch.error = CommunicationError::None;
    batch.exceptionCode = 0;

    const double sample = static_cast<double>(m_sampleIndex);
    const std::array<double, kChannelCount> valuesMilliamp{
        12.0 + 8.0 * std::sin(sample * 0.10),
        12.0 + 4.8 * std::cos(sample * 0.08),
        12.0 + 6.4 * std::sin(sample * 0.13 + 1.0),
        12.0 + 3.2 * std::cos(sample * 0.05 + 0.5)
    };

    for (std::size_t channel = 0; channel < kChannelCount; ++channel)
    {
        batch.channels[channel].rawMilliamp = valuesMilliamp[channel];
        batch.channels[channel].calibratedMilliamp = valuesMilliamp[channel];
        batch.channels[channel].valid = true;
    }

    ++m_sampleIndex;
    return batch;
}

void SignalSimulator::Reset() noexcept
{
    m_sampleIndex = 0;
}

std::uint64_t SignalSimulator::SampleIndex() const noexcept
{
    return m_sampleIndex;
}
