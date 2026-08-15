# Calibration and alarms

All measurement, calibration, threshold, and hysteresis values in the refactored application are expressed in **milliamps (mA)**.

## Measurement stages

Each valid channel sample has two distinct values:

1. **Raw current** — the engineering value produced by the device-specific register conversion.
2. **Calibrated current** — the result of applying the channel's linear calibration once.

If a channel has no completed calibration, its calibrated value is equal to its raw value. Communication failures have `valid = false`; they are not passed through calibration or interpreted as numerical current measurements.

## Two-point calibration

For each channel, the model stores two measured/reference pairs:

```text
(raw₁, reference₁)
(raw₂, reference₂)
```

The calibrated value is:

```text
calibrated = slope × raw + offset

slope  = (reference₂ - reference₁) / (raw₂ - raw₁)
offset = reference₁ - slope × raw₁
```

Both points must be finite and their raw values must be different. If either point is missing or the raw span is effectively zero, calibration remains disabled and the identity transform is used.

### Worked example

Suppose one channel produces these stable readings:

| Point | Raw reading | Reference current |
| --- | ---: | ---: |
| 1 | 4.08 mA | 4.00 mA |
| 2 | 19.86 mA | 20.00 mA |

The resulting coefficients are approximately:

```text
slope  = 16.00 / 15.78 = 1.01394
offset = 4.00 - 1.01394 × 4.08 = -0.13688 mA
```

A subsequent raw reading of `12.00 mA` becomes approximately `12.03 mA`.

This is an illustrative arithmetic example, not a statement about the accuracy of the bench hardware.

## Recommended calibration workflow

1. Allow the module, source, and reference instrument to reach stable operating conditions.
2. Verify the channel is in the correct 4–20 mA mode and that raw values are plausible.
3. Apply a stable first current near the lower part of the intended working span.
4. Record the reference current and capture the averaged raw point.
5. Apply a stable, clearly separated second current near the upper part of the intended working span.
6. Record the reference current and capture the second averaged raw point.
7. Calculate the per-channel coefficients.
8. Verify at least one independent intermediate current that was not used for the fit.
9. Repeat for every channel; coefficients are not shared implicitly between channels.

For the thesis bench, the M-7024L may provide these stimuli, but it remains under separate DCON control. Use an appropriate reference meter when accuracy must be demonstrated rather than relying only on the commanded output value.

Calibration coefficients are currently session state. Recheck them after restarting the application, changing wiring, changing module range/mode, or replacing hardware.

## Alarm model

Each channel is evaluated independently using:

- `minimumMilliamp` — lower alarm boundary;
- `maximumMilliamp` — upper alarm boundary;
- `warningBandFraction` — fraction of the configured span reserved as an inner warning band at each end;
- `hysteresisMilliamp` — distance a signal must move back from a boundary before a previous state may clear.

The default domain configuration is:

| Parameter | Default |
| --- | ---: |
| Minimum | 4.0 mA |
| Maximum | 20.0 mA |
| Warning-band fraction | 10% per side |
| Hysteresis | 0.2 mA |

For those defaults, the initial transition boundaries are:

| Level | Condition when entering from Normal |
| --- | --- |
| Alarm | `value ≤ 4.0 mA` or `value ≥ 20.0 mA` |
| Warning | `value ≤ 5.6 mA` or `value ≥ 18.4 mA` |
| Normal | Between the warning boundaries |

The warning boundaries come from 10% of the 16 mA configured span:

```text
warning low  = 4.0 + (20.0 - 4.0) × 0.10 = 5.6 mA
warning high = 20.0 - (20.0 - 4.0) × 0.10 = 18.4 mA
```

## Hysteresis behavior

Hysteresis makes clearing a state stricter than entering it:

- after a low alarm, a value at or below `minimum + hysteresis` remains Alarm;
- after a high alarm, a value at or above `maximum - hysteresis` remains Alarm;
- after a low warning, a value at or below `warning low + hysteresis` remains Warning;
- after a high warning, a value at or above `warning high - hysteresis` remains Warning.

With the defaults, a low alarm therefore remains latched through `4.2 mA`, and a low warning remains through `5.8 mA`. The same behavior is mirrored at the upper boundary. The state is not a permanent latch: it clears when the signal has returned far enough into the interior range.

The implementation constrains the warning fraction and hysteresis relative to the configured span to prevent inverted state bands. The UI should still reject a minimum greater than or equal to the maximum and any negative hysteresis.

## Communication failures are not process alarms

A timeout, CRC mismatch, invalid slave/function/byte count, Modbus exception, closed port, or transport failure describes data quality and connectivity. It does not prove that the electrical input crossed an alarm threshold.

Accordingly:

- invalid samples do not update electrical alarm state;
- invalid samples do not become `0.0 mA` values;
- CSV output records their status and leaves channel-value fields empty;
- recovery should resume with the next valid measurement rather than inventing a connecting line through zero.

This distinction is critical for both trustworthy charts and meaningful alarm logs.
