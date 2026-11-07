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

## 2026-11-03

### Dynamic indication

- Implemented the display module in `src/display.c` and `inc/display.h`.
  A cooperative task scans K1..K5 once per system tick: approximately 1 ms
  per slot and 5 ms per complete frame. Tasks must remain short and
  non-blocking to maintain this timing.
- Kept the frame buffer separate from the physical port driver. The BSP
  disables all five selections before writing the active-high PA0..PA7 bus
  and enabling the next position. The generated AVR instructions were
  checked to confirm this write order.
- Initially used separate digit and LED fields, then replaced them with one
  `slots[DISPLAY_SLOTS]` array in `SDisplayFrame_t`. Four segment bytes in
  K1..K4 order are followed by the LED byte for K5; scanning now reads the
  next array element without a separate LED branch.
- Added `display_set_frame()` with pending and active frames. Updates latch
  together at K1 so one scan never mixes two frames. The API is used only
  from startup or cooperative tasks, not interrupt handlers.
- Defined segment glyphs with A..G on bits 0..6 and the decimal point on
  bit 7. On K5, bits 0..7 drive the LEDs on PA0..PA7 directly.
- Removed the redundant `Boiler` prefix from the display module's file names,
  types, functions, and macros. Short identifiers are preferred; glyph enum
  assignments are visually aligned at `=`.
- Startup now displays `E001` for invalid EEPROM configuration and `----`
  while the remaining application is not implemented. Both heaters remain
  disabled.

### Task descriptors and error results

- Added `inc/tasks.h`: scheduler descriptors use `ETask_t`, starting with
  `eTaskDisplay`; dispatch priorities use named macros rather than literals.
- Added `inc/errors.h` with separate operation results (`EResult_t`) and
  application display codes (`EError_t`, E001..E009).
- Converted public storage and BSP EEPROM operations from Boolean results to
  `EResult_t`. Success is `eResultOk = 0`, so callers compare explicitly
  rather than testing the result as a Boolean. Predicate functions retain
  Boolean results; AcroSched retains its library status type.
- Replaced the initial generic `eResultError` with specific failure causes:
  invalid arguments, ranges outside EEPROM, no valid configuration,
  backend read/write failures, and record verification failures.
- Storage now propagates backend failure causes without discarding them.
  Saving stops if the initial EEPROM scan fails; finding no valid record
  still permits saving a new configuration. A failed scan is not marked
  as completed, so subsequent save attempts must retry it.
- AVR EEPROM primitives do not provide hardware read/write failure status.
  The BSP validates arguments and ranges; storage verifies record readback.
- Added `display_show_error()` to format E001..E009 while preserving the
  pending LED byte. Zero or unknown codes return `eResultInvalidArgument`
  without changing the pending frame. Segment encoding remains in the
  display module; a separate error-handler module was considered unnecessary.

### Verification and remaining work

- Added host tests under `tests/`, run with `make test`. They cover scan
  order, all 256 bus patterns, frame-boundary updates, reinitialization,
  startup screens, all nine error screens, rejected error codes, and LED
  preservation.
- Added storage tests for save/load results, invalid arguments, empty or
  invalid records, capacity limits, backend read/write failures, failed
  verification, and preventing writes after an initial scan failure.
- Host tests and a full AVR rebuild (`make -B all`) passed. The firmware
  uses 2304 bytes of program memory and 209 bytes of static RAM.
- Hardware checks remain: K1..K5 order, polarity, absence of ghosting, and
  brightness/flicker. LED role assignment/blinking, buzzer feedback, sensor
  service screens, and temperature formatting are not implemented yet.

### Documentation organization

- Moved implementation details and engineering decisions from `README.md`
  into this dated history entry. The README is reserved for the project
  overview and user-facing information; changes and decisions belong here,
  with release-oriented summaries retained in `CHANGELOG.md`.

### MPLAB build integration

- Updated `.vscode/Boiler.mplab.json` for AVR-GCC 7.3.0: project-relative
  include/library paths, `-Os`, and disabled packed structures, short enums,
  unsigned char, and unsigned bitfields to preserve the AcroSched ABI.
  Recursive file sets include display, storage, and the new headers.
- Completed storage renaming in the implementation, application, and tests:
  `storage.h` and `storageInit/Get/Save` now agree.
- Fixed binary-directory resolution in Toolchain Support for MPLAB 1.11.2.
  It resolved a relative path against the wrong directory, losing compiler,
  objcopy, and objdump paths for the nested AVR installation.
  Changed the installed bundle's calculation from
  `c.relative(it.compilerBaseFolder,it.compilerBinFolder)` to
  `c.relative(Me,it.compilerBinFolder)`, where `Me` is the scan root.
  The bundle is `microchip.toolchains-1.11.2/dist/extension.js`; its original
  is backed up in the session files directory. Extension updates can
  overwrite this local correction.
- The compiler installation is unchanged. MPLAB generates CMake normally;
  all generated `cmake/` files are ignored, with no custom CMake hook or
  additional toolchain registration required.
- Verified JavaScript syntax, standard/nested installation paths, and the
  full IDE build after reloading VS Code. A clean native preset also passed
  without manual compiler or image-tool overrides:

  ```sh
  cmake --fresh --preset Boiler_boiler_conf -S output/cmake/Boiler/boiler
  cmake --build output/_build/Boiler/boiler
  ```

  Program memory is 2288 bytes; static RAM is 209 bytes.

### MPLAB generated directories

- Set `MPLAB_BUILD_ROOT=output` in the project's user variables. A local
  correction to `microchip.runcmake-1.11.0/dist/extension.js` passes
  `projectCMakefilesBaseDir` and `buildBaseDir` to the shared build-tree
  factory, placing generated files in `output/cmake/` and `output/_build/`.
  Projects without this variable retain their default layout.
- Updated the editor's compile-commands directory. Firmware images remain
  in `output/mplab/`; generated directories no longer occupy the project
  root. Verified IDE generation and a clean native preset build at the new
  paths using the commands above.
- The CMake Runner correction, like the toolchain correction, is local to
  the installed extension and may be overwritten by an extension update.

## 2026-11-04

### Startup sound and HL4 indication

- Confirmed on hardware that E001 displays correctly after correcting
  insufficient supply power; the segment and digit-select polarities stand.
- Confirmed the active, self-oscillating buzzer on PC2 is active high.
  Added a single non-blocking 100 ms startup beep after task registration,
  silenced on expiry or fatal scheduler failure.
