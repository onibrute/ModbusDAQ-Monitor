#pragma once

#include "ISerialTransport.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class FakeSerialTransport final : public ISerialTransport
{
public:
    bool Open(const std::string& portName, const std::uint32_t baudRate) override
    {
        m_portName = portName;
        m_baudRate = baudRate;
        m_open = m_openSucceeds;
        return m_open;
    }

    void Close() noexcept override
    {
        m_open = false;
    }

    bool Write(const std::vector<std::uint8_t>& data) override
    {
        ++m_writeCallCount;
        if (!m_open || !m_writeSucceeds)
        {
            return false;
        }

        m_lastWrite = data;
        return true;
    }

    bool ReadExact(
        std::vector<std::uint8_t>& buffer,
        const std::size_t size,
        const unsigned int timeoutMs) override
    {
        ++m_readCallCount;
        m_requestedReadSizes.push_back(size);
        m_readTimeouts.push_back(timeoutMs);

        if (!m_open ||
            m_failReads ||
            m_responseCursor + size > m_response.size())
        {
            if (m_closeOnReadFailure)
            {
                m_open = false;
            }
            return false;
        }

        buffer.assign(
            m_response.begin() + static_cast<std::ptrdiff_t>(m_responseCursor),
            m_response.begin() +
                static_cast<std::ptrdiff_t>(m_responseCursor + size));
        m_responseCursor += size;
        return true;
    }

    bool Purge() override
    {
        ++m_purgeCallCount;
        return m_open && m_purgeSucceeds;
    }

    bool IsOpen() const noexcept override
    {
        return m_open;
    }

    unsigned long LastErrorCode() const noexcept override
    {
        return m_lastErrorCode;
    }

    void SetResponse(std::vector<std::uint8_t> response)
    {
        m_response = std::move(response);
        m_responseCursor = 0;
    }

    void SetOpen(const bool open) noexcept
    {
        m_open = open;
    }

    void SetReadFailure(
        const bool failReads,
        const bool closePortOnFailure = false) noexcept
    {
        m_failReads = failReads;
        m_closeOnReadFailure = closePortOnFailure;
    }

    void SetLastErrorCode(const unsigned long errorCode) noexcept
    {
        m_lastErrorCode = errorCode;
    }

    const std::vector<std::uint8_t>& LastWrite() const noexcept
    {
        return m_lastWrite;
    }

    const std::vector<std::size_t>& RequestedReadSizes() const noexcept
    {
        return m_requestedReadSizes;
    }

    const std::vector<unsigned int>& ReadTimeouts() const noexcept
    {
        return m_readTimeouts;
    }

    std::size_t PurgeCallCount() const noexcept
    {
        return m_purgeCallCount;
    }

    std::size_t WriteCallCount() const noexcept
    {
        return m_writeCallCount;
    }

    std::size_t ReadCallCount() const noexcept
    {
        return m_readCallCount;
    }

private:
    bool m_open = true;
    bool m_openSucceeds = true;
    bool m_writeSucceeds = true;
    bool m_purgeSucceeds = true;
    bool m_failReads = false;
    bool m_closeOnReadFailure = false;
    unsigned long m_lastErrorCode = 0;
    std::string m_portName;
    std::uint32_t m_baudRate = 0;
    std::vector<std::uint8_t> m_response;
    std::size_t m_responseCursor = 0;
    std::vector<std::uint8_t> m_lastWrite;
    std::vector<std::size_t> m_requestedReadSizes;
    std::vector<unsigned int> m_readTimeouts;
    std::size_t m_purgeCallCount = 0;
    std::size_t m_writeCallCount = 0;
    std::size_t m_readCallCount = 0;
};
