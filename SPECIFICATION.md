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

HL7 is connected to segment C (PA2, bit 2) in the K5 LED slot. In the
current temperature-interface stage it stays on when Mode 3 is selected by a
valid configuration, including while a sensor error is displayed. It is
off without a valid configuration and throughout sensor assignment.

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

The implemented direct backend requires three-wire DS18B20 connections:
separate VDD, common GND, and DQ on PD2 with an external pull-up. PD2 is
open-drain in software: its output latch remains zero, DDR switches between
output-low and released input, and the internal pull-up is disabled. Other
PORTD/DDRD bits, including the safe heater outputs, are preserved. Parasite
power is detected by Read Power Supply and rejected with E008; no strong
pull-up or parasite-power conversion is implemented.

Standard-speed slots use 6 us low for Write 1, 60 us for Write 0, and a
3 us low followed by 8 us to the read sample. Slot duration is at least
70 us. Reset uses 480 us low, a presence sample 70 us after release and
410 us recovery. Only timing-critical low/sample intervals mask interrupts,
preserving their incoming state; the longest masked interval is about 70 us.
The approximately 1 ms reset is the largest indivisible operation. Byte
transfers and search triplets are spread over cooperative task invocations.
Actual line rise time and display smoothness require board verification.
Protocol and timing reference:
[DS18B20 data sheet](https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf).

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

### 3.1. Fake Sensor Backend

The explicit `SENSORS_USE_FAKE=1` build option enables an isolated test
implementation; normal builds default to zero. It does not use GPIO, 1-Wire,
EEPROM or heater outputs. Fake-enabled firmware initializes it at startup.

The backend provides discovery and temperature reads addressed by complete
ROM, with two fixed DS18B20-family ROMs containing valid Dallas CRC bytes.
Fixture indices control simulation only and do not assign installation points.
Initialization restores both devices as present, at 25 and 30 degrees Celsius.
Test controls can independently disconnect either device and set signed
temperatures in 1/16 degree units within the DS18B20 range (-55..125 C).

Discovery succeeds with a count of zero on an empty simulated bus. Reads of
absent or unknown ROMs return `eResultNotFound`, not a synthetic temperature.
Uninitialized calls return `eResultNotReady`. Failed calls preserve outputs.
The fake returns immediately and does not model conversion time, bus faults
or scratchpad CRC failures. Real backends will require incremental conversion
handling before they can replace it; fake readings never authorize heating.

Fake-enabled firmware explicitly starts an automatic temperature sweep after
backend initialization. Both fixtures start at -55.0 C, increase together by
0.1 C every 500 ms through +125.0 C inclusive, then return to -55.0 C and
repeat. A tenths accumulator is converted to the nearest 1/16 C reading;
adding a rounded raw increment repeatedly is forbidden because it would drift.
Each endpoint lasts one step interval; the complete 1801-sample cycle takes
900.5 seconds. Disconnected fixtures advance too, and assignment/save/apply
does not restart the sweep or change presence. Timing is non-blocking,
handles tick wrap and retains elapsed partial intervals after a late poll.
The low-level initialization and manual fixture controls remain available
for deterministic tests without starting the sweep.

### 3.2. Real DS18B20 Backend

The default build (`SENSORS_USE_FAKE=0`) uses software 1-Wire. A task polled
each tick performs at most one reset, one byte, or one Search ROM triplet
per invocation. Discovery handles ROM collisions and validates Dallas CRC-8,
with a capacity of two devices; further branches produce E006.

Initialization completes before the polling task is registered; update and
refresh require a prior `sensors_init()` call and do not check an initialization
flag at runtime. Before initialization, search/read report NotReady through
the initial snapshot state; argument validation remains active.

Each discovered DS18B20 is addressed with Match ROM for Read Power Supply
and Read Scratchpad. After verifying external power, the backend reads and
validates the scratchpad and ensures 12-bit resolution (0.0625 C). If needed,
Write Scratchpad changes the volatile configuration to `0x7F`, preserving
the original TH/TL alarm bytes. A second scratchpad read verifies both the
configuration and alarms; mismatches fail with E008, while bad CRC gives E005.
Already configured 12-bit devices are not rewritten. No Copy Scratchpad
command or sensor EEPROM write is performed; setup is checked every scan.

Only after all devices pass setup does a broadcast Convert T start conversion.
The task waits at least 750 ticks without blocking, checks completion through
read slots and reports E008 if still busy at 800 ticks. Readings are checked
against -55..125 C and accepted only after the entire nine-byte scratchpad
passes CRC and confirms 12-bit configuration.
All-zero data is rejected even though its CRC is zero. Power-on +85 C is
never published before an explicitly requested conversion has completed;
a legitimate converted +85 C remains valid.

