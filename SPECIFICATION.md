# Boiler Project Specification

This is the working technical specification for the project and the primary
source of requirements during development.

**Specification date:** 2026-09-11  
**Document version:** 0.1.0 (draft)  
**Microcontroller:** ATmega16A  
**Language:** C11  
**Scheduler:** AcroSched 2.2.0, cooperative kernel

## 1. Purpose

The device controls two independent heating channels and measures temperature
with two DS18B20 sensors. Depending on the fitted hardware, the outputs control
CPC1008N solid-state relays or MOC3023S optotriacs, which drive external power
components.

The device does not control the gas boiler ignition or water-flow automation.
Those functions belong to the external boiler control circuit.

## 1.1. Development and Build Baseline

The canonical project build uses the AVR-GCC toolchain and GNU Make. VS Code
is the primary source editor and development interface, but the firmware must
be buildable without VS Code-specific project metadata.

The repository build must produce the firmware image, ELF file, map file, and
HEX file from the same command-line build. Device programming and debugging
may be performed with a compatible Microchip tool and MPLAB X v6.35 when
available; MPLAB X is not the source-of-truth project format.

Atmel Studio on another machine may be used as a secondary environment only if
it invokes the repository build or reproduces the same compiler, linker, and
configuration options. The new firmware must not depend on the historical
Atmel Studio project files in `OldCode`.

AcroSched is built from its sources on the same machine and with the same
AVR-GCC toolchain as the firmware. Record its exact library file, headers,
compiler options, include path, and linker options in the build documentation.

## 1.2. Development Stages

Development is incremental. Each stage must be buildable and testable before
the next hardware or operating-mode scope is added.

1. Implement a proportional temperature controller using CPC1008N outputs,
  one DS18B20 sensor, and software 1-Wire. This stage covers the single-
  channel first-circuit operation represented by Mode 3.
2. Add the DS2480B sensor driver while retaining software 1-Wire as an
  alternative build configuration. The two drivers remain mutually
  exclusive; proportional control and the single-channel scope are unchanged.
3. Add the remaining operating scope, including Modes 1 and 2 and the
  associated two-sensor behavior.
4. Add MOC30xx optotriac support. Resolve the power-stage, synchronization,
  and any required schematic questions at this stage.

The CPC1008N and software 1-Wire stages are the initial firmware and bench-
testing target. MOC30xx support is intentionally excluded from the first
three stages.

## 2. Hardware Configuration

### 2.1. MCU and Clock

- ATmega16A.
- Crystal oscillator: 14.7456 MHz.
- Target scheduler tick: 1 ms.
- Mains frequency: 50 Hz.

### 2.2. Pinout

| Pin | Function | Direction/mode |
| --- | --- | --- |
| PA0 | Segment A | output |
| PA1 | Segment B | output |
| PA2 | Segment C | output |
| PA3 | Segment D | output |
| PA4 | Segment E | output |
| PA5 | Segment F | output |
| PA6 | Segment G | output |
| PA7 | Decimal point | output |
| PB0 | K1, digit 1 | output |
| PB1 | K2, digit 2 | output |
| PB2 | K3, digit 3 | output |
| PB3 | K4, digit 4 | output |
| PB4 | K5, LED indication | output |
| PC2 | Buzzer | output |
| PC3 | Down | input |
| PC4 | Right | input |
| PC5 | Up | input |
| PC6 | Left | input |
| PC7 | Enter | input |
| PD0 | UART Rx for DS2480B | input when DS2480B is used |
| PD1 | UART Tx for DS2480B | output when DS2480B is used |
| PD2 | Direct 1-Wire | input/output for software 1-Wire |
| PD3 / INT1 | Zero-cross Sync | external interrupt input |
| PD4 | Out 2 | heater output control |
| PD5 | Out 1 | heater output control |

### 2.3. Dynamic Indication

The display has four common-cathode digits. The LED indication uses the fifth
select line K5 and the same PA0..PA7 segment bus.

