#include "pch.h"
#include "CsvLogger.h"

#include "MeasurementHistory.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

CsvLogger::~CsvLogger()
{
    Stop();
}

bool CsvLogger::Start(
    const std::filesystem::path& filePath,
    std::string& errorMessage)
{
    Stop();
    errorMessage.clear();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errorMessage.clear();
    }

    m_output.open(filePath, std::ios::out | std::ios::trunc);
    if (!m_output.is_open())
    {
        errorMessage = "Could not create the CSV log file.";
        return false;
    }

    WriteHeader(m_output);
    if (!m_output.good())
    {
        errorMessage = "Could not write the CSV header.";
        m_output.close();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopRequested = false;
        std::queue<MeasurementBatch> empty;
        m_queue.swap(empty);
    }
    m_running.store(true);
    try
    {
        m_worker = std::thread(&CsvLogger::Run, this);
    }
    catch (...)
    {
        m_running.store(false);
        errorMessage = "Could not start the CSV logging worker.";
        m_output.close();
        return false;
    }
    return true;
}

void CsvLogger::Enqueue(const MeasurementBatch& batch)
{
    if (!m_running.load())
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running.load() || m_stopRequested)
        {
            return;
        }
        m_queue.push(batch);
    }
    m_condition.notify_one();
}

void CsvLogger::Stop() noexcept
{
    if (m_worker.joinable())
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopRequested = true;
        }
        m_condition.notify_one();
        m_worker.join();
    }

    m_running.store(false);
    if (m_output.is_open())
    {
        m_output.flush();
        m_output.close();
    }
}

bool CsvLogger::IsRunning() const noexcept
{
    return m_running.load();
}

std::string CsvLogger::TakeErrorMessage()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string message;
    message.swap(m_errorMessage);
    return message;
}

bool CsvLogger::ExportHistory(
    const std::filesystem::path& filePath,
    const MeasurementHistory& history,
    std::string& errorMessage)
{
    errorMessage.clear();
    std::ofstream output(filePath, std::ios::out | std::ios::trunc);
    if (!output.is_open())
    {
        errorMessage = "Could not create the CSV export file.";
        return false;
    }

    WriteHeader(output);
    for (const auto& batch : history.Samples())
    {
        WriteBatch(output, batch);
    }

    if (!output.good())
    {
        errorMessage = "An error occurred while writing the CSV export.";
        return false;
    }
    return true;
}

const char* CsvLogger::ErrorName(CommunicationError error) noexcept
{
    switch (error)
    {
    case CommunicationError::None: return "OK";
    case CommunicationError::Timeout: return "TIMEOUT";
    case CommunicationError::CrcMismatch: return "CRC_MISMATCH";
    case CommunicationError::InvalidSlave: return "INVALID_SLAVE";
    case CommunicationError::InvalidFunction: return "INVALID_FUNCTION";
    case CommunicationError::InvalidByteCount: return "INVALID_BYTE_COUNT";
    case CommunicationError::ModbusException: return "MODBUS_EXCEPTION";
    case CommunicationError::PortClosed: return "PORT_CLOSED";
    case CommunicationError::TransportError: return "TRANSPORT_ERROR";
    default: return "UNKNOWN";
    }
}

void CsvLogger::WriteHeader(std::ostream& output)
{
    output << "timestamp,status,exception_code,channel_1_mA,channel_2_mA,"
        "channel_3_mA,channel_4_mA\n";
}

void CsvLogger::WriteBatch(std::ostream& output, const MeasurementBatch& batch)
{
    const auto timeValue = std::chrono::system_clock::to_time_t(batch.timestamp);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        batch.timestamp.time_since_epoch()) % 1000;
    std::tm localTime{};
    localtime_s(&localTime, &timeValue);

    output << std::put_time(&localTime, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << milliseconds.count()
        << ',' << ErrorName(batch.error) << ',';

    if (batch.error == CommunicationError::ModbusException)
    {
        output << static_cast<unsigned int>(batch.exceptionCode);
    }

    output << std::setfill(' ');
    for (const auto& channel : batch.channels)
    {
        output << ',';
        if (channel.valid)
        {
            output << std::fixed << std::setprecision(3)
                << channel.calibratedMilliamp;
        }
    }
    output << '\n';
}

void CsvLogger::RecordError(const char* message) noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errorMessage = message;
        m_stopRequested = true;
        std::queue<MeasurementBatch> empty;
        m_queue.swap(empty);
    }
    catch (...)
    {
        // The worker must remain noexcept even if allocating the diagnostic
        // message fails. IsRunning() still turns false below.
    }
}

void CsvLogger::Run() noexcept
{
    for (;;)
    {
        MeasurementBatch batch;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this]
            {
                return m_stopRequested || !m_queue.empty();
            });

            if (m_queue.empty() && m_stopRequested)
            {
                break;
            }

            batch = std::move(m_queue.front());
            m_queue.pop();
        }

        WriteBatch(m_output, batch);
        if (!m_output.good())
        {
            RecordError("CSV logging stopped because a write failed.");
            break;
        }
    }

    m_output.flush();
    if (!m_output.good())
    {
        RecordError("CSV logging stopped because the final flush failed.");
    }
    m_running.store(false);
}
