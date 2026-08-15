cmake_minimum_required(VERSION 3.16)

project(ModbusDAQMonitorCoreTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(APP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../src/ModbusDAQMonitor")

add_executable(core_logic_tests
    FakeSerialTransport.h
    test_main.cpp
    "${APP_SOURCE_DIR}/domain/AlarmEvaluator.cpp"
    "${APP_SOURCE_DIR}/domain/CalibrationModel.cpp"
    "${APP_SOURCE_DIR}/domain/MeasurementHistory.cpp"
    "${APP_SOURCE_DIR}/protocol/ModbusRtuClient.cpp"
    "${APP_SOURCE_DIR}/services/AcquisitionService.cpp"
    "${APP_SOURCE_DIR}/services/CsvLogger.cpp"
    "${APP_SOURCE_DIR}/simulation/SignalSimulator.cpp"
    "${APP_SOURCE_DIR}/transport/Win32SerialPort.cpp"
)

# The application sources include pch.h, but the pure logic does not depend on
# MFC.  This test-only empty precompiled-header shim keeps this target portable.
target_include_directories(core_logic_tests PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/portable_include"
    "${APP_SOURCE_DIR}/domain"
    "${APP_SOURCE_DIR}/protocol"
    "${APP_SOURCE_DIR}/services"
    "${APP_SOURCE_DIR}/simulation"
    "${APP_SOURCE_DIR}/transport"
)

if(MSVC)
    target_compile_options(core_logic_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(core_logic_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME core_logic_tests COMMAND core_logic_tests)
