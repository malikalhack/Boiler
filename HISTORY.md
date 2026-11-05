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
  With user approval, changed the installed bundle's calculation from
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
- Hardware verification: the user confirmed a short startup beep and
  blinking HL4 after flashing.

## 2026-11-05

### Button input

- Added active-low PC3..PC7 reading with existing internal pull-ups,
  independent 20 ms press/release debounce, and a 1 ms polling task.
- Added synchronous press/release events and Up/Down repeats. The user
  replaced the original four slow changes with one initial press, a 3 s
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
