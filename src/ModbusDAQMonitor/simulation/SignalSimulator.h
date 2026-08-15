#pragma once

#include "../domain/Measurement.h"

#include <cstdint>

class SignalSimulator
{
public:
    MeasurementBatch Generate();

    void Reset() noexcept;
    std::uint64_t SampleIndex() const noexcept;

private:
    std::uint64_t m_sampleIndex = 0;
};