- HL4 maps to segment D / PA3 in K5. It is continuously on in the
  no-error state, or blinks 500 ms on / 500 ms off while an error is active.
  Error-state transitions restart the ON phase without repeating the beep.
- Added a cooperative feedback task using elapsed atomic tick snapshots,
  including tick wrap handling. Masked LED updates preserve other LED bits
  and digits and continue to latch at the display frame boundary.
- A valid EEPROM currently selects no-error indication, not authorization
  to heat: heater outputs remain disabled until application checks exist.
- Display/feedback and storage host tests pass, including beep duration,
  blink phases, state transitions and tick wrap. The complete Make AVR build
  uses 2700 bytes of program memory and 216 bytes of static RAM.
- Hardware verification: confirmed a short startup beep and
  blinking HL4 after flashing.

## 2026-11-05

### Mode 3 temperature interface and automatic fake sweep

- Agreed and implemented the first temperature screen for Mode 3, without
  heater control or channel switching. It reads the complete ROM assigned to
  SENSOR_1, including a single-channel profile assigned to the second fixture.
  Existing profile checks remain: P2 still requires both assigned devices;
  extra unassigned devices are still errors.
- Added adaptive right-aligned formatting across -55..125 C. Round to tenths
  first and show one decimal place within -9.9..99.9; outside that interval,
  round the original reading to whole degrees. Halfway values round away
  from zero. Leading zeros are blank, negative readings retain the minus
  sign, and integer values within the fractional range retain their .0.
  Formatting preserves all LED bits and frame-boundary latching.
- HL7 is segment C / PA2 / bit 2 in K5. It is steadily on for configured
  Mode 3, including sensor diagnostics, and off during assignment or E001.
  HL4 retains its existing steady/error-blink behavior.
- Ordinary temperature-screen buttons have no action. Left+Right at startup,
  Enter at E001 and all existing assignment-menu controls are unchanged.
  Reserved the upper/lower square channel markers for future two-channel UI;
  no channel digit or marker is displayed in this single-channel stage.
- Fake-enabled firmware explicitly starts both fixtures at -55.0 C, increases
  them by 0.1 C every 500 ms through +125.0 C, then restarts at -55.0 C.
  The 1801-sample cycle lasts 900.5 seconds, with each endpoint held for
  500 ms. A tenths accumulator is converted to the nearest 1/16 C reading
  without cumulative quantization drift. Timing handles tick wrap, late
  polls and partial intervals. Disconnected fixtures advance too.
- The sweep continues while assignment owns the display; save/apply does not
  reset temperatures or presence. Returning from service rechecks the current
  assignments before showing a temperature or diagnostic, avoiding a fatal
  read of a sensor disconnected during service. A regression also verifies
  immediate temperature restoration after cancellation.
- Low-level fake initialization still supplies the original 25/30 C fixtures
  for deterministic tests; firmware explicitly opts into the sweep. No EEPROM
  layout or stored settings changed, no temperature is written to EEPROM,
  and both heater outputs remain off. Extended measurement display range
  does not alter water-temperature safety limits.
- Verification: `make test` passed all C suites and 26 Python tests, including
  all 2881 representable DS18B20 readings, rounding/format boundaries, LED
  preservation, rejected out-of-range values and frame-boundary updates.
  Sweep tests cover every 0.1 C sample, both endpoints, wrap/restart,
  disconnected devices and delayed polls; startup tests verify complete
  displayed cycles for both profiles and all four initial presence masks.
  Menu tests cover ROM-based readings, inactive ordinary buttons, diagnostic
  recovery, service display priority and save/cancel/reload behavior.
  Focused display, fake-sensor and menu suites also passed with
  AddressSanitizer/UndefinedBehaviorSanitizer.
- AVR verification passed:
  `make -B all` (3568 bytes Flash / 254 bytes static RAM);
  `make -B all BUILD_DIR=output/test-config FAKE_SENSORS=1 TEST_CONFIG=1
  TEST_PROFILE=1 FAKE_PRESENT=1` (4922 bytes Flash / 278 bytes static RAM);
  `make -B all BUILD_DIR=output/menu-test FAKE_SENSORS=1 TEST_CONFIG=0`
  (8582 bytes Flash / 426 bytes static RAM).
- Hardware verification:  confirmed the correct mode LED (HL7),
  correct temperature display across -55..125 C and blank leading zeros.

### Startup service re-entry

- Implemented the agreed Left+Right startup gesture: hold before reset/power-on,
  pass normal debounce and hold another 3 s. It opens service with a valid
  record or from any diagnostic, including E004, without erasing EEPROM.
  Early release cancels the attempt; normal display/button tasks keep running.
- Cancellation preserves old assignments and restores their idle/error screen;
  changing fake presence can then produce a new ordinary diagnostic. Service
  owns the display until the saved configuration has actually been applied.
- Fixed menu bookkeeping after application: read the actual fake presence,
  rather than replacing its UI mask with the build's startup default.
  Applying configuration does not reinitialize or reconnect fake devices.
- Added regressions for P1 assigned to either fixture, startup holds at exact
  boundaries and across tick wrap, early release, re-entry from valid/E002/
  E003/E004 states, cancellation and reassignment with old records present.
  The reported immediate E004 after P1/first-fixture application has not been
  reproduced in end-to-end host tests; hardware rechecking remains necessary.
- Verification: `make test` passed, including original regression suites,
  exact 20 ms + 3 s startup timing, cancelled/late gestures and application
  on a sensor-monitor tick boundary. Focused ASan/UBSan tests also passed.
  Normal, RAM-test and menu AVR builds passed; menu firmware uses 7798 bytes
  Flash / 426 bytes static RAM with AVR-GCC 5.4.0. The installed MPLAB
  AVR-GCC 7.3.0 also built it successfully (7768 bytes Flash / 426 bytes RAM).
- A temporary standalone diagnostic, kept outside project sources, confirmed
  hardware button masks: idle 00, Left 08, Right 02, both 10. Reset through
  the programmer with both held showed startup/current masks 10/10.
  Found an Enter button fault requiring repair. Menu re-entry and
  the immediate E004 report still require retesting with the main image.
