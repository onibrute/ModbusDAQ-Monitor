#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class ISerialTransport
{
public:
    virtual ~ISerialTransport() = default;

    virtual bool Open(const std::string& portName, std::uint32_t baudRate) = 0;
    virtual void Close() noexcept = 0;
    virtual bool Write(const std::vector<std::uint8_t>& data) = 0;
    virtual bool ReadExact(
        std::vector<std::uint8_t>& buffer,
        std::size_t size,
        unsigned int timeoutMs) = 0;
    virtual bool Purge() = 0;
    virtual bool IsOpen() const noexcept = 0;
    virtual unsigned long LastErrorCode() const noexcept = 0;
};
