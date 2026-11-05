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

#### Sensor test build

For a bench test with fake sensors and RAM-only configuration:

```sh
make -B all BUILD_DIR=output/test-config FAKE_SENSORS=1 TEST_CONFIG=1 FAKE_PRESENT=1
```

Flash `output/test-config/boiler.hex`. EEPROM is not accessed by this test
mode; heater outputs remain off. With the default two-channel profile:

| `FAKE_PRESENT` | Simulated devices | Display |
| --- | --- | --- |
| 0 | Neither | E002 |
| 1 | First only | E003 |
| 2 | Second only | E002 |
| 3 | Both | Dashes, HL4 continuously on |

Add `TEST_PROFILE=1` for a single-channel profile. It requires only the first
assigned ROM: `FAKE_PRESENT=1` succeeds; an extra second device gives E004.
The build without test options continues to use EEPROM, with E001 for
missing or invalid assignments.

For MPLAB, regenerate CMake after adding sources and set equivalent compiler
macros: `SENSORS_USE_FAKE=1`, `CONFIG_USE_TEST=1`,
`CONFIG_TEST_PROFILE=2`, `SENSOR_FAKE_PRESENT_MASK=1`.
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