- Main-firmware diagnostics captured an initial raw mask of 15, followed by
  the correct debounced Left+Right mask of 10. The exact-match startup gate
  had already disarmed service entry. Initial eligibility now requires both
  chord bits but tolerates extra active inputs while levels settle; the
  existing debounce/hold checks still require the exact combination.
  Added regressions for every initial superset, including the observed 15,
  and for rejection of persistent extra keys. Removed temporary diagnostics.
  Verification: `make test` passed all C suites and 26 Python tests; normal,
  RAM-test and menu AVR builds passed. Menu firmware uses 7800 bytes Flash /
  426 bytes static RAM with AVR-GCC 5.4.0.
  Hardware verification:  confirmed that startup Left+Right now
  opens profile selection. A subsequent board test confirmed successful
  configuration/application with four dashes displayed. The earlier immediate
  E004 did not recur in this test; its original cause has not been established.

### E001 sensor assignment menu

- Removed donE and its extra Enter confirmation. Enter on SAVE now starts
  the cooperative transaction; final verification leads directly to applying
  the record and restoring the idle/diagnostic screen without reset or beep.
  Keys stay locked through the handoff; write failures retain retry/cancel.
  Updated regressions for automatic completion, held Enter, changed presence
  during saving and all sensor-monitor phases.
  Verification: `make test` passed all C suites and 26 Python tests.
  `make -B all BUILD_DIR=output/menu-test FAKE_SENSORS=1 TEST_CONFIG=0
  FAKE_PRESENT=1` passed (7776 bytes Flash / 420 bytes static RAM).
- Hardware verification:  clarified the actual build command:
  `make -B all BUILD_DIR=output/menu-test FAKE_SENSORS=1 TEST_CONFIG=0`.
  Without an override, FAKE_PRESENT defaults to 3 (both devices).
  P2 assignment produced four dashes both after application and power cycling.
  Startup service opened at P1 with both presence points: the menu initializes
  a new candidate at P1, independently of the saved P2 record; entering or
  cancelling service does not overwrite that record. No build/runtime
  mismatch is indicated by these results.
- Replaced the CHCK prompt with CONF (confirm), avoiding the seven-segment
  K approximation. Verification and confirmation behavior are unchanged;
  updated the menu test and user/specification documentation.
- Restored the copyright line in all project-owned C source/header file
  headers, including tests/stubs, and documented ESaveStage with @enum/@brief.
- Added the agreed Enter-only entry from E001, profile selection with Up/Down,
  S1/S2 assignment, CHCK, explicit SAVE and donE confirmation. Assignment
  changes remain in RAM until the entire procedure succeeds and is confirmed.
- Right cycles fake device presence; decimal points in the first two digits
  indicate the two fixtures. ROM matching, not fixture/discovery order,
  defines locations. Left cancels before writing; Enter retries failed steps.
- Added cooperative EEPROM saving: readiness checks, at most one byte per
  poll, payload verification, commit and final readback. The existing blocking
  save API remains available for startup/tests and rejects concurrent saves.
  Keys are locked during writing; specific save errors remain available through
  the menu result API, with E001 and retry/cancel on screen.
- Ordinary diagnostics yield the display during service. After donE, Enter
  applies the verified EEPROM cache without an MCU reset or another beep.
  All heater outputs stay off. RAM-only test builds retain their no-EEPROM
  behavior; the menu requires fake sensors with CONFIG_USE_TEST=0.
- Applying a saved configuration runs in the service task rather than the
  button callback. The EEPROM format, startup scan, slot selection rules and
  existing sensor/button representations remain unchanged.
- Verification: `make test` and all three AVR builds passed: `make -B all`,
  `make -B all BUILD_DIR=output/test-config FAKE_SENSORS=1 TEST_CONFIG=1`,
  and `make -B all BUILD_DIR=output/menu-test FAKE_SENSORS=1 TEST_CONFIG=0
  OPT_FLAGS='-Os -fstack-usage'`. Tests exercise actual debounced button
  events, both profiles, prompt/error/decimal-point frames, held Enter,
  cancellation, duplicate ROMs, pre-save rechecks, all 42 byte-write failure
  points, failed readback, corrupted payload/commit, retry, preservation of a
  previous record, application without another beep and EEPROM reload on boot.
  The focused menu suite also passed AddressSanitizer/UndefinedBehaviorSanitizer,
  including rejection of Enter chords.
- Menu firmware uses 7358 bytes Flash and 420 bytes static RAM, leaving
  604 bytes of the ATmega16A's 1024-byte SRAM for stack. Compiler stack-usage
  output and scheduler callback frames were inspected. Normal firmware uses
  3568 bytes Flash / 254 bytes static RAM; RAM-test firmware remains at
  4194 bytes / 273 bytes. Hardware and MPLAB menu verification are pending.

### Configuration and assigned-sensor diagnostics

- Added application-level profile/ROM validation above the unchanged EEPROM
  storage format: profiles 1/2, DS18B20 family, Dallas CRC and unique required
  assignments. Invalid assignments produce E001; application settings remain
  opaque until control logic exists.
- Added an explicit RAM-only test configuration instead of writing fixture data
  to EEPROM. CONFIG_USE_TEST requires the fake sensor backend and bypasses all
  EEPROM calls. Test profile and startup fixture presence are selectable through
  Make options or MPLAB compiler macros.
- Fake-enabled startup and a 100 ms diagnostic task check complete assigned
  ROMs. Missing first/required second devices produce E002/E003; a second
  device is not required for a single-channel profile. Missing assignments
  take precedence over unknown extras (E004); both missing produces E002.
- Recovery clears the display to dashes while preserving other LED bits,
  returns HL4 to steady light and does not restart the beep. Unexpected
  backend operation failures use the existing fatal-handler pattern.
  All heaters remain off; the normal build has no real bus checks yet.
- Added a user-facing test-build command and expected display table in README.
  Sensor configuration menus and persistent assignment remain subsequent work.
- Verification: `make test`, `make -B all`, and
  `make -B all BUILD_DIR=output/test-config FAKE_SENSORS=1 TEST_CONFIG=1
  FAKE_PRESENT=1` passed. Tests cover invalid profiles/ROMs, duplicate
  assignments, swapped ROM locations, both profiles with all four presence
  masks, E002/E003/E004, recovery, LED phases, no repeated beep, untouched
  outputs on operation failure and zero EEPROM calls in RAM mode.
  Normal firmware: 3538 bytes Flash / 252 bytes static RAM; RAM test
  firmware: 4194 bytes Flash / 273 bytes static RAM.
- Hardware verification:  confirmed Make test builds with
  FAKE_PRESENT=1 show E003, FAKE_PRESENT=2 show E002, and FAKE_PRESENT=3
  show four dashes with HL4 continuously on. MPLAB test-mode verification
  remains pending.

### Fake sensor backend

