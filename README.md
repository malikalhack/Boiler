# Boiler project.  
***Note:*** *One of my first projects in C language. It started in 2016.
The previous code was abandoned, and the project was recommenced in 2026.*  

##### Current view

![Current view](/Images/Top_view.jpg "Current view")

##### First implementation view.

![First implementation view](/Images/First_impl.jpg "First implementation view")

#### The main features of the project.  
The boller device allows you to control two heating elements, with
a capacity of up to 2 kW. The boiler is built on the ATMega16A controller,
uses two DS18B20 sensors to control the temperature in each of the heating tanks.
A seven-segment indicator is used to indicate the current temperature.
Eight LEDs act as additional indication. The boiler is controlled using five buttons.  

#### Build

Build the AVR firmware with AVR-GCC and GNU Make using `make`.
Run host tests with `make test` (requires a host C compiler, Python 3 and Git).

GitHub Actions runs both checks for pull requests and pushes to `development`,
`main`, or `master`, and stores the firmware images, map and `VERSION` as an
artifact. Hardware behavior still requires testing on the board.

#### Real DS18B20 (software 1-Wire)

Connect a separately powered DS18B20: VDD to its supply, GND to common ground,
and DQ to PD2 with an external pull-up. Parasite power is not supported.
Keep heater loads disconnected for this interface-only bench stage;
both firmware heater outputs remain off.

```sh
make -B all BUILD_DIR=output/real-sensor
```

Flash `output/real-sensor/boiler.hex`. This is the normal EEPROM-backed build,
without fake sensors or the RAM test configuration. It does not erase the old
assignments. A saved fake ROM will not match a real sensor: enter service with
Left+Right held before reset/power-on and for 3 seconds after debounce.
From E001, Enter also opens assignment.

For the first single-sensor test:

1. Release the startup keys at `P  1`, then Enter.
2. Leave the one sensor connected at `S1`, then Enter.
3. At `CONF`, Enter; at `SAVE`, Enter again to confirm writing EEPROM.
4. Wait for automatic application and the temperature screen.

Each real-bus check takes about one second; the prompt remains visible while
it runs. Right has no action and no fixture-presence decimal points appear.
On a failed check, Enter retries and Left cancels before writing begins.
Normal temperature-screen buttons remain inactive.

The backend ensures 12-bit resolution (0.0625 C) before conversion, with
configuration readback and preserved alarm thresholds. It changes sensor
RAM only, not sensor EEPROM. The display still rounds to tenths or whole
degrees; higher sensor resolution does not add displayed decimal places.

The initial display can show dashes until the first checked conversion.
Temperature uses the previously verified adaptive format; HL7 indicates
Mode 3 and HL4 is steady without an error. Verify temperature changes by
gently warming the sensor, then verify power-cycle restoration. With valid
assignments, disconnecting the sensor eventually shows E002; reconnecting
restores temperature without a reset or another beep. E005 means failed
ROM/scratchpad CRC. E008 indicates a bus/data/power/configuration/conversion
failure.
Check the external pull-up, wiring and supply if either persists.

Occasional subtle display jitter is currently a known limitation of
cooperative scanning during software 1-Wire transfers.

For MPLAB, regenerate CMake to include the new source files and remove the
fake/RAM-test macros (or set `SENSORS_USE_FAKE=0`, `CONFIG_USE_TEST=0`).
The software 1-Wire backend is selected by default. Existing sensor assignment
format and EEPROM addresses are unchanged.

#### Sensor test build

For a bench test with fake sensors and RAM-only configuration:

```sh
make -B all BUILD_DIR=output/test-config FAKE_SENSORS=1 TEST_CONFIG=1 TEST_PROFILE=1 FAKE_PRESENT=1
```

Flash `output/test-config/boiler.hex`. EEPROM is not accessed by this test
mode; heater outputs remain off. This single-channel build shows the
temperature of the assigned SENSOR_1 in Mode 3, with HL7 (segment C) and HL4
continuously on while no error is active.

Both fake devices sweep together from -55 to +125 C, increasing by 0.1 C
every 500 ms, then returning to -55 C and repeating. A full cycle takes
about 15 minutes. The screen shows tenths within -9.9..99.9 C and rounded
whole degrees outside that range. Leading zeros are blank; negative readings
retain their minus sign. Ordinary buttons do not change readings or channels.

For two-channel profile diagnostics, use `TEST_PROFILE=2` (the Make default):

| `FAKE_PRESENT` | Simulated devices | Display |
| --- | --- | --- |
| 0 | Neither | E002 |
| 1 | First only | E003 |
| 2 | Second only | E002 |
| 3 | Both | SENSOR_1 temperature, HL4 and HL7 continuously on |

