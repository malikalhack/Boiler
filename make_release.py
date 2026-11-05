#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Update project and file-header versions transactionally, then verify the build.

Usage:
    python3 make_release.py 0.2.0
    python3 make_release.py 0.2.0 --make-args all
    python3 make_release.py 0.2.0 --skip-build
    python3 make_release.py --version
    python3 make_release.py --help

The optional repository path defaults to this script's directory. Git-tracked
project-owned C/C++ files under inc, src, bsp and tests are selected by their
@authors header. VERSION is updated to the same version. README, dates,
changelogs, EEPROM format versions, lib and OldCode are not changed.
By default, the firmware build and host tests are both checked.

Written files are read back and compared byte-for-byte with the prepared
updates before build verification. A mismatch triggers rollback.

Backups use the suffix .release.bak. Existing backups abort the operation;
after an interrupted process they can be restored manually before retrying.

Author: Anton Chernov
Date: 2026-10-05
"""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, Mapping, Optional, Pattern, Sequence, Set


SCRIPT_VERSION = "1.0.0"
BACKUP_SUFFIX = ".release.bak"
CONFIG_PATH = Path("inc/boiler_config.h")
VERSION_PATH = Path("VERSION")
SOURCE_SUFFIXES: Set[str] = {
    ".c", ".h", ".cc", ".cpp", ".cxx", ".hh", ".hpp"
}
OWNER: Pattern[bytes] = re.compile(
    rb"(?m)^[ \t]*\*[ \t]+@authors[ \t]+Anton Chernov[ \t]*\r?$"
)
HEADER_VERSION: Pattern[bytes] = re.compile(
    rb"(?m)^([ \t]*\*[ \t]+@version[ \t]+)"
    rb"[0-9]+\.[0-9]+\.[0-9]+([ \t]*\r?$)"
)


def parse_version(value: str) -> str:
    if not re.fullmatch(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)"
                        r"\.(?:0|[1-9][0-9]*)", value):
        raise argparse.ArgumentTypeError("Version must be MAJOR.MINOR.PATCH.")
    return value


def replace_one(
    pattern: Pattern[bytes], value: bytes, content: bytes, label: str
) -> bytes:
    updated, count = pattern.subn(
        lambda match: match[1] + value + match[2], content
    )
    if count != 1:
        raise ValueError(f"Expected exactly one {label}; found {count}.")
    return updated


def prepare_updates(root: Path, version: str) -> Dict[Path, bytes]:
    tracked = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z", "--",
         "inc", "src", "bsp", "tests"],
        check=True, stdout=subprocess.PIPE,
    ).stdout
    updates = {}
    for entry in tracked.split(b"\0"):
        if not entry:
            continue
        relative = Path(entry.decode("utf-8"))
        if relative.suffix not in SOURCE_SUFFIXES:
            continue
        path = root / relative
        if path.is_symlink():
            raise ValueError(f"Refusing to modify a symlink: {relative}")
        original = path.read_bytes()
        header_end = original.find(b"*/")
        header = original[:header_end + 2] if header_end >= 0 else b""
        if OWNER.search(header) is None:
            continue
        updated_header = replace_one(
            HEADER_VERSION, version.encode("ascii"), header,
            f"@version in {relative}",
        )
        updated = updated_header + original[header_end + 2:]
        updates[path] = updated
    if root / CONFIG_PATH not in updates:
        raise ValueError(f"Missing project-owned version file: {CONFIG_PATH}")
    version_path = root / VERSION_PATH
    if version_path.is_symlink():
        raise ValueError(f"Refusing to modify a symlink: {VERSION_PATH}")
    current_version = version_path.read_text(encoding="utf-8").strip()
    try:
        parse_version(current_version)
    except argparse.ArgumentTypeError as error:
        raise ValueError(f"Invalid {VERSION_PATH}: {error}") from error
    updates[version_path] = f"{version}\n".encode("ascii")
    return updates


def backup_path(path: Path) -> Path:
    return path.with_name(path.name + BACKUP_SUFFIX)


def restore_files(backups: Sequence[Path]) -> None:
    failed = []
    for path in reversed(backups):
        try:
            backup_path(path).replace(path)
        except OSError as error:
            failed.append(path)
            print(f"Cannot restore {path}: {error}", file=sys.stderr)
    if failed:
        raise RuntimeError("Restore failed; keep remaining .release.bak files.")


def update_and_build(
    root: Path, updates: Mapping[Path, bytes], make_args: Sequence[str],
    skip_build: bool,
) -> None:
    for path in updates:
        backup = backup_path(path)
        if backup.exists() or backup.is_symlink():
            raise FileExistsError(f"Existing backup must be resolved: {backup}")

    backups = []
    try:
        for path in updates:
            backup = backup_path(path)
            destination = backup.open("xb")
            try:
                with destination, path.open("rb") as source:
                    shutil.copyfileobj(source, destination)
            except BaseException:
                backup.unlink()
                raise
            backups.append(path)
            shutil.copystat(path, backup)
        for path, content in updates.items():
            path.write_bytes(content)
        for path, content in updates.items():
            if path.read_bytes() != content:
                raise ValueError(f"Written content verification failed: {path}")
        print("Written content verified for all updated files.", flush=True)
        if skip_build:
            print("Build explicitly skipped (--skip-build).", flush=True)
        else:
            command = ["make", "-B", *make_args]
            print(f"Build command: {command}", flush=True)
            subprocess.run(command, cwd=root, check=True)
    except BaseException:
        restore_files(backups)
        print("Original source files restored.", file=sys.stderr)
        raise

    for path in backups:
        backup_path(path).unlink()


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "-V", "--version", action="version",
        version=f"%(prog)s {SCRIPT_VERSION}",
        help="Show the release script version and exit.",
    )
    parser.add_argument(
        "version", type=parse_version, help="New project version: MAJOR.MINOR.PATCH.",
    )
    parser.add_argument(
        "repository", nargs="?", type=Path,
        default=Path(__file__).resolve().parent,
        help="Repository path (default: this script's directory).",
    )
    parser.add_argument(
        "--skip-build", action="store_true",
        help="Update versions without build verification.",
    )
    parser.add_argument(
        "--make-args", nargs=argparse.REMAINDER,
        default=["all", "test"],
        help="Make goals/variables; must be the final option.",
    )
    args = parser.parse_args(argv)
    root = args.repository.resolve()
    try:
        if not (root / "Makefile").is_file():
            raise ValueError(f"Missing Makefile in {root}")
        updates = prepare_updates(root, args.version)
        print(f"Updating {len(updates)} files to {args.version}.", flush=True)
        update_and_build(root, updates, args.make_args, args.skip_build)
    except KeyboardInterrupt:
        print("Release update interrupted.", file=sys.stderr)
        return 130
    except (OSError, ValueError, RuntimeError,
            subprocess.CalledProcessError) as error:
        print(f"Release update failed: {error}", file=sys.stderr)
        return 1
    print("Version update completed; backups removed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