- Added a sensor discovery/read interface and an explicitly enabled fake
  implementation (`SENSORS_USE_FAKE=1`, disabled by default).
- Two fixed, distinct DS18B20 ROMs have valid Dallas CRC bytes. Devices
  start present at 25 and 30 C; fixture controls independently change
  presence and temperature (-55..125 C, signed 1/16 degree units).
- Reads match the entire ROM, not discovery order. Missing or unknown
  devices return the new operation result `eResultNotFound`; use before
  initialization returns `eResultNotReady`. Existing result values and
  E001..E009 application codes are unchanged.
- The module performs no bus, EEPROM or heater access. Firmware startup,
  configuration checks and displayed errors remain unchanged; E002/E003
  integration and the assignment menu are subsequent steps.
- The fake is immediate, without conversion timing or bus/CRC fault
  simulation. Real asynchronous conversion handling remains future work.
- Verification: `make test` and `make -B all` passed. Tests cover all four
  presence combinations, ROM CRCs, complete-ROM matching, every raw
  temperature in range, bounds, initialization/reset and unchanged outputs
  on failure. The enabled fake also compiles with AVR-GCC using
  `avr-gcc -std=c11 -Wall -Wextra -Werror -mmcu=atmega16a -Iinc -Os
  -DSENSORS_USE_FAKE=1 -c src/sensors_fake.c -o output/tests/sensors_fake_avr.o`.
  Default firmware remains at 3134 bytes Flash and 252 bytes static RAM.

### Button input

- Added active-low PC3..PC7 reading with existing internal pull-ups,
  independent 20 ms press/release debounce, and a 1 ms polling task.
- Added synchronous press/release events and Up/Down repeats. Replaced
  the original four slow changes with one initial press, a 3 s
  pause, then repeats every 500 ms starting exactly at the 3 s boundary.
- Raw release suspends repeats before release debounce completes. Timing
  uses elapsed atomic tick snapshots and handles tick wrap; delayed polls
  emit at most one repeat per button, not a burst.
- The driver supports an optional non-blocking event handler and a pressed
  state mask. Startup currently uses state-only polling, as agreed: menus,
  sensor assignment, numeric editing and heater control are not added here.
- Verified `make test` and `make -B all`: button, display/startup and storage
  tests plus 26 existing Python tests pass. Button tests cover all 32 input
  combinations, bounce, exact debounce/hold/repeat boundaries, independent
  simultaneous buttons, release, reinitialization, delayed polls and holds
  spanning tick wrap. Firmware uses 3134 bytes Flash and 252 bytes static RAM.
- MPLAB's recursive file set already covers the new module; regenerate its
  CMake files before building so the source list includes `src/buttons.c`.
  Hardware button verification remains pending.

### GitHub Actions and pull requests

- Adapted the copied development version workflow to this project: `VERSION`
  starts at `0.1.0` and receives one patch increment per merged PR. Removed
  AcroSched-specific macros and paths. The initial VERSION-only implementation
  was superseded by the transactional release-script integration below.
- Retained duplicate-run detection, push-conflict retries and `[skip ci]`
  handling. Repository
  branch rules must permit the workflow token (or `VERSION_BUMP_TOKEN`) to push.
- Added CI for host tests and a complete ATmega16A AVR build, with firmware,
  map and version artifacts. The distributed AcroSched binary is linked as-is;
  its source is not required by CI.
- Retained the API-only PR commit-summary workflow and its marked section;
  PR code is not checked out by this write-enabled workflow.
- Cleaned the English PR template: removed scheduler/port/preemptive-suite
  specifics and the fixed host OS, and added project build/test commands,
  EEPROM compatibility, memory usage and explicit hardware-test results.
- Verification: `make test` and `make -B all` passed with AVR-GCC 5.4.0
  (2700 bytes Flash, 216 bytes static RAM). Workflow YAML and embedded Python
  syntax checks passed. Isolated Git tests verified patch increments,
  VERSION-only commits, duplicate-run detection, skip markers, rejection of
  malformed versions and retry after a real concurrent push. Mocked GitHub
  API tests verified summary preservation, escaping, idempotency and marker
  validation. Hosted Actions execution remains to be checked after pushing.

### Transactional release script

- Adapted the supplied `make_release.py`: synchronize `VERSION` and Doxygen
  `@version` headers in tracked project-owned files under `inc`, `src`, `bsp`
  and `tests`. Select ownership by `@authors Anton Chernov` so test files and
  host stubs without copyright lines are included.
- Excluded the distributed AcroSched library, archived code, documentation
  dates and EEPROM format versions. Retained exclusive backups, byte-for-byte
  write verification, explicit failures and rollback on build/test errors.
- Replaced the external scheduler build targets with root `make -B all test`.
  The merge workflow calls the same script with `--skip-build` and stages
  `VERSION` plus the updated project headers. No release was performed merely
  to adapt the script; existing versions remain until the next release.
- Added 12 release-script tests to `make test`: source selection, preserved
  content/line endings, malformed headers/versions, symlinks, backup collisions,
  readback failures, rollback and CLI/build behavior. All passed, alongside
  the existing C tests and `make -B all`.
- Verified a real manual release with full firmware/test verification in an
  isolated clone, then ran the embedded workflow against a local bare remote:
  it committed all 16 project headers and `VERSION`, left `lib` untouched and
  skipped a repeated event without another commit. Workflow syntax checks passed.
- Captured expected rollback-test diagnostics so deliberate build/readback
  failures no longer look like failed `make test` runs. The build-failure test
  runs the real release CLI and checks its nonzero exit, error reporting and
  complete file restoration; production error output remains unchanged.

### Version updates on push

- Replaced the PR-closed trigger with push to `development`, covering direct
  pushes and PR merges without duplicate increments. Each push increments
  patch once, independent of the number of commits.
- Retained `[skip ci]` checks for pushed commit messages and merged PR
  titles/bodies. Query associated PRs through the GitHub API with explicit
  pull-request read permission; API errors abort the update.
- Key duplicate-run detection by pushed SHA instead of PR number. Checkout
  the event SHA, retry against current development after push conflicts, and
  require the event commit to remain in development history before bumping.
  Automatic version commits retain `[skip ci]` to prevent recursion.
- CI and version updates run independently; direct pushes are not implicitly
  gated on build/test success.
