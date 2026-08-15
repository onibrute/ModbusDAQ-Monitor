#pragma once

#include "Measurement.h"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class MeasurementHistory;

class CsvLogger final
{
public:
    CsvLogger() = default;
    ~CsvLogger();

    CsvLogger(const CsvLogger&) = delete;
    CsvLogger& operator=(const CsvLogger&) = delete;

    bool Start(const std::filesystem::path& filePath, std::string& errorMessage);
    void Enqueue(const MeasurementBatch& batch);
    void Stop() noexcept;
    bool IsRunning() const noexcept;
    std::string TakeErrorMessage();

    static bool ExportHistory(
        const std::filesystem::path& filePath,
        const MeasurementHistory& history,
        std::string& errorMessage);
    static const char* ErrorName(CommunicationError error) noexcept;

private:
    static void WriteHeader(std::ostream& output);
    static void WriteBatch(std::ostream& output, const MeasurementBatch& batch);
    void RecordError(const char* message) noexcept;
    void Run() noexcept;

    std::ofstream m_output;
    std::queue<MeasurementBatch> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::thread m_worker;
    std::atomic<bool> m_running{ false };
    bool m_stopRequested = false;
    std::string m_errorMessage;
};