The backend publishes one coherent cache only after the whole acquisition
succeeds. A new periodic acquisition retains the previous checked snapshot
until it completes; a failed acquisition immediately invalidates that cache.
An initial acquisition or explicit service refresh reports Busy, not a
synthetic temperature or an application error. The initial display is
dashes for a valid configuration until the first result; E001 still takes
precedence for invalid assignments. After each result a 100 ms pause precedes
the next scan; normal updates take approximately one second, rather than
the fake backend's 500 ms sweep interval.

Empty-bus discovery produces a zero-device snapshot and the assigned-sensor
diagnostic E002/E003. Unknown ROMs produce E004 through the existing profile
checks; ROM/scratchpad CRC failures produce E005. Stuck-low/failed bus
transactions, invalid scratchpads, unsupported parasite power and conversion
timeouts produce E008. Acquisition retries automatically; recovery restores
temperature and steady HL4 without another beep. Heating stays disabled.

Display scanning remains cooperative. One software reset occupies about
960 us and one transferred byte about 560 us, so scanning can be visibly
delayed despite enabled interrupts between critical timing intervals.
Task priorities cannot preempt an already running bus operation. Two sensors
add more addressed transfers, not twice the duration of one reset or byte.
Board testing confirmed occasional subtle display jitter; its scheduling
solution is not yet agreed, and scanning has not been moved to an interrupt.
Digit switching must remain in the display module as a scheduler task.
BSP only writes GPIO; scan frequency is controlled by the scheduler period,
not by an interrupt-specific divider or condition.

## 4. EEPROM Configuration

Persisted operating-profile values are `1` for `SINGLE_CHANNEL` and `2` for
`TWO_CHANNEL`; zero and other values are invalid. The application validates
required DS18B20 ROM family codes, Dallas CRCs and uniqueness after loading
the newest intact record. The storage module validates record integrity;
application validation does not change the EEPROM layout or format version.
The second assignment is ignored for the single-channel profile.
Application settings remain opaque until their control logic is implemented.

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

Press `Enter` while E001 is displayed to start assignment.

For re-entry regardless of configuration validity or the displayed error,
hold `Left + Right` before power-on/reset. After normal debounce, keep the
combination held for 3 seconds until profile selection appears, then release
the keys. Releasing early cancels the startup attempt. Pressing the combination
later during ordinary operation does not open service. Timing is non-blocking
and handles tick wrap; a raw release prevents entry at the hold boundary.
The initial sample requires both Left and Right but tolerates extra active
inputs while levels settle. The exact combination must appear within the
normal debounce interval; extra keys that remain pressed reject entry.
Both heater outputs remain disabled while the combination and service procedure
are being evaluated. A valid old configuration remains unchanged until a
complete replacement has been confirmed, committed and verified.

### 5.2. Assignment Sequence

The user selects the operating profile before assigning sensors.

For `SINGLE_CHANNEL`:

```text
PROFILE  select single-channel profile, press Enter
S1?      connect only the sensor at location 1, press Enter
S1OK     keep the ROM code in temporary RAM
CHECK    verify the single connected sensor, press Enter
SAVE?    Enter rechecks sensors, writes, verifies and applies automatically
```

For `TWO_CHANNEL`:

```text
PROFILE  select two-channel profile, press Enter
S1?      disconnect the sensor at location 2, press Enter
S1OK     keep the first ROM code in temporary RAM
S2?      disconnect the sensor at location 1, press Enter
S2OK     keep the second ROM code in temporary RAM
CHECK    connect both sensors, press Enter
SAVE?    Enter rechecks sensors, writes, verifies and applies automatically
```

Exactly the number of suitable devices required by the selected profile must
be found during each assignment step. `Enter` is required before each
measurement and before committing the EEPROM record. A single-channel
configuration does not make Modes 1 and 2 available.

The new ROM codes must not replace the active configuration before the complete
procedure succeeds. If the procedure is interrupted, the previous valid
configuration remains unchanged. If no previous configuration exists, the
device remains in the error state.

### 5.3. Four-Digit Service UI and Backend Controls

The service is enabled with `CONFIG_USE_TEST=0` for both real and fake
backends. Unlike the RAM configuration test mode, this service
**writes EEPROM after explicit confirmation**. Heater outputs stay off.

| Screen | Action |
| --- | --- |
| `P  1` / `P  2` | Up/Down select single/two-channel profile; Enter confirms |
| `S1  ` | Leave one device present; Enter discovers and assigns its ROM |
| `S2  ` | Leave the other device present; Enter assigns it |
| `CONF` | Connect all required devices; Enter verifies assignments/readings |
| `SAVE` | Enter rechecks devices, confirms writing and automatically applies the verified record |

