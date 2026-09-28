# Boiler Project History

This log records decisions, completed work, findings, errors, and important
observations. Release-oriented changes belong in `CHANGELOG.md`.

---

## 2026-09-11

### Technical specification

- Created the main working document `SPECIFICATION.md`.
- Fixed the target platform as ATmega16A, C11, with a 14.7456 MHz crystal.
- Fixed the AcroSched 2.2.0 cooperative-kernel integration.
- Fixed safe behavior for an empty or invalid EEPROM: show `E001`, disable
  heating, and enter the assignment flow through `Enter`.
- Fixed DS18B20 assignment by complete ROM code rather than bus discovery order.
- Fixed the startup service procedure using `Left + Right` and `Enter`.
- Fixed the mutually exclusive DS2480B and software 1-Wire configurations.
- Fixed the shared PD3/INT1 zero-cross input for the MOC3023S option.
- Fixed five-position dynamic indication: K1..K4 are display digits and K5 is
  the LED bank.
- Fixed the repository policy: AcroSched sources are excluded, while its
  binary library and public headers may be distributed with Boiler.
- Selected AVR-GCC and GNU Make as the canonical build basis. VS Code is the
  primary development interface; MPLAB X v6.35 and Atmel Studio are optional
  secondary environments, not the repository project format.

### Findings and constraints

- Dashes alone are not sufficient for a missing EEPROM configuration because
  they do not distinguish an error from an intermediate measurement state.
- The zero-cross detector produces the synchronization event on the falling
  edge at PD3: the optocoupler turns off before zero crossing, the pull-up
  raises the line, and the transistor turns on after zero crossing and pulls it
  low. The detector's transformer-related phase shift must be measured before
  phase control.
- User temperature setpoints and the emergency cutoff were fixed at a maximum
-  of 90 degrees Celsius. The installed-system maximum is now a stored
  configuration parameter with a firmware ceiling of 90 degrees Celsius.
  Hysteresis remains configurable from 0 to 50 degrees Celsius below the
  setpoint. The specific pipe data sheet and thermal overshoot behavior still
  require verification.
- Mode 2 now has a configurable maximum channel temperature difference;
  exceeding it disables both heaters and enters a latched mismatch error.
- User-interface behavior was defined for independent-channel, dependent-
  channel, and reserved modes. Numeric settings are edited as whole integer
  values. Long-press repetition uses four one-second steps, then a 500 ms
  accelerated interval, with 250 ms reserved for later usability testing.
- Mode 2 temperature mismatch behavior was fixed: exceeding the configured
  difference disables both heaters and latches the mismatch error.
- CPC1008N is the first output configuration for interface and control
  debugging. MOC3023S control and zero-cross power-stage work are deferred to
  the final hardware stage.
- Confirmed the CPC1008N input contract: PD4 and PD5 sink the input stages'
  cathodes directly; their anodes are supplied through current-limiting
  resistors. The relay input is ON at `DDRD=output, PORTD=0` and OFF at
  `DDRD=output, PORTD=1`.
- The firmware boundary ends at the CPC1008N input. The load-side power
  circuit is outside the board control contract.
- The temporary Russian requirements file was removed after its requirements
  were transferred to `SPECIFICATION.md`.

### Next work

- Resolve the open questions in section 11 of `SPECIFICATION.md`.
- Obtain the built AcroSched 2.2.0 library and record its integration options.
- Create the repository build files and integrate the locally built AcroSched
  library.
- Start BSP implementation after confirming the electrical levels.

## 2026-09-28

### Development stages

- Defined the staged implementation order: proportional CPC1008N control with
  one DS18B20 and software 1-Wire; addition of the selectable DS2480B driver;
  Modes 1 and 2 with the full two-sensor scope; then MOC30xx optotriac support
  and power-stage review.
- Defined Mode 3 as the initial single-channel mode for the first heating
  channel and Mode 4 as the sensor-configuration service mode. Mode 4 assigns
  DS18B20 ROM codes to physical locations and stores the confirmed assignment
  in EEPROM.
- Corrected the AcroSched description: it is built locally from its external
  source project with the same AVR-GCC toolchain as Boiler.