```text
K1 -> K2 -> K3 -> K4 -> K5
```

- K1..K4 select the four seven-segment digits.
- K5 selects the eight LEDs.
- Segments and LEDs are active high.
- PA0..PA7 use non-inverted logic.
- The selected position is active high.
- The current position must be disabled before changing the bus data.
- The display buffer is separate from the physical port driver.

The target slot-switching period is approximately 1 ms. The final value is
tuned during brightness and flicker testing.

HL4 is connected to segment D (PA3, bit 3) in the K5 LED slot. It remains
on during normal operation. While an error is active, it blinks with a
one-second period: 500 ms on and 500 ms off (50 percent duty cycle).

The buzzer on PC2 is active, with a built-in oscillator, and sounds when
the pin is high. Startup produces one short 100 ms signal; it must not
sound continuously in an error state. Timing is non-blocking.

### 2.4. Heater Outputs

PD4 and PD5 are connected directly to the cathodes of the CPC1008N input
stages. The anodes are supplied from +5 V through current-limiting resistors.
The outputs are active low. The resistor value and the CPC1008N input circuit
are board-level details; the firmware controls only the relay input state.

Supported hardware configurations:

- CPC1008N: discrete on/off control used for the first implementation and
  interface debugging;
- MOC3023S: control of an external power triac, added in the final hardware
  stage.

The safe OFF state is:

```text
DDRD corresponding bit = output
PORTD corresponding bit = 1
```

The discrete ON state is:

```text
PORTD corresponding bit = 0
DDRD corresponding bit = output
```

When disabling an output, write `1` to PORTD before changing or retaining the
pin direction as output. This prevents an unintended low pulse during output
initialization or shutdown. Heaters are disabled during startup, errors,
service procedures, watchdog reset handling, and invalid sensor configuration.
The firmware does not define or control the load-side power circuit beyond
turning the CPC1008N input on or off.

### 2.5. Zero-Cross Synchronization

PD3 is connected directly to INT1. During most of each half-cycle the
opto-detector transistor holds the line low. Before the mains zero crossing
the transistor turns off and the pull-up brings the line high. After the zero
crossing the transistor turns on again and pulls the line low. The application
uses this high-to-low transition (`1 -> 0`) after the zero crossing as the
synchronization event. Mains frequency is 50 Hz.

Both MOC3023S channels use the same Sync signal. The INT1 handler only records
the event and passes it to application logic. PID calculations, sensor access,
and other long operations are forbidden in the ISR.

The first MOC3023S implementation provides full half-cycle conduction and
switch-off without phase control. Phase-angle control and PID are separate
future work.

The zero-cross detector is located after a transformer. Its propagation delay
and phase shift must be measured or calculated when selecting the firing delay
for the power stage. The measured timing at the ATmega16A pin is the timing
reference; the detector edge must not be assumed to coincide exactly with the
AC zero crossing. Configure INT1 for the falling edge (`ISC11:ISC10 = 10`) and
verify the timing of that edge against the transformer secondary waveform.

### 2.6. 1-Wire and DS2480B

The following build configurations are mutually exclusive:

```text
BOILER_USE_DS2480B    = 1, BOILER_USE_SOFT_1WIRE = 0
BOILER_USE_DS2480B    = 0, BOILER_USE_SOFT_1WIRE = 1
```

When DS2480B is used:

- PD0 and PD1 are crossed UART Rx/Tx lines;
- working UART speed is 115200 baud;
- DS2480B starts at 9600 baud;
- PD2 is unused and configured as an input without pull-up;
- no hardware reset line is provided for DS2480B.

Initialization starts communication at 9600 baud, configures DS2480B for
115200 baud, switches the ATmega16A UART to 115200 baud, and verifies the
communication.

When software 1-Wire is selected, PD2 is used. DS2480B and direct 1-Wire must
never be enabled in the same build.

## 3. Sensors and ROM Assignment

