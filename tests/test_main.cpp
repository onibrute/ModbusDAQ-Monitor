#include "FakeSerialTransport.h"
#include "AlarmEvaluator.h"
#include "CalibrationModel.h"
#include "CsvLogger.h"
#include "MeasurementHistory.h"
#include "ModbusRtuClient.h"
#include "SignalSimulator.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
class AssertionFailure final : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

void Check(
    const bool condition,
    const char* expression,
    const char* file,
    const int line)
{
    if (!condition)
    {
        throw AssertionFailure(
            std::string(file) + ":" + std::to_string(line) +
            ": check failed: " + expression);
    }
}

void CheckNear(
    const double actual,
    const double expected,
    const double tolerance,
    const char* actualExpression,
    const char* expectedExpression,
    const char* file,
    const int line)
{
    if (!std::isfinite(actual) ||
        !std::isfinite(expected) ||
        std::abs(actual - expected) > tolerance)
    {
        throw AssertionFailure(
            std::string(file) + ":" + std::to_string(line) +
            ": expected " + actualExpression + " ~= " + expectedExpression +
            ", actual=" + std::to_string(actual) +
            ", expected=" + std::to_string(expected));
    }
}

#define CHECK(expression) \
    Check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

#define CHECK_NEAR(actual, expected, tolerance) \
    CheckNear( \
        static_cast<double>(actual), \
        static_cast<double>(expected), \
        static_cast<double>(tolerance), \
        #actual, \
        #expected, \
        __FILE__, \
        __LINE__)

MeasurementBatch MakeBatch(const double marker)
{
    MeasurementBatch batch;
    batch.error = CommunicationError::None;
    for (std::size_t channel = 0; channel < kChannelCount; ++channel)
    {
        batch.channels[channel].rawMilliamp =
            marker + static_cast<double>(channel);
        batch.channels[channel].calibratedMilliamp =
            marker + static_cast<double>(channel);
        batch.channels[channel].valid = true;
    }
    return batch;
}

std::vector<std::uint8_t> WithModbusCrc(
    std::initializer_list<std::uint8_t> payload)
{
    std::vector<std::uint8_t> frame(payload);
    const auto crc = ModbusRtuClient::CalculateCrc(frame.data(), frame.size());
    frame.push_back(static_cast<std::uint8_t>(crc & 0xFFU));
    frame.push_back(static_cast<std::uint8_t>(crc >> 8U));
    return frame;
}

void TestModbusCrcKnownVector()
{
    // Modbus request: slave 1, function 3, address 0, ten registers.
    // The transmitted CRC bytes for this standard vector are C5 CD.
    constexpr std::array<std::uint8_t, 6> request{
        0x01, 0x03, 0x00, 0x00, 0x00, 0x0A
    };

    const auto crc = ModbusRtuClient::CalculateCrc(
        request.data(),
        request.size());

    CHECK(crc == 0xCDC5U);
}

void TestModbusValidResponse()
{
    FakeSerialTransport transport;
    transport.SetResponse(WithModbusCrc({
        0x01, 0x03, 0x04, 0x12, 0x34, 0xAB, 0xCD
    }));
    ModbusRtuClient client(transport);

    const auto result = client.ReadHoldingRegisters(1, 0x0010, 2, 250);

    CHECK(result.IsValid());
    CHECK(result.error == CommunicationError::None);
    CHECK(result.exceptionCode == 0);
    CHECK(result.registers.size() == 2);
    CHECK(result.registers[0] == 0x1234U);
    CHECK(result.registers[1] == 0xABCDU);

    CHECK(transport.PurgeCallCount() == 1);
    CHECK(transport.WriteCallCount() == 1);
    CHECK(transport.ReadCallCount() == 2);
    CHECK(transport.RequestedReadSizes().size() == 2);
    CHECK(transport.RequestedReadSizes()[0] == 3);
    CHECK(transport.RequestedReadSizes()[1] == 6);
    CHECK(transport.ReadTimeouts()[0] == 250);
    CHECK(transport.ReadTimeouts()[1] == 250);

    const auto& request = transport.LastWrite();
    CHECK(request.size() == 8);
    CHECK(request[0] == 0x01);
    CHECK(request[1] == 0x03);
    CHECK(request[2] == 0x00);
    CHECK(request[3] == 0x10);
    CHECK(request[4] == 0x00);
    CHECK(request[5] == 0x02);
    const auto requestCrc = ModbusRtuClient::CalculateCrc(
        request.data(),
        request.size() - 2);
    CHECK(request[6] == static_cast<std::uint8_t>(requestCrc & 0xFFU));
    CHECK(request[7] == static_cast<std::uint8_t>(requestCrc >> 8U));
}

