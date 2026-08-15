# Core logic tests

This directory contains a dependency-free C++17 test executable for the
portable domain, simulation, and Modbus RTU logic.

The suite covers:

- a known Modbus CRC vector;
- request construction and decoding of a valid multi-register response;
- invalid CRC, slave, function, and byte count responses;
- Modbus exception responses, timeouts, closed ports, and transport errors;
- calibration validation and two-point calculation;
- alarm thresholds and hysteresis;
- bounded measurement history;
- deterministic signal simulation after reset;
- CSV logging that keeps communication failures distinct from valid numeric samples.

`FakeSerialTransport` feeds complete and malformed RTU frames to
`ModbusRtuClient`, so protocol behavior is tested without a COM port or other
hardware.

## Build and run

```powershell
cmake -S tests -B tests/build
cmake --build tests/build --config Release
ctest --test-dir tests/build -C Release --output-on-failure
```

The executable prints one result per scenario and currently finishes with:

```text
15/15 tests passed
```

The `portable_include/pch.h` file is deliberately empty. It replaces the MFC
precompiled header only for this test target, because the tested sources use
the standard library and do not otherwise depend on MFC.
