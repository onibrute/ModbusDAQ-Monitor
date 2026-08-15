#include "pch.h"
#include "Win32SerialPort.h"

#include <algorithm>
#include <chrono>
#include <limits>

namespace
{
constexpr DWORD kSerialBufferSize = 4096;
constexpr DWORD kReadSliceMs = 20;
constexpr DWORD kWriteTimeoutMs = 250;
}

Win32SerialPort::~Win32SerialPort()
{
    Close();
}

bool Win32SerialPort::Open(const std::string& portName, std::uint32_t baudRate)
{
    Close();
    m_lastError = ERROR_SUCCESS;

    if (portName.empty() || baudRate == 0)
    {
        m_lastError = ERROR_INVALID_PARAMETER;
        return false;
    }

    const std::string devicePath = "\\\\.\\" + portName;
    m_handle = ::CreateFileA(
        devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (!IsOpen())
    {
        RememberLastError();
        return false;
    }

    if (!::SetupComm(m_handle, kSerialBufferSize, kSerialBufferSize))
    {
        RememberLastError();
        Close();
        return false;
    }

    DCB state{};
    state.DCBlength = sizeof(state);
    if (!::GetCommState(m_handle, &state))
    {
        RememberLastError();
        Close();
        return false;
    }

    state.BaudRate = baudRate;
    state.ByteSize = 8;
    state.Parity = NOPARITY;
    state.StopBits = ONESTOPBIT;
    state.fBinary = TRUE;
    state.fParity = FALSE;
    state.fOutxCtsFlow = FALSE;
    state.fOutxDsrFlow = FALSE;
    state.fDtrControl = DTR_CONTROL_DISABLE;
    state.fDsrSensitivity = FALSE;
    state.fTXContinueOnXoff = TRUE;
    state.fOutX = FALSE;
    state.fInX = FALSE;
    state.fErrorChar = FALSE;
    state.fNull = FALSE;
    state.fRtsControl = RTS_CONTROL_DISABLE;
    state.fAbortOnError = FALSE;

    if (!::SetCommState(m_handle, &state))
    {
        RememberLastError();
        Close();
        return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = kReadSliceMs;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = kReadSliceMs;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = kWriteTimeoutMs;
    if (!::SetCommTimeouts(m_handle, &timeouts))
    {
        RememberLastError();
        Close();
        return false;
    }

    if (!Purge())
    {
        Close();
        return false;
    }

    return true;
}

void Win32SerialPort::Close() noexcept
{
    if (IsOpen())
    {
        ::CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
}

bool Win32SerialPort::Write(const std::vector<std::uint8_t>& data)
{
    if (!IsOpen())
    {
        m_lastError = ERROR_INVALID_HANDLE;
        return false;
    }

    m_lastError = ERROR_SUCCESS;

    std::size_t totalWritten = 0;
    while (totalWritten < data.size())
    {
        const auto remaining = data.size() - totalWritten;
        const DWORD requested = static_cast<DWORD>(std::min<std::size_t>(
            remaining,
            (std::numeric_limits<DWORD>::max)()));
        DWORD written = 0;
        if (!::WriteFile(m_handle, data.data() + totalWritten, requested, &written, nullptr))
        {
            RememberLastError();
            return false;
        }
        if (written == 0)
        {
            m_lastError = ERROR_WRITE_FAULT;
            return false;
        }
        totalWritten += written;
    }

    return true;
}

bool Win32SerialPort::ReadExact(
    std::vector<std::uint8_t>& buffer,
    std::size_t size,
    unsigned int timeoutMs)
{
    buffer.clear();
    if (!IsOpen())
    {
        m_lastError = ERROR_INVALID_HANDLE;
        return false;
    }
    if (size == 0)
    {
        m_lastError = ERROR_SUCCESS;
        return true;
    }

    m_lastError = ERROR_SUCCESS;

    buffer.resize(size);
    std::size_t totalRead = 0;
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeoutMs);

    while (totalRead < size && std::chrono::steady_clock::now() < deadline)
    {
        const auto remaining = size - totalRead;
        const DWORD requested = static_cast<DWORD>(std::min<std::size_t>(
            remaining,
            (std::numeric_limits<DWORD>::max)()));
        DWORD bytesRead = 0;
        if (!::ReadFile(m_handle, buffer.data() + totalRead, requested, &bytesRead, nullptr))
        {
            RememberLastError();
            buffer.clear();
            return false;
        }
        totalRead += bytesRead;
    }

    if (totalRead != size)
    {
        m_lastError = WAIT_TIMEOUT;
        buffer.clear();
        return false;
    }

    return true;
}

bool Win32SerialPort::Purge()
{
    if (!IsOpen())
    {
        m_lastError = ERROR_INVALID_HANDLE;
        return false;
    }

    m_lastError = ERROR_SUCCESS;

    if (!::PurgeComm(m_handle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR))
    {
        RememberLastError();
        return false;
    }
    return true;
}

bool Win32SerialPort::IsOpen() const noexcept
{
    return m_handle != INVALID_HANDLE_VALUE;
}

unsigned long Win32SerialPort::LastErrorCode() const noexcept
{
    return m_lastError;
}

void Win32SerialPort::RememberLastError() noexcept
{
    m_lastError = ::GetLastError();
}