The configuration contains an operating profile that determines how many
DS18B20 sensors and heating channels are required. The supported profiles are:

```text
SINGLE_CHANNEL -> SENSOR_1 is required; SENSOR_2 is unused
TWO_CHANNEL    -> SENSOR_1 and SENSOR_2 are required
```

In both profiles, the physical installation point is defined by the complete
64-bit ROM code, never by Search ROM discovery order. Both DS18B20 devices,
when present, share one 1-Wire bus.

The configuration stores two assignments:

```text
SENSOR_1 -> ROM code of the sensor at location/circuit 1
SENSOR_2 -> ROM code of the sensor at location/circuit 2
```

`SENSOR_2` is unused and has no valid ROM value in the `SINGLE_CHANNEL`
profile. Normal operation must address each configured sensor with Match ROM.
Discovery order must never be used to map a sensor to a physical location.

Before heating is enabled, verify:

- EEPROM configuration validity;
- the family code and ROM CRC for every stored ROM code;
- the required assigned sensors are present on the bus;
- no unknown or extra device is present for the selected profile;
- scratchpad CRC for every required sensor;
- a valid temperature reading from every required sensor.

Any failure disables both heater outputs.

## 4. EEPROM Configuration

The first EEPROM addresses are reserved. Application configuration starts at
or after address `0x20`.

Each configuration record must contain at least:

```text
magic
format_version
sequence
sensor_1_rom[8]
sensor_2_rom[8]
operating_profile
application_settings
crc
commit_marker
```

The preferred layout uses two configuration slots. A new record is written to
the inactive slot, then read back and verified. Only after successful
verification does it receive its commit marker. At startup, the valid slot with
the newest sequence number is selected.

Partial updates of individual ROM codes are forbidden. New values remain in RAM
until both sensors have been verified and the user has confirmed the write.

The following states are invalid:

- empty EEPROM;
- `0xFF` instead of a valid record;
- invalid magic;
- unknown format version;
- CRC failure;
- incomplete record;
- missing or invalid ROM code required by the selected profile;
- duplicate ROM codes;
- incomplete sensor assignment.

## 5. Sensor Assignment Service

There is no separate service firmware. Assignment is performed by the main
firmware without a programmer.

### 5.1. Entering Service Mode

At startup, hold `Left + Right`. The combination must pass normal debounce and
remain held for several seconds. Both heater outputs remain disabled while the
combination and service procedure are being evaluated.

### 5.2. Assignment Sequence

The user selects the operating profile before assigning sensors.

For `SINGLE_CHANNEL`:

```text
PROFILE  select single-channel profile, press Enter
S1?      connect only the sensor at location 1, press Enter
S1OK     keep the ROM code in temporary RAM
CHECK    verify the single connected sensor, press Enter
SAVE?    confirm the EEPROM write with Enter
DONE     verify the record and restart
```

For `TWO_CHANNEL`:

```text
PROFILE  select two-channel profile, press Enter
S1?      disconnect the sensor at location 2, press Enter
S1OK     keep the first ROM code in temporary RAM
S2?      disconnect the sensor at location 1, press Enter
S2OK     keep the second ROM code in temporary RAM
CHECK    connect both sensors, press Enter
SAVE?    confirm the EEPROM write with Enter
DONE     verify the record and restart
```

Exactly the number of suitable devices required by the selected profile must
be found during each assignment step. `Enter` is required before each
measurement and before committing the EEPROM record. A single-channel
configuration does not make Modes 1 and 2 available.

The new ROM codes must not replace the active configuration before the complete
procedure succeeds. If the procedure is interrupted, the previous valid
configuration remains unchanged. If no previous configuration exists, the
device remains in the error state.

## 6. Missing Configuration State

If the EEPROM is empty or contains no valid record, enter `CONFIG_INVALID`.

Required behavior:

