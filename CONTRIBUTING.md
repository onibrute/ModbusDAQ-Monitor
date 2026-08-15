# Contributing

Contributions should preserve the project's value as a clear, honest engineering portfolio example. The application is a bench-validated academic prototype; changes must not describe it as certified, production-ready, or deterministic real-time software without corresponding evidence.

## Development environment

Install Visual Studio 2022 with:

- Desktop development with C++;
- MSVC v143 build tools;
- C++ MFC for the selected x86/x64 toolset;
- a current Windows SDK.

Open `ModbusDAQMonitor.sln` and build `Debug|x64` before making changes. Build both `Debug|x64` and `Release|x64` before submitting a change.

## Code boundaries

- Keep MFC controls, messages, and drawing in `src/ModbusDAQMonitor/ui/`.
- Keep calibration, alarm, measurement, and history rules in `domain/` and free of MFC/Win32 dependencies.
- Keep Modbus frame construction and validation in `protocol/`.
- Access serial hardware through `ISerialTransport`; Win32 handle ownership belongs in `transport/`.
- Put workflow orchestration and persistence in `services/`.
- Keep hardware-free sample generation in `simulation/`.
- Include `pch.h` first in `.cpp` files compiled with the MFC project's precompiled-header setting.

Do not put message boxes or dialog-control access inside transport, protocol, or domain code.

## Correctness rules

Changes must preserve these invariants:

1. A communication error is represented explicitly and never substituted with a valid `0.0 mA` measurement.
2. Slave address, function code, byte count, CRC, and Modbus exception frames are validated before registers are accepted.
3. Calibration is performed in mA and applied exactly once to each valid sample.
4. Alarm evaluation receives calibrated, valid samples only.
5. The Modbus slave ID used for calibration reads is the same configured slave ID used for acquisition.
6. The M-7024L remains outside the application control path unless a separately scoped and documented feature intentionally changes that boundary.

## Suggested change workflow

1. Create a focused branch.
2. Make the smallest cohesive change.
3. Build the solution in Debug and Release for x64.
4. Exercise simulation mode for UI and data-flow changes.
5. For protocol or hardware changes, repeat the bench checklist below.
6. Update README or `docs/` whenever assumptions, units, register mapping, or operator behavior change.
7. Review the Git diff and remove generated output before committing.

Do not commit `.vs/`, build directories, database/cache files, local `.user` settings, crash dumps, or runtime CSV logs. Use `exports/`, `captures/`, or `runtime-data/` for disposable local output; those directories are ignored.

## Hardware-change checklist

For changes to serial configuration, Modbus parsing, register mapping, or scaling, record:

- exact module model and firmware if available;
- input range/mode;
- baud rate, framing, and slave ID;
- register addresses and raw example frames/values;
- expected engineering-unit conversion;
- at least two applied reference currents and observed readings;
- timeout, disconnected-device, and invalid-response behavior;
- confirmation that M-7024L control, if used for stimulus, was performed separately through DCON.

Do not include credentials, private machine paths, or sensitive site wiring information in issues or commits.

## Pull-request checklist

- [ ] `Debug|x64` builds without new warnings.
- [ ] `Release|x64` builds without new warnings.
- [ ] Simulation mode still starts, stops, resets, and exports cleanly.
- [ ] Communication failures remain distinguishable from numerical measurements.
- [ ] Units are consistently shown as mA.
- [ ] New logic is placed in the appropriate architectural layer.
- [ ] Hardware assumptions and operator-visible changes are documented.
- [ ] Generated files and local logs are absent from the diff.