- Added push-workflow integration tests using temporary Git repositories and a
  local GitHub CLI stub: direct/merge/multi-commit pushes, PR and commit skip
  markers, unrelated PRs, API errors, duplicate events, recursion prevention,
  branch creation and rejected-push retries. `make test` passed with both C
  suites and all 22 Python tests; YAML, embedded Python and trigger/permission
  checks passed. Hosted push execution remains to be verified after publishing.
- Fixed a hosted failure after rewriting the branch tip: the previous SHA was
  absent from the runner clone, making `git rev-list before..after` invalid
  despite a full fetch. Select pushed commits and messages from the event
  payload instead, retaining skip checks without requiring the old object.
  Use the event head for empty commit lists, reject missing head data explicitly,
  and skip branch-deletion events.
- Regression verification passed: both C suites and all 26 Python tests,
  including a real force push followed by a fresh clone where the previous
  SHA is demonstrably absent. Skip handling, empty/malformed event data and
  workflow YAML/Python syntax checks also passed.

## 2026-11-07

### Real DS18B20 through software 1-Wire

- Connected a three-wire DS18B20: separate VDD, common ground and
  DQ on PD2 with an external pull-up. Implemented the direct backend as the
  normal build; fake backends remain explicitly selectable. Heating remains
  disabled, and the sensor code neither changes heater GPIO nor calls an
  output-enable function.
- Added standard-speed software open-drain primitives in `bsp/onewire.c`.
  PD2's output latch stays low; DDR selects output-low or released input.
  Other PORTD/DDRD bits are preserved. Reset checks idle level, presence
  and recovery; stuck-low conditions are explicit bus failures.
- Used 70 us slots: Write 1 low for 6 us, Write 0 low for 60 us, and Read
  low for 3 us then sample after another 8 us. Reset is 480 us low,
  presence sampled 70 us after release and 410 us recovery. Interrupt state
  is preserved, with only timing-critical intervals masked (at most about
  70 us). Inspected generated AVR instructions: GPIO changes use PD2-only
  SBI/CBI and the read sample is within the required 15 us window.
  Actual electrical timing and display smoothness still need board testing.
- Added a cooperative DS18B20 acquisition state machine. Each 1 ms task
  invocation performs at most one reset, byte transfer or Search ROM
  triplet. ROM search handles collisions, checks Dallas CRC-8 and detects
  more than two devices. Shared the CRC helper with configuration validation.
- Match ROM selects each DS18B20 for power checks and scratchpad reads.
  Externally powered devices convert together using Skip ROM / Convert T;
  conversion waits are non-blocking, with completion checked after 750 ticks
  and an explicit timeout if still busy at 800 ticks. Parasite power is
  detected and rejected; no strong pull-up or sensor EEPROM write is added.
- Scratchpads require a valid nine-byte CRC and configuration; temperatures
  are normalized for 9..12-bit resolution and checked against -55..125 C.
  CRC-valid all-zero data is rejected. No power-on +85 C reading is published
  before a requested conversion completes; legitimate converted +85 C is
  accepted.
- Published readings are coherent cached snapshots addressed by full ROM,
  never search order. Initial acquisition and service refresh return Busy
  without modifying caller outputs. A periodic scan retains its previous
  checked snapshot until the next complete result; acquisition failure
  invalidates it. A 100 ms retry/scan pause gives approximately one-second
  normal updates. The initial valid-config screen shows dashes while waiting.
- Enabled normal-build diagnostics and the existing assignment service with
  real sensors. Enter at S1/S2, CONF and SAVE requests a fresh acquisition;
  the prompt remains visible while waiting. Right and fake-presence points
  are disabled on real hardware. Failed checks remain retryable/cancellable;
  old assignments survive until explicit SAVE succeeds. Unsupported sensor
  families cannot be assigned. EEPROM layout and format remain unchanged.
- Missing assigned devices retain E002/E003 and extras E004. ROM/scratchpad
  CRC failures show E005; bus, invalid-data, power and conversion-timeout
  failures show E008 instead of halting. Recovery restores temperature and
  steady HL4 without another beep. HL7 and adaptive formatting are unchanged.
- Verification: `make test` passed all C suites and 26 Python tests. The new
  bit-level bus emulator exercises colliding ROM searches, empty/one/two/
  excess devices, full-ROM addressing, all 2881 representable temperatures,
  resolution masking, CRC failures, stuck-low/lost bus, parasite power,
  invalid data, exact conversion/timeout boundaries and tick wrap.
  Real-firmware integration tests cover E001 entry, startup Left+Right,
  fresh asynchronous assignment/check/save, automatic application, EEPROM
  reload, temperature frames, disconnection/reconnection, E004/E005/E008,
  unsupported families, retry/cancel and old-record preservation.
  The new real-backend/integration suite also passed ASan/UBSan.
- AVR builds passed with AVR-GCC 5.4.0:
  `make -B all` and `make -B all BUILD_DIR=output/real-sensor`
  (9646 bytes Flash / 470 bytes static RAM);
  `make -B all BUILD_DIR=output/menu-test FAKE_SENSORS=1 TEST_CONFIG=0`
  (8550 bytes Flash / 426 bytes static RAM);
  `make -B all BUILD_DIR=output/test-config FAKE_SENSORS=1 TEST_CONFIG=1
  TEST_PROFILE=1 FAKE_PRESENT=1`
  (4940 bytes Flash / 278 bytes static RAM).
  Also built the real image with `OPT_FLAGS='-Os -fstack-usage'` and inspected
  callback reports; 554 bytes of SRAM remain beyond static allocations.
  The linked real image contains neither fake controls nor heater-on code.
- Updated the specification and board-test instructions. MPLAB's recursive
  source sets already include the new modules; regenerate CMake and remove
  fake/RAM macros before its real build. Board verification of the new driver
  and MPLAB compilation are pending. Git branches were not changed.

### Bench feedback: 12-bit resolution and documentation

- The real-sensor firmware ran overnight on the board. Temperature remained
  visible and rose when the sensor was warmed by hand. Observed
  subtle display jitter and only 0/3/5/8 in the tenths position. Those digits
  are consistent with 10-bit, 0.25 C readings; the previous driver preserved
  the sensor's existing resolution, which was not independently measured.
- Agreed to configure 12-bit resolution explicitly, but not to change display
  architecture yet. Cooperative task priorities cannot interrupt a running
  bus operation: a reset occupies about 960 us and a byte about 560 us.
  Two sensors add addressed transfers rather than double one operation's
  duration. Slot-level dispatch and timer-driven 1-Wire were discussed as
  alternatives; no scheduling change or display ISR was implemented.
  Display jitter remains a known, unresolved board limitation.