void TestModbusInvalidCrc()
{
    auto response = WithModbusCrc({
        0x01, 0x03, 0x04, 0x00, 0x01, 0x00, 0x02
    });
    response.back() ^= 0xFFU;

    FakeSerialTransport transport;
    transport.SetResponse(std::move(response));
    ModbusRtuClient client(transport);

    const auto result = client.ReadHoldingRegisters(1, 0, 2);
    CHECK(!result.IsValid());
    CHECK(result.error == CommunicationError::CrcMismatch);
    CHECK(result.registers.empty());
}

void TestModbusWrongSlave()
{
    FakeSerialTransport transport;
    transport.SetResponse(WithModbusCrc({
        0x02, 0x03, 0x04, 0x00, 0x01, 0x00, 0x02
    }));
    ModbusRtuClient client(transport);

    const auto result = client.ReadHoldingRegisters(1, 0, 2);
    CHECK(!result.IsValid());
    CHECK(result.error == CommunicationError::InvalidSlave);
}

void TestModbusWrongFunction()
{
    FakeSerialTransport transport;
    transport.SetResponse(WithModbusCrc({
        0x01, 0x04, 0x04, 0x00, 0x01, 0x00, 0x02
    }));
    ModbusRtuClient client(transport);

    const auto result = client.ReadHoldingRegisters(1, 0, 2);
    CHECK(!result.IsValid());
    CHECK(result.error == CommunicationError::InvalidFunction);
}

void TestModbusWrongByteCount()
{
    FakeSerialTransport transport;
    transport.SetResponse(WithModbusCrc({
        0x01, 0x03, 0x02, 0x00, 0x01
    }));
    ModbusRtuClient client(transport);

    const auto result = client.ReadHoldingRegisters(1, 0, 2);
    CHECK(!result.IsValid());
    CHECK(result.error == CommunicationError::InvalidByteCount);
}

void TestModbusExceptionResponse()
{
    FakeSerialTransport transport;
    transport.SetResponse(WithModbusCrc({0x01, 0x83, 0x02}));
    ModbusRtuClient client(transport);

    const auto result = client.ReadHoldingRegisters(1, 0, 2);
    CHECK(!result.IsValid());
    CHECK(result.error == CommunicationError::ModbusException);
    CHECK(result.exceptionCode == 0x02);
    CHECK(result.registers.empty());
}

void TestModbusTimeout()
{
    FakeSerialTransport transport;
    transport.SetReadFailure(true);
    ModbusRtuClient client(transport);

    const auto result = client.ReadHoldingRegisters(1, 0, 2, 75);
    CHECK(!result.IsValid());
    CHECK(result.error == CommunicationError::Timeout);
    CHECK(transport.IsOpen());
    CHECK(transport.ReadCallCount() == 1);
    CHECK(transport.ReadTimeouts()[0] == 75);
}

