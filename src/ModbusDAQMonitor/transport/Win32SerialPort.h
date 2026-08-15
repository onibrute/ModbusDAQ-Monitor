#pragma once

#include "ISerialTransport.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

class Win32SerialPort final : public ISerialTransport
{
public:
    Win32SerialPort() noexcept = default;
    ~Win32SerialPort() override;

    Win32SerialPort(const Win32SerialPort&) = delete;
    Win32SerialPort& operator=(const Win32SerialPort&) = delete;

    bool Open(const std::string& portName, std::uint32_t baudRate) override;
    void Close() noexcept override;
    bool Write(const std::vector<std::uint8_t>& data) override;
    bool ReadExact(
        std::vector<std::uint8_t>& buffer,
        std::size_t size,
        unsigned int timeoutMs) override;
    bool Purge() override;
    bool IsOpen() const noexcept override;
    unsigned long LastErrorCode() const noexcept override;

private:
    void RememberLastError() noexcept;

    HANDLE m_handle = INVALID_HANDLE_VALUE;
    DWORD m_lastError = ERROR_SUCCESS;
};