- Added per-device scratchpad setup before broadcast conversion. CRC and
  fixed configuration bits are validated; lower resolutions are changed
  to `0x7F` using Write Scratchpad while preserving TH/TL. A fresh Match ROM /
  Read Scratchpad verifies the configuration and both alarm bytes.
  Already configured 12-bit devices are not rewritten. Setup is checked
  every scan; no Copy Scratchpad command or sensor EEPROM write is used.
  Readback mismatches and unexpected final resolution report E008; bad CRC
  retains E005. Failed setup does not publish a temperature or loop forever.
- Restored section/function separators and completed the new driver's macro,
  enum, structure, helper and public-API documentation. Public declarations
  carry full Doxygen documentation without duplicated definition comments.
  Replaced advisory `@details` with `@note` in project headers, documented
  affected enumerators and corrected parameter/return descriptions.
  Doxygen syntax and documented-parameter checks passed without warnings.
- Extended the bus emulator with volatile Write Scratchpad and pre-conversion
  reads. Tests verify 9/10/11-bit upgrades, unchanged 12-bit devices, two-device
  addressing and alarm preservation, ignored writes, corrupted alarm/CRC
  readback, recovery and rejected lower-resolution conversion results.
  Unsupported commands, including Copy Scratchpad, fail the emulator.
  Existing per-poll work limits, exact conversion/timeout boundaries, all
  2881 raw temperatures and real-firmware integration tests still pass.
- Verification: `make test` passed all C suites and 26 Python tests.
  The real-backend/integration suite passed ASan/UBSan.
  AVR builds passed:
  `make -B all` and `make -B all BUILD_DIR=output/real-sensor`
  (9928 bytes Flash / 478 bytes static RAM);
  `make -B all BUILD_DIR=output/menu-test FAKE_SENSORS=1 TEST_CONFIG=0`
  (8550 bytes Flash / 426 bytes static RAM);
  `make -B all BUILD_DIR=output/test-config FAKE_SENSORS=1 TEST_CONFIG=1
  TEST_PROFILE=1 FAKE_PRESENT=1`
  (4940 bytes Flash / 278 bytes static RAM).
  The 12-bit revision still needs board verification.
- Updated the specification and user-facing bench instructions. Heating
  remains disabled; Git branches were not changed and no commit was created.

### Driver initialization invariant and expression cleanup

- Removed the separate `initialized` flag entirely: startup already completes
  sensor initialization before registering scheduler tasks. Update and refresh
  explicitly require prior initialization; no equivalent check was added
  to their callers.
- Initialized the existing snapshot result to NotReady. Search/read retain
  their pre-initialization error, argument validation and unchanged outputs
  through the normal snapshot-state check, without a second flag check.
  The fake backend's controls and behavior are unchanged.
- Removed redundant parentheses around comparisons joined by logical
  operators, the real-backend build condition and the shifted temperature
  byte. Kept required grouping for bit-mask comparisons and mixed AND/OR
  conditions. Physical bus checks, CRC validation, casts, slot timing,
  scheduler priorities and display architecture were not changed.
- Extended real-bus tests to assert initialization before any bus transfer
  and before scheduler registration, including after firmware reset.
  Pre-initialization tests verify invalid arguments, NotReady, unchanged
  ROM/count/temperature outputs and no bus activity. The focused protocol
  and real-firmware suite passed; final tests also passed ASan/UBSan.
- AVR builds passed:
  `make -B all` and `make -B all BUILD_DIR=output/real-sensor`
  (9890 bytes Flash / 477 bytes static RAM);
  `make -B all BUILD_DIR=output/test-config FAKE_SENSORS=1 TEST_CONFIG=1
  TEST_PROFILE=1 FAKE_PRESENT=1`
  (4940 bytes Flash / 278 bytes static RAM).
  The physical 1-Wire object is byte-identical to the previous build,
  confirming unchanged generated timing code. Whitespace and 85-column
  checks passed. Heating remains disabled and Git branches were not changed.

### Board verification and display scheduling constraint

- Board testing confirmed visibly finer readings with the verified 12-bit
  sensor configuration. Raising display priority to 1 gave no perceptible
  improvement: subtle display jitter remains roughly once per second.
- Software `_delay_us` waits keep the cooperative sensor task running even
  where interrupts are enabled. These waits, rather than display-module
  placement, are the blocking work to address in the next driver design.
- Rejected moving `display_scan`, fully or partly, into BSP/interrupt handling.
  Digit selection belongs to the display module as a scheduler task; BSP
  only writes GPIO. Scan frequency must remain controlled by the scheduler
  period, without an interrupt-specific divider or condition.
- The next direction to discuss is asynchronous, timer-timed 1-Wire,
  returning control to the scheduler while wire phases are pending.
  No new timing engine, display ISR or priority change was implemented.

### Configured-priority checks in scheduler test substitutes

- Fixed a startup-test failure after display priority was raised to 1.
  The configuration-startup, assignment-menu and real 1-Wire scheduler
  substitutes still incorrectly required every priority argument to be zero.
- Added a shared test-only table indexed by task descriptor, using the
  configured `TASK_PRIORITY_*` constants. All three substitutes check
  descriptor bounds before comparing the supplied priority to that table.
  Priority validation is retained; firmware priorities are not changed.
- `make test` passed all C suites, all eight RAM startup profile/presence
  combinations and all 26 Python tests. Git branches were not changed.

### Removing the temporary sensor backend

- Removed the firmware's temporary sensor backend and its public controls:
  `src/sensors_fake.c`, `inc/sensors_fake.h`, fixed readings, automatic sweep
  and simulated disconnection support. Removed the dependent RAM configuration
  mode and fixed assignment ROMs. Firmware now always loads EEPROM assignments
  and uses real software 1-Wire.
- Removed the obsolete build variables and compiler definitions:
  `FAKE_SENSORS`, `FAKE_PRESENT`, `TEST_CONFIG`, `TEST_PROFILE`,
  `SENSORS_USE_FAKE`, `SENSOR_FAKE_PRESENT_MASK`, `CONFIG_USE_TEST` and
  `CONFIG_TEST_PROFILE`. Removed their branches from startup, diagnostics
  and service; acquisition is polled every tick.
- Removed fixture-presence state and arguments from `menu_init`/`menu_open`,
  the Right-button simulation and presence decimal points. Real service
  behavior is retained: startup Left+Right, Enter from E001, physical sensor
  assignment, fresh acquisitions, cancellation and automatic application
  after verified SAVE. Right remains inactive.
