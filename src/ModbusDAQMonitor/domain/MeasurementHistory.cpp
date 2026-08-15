#include "pch.h"
#include "MeasurementHistory.h"

#include <utility>

MeasurementHistory::MeasurementHistory(const std::size_t capacity) noexcept
    : m_capacity(capacity)
{
}

void MeasurementHistory::SetCapacity(const std::size_t capacity) noexcept
{
    m_capacity = capacity;
    TrimToCapacity();
}

std::size_t MeasurementHistory::Capacity() const noexcept
{
    return m_capacity;
}

std::size_t MeasurementHistory::Size() const noexcept
{
    return m_samples.size();
}

bool MeasurementHistory::Empty() const noexcept
{
    return m_samples.empty();
}

void MeasurementHistory::Push(MeasurementBatch sample)
{
    if (m_capacity == 0)
    {
        return;
    }

    while (m_samples.size() >= m_capacity)
    {
        m_samples.pop_front();
    }

    m_samples.push_back(std::move(sample));
}

void MeasurementHistory::Clear() noexcept
{
    m_samples.clear();
}

const std::deque<MeasurementBatch>& MeasurementHistory::Samples() const noexcept
{
    return m_samples;
}

const MeasurementBatch* MeasurementHistory::Latest() const noexcept
{
    return m_samples.empty() ? nullptr : &m_samples.back();
}

void MeasurementHistory::TrimToCapacity() noexcept
{
    while (m_samples.size() > m_capacity)
    {
        m_samples.pop_front();
    }
}
