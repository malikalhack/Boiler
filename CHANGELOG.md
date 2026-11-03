# Changelog

All notable changes to the Boiler project are documented here. The format is
based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Project
versions use Semantic Versioning.

---

## [Unreleased]

### Added

- Added buffered five-position display scanning, raw segment/LED frame API,
  and startup `E001` indication for invalid EEPROM configuration.
- Added shared operation results and application error codes, display-side
  error formatting, and enumerated task descriptors with named priorities.

### Changed

- Storage and BSP EEPROM operations return `EResult_t` with zero for success;
  callers must compare explicitly with `eResultOk`. Failures distinguish
  invalid arguments, out-of-range EEPROM access, missing valid configuration,
  backend read/write errors, and verification errors.
- Saving configuration now stops if the initial EEPROM scan fails, while
  still allowing a new record when the scan finds no valid configuration.

The project specification and engineering decisions are documented separately
in `SPECIFICATION.md` and `HISTORY.md`.