- Out 1 and Out 2 are disabled;
- normal operation is unavailable;
- the display shows `E001`;
- the error/mode LED blinks;
- the buzzer gives one short startup signal and does not sound continuously;
- pressing `Enter` starts the sensor assignment procedure;
- leaving an incomplete procedure returns to `E001`.

`E001` means: sensor configuration is missing or invalid. Dashes may be used
as an intermediate display, but must not be the only indication of this state.

## 7. Error Codes

The list is extensible. Initial codes:

| Code | Meaning |
| --- | --- |
| E001 | Sensor configuration missing or invalid |
| E002 | Sensor at location 1 is missing |
| E003 | Sensor at location 2 is missing |
| E004 | Unknown sensor detected |
| E005 | ROM or scratchpad CRC failure |
| E006 | Invalid number of 1-Wire devices |
| E007 | DS2480B is unavailable or not initialized |
| E008 | Direct 1-Wire interface failure |
| E009 | The last reset was caused by the watchdog |

When multiple errors are present, show the error that blocks safe heater
startup while retaining the other causes in diagnostic state.

## 8. Operating Modes

### Mode 1: Two Independent Circuits

Mode 1 is available only with the `TWO_CHANNEL` profile.

- Channel 1 controls the high-temperature closed heating circuit.
- Channel 2 controls user water according to its configured temperature.
- Each channel has its own sensor and output.
- In the user-water mode, channel 2 inhibits channel 1 according to the
  control rules that must be finalized before implementing the control module.

### Mode 2: Two-Tank Boiler

Mode 2 is available only with the `TWO_CHANNEL` profile.

- Each channel controls its own heater.
- The tanks are not isolated and normally have similar temperatures.
- An excessive configured temperature difference creates an error.
- An excessive temperature difference disables both heaters and latches a
  temperature-mismatch error.

### Temperature Settings

- The maximum allowed water temperature is a configuration parameter selected
  for the installed pipe and system hardware.
- The firmware hard upper limit for this parameter is 90 degrees Celsius.
- A user setpoint must not exceed the configured maximum water temperature.
- User hysteresis is configurable from 0 to 50 degrees Celsius below the
  corresponding setpoint.
- When either measured temperature reaches the configured maximum water
  temperature, the affected heater is disabled.
- Thermal overshoot must be handled by disabling the heater early enough that
  the measured water temperature does not exceed the configured maximum.
- The configured maximum temperature and the resulting emergency cutoff are
  stored in the validated EEPROM application settings.

### Mode 2 Temperature Difference

- Mode 2 has a configurable maximum allowed temperature difference between the
  two channels.
- The difference is calculated as the absolute value of the two valid measured
  temperatures.
- If the difference exceeds the configured limit, both heaters are disabled
  and the system enters a latched temperature-mismatch error state.
- The error remains active until the difference returns within the limit and
  the user acknowledges the error, or until the device is restarted, according
  to the final safety policy.
- The configured difference limit is stored in the validated EEPROM
  application settings.

### Mode 3: Single First Channel

Mode 3 controls only the first heating channel using one DS18B20 sensor. It
is the initial single-channel operating mode for the proportional CPC1008N
implementation. It is available with the `SINGLE_CHANNEL` profile; the second
sensor and second heater remain unused. The mode may also be available with a
`TWO_CHANNEL` profile, but it still uses only SENSOR_1 and channel 1.

### Mode 4: Sensor Configuration

Mode 4 is the sensor-configuration service mode. It selects the operating
profile, discovers the required DS18B20 devices, assigns their complete ROM
codes to the physical locations, and saves the confirmed assignment in EEPROM
for subsequent operation. The procedure is defined in Section 5. Heating
remains disabled throughout the procedure. If the procedure is interrupted or
fails, the previous valid configuration remains unchanged; without a previous
valid configuration, the device returns to `CONFIG_INVALID`.

### User Interface Displays

- In Mode 1, the display shows the selected channel temperature.
- `Left` selects channel 1 and `Right` selects channel 2 in Mode 1.
- In Mode 2, the display alternates between the temperatures of both channels.
- Pressing `Left` or `Right` selects the corresponding channel and holds its
  temperature on the display for 5 seconds without automatic switching.
