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
Host-only API substitutes and a bit-level 1-Wire emulator exercise the firmware;
neither is linked into the AVR image.

GitHub Actions runs both checks for pull requests and pushes to `development`,
`main`, or `master`, and stores the firmware images, map and `VERSION` as an
artifact. Hardware behavior still requires testing on the board.

#### Real DS18B20 (software 1-Wire)

Connect a separately powered DS18B20: VDD to its supply, GND to common ground,
and DQ to PD2 with an external pull-up. Parasite power is not supported.
Keep heater loads disconnected for this interface-only bench stage;
both firmware heater outputs remain off.

```sh
make -B all
```

Flash `output/real-sensor/boiler.hex`. Firmware uses real sensors and
EEPROM-backed assignments. It does not erase old records.
To change assignments that do not match connected sensors, enter service with
Left+Right held before reset/power-on and for 3 seconds after debounce.
From E001, Enter also opens assignment.

For the first single-sensor test:

1. Release the startup keys at `P  1`, then Enter.
2. Leave the one sensor connected at `S1`, then Enter.
3. At `CONF`, Enter; at `SAVE`, Enter again to confirm writing EEPROM.
4. Wait for automatic application and the temperature screen.

Each real-bus check takes about one second; the prompt remains visible while
it runs. Right has no action and no presence decimal points appear.
On a failed check, Enter retries and Left cancels before writing begins.
Normal temperature-screen buttons remain inactive.

The backend ensures 12-bit resolution (0.0625 C) before conversion, with
configuration readback and preserved alarm thresholds. It changes sensor
RAM only, not sensor EEPROM. The display rounds to one decimal place across
the full -55..125 C sensor range, including -55.0 and 125.0.

The initial display can show dashes until the first checked conversion.
Temperature is right-aligned, with leading zeros blank. HL3 identifies
channel 1 and HL7 indicates Mode 3. During an error, the channel LEDs are
off, the error code is displayed, and only the current mode LED blinks
(500 ms on, 500 ms off). Verify temperature changes by
gently warming the sensor, then verify power-cycle restoration. With valid
assignments, disconnecting the sensor eventually shows E002; reconnecting
restores temperature without a reset or another beep. E005 means failed
ROM/scratchpad CRC. E008 indicates a bus/data/power/configuration/conversion
failure.
Check the external pull-up, wiring and supply if either persists.

The long software 1-Wire Reset waits now use dedicated Timer0 and return
control to the scheduler. Timer1 and the display task are unchanged.
Presence sampling still occupies about 75 us in the timer ISR, and byte
transfers remain synchronous (about 560 us each). Check display smoothness
on the board; this mixed first stage does not guarantee that all jitter
has disappeared. Heating remains disabled.

For MPLAB, remove obsolete test-build compiler definitions and regenerate
CMake to refresh the source list. Software 1-Wire is the implemented backend.
Existing sensor assignment
format and EEPROM addresses are unchanged.

#### Sensor assignment menu

With missing/invalid EEPROM configuration the display shows E001; press
Enter to start. **The menu writes EEPROM after confirmation at SAVE.**
It does not enable heaters.

- `P  1` / `P  2`: Up/Down select the profile, Enter confirms.
- `S1` / `S2`: Enter assigns the sole present device.
- Right has no action; connect/disconnect physical sensors as prompted.
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

Configuration saved by the menu is loaded from EEPROM on subsequent starts.
After a successful save, or after cancelling with a valid old configuration,
the firmware rechecks sensor presence and shows SENSOR_1 temperature or the
current diagnostic. HL8 indicates Mode 4 inside service, blinking only when
a service error is displayed; HL3/HL4 remain off there. Outside service,
HL7 indicates Mode 3, steady for normal operation or blinking for errors,
including E001 before configuration. Showing temperature lights HL3 again.
Two-channel assignments still check both devices but show only SENSOR_1 at
this stage; channel switching and heating are not implemented.

The K5 LED bank uses the following segment assignments:

| LED | Meaning | Segment |
| --- | --- | --- |
| HL1 | Heating | A |
| HL2 | At setpoint | F |
| HL3 | Displayed channel 1 | E |
| HL4 | Displayed channel 2 | D |
| HL5 | Mode 1 | B |
| HL6 | Mode 2 | G |
| HL7 | Mode 3 | C |
| HL8 | Mode 4 | DP |

HL1 and HL2 remain off in this non-heating interface stage. Channel identity
uses LEDs, not a digit or square glyph on the temperature display.

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
