#pragma once

#include "Measurement.h"

#include <cstddef>
#include <deque>

class MeasurementHistory
{
public:
    explicit MeasurementHistory(std::size_t capacity = 1000) noexcept;

    void SetCapacity(std::size_t capacity) noexcept;
    std::size_t Capacity() const noexcept;
    std::size_t Size() const noexcept;
    bool Empty() const noexcept;

    void Push(MeasurementBatch sample);
    void Clear() noexcept;

    const std::deque<MeasurementBatch>& Samples() const noexcept;
    const MeasurementBatch* Latest() const noexcept;

private:
    void TrimToCapacity() noexcept;

    std::size_t m_capacity;
    std::deque<MeasurementBatch> m_samples;
};