void TestModbusTransportReadError()
{
    FakeSerialTransport transport;
    transport.SetReadFailure(true);
    transport.SetLastErrorCode(1117UL); // ERROR_IO_DEVICE
    ModbusRtuClient client(transport);

    const auto result = client.ReadHoldingRegisters(1, 0, 2, 75);
    CHECK(!result.IsValid());
    CHECK(result.error == CommunicationError::TransportError);
    CHECK(transport.IsOpen());
}

void TestModbusPortClosed()
{
    FakeSerialTransport initiallyClosed;
    initiallyClosed.SetOpen(false);
    ModbusRtuClient closedClient(initiallyClosed);

    const auto initiallyClosedResult =
        closedClient.ReadHoldingRegisters(1, 0, 2);
    CHECK(!initiallyClosedResult.IsValid());
    CHECK(initiallyClosedResult.error == CommunicationError::PortClosed);
    CHECK(initiallyClosed.PurgeCallCount() == 0);
    CHECK(initiallyClosed.WriteCallCount() == 0);
    CHECK(initiallyClosed.ReadCallCount() == 0);

    FakeSerialTransport closesDuringRead;
    closesDuringRead.SetReadFailure(true, true);
    ModbusRtuClient interruptedClient(closesDuringRead);

    const auto interruptedResult =
        interruptedClient.ReadHoldingRegisters(1, 0, 2);
    CHECK(!interruptedResult.IsValid());
    CHECK(interruptedResult.error == CommunicationError::PortClosed);
    CHECK(!closesDuringRead.IsOpen());
}

void TestCalibrationModel()
{
    CalibrationModel calibration;

    CHECK(!calibration.IsCalibrated(0));
    CHECK(!calibration.Calculate(0));
    CHECK_NEAR(calibration.Apply(0, 9.5), 9.5, 1.0e-12);

    CHECK(!calibration.SetFirstPoint(kChannelCount, 4.0, 4.0));
    CHECK(!calibration.SetFirstPoint(
        0,
        std::numeric_limits<double>::quiet_NaN(),
        4.0));
    CHECK(!calibration.SetSecondPoint(
        0,
        20.0,
        std::numeric_limits<double>::infinity()));

    CHECK(calibration.SetFirstPoint(0, 2.0, 4.0));
    CHECK(calibration.SetSecondPoint(0, 18.0, 20.0));
    CHECK(calibration.Calculate(0));
    CHECK(calibration.HasFirstPoint(0));
    CHECK(calibration.HasSecondPoint(0));
    CHECK(calibration.IsCalibrated(0));
    CHECK_NEAR(calibration.Slope(0), 1.0, 1.0e-12);
    CHECK_NEAR(calibration.Offset(0), 2.0, 1.0e-12);
    CHECK_NEAR(calibration.Apply(0, 10.0), 12.0, 1.0e-12);

    CHECK(calibration.SetFirstPoint(1, 7.0, 4.0));
    CHECK(calibration.SetSecondPoint(1, 7.0, 20.0));
    CHECK(!calibration.Calculate(1));
    CHECK(!calibration.IsCalibrated(1));
    CHECK_NEAR(calibration.Apply(1, 11.0), 11.0, 1.0e-12);

    calibration.Reset(0);
    CHECK(!calibration.HasFirstPoint(0));
    CHECK(!calibration.HasSecondPoint(0));
    CHECK(!calibration.IsCalibrated(0));
    CHECK_NEAR(calibration.Apply(0, 10.0), 10.0, 1.0e-12);

    calibration.Reset();
    CHECK(!calibration.HasFirstPoint(1));
}