- After the 5-second hold expires, Mode 2 resumes alternating both channels.
- In any future reserved mode, the display shows dashes.
- The sensor-assignment service uses the dedicated service screens defined in
  Section 5.

### Value Editing and Long Press

- Temperature and other numeric settings are edited as whole integer values,
  not digit by digit.
- A short `Up` or `Down` press changes the selected value by one unit.
- Holding `Up` or `Down` first changes the value once on the debounced press.
- No further changes occur during the next 3 seconds.
- The first repeat occurs at 3 seconds after the debounced press, followed
  by one change every 500 ms until release.
- The accelerated interval is a configuration parameter that may be reduced
  to 250 ms after hardware testing confirms that the setting remains usable.
- Value changes are clamped to the valid range and never wrap around.

### Button Input

- PC3..PC7 use internal pull-ups; a pressed button connects its input to ground.
- All five buttons are polled independently every system tick.
- Both press and release require 20 ms of stable sampled input.
- Press, release and Up/Down repeat events are delivered synchronously to
  a non-blocking cooperative handler. Left, Right and Enter do not repeat.
- Raw release suspends repeats immediately, while its release event still
  requires debounce. Late polling does not generate bursts of missed repeats.
- The initial button-driver stage runs state-only polling without a menu
  handler. It does not change E001, start sensor assignment or enable heaters;
  these actions will be wired when the service UI is implemented.

## 9. AcroSched Constraints

AcroSched 2.2.0 is built locally from its sources with the same AVR-GCC
toolchain as the Boiler firmware. The AcroSched source tree is maintained as
an external project and is not included in the Boiler repository. Its built
library and public headers are integrated into the Boiler build.

Initial library configuration:

```text
ACROSCHED_USE_PREEMPT       0
ACROSCHED_TICK_TYPE         uint16_t
ACROSCHED_MAX_TASKS         8
ACROSCHED_USE_WATCHDOG      1
ACROSCHED_USE_IPC           0
ACROSCHED_USE_TIMERS        0
ACROSCHED_USE_IDLE_HOOK     0
ACROSCHED_USE_HOOKS         0
ACROSCHED_USE_INTROSPECTION 0
ACROSCHED_USE_STATISTICS    0
```

The scheduler is linked as one library. Boiler builds must not depend on how
AcroSched source files are split internally. The exact library filename,
include path, and linker options will be recorded in build documentation when
the binary is available.

Tasks must be short and non-blocking. Long DS18B20, DS2480B, and EEPROM
operations must be implemented as incremental state machines.

Initial task candidates:

- display scanning;
- button polling and debounce;
- incremental sensor communication;
- heater control;
- safety checks;
- user-interface processing;
- buzzer control;
- deferred settings storage.

## 10. Safety Requirements

Heating is forbidden when any of the following is true:

- EEPROM configuration is invalid;
- an assigned sensor is missing;
- an unknown or extra sensor is detected;
- ROM or scratchpad CRC fails;
- DS2480B or software 1-Wire fails;
- the emergency temperature limit is exceeded;
- the allowed temperature difference is exceeded;
- the sensor assignment service is active;
- a watchdog reset occurred and system verification has not completed.

After power-up, outputs remain disabled until all checks finish. Every hardware
output must have a safe state before the scheduler starts.

## 11. Open Questions

These items must be resolved before implementing the corresponding modules:

- verify the 90 degrees Celsius limit against the data sheet of the specific
  plastic pipe and define the response to thermal overshoot;
- define the MOC3023S gate, snubber, protection, and zero-cross drive circuit
  during the final hardware stage;
- decide whether PID is required and scope it as a separate phase.

## 12. Project Documents

- `HISTORY.md` - decisions, completed work, findings, and errors.
- `CHANGELOG.md` - release-oriented project changes.
- `README.md` - project overview and development entry point.