The single-channel profile requires only the first
assigned ROM: `FAKE_PRESENT=1` succeeds; an extra second device gives E004.
The two-channel profile still checks both assignments but shows only SENSOR_1
at this stage; channel switching and heating are not implemented.
The build without test options continues to use EEPROM, with E001 for
missing or invalid assignments.

For MPLAB, regenerate CMake after adding sources and set equivalent compiler
macros: `SENSORS_USE_FAKE=1`, `CONFIG_USE_TEST=1`,
`CONFIG_TEST_PROFILE=1`, `SENSOR_FAKE_PRESENT_MASK=1`.
Remove the test macros to return to the normal build.

#### Sensor assignment menu (fake sensors)

```sh
make -B all BUILD_DIR=output/menu-test FAKE_SENSORS=1 TEST_CONFIG=0
```

Flash `output/menu-test/boiler.hex`. With missing/invalid EEPROM configuration
the display shows E001; press Enter to start. **This build writes EEPROM
after confirmation at SAVE.** It does not enable heaters.

- `P  1` / `P  2`: Up/Down select the profile, Enter confirms.
- `S1` / `S2`: Enter assigns the sole present device.
- Right cycles fake presence: none, first, second, both. Decimal points in
  digits 1 and 2 show which fixtures are present.
- `CONF`: Enter checks all assigned devices.
- `SAVE`: Enter confirms writing; after verification the configuration is
  applied automatically, without another confirmation or reset.
- Left cancels before writing. On errors, Enter retries and Left cancels.

For two channels, use first-only at S1, second-only at S2, then both at CONF.
For one channel, leave just its assigned device present.
Enter opens the menu from E001. To reconfigure a valid record or enter from
another error, hold **Left+Right before power-on/reset**, then keep holding
for 3 seconds after button debounce, until `P  1` appears. Release the keys
before navigating. This gesture works only at startup; releasing early
cancels it. Cancelling service preserves the old EEPROM configuration.

For MPLAB, regenerate CMake and set `SENSORS_USE_FAKE=1`,
`CONFIG_USE_TEST=0` (remove previous RAM-test macros). Presence defaults to both.
Configuration saved by the menu is loaded from EEPROM on subsequent starts.
After a successful save, or after cancelling with a valid old configuration,
the firmware rechecks sensor presence and shows SENSOR_1 temperature or the
current diagnostic. HL7 is off inside service and on again with a valid
configuration outside service, including during sensor errors. The sweep
continues during service; saving/applying does not restart it.
Fake presence resets to the build's `FAKE_PRESENT` value at startup; use
`FAKE_PRESENT=1` for a single-channel configuration assigned to the first
fixture, otherwise the default second fixture is correctly reported as E004.

#### Version and pull requests

`VERSION` contains the project version (`MAJOR.MINOR.PATCH`), initially `0.1.0`.
It is release metadata, not embedded in the firmware. After each push to
`development` (including PR merges), Actions increments the patch component
once per push, regardless of its commit count, and runs `make_release.py`
to synchronize `VERSION` and the `@version` headers of Git-tracked project-owned
C/C++ files in `inc`, `src`, `bsp` and `tests`. Files are selected by the
`@authors Anton Chernov` header; `lib`, `OldCode` and EEPROM format versions
are not changed.

For manual releases, run `python3 make_release.py 0.2.0` with the desired version.
By default this verifies written content, rebuilds firmware and runs host tests;
errors restore the original files. `--skip-build` explicitly skips build/test
verification, as used by the automatic workflow. CI runs separately;
the automatic bump does not wait for its result. Interrupted runs
may leave `.release.bak` files; restore or resolve them before retrying.
The script does not commit, tag or push changes.

A `[skip ci]` marker in any pushed commit message suppresses the automatic bump.
For PRs merged by that push, their titles and bodies (including the automatic
commit summary) are also checked via the GitHub API. API failures stop the
workflow rather than bypassing this check. Version commits also use `[skip ci]`
to avoid redundant CI and version runs. Repeated runs for the same pushed
commit do not increment the version again.
Commit selection uses the push event payload, so rewritten history does not
require the previous branch-tip commit to remain available.

The version workflow needs permission to push to `development`. The default
`GITHUB_TOKEN` works when repository rules allow it; otherwise configure the
optional `VERSION_BUMP_TOKEN` secret with repository contents write access
and allow its owner to push under the branch rules. The workflow's
`GITHUB_TOKEN` also needs pull-request read access for the skip check.
The commit-summary workflow needs pull-request write access and updates only
the marked `Commits` section, preserving the rest of the PR description.

#### Documentation

- [Specification](SPECIFICATION.md) - technical requirements.
- [History](HISTORY.md) - completed work, changes, and engineering decisions.
- [Changelog](CHANGELOG.md) - release-oriented changes.