void TestAlarmEvaluatorHysteresis()
{
    AlarmEvaluator evaluator;
    AlarmConfiguration configuration;
    configuration.minimumMilliamp = 4.0;
    configuration.maximumMilliamp = 20.0;
    configuration.hysteresisMilliamp = 0.5;
    configuration.warningBandFraction = 0.1;

    CHECK(evaluator.Update(0, 12.0, configuration) == AlarmLevel::Normal);

    CHECK(evaluator.Update(0, 5.6, configuration) == AlarmLevel::Warning);
    CHECK(evaluator.Update(0, 6.0, configuration) == AlarmLevel::Warning);
    CHECK(evaluator.Update(0, 6.2, configuration) == AlarmLevel::Normal);

    CHECK(evaluator.Update(0, 3.9, configuration) == AlarmLevel::Alarm);
    CHECK(evaluator.Update(0, 4.5, configuration) == AlarmLevel::Alarm);
    CHECK(evaluator.Update(0, 4.6, configuration) == AlarmLevel::Warning);
    CHECK(evaluator.Update(0, 6.2, configuration) == AlarmLevel::Normal);

    // A jump from a low alarm directly to the high warning band must not be
    // retained as a high alarm merely because both sides share one level.
    CHECK(evaluator.Update(0, 3.9, configuration) == AlarmLevel::Alarm);
    CHECK(evaluator.Update(0, 19.7, configuration) == AlarmLevel::Warning);
    CHECK(evaluator.Update(0, 17.8, configuration) == AlarmLevel::Normal);

    CHECK(evaluator.Update(0, 20.1, configuration) == AlarmLevel::Alarm);
    CHECK(evaluator.Update(0, 19.5, configuration) == AlarmLevel::Alarm);
    CHECK(evaluator.Update(0, 19.4, configuration) == AlarmLevel::Warning);
    CHECK(evaluator.Update(0, 17.8, configuration) == AlarmLevel::Normal);

    const auto previous = evaluator.CurrentLevel(0);
    CHECK(evaluator.Update(
        0,
        std::numeric_limits<double>::quiet_NaN(),
        configuration) == previous);

    AlarmConfiguration invalidConfiguration = configuration;
    invalidConfiguration.minimumMilliamp = 20.0;
    invalidConfiguration.maximumMilliamp = 4.0;
    CHECK(evaluator.Update(0, 0.0, invalidConfiguration) == previous);

    evaluator.Reset(0);
    CHECK(evaluator.CurrentLevel(0) == AlarmLevel::Normal);
    CHECK(evaluator.CurrentLevel(kChannelCount) == AlarmLevel::Normal);
}

void TestMeasurementHistoryCapacityAndOrder()
{
    MeasurementHistory history(2);
    CHECK(history.Empty());
    CHECK(history.Latest() == nullptr);

    history.Push(MakeBatch(1.0));
    history.Push(MakeBatch(2.0));
    history.Push(MakeBatch(3.0));

    CHECK(history.Size() == 2);
    CHECK_NEAR(
        history.Samples().front().channels[0].rawMilliamp,
        2.0,
        1.0e-12);
    CHECK(history.Latest() != nullptr);
    CHECK_NEAR(
        history.Latest()->channels[0].rawMilliamp,
        3.0,
        1.0e-12);

    history.SetCapacity(1);
    CHECK(history.Capacity() == 1);
    CHECK(history.Size() == 1);
    CHECK_NEAR(
        history.Latest()->channels[0].rawMilliamp,
        3.0,
        1.0e-12);

    history.SetCapacity(0);
    CHECK(history.Empty());
    history.Push(MakeBatch(4.0));
    CHECK(history.Empty());

    history.SetCapacity(2);
    history.Push(MakeBatch(5.0));
    CHECK(history.Size() == 1);
    history.Clear();
    CHECK(history.Empty());
    CHECK(history.Latest() == nullptr);
}