- Removed suites specific to the deleted backend and RAM mode. Configuration
  and EEPROM/menu unit tests retain their coverage using a shared host-only
  sensor API substitute in `tests/sensor_stub.c`/`.h`, with explicit test
  connections and readings. It models Busy until the next test poll and is
  never part of AVR or MPLAB source sets. The separate bit-level emulator
  still executes the real driver, including discovery, CRC, resolution,
  conversion timing, temperature range and firmware integration.
- Updated menu unit tests for 1 ms polling and fixed readings without a sweep.
  Preserved EEPROM write/readback fault injection, old-record protection,
  startup hold/debounce boundaries and reassignment handoff checks.
  Added a Right-inactive assertion without replacing the held-Enter test.
- Updated README and specification to describe only the supported firmware
  and physical assignment procedure. MPLAB source globs already exclude
  tests; regenerate its source list after removing modules.
  Historical entries above intentionally retain the earlier test stages.
- Verification: `make test` passed all remaining C suites and 26 Python
  tests. Menu/EEPROM and real 1-Wire suites also passed ASan/UBSan.
  Doxygen documented-parameter/syntax, whitespace and source-width checks
  passed. No obsolete backend or RAM-mode references remain in active code,
  build configuration or current user/specification documentation.
- `make -B all` and `make -B all BUILD_DIR=output/real-sensor` passed
  (9804 bytes Flash / 476 bytes static RAM); their HEX images match.
  The linked image contains real 1-Wire and neither test substitutes nor
  heater-on code. The physical 1-Wire object remains byte-identical:
  timing, display scheduling and heater safety were not changed.
  EEPROM format and assignments are preserved. No branch change or commit
  was made during this cleanup.

### Agreed non-blocking software 1-Wire direction

- Agreed to use a hardware timer for long wire waits while returning control
  to the cooperative scheduler. Timer events must perform required bus
  transitions at their deadlines, not merely set a flag for a later
  millisecond task to change the line.
- The first stage may retain short critical software delays while making
  the 480 us Reset pulse and 410 us recovery non-blocking. Other intervals
  must be selected by their permitted timing error, not duration alone.
  Synchronous byte transfers remain another source of display latency.
- Display scanning stays in the display module as a scheduler task; timer
  interrupt work belongs to 1-Wire timing, not display scanning.
  A hardware 1-Wire master would handle physical protocol timing, but its
  host communication must also avoid blocking waits.
  No timer redesign was implemented in this indication change.

### Channel LEDs, mode-error indication and fixed temperature tenths

- Replaced planned on-display channel squares with HL3 for channel 1 and
  HL4 for channel 2. Channel LEDs are off on diagnostics, dashes and all
  service screens; Mode 3 still shows only the assigned SENSOR_1.
  Channel switching and heating remain outside this stage.
- Added the complete K5 LED mapping: HL1=A, HL2=F, HL3=E, HL4=D,
  HL5=B, HL6=G, HL7=C and HL8=DP. HL1/HL2 remain off.
- Moved error indication from HL4 to the current mode LED only, not all
  four mode LEDs. HL7 is steady in normal Mode 3 and blinks on errors,
  including E001 before configuration. Assignment uses HL8 for Mode 4:
  steady on normal prompts and blinking only for its displayed errors.
  The period remains one second with 500 ms ON and 500 ms OFF.
- Feedback owns all mode LED bits and preserves channel/heating bits.
  Added validated mode selection without another beep. Repeated temperature
  or error rendering does not restart blinking; mode/error transitions
  restart the ON phase. Startup initializes feedback before status rendering.
  Service exit restores normal indication even while acquisition is Busy
  or configuration is absent.
- Always display one decimal place throughout -55..125 C, including
  -55.0, -10.5, 100.0 and 125.0. Kept right alignment, blank leading zeros,
  the minus sign, rounding halfway values away from zero, invalid-input
  frame preservation and frame-boundary latching.
- Updated specification, README and API documentation. Extended tests
  over all 2881 valid raw temperatures, exact segment-to-LED mapping,
  all four modes, blink boundaries and wraparound, unchanged-state phase
  preservation, invalid modes, service errors/retries/cancellation,
  temperature extremes with channel LEDs and real-bus startup/handoff.
  Corrected one invalid-configuration fixture to keep its active diagnostic
  consistent with its configuration-loaded flag.
- Verification: `make test` passed six C suites and 26 Python tests.
  The final display, menu/EEPROM and real-firmware suites passed ASan/UBSan
  using non-PIE host binaries. An initial PIE sanitizer launch exited with
  SIGSEGV without a report; the debugger run and final non-PIE runs passed.
  Doxygen syntax/parameter, whitespace and 85-column source checks passed.
  `make -B all` passed (9864 bytes Flash / 477 bytes static RAM).
  The physical 1-Wire object is byte-identical and no heater-on code is
  linked. EEPROM, bus timing and scheduler priorities are unchanged.
  No branch change or commit was made.

### First mixed timer/software 1-Wire stage

- Assigned dedicated Timer0 to Reset's long waits, leaving Timer1's system
  tick and scheduler-owned display scanning unchanged. Timer0 uses CTC and
  /64 (approximately 4.34 us per counter step); OC0 is not connected to GPIO.
  The shared Timer0/Timer1 prescaler is not reset. Compare thresholds 112
  and 96 round up and include a guard step so the unknown prescaler phase
  cannot shorten the 480 us Reset low or 410 us recovery minimum.
- Changed `onewire_reset` to start/poll with Busy. The first timer interrupt
  releases PD2 and retains the verified 70 us software presence wait; the
  second checks recovery, stops Timer0 and leaves the result for polling.
  Reset deadlines do not depend on another millisecond task invocation.
  Repeated polling does not restart the timer; a result is consumed once.
  Initialization cancels pending Reset and clears only Timer0 flags/enables.
- Discovery, addressed selection and broadcast conversion now retain their
  Reset state on Busy, without sending premature bits or publishing errors.
  Refresh during a pending Reset restarts acquisition state but lets that
  Reset finish; reinitialization cancels it. Other bus errors, coherent-cache
  publication, 12-bit setup and acquisition diagnostics are preserved.
- Added a host register/clock test executing the production timer driver.
  Checked all 64 initial shared-prescaler phases against the actual minimum
  waits, no foreground software delay, polling without counter restart,
  presence/absence, idle/recovery faults, cancellation, interrupt-state
  restoration, delayed result consumption and progress without polling.
  GPIO ownership, Timer1 registers and prescaler state are also checked.