Letters use seven-segment approximations, including N and V. Successful
assignment immediately advances to the next prompt; S1OK/S2OK are conceptual
states, not additional confirmation screens. At each assignment step exactly
one suitable device must be present. Two-channel assignments must be distinct.

In fake builds, Right cycles simulated presence:
neither -> first -> second -> both -> neither.
The first two digit decimal points indicate fixture presence, independently
of assigned locations. Right never writes EEPROM or assigns a ROM by index.
This is a test control, not a feature for the real sensor backend.

For real sensors, Right has no action and fixture decimal points are off.
Connect/disconnect physical sensors as prompted. Enter at S1/S2, CONF and
SAVE requests a fresh complete acquisition, including conversion and CRC,
before assigning, advancing or starting EEPROM writing. The prompt stays
visible while Busy; a measurement error remains retryable with Enter or
cancellable with Left. Unsupported sensor families cannot be assigned.

Left cancels any step before writing and restores the current diagnostic
or temperature screen with no EEPROM change. Changing fake presence during
service can produce a new ordinary diagnostic after cancellation, without
changing the old assignments. Failed measurements/checks display their error;
Enter retries the
same operation and Left cancels. Failed EEPROM writes/readback preserve the
old cached configuration and expose the specific operation result through
the menu diagnostic API. Their UI uses E001; Enter retries the save.

Writing is cooperative: one byte is launched only when the EEPROM engine
is ready, followed by payload readback, commit and final verification.
All keys are ignored once writing begins, through the application handoff.
After final verification the saved record is applied automatically without
an additional screen, button press, reset or startup beep. Ordinary diagnostics
do not overwrite service screens. Enter combined with another held button
does not confirm a step. After application, the idle screen shows the assigned SENSOR_1 temperature
with steady HL4 and HL7, or the current diagnostic if sensor presence has
since changed.
Applying the saved record does not reset simulated presence to its startup
mask or restart the temperature sweep. Subsequent service initialization
reads actual backend presence.
Diagnostics remain paused through the application handoff, so they cannot
check an old assignment against the new service setup.

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

### 6.1. RAM Configuration Test Mode

`CONFIG_USE_TEST=1` requires `SENSORS_USE_FAKE=1`. It loads fixed fixture ROMs
into RAM and bypasses EEPROM initialization, reads and writes entirely.
`CONFIG_TEST_PROFILE` selects profile 1 or 2 (default 2).
`SENSOR_FAKE_PRESENT_MASK` selects startup presence, bit 0 for the first
fixture and bit 1 for the second (default 3). These are test controls only;
locations are still matched using the full configured ROM.

The fake-enabled firmware checks required devices at startup and every
100 ms. E001 blocks sensor checks; a missing first device produces E002,
a missing required second device produces E003. Missing assigned devices
have priority over unassigned extras, which produce E004. If both devices
are missing, E002 is displayed. Recovery restores the assigned SENSOR_1
temperature and makes HL4 steady without repeating the startup beep.

The normal build loads and validates EEPROM assignments and performs the
real bus acquisition described in Section 3.2. No build at this stage enables
heater outputs.
The RAM test mode is not a persistent configuration or a sensor service menu.

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

The current interface stage implements only Mode 3 temperature indication
with real or fake sensors; no build enables heating. Both profiles show only the
ROM assigned to SENSOR_1, with no channel switching or channel marker.
Existing profile-based diagnostics are retained: a two-channel assignment
still requires both devices, and unknown extras remain errors.

Temperature formatting is right-aligned on the four digits, with leading
zeros blank and a minus sign for negative readings. First round the original
reading to the nearest tenth. If that result is within -9.9..99.9 C,
show one decimal place (including the trailing zero, e.g. 25.0). Otherwise
round the original reading to whole degrees. Halfway values round away from
zero. Examples: -55, -10.5 -> -11, -9.9375 -> -9.9, 0.0, 99.9375 -> 99.9,
100, 100.5 -> 101, 125. The complete DS18B20 range is displayable for testing;
this does not extend allowable water setpoints or authorize heating.

Ordinary temperature-screen buttons have no action in this stage. Startup
Left+Right and Enter at E001 retain their service-entry behavior, and all
existing assignment-menu controls remain available. Diagnostics and service
screens take precedence over temperature. Returning from service rechecks
the current sensor setup before restoring temperature or a diagnostic.

The following two-channel display/control rules are future scope. Channel
identity will use an upper square for channel 1 and a lower square for
channel 2, not a numeric channel label.

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
- Real and fake builds with EEPROM configuration route button events to the
  assignment menu. RAM configuration test builds retain state-only polling.
  No button event enables heaters.

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