void TestSignalSimulatorIsValidAndRepeatableAfterReset()
{
    SignalSimulator simulator;
    CHECK(simulator.SampleIndex() == 0);

    const MeasurementBatch first = simulator.Generate();
    CHECK(first.IsValid());
    CHECK(first.error == CommunicationError::None);
    CHECK(simulator.SampleIndex() == 1);

    for (const auto& channel : first.channels)
    {
        CHECK(channel.valid);
        CHECK(std::isfinite(channel.rawMilliamp));
        CHECK(channel.rawMilliamp >= 4.0);
        CHECK(channel.rawMilliamp <= 20.0);
        CHECK_NEAR(
            channel.calibratedMilliamp,
            channel.rawMilliamp,
            1.0e-12);
    }

    const MeasurementBatch second = simulator.Generate();
    CHECK(second.IsValid());
    CHECK(simulator.SampleIndex() == 2);
    CHECK(std::abs(
        second.channels[0].rawMilliamp -
        first.channels[0].rawMilliamp) > 1.0e-9);

    simulator.Reset();
    CHECK(simulator.SampleIndex() == 0);
    const MeasurementBatch firstAfterReset = simulator.Generate();
    for (std::size_t channel = 0; channel < kChannelCount; ++channel)
    {
        CHECK_NEAR(
            firstAfterReset.channels[channel].rawMilliamp,
            first.channels[channel].rawMilliamp,
            1.0e-12);
    }
}

void TestCsvLoggerKeepsCommunicationErrorsDistinctFromMeasurements()
{
    const auto uniqueSuffix = std::chrono::steady_clock::now()
        .time_since_epoch()
        .count();
    const auto path = std::filesystem::temp_directory_path() /
        ("modbus_daq_monitor_logger_test_" +
         std::to_string(uniqueSuffix) + ".csv");

    CsvLogger logger;
    std::string errorMessage;
    CHECK(logger.Start(path, errorMessage));

    auto valid = MakeBatch(12.0);
    valid.timestamp = std::chrono::system_clock::now();
    logger.Enqueue(valid);

    MeasurementBatch timeout;
    timeout.timestamp = std::chrono::system_clock::now();
    timeout.error = CommunicationError::Timeout;
    logger.Enqueue(timeout);
    logger.Stop();

    std::ifstream input(path);
    CHECK(input.is_open());
    const std::string content(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    input.close();

    CHECK(content.find("timestamp,status,exception_code") != std::string::npos);
    CHECK(content.find(",OK,,12.000,13.000,14.000,15.000") != std::string::npos);
    CHECK(content.find(",TIMEOUT,,,,,") != std::string::npos);

    std::error_code removeError;
    std::filesystem::remove(path, removeError);
    CHECK(!removeError);
}
}

int main()
{
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"Modbus CRC known vector", TestModbusCrcKnownVector},
        {"Modbus valid response", TestModbusValidResponse},
        {"Modbus invalid CRC", TestModbusInvalidCrc},
        {"Modbus wrong slave", TestModbusWrongSlave},
        {"Modbus wrong function", TestModbusWrongFunction},
        {"Modbus wrong byte count", TestModbusWrongByteCount},
        {"Modbus exception response", TestModbusExceptionResponse},
        {"Modbus timeout", TestModbusTimeout},
        {"Modbus transport read error", TestModbusTransportReadError},
        {"Modbus port closed", TestModbusPortClosed},
        {"Calibration model", TestCalibrationModel},
        {"Alarm evaluator hysteresis", TestAlarmEvaluatorHysteresis},
        {"Measurement history", TestMeasurementHistoryCapacityAndOrder},
        {"Signal simulator", TestSignalSimulatorIsValidAndRepeatableAfterReset},
        {"CSV logger error separation", TestCsvLoggerKeepsCommunicationErrorsDistinctFromMeasurements}
    };

    std::size_t failures = 0;
    for (const auto& test : tests)
    {
        try
        {
            test.second();
            std::cout << "[PASS] " << test.first << '\n';
        }
        catch (const std::exception& error)
        {
            ++failures;
            std::cerr << "[FAIL] " << test.first << ": " << error.what() << '\n';
        }
        catch (...)
        {
            ++failures;
            std::cerr << "[FAIL] " << test.first << ": unknown exception\n";
        }
    }

    std::cout << (tests.size() - failures) << '/' << tests.size()
              << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