- Made the separate bit-level emulator return Reset Busy for two ticks.
  It rejects bit traffic while Reset is pending. Added acquisition-refresh,
  reinitialization and tick-wrap checks, retaining discovery, conversion,
  full temperature range, CRC/setup faults and real-firmware service coverage.
  Test-only AVR register, interrupt, delay and atomic models remain excluded
  from AVR/MPLAB firmware source sets.
- Verification: `make test` passed seven C suites and 26 Python tests.
  Timer0 and real-firmware tests passed ASan/UBSan with non-PIE host binaries.
  Doxygen syntax/parameter, whitespace and 85-column source checks passed.
  `make -B all` passed (10098 bytes Flash / 482 bytes static RAM).
  Generated bit-write/read code, display and main BSP objects are unchanged.
  Inspected the timer ISR: its presence path occupies about 75 us including
  entry/exit overhead. Byte transfers still occupy about 560 us in a task;
  this is not a fully asynchronous bit/byte implementation.
- Updated API, specification and bench guidance. This stage still needs
  board verification of acquisition and the effect on display jitter.
  Heating remains disabled; EEPROM layout and scheduler priorities are
  unchanged. No branch change or commit was made.

### Timer0 test and stub style

- Applied the project C style to the host AVR register, interrupt, atomic
  and delay headers: section banners, function separators, closing comments
  and documented declarations. Modeled AVR bit names remain platform macros
  rather than members of a project-defined enumeration.
- Replaced unexplained timer-test values with documented constants for
  independent timing expectations, register sentinels and observation windows.
  Documented the test entry point and removed a nested result-selection
  expression. Preserved the hardware model, timing bounds and test coverage;
  production firmware is unchanged.
- Verification: `make test` passed seven C suites and 26 Python tests.
  Doxygen syntax/parameter, whitespace, UTF-8 and 85-column checks passed.

### Second mixed timer/software 1-Wire stage

- Bench verification confirmed first-stage sensor availability and temperature
  display. Jitter remained; its relative change was not quantified.
- Extended dedicated Timer0 to presence, Write 0 low and all slot recovery
  intervals. Reset low/recovery retain /64; presence and slots use /8 without
  resetting the shared Timer0/Timer1 prescaler. Timer1, scheduler priorities,
  display scanning, EEPROM layout and disabled heater outputs are unchanged.
- Replaced the presence ISR's 70 us software wait with a separate compare.
  A 62 us timer wait (compare 116) leaves room for register setup and ISR
  latency within the guaranteed 60..75 us device-presence overlap. The host
  clock model includes CTC's compare/clear cycle, all 64 shared-clock phases,
  earliest/latest minimum-length presence and bounded foreground/Timer1
  interference. AVR disassembly confirms 40 cycles through the early sample,
  23 cycles from Reset release to timer start and a 41-cycle clock-running
  return tail (including the 35-cycle epilogue). The additional blocking
  model allows 86 cycles. Checked linked runtime masking, including EEPROM
  writes; task-list changes remain startup-only. Clock changes, new handlers
  or longer masked regions require timing revalidation.
- Added start/poll operations for single bits, autonomous LSB-first bytes and
  complete Search ROM triplets. Timer0 starts subsequent slots without another
  task invocation; the search operation reads both bits and selects the sole
  or preferred branch. Both reads high fail without a branch write.
  Results are consumed once, inputs are latched at start and outputs remain
  unchanged until successful completion. Idle/recovery low faults explicitly
  stop the operation and invalidate the acquisition.
- Wired Busy/result handling into discovery, Match/Skip ROM, address bytes,
  volatile setup, power checks, conversion commands and scratchpad reads.
  Indices and phases advance only on Ok. Refresh retains pending Reset but
  its next Reset preempts any old transfer or unconsumed result; initialization
  cancels all bus work. Conversion timeout uses the read-start tick, so a
  pre-deadline busy sample cannot become a timeout through later polling.
- Extended the actual-driver model over all 256 read/write byte values at
  all 64 clock phases, all search pairs/branch preferences, slot/sample
  bounds, autonomous progress, polling/input latching, invalid outputs,
  faults, IRQ restoration, cancellation and Reset preemption.
  Extended the protocol emulator and firmware integration tests with delayed
  completions, in-flight/completed refresh, reinitialization, tick wrap,
  bus faults and conversion-deadline boundaries. Preserved existing test order.
  Replaced fixed-delay diagnostic/cancellation assertions with an explicit
  three-second response bound where an old and new acquisition can overlap.
- Only the 6 us Write 1 pulse and 3+8 us read-sample interval still use
  software delays. This remains a mixed driver; a whole byte and presence
  waiting no longer block a task or ISR. Acquisition and jitter still need
  second-stage board verification. No branch change or commit was made.
- Verification: `make -B all` passed (10738 bytes Flash / 490 bytes static RAM).
  `make test` passed seven C suites and 26 Python tests. The final actual-driver
  and real-firmware suites passed ASan/UBSan with non-PIE host binaries.
  Doxygen syntax/parameter, UTF-8, whitespace, 85-column and three-level
  nesting checks passed for new/rewritten functions. Display and main BSP
  objects are byte-identical to the first stage; the linked image retains
  strong Timer0/Timer1 handlers and contains no heater-on symbol.

### Required version-bump token under repository rules

- The automatic version push was rejected by repository rules requiring a
  pull request and verified commit signatures. Changed checkout from an
  optional token with `GITHUB_TOKEN` fallback to the required Actions secret
  `VERSION_BUMP_TOKEN`. Checkout persists that authentication for the existing
  fetch/push operations; absent credentials now fail at checkout.
- The token needs repository contents write access and its authenticated
  owner needs bypass permission for both rules on direct automation pushes.
  Merely providing a token does not bypass the rules. Kept the separate
  `GITHUB_TOKEN` for read-only PR skip checks and reduced its contents
  permission to read. No secret values are stored in the repository.
- Added a workflow-authentication regression check and updated setup guidance.
  Version selection, retry/idempotency and skip behavior are unchanged.
- Verification: `python3 -m unittest discover -s tests -p 'version_workflow_test.py'`
  passed all 15 tests; `git diff --check` passed. The updated workflow still
  needs a GitHub run after pushing these changes.

### Documentation voice

- Standardized history and documentation wording to the project author's
  voice, without third-person attribution of decisions or confirmations.
  Rephrased existing history entries and specification instructions while
  preserving their technical meaning.
