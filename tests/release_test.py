from contextlib import redirect_stderr
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("make_release", ROOT / "make_release.py")
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)

HEADER = (
    b"/**\n"
    b" * @file example.c\n"
    b" * @version 0.1.0\n"
    b" * @authors Anton Chernov\n"
    b" * @date 2026-11-03\n"
    b" */\n"
)


class ReleaseTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="boiler-release-test-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.git("init", "-q")
        self.write("VERSION", b"0.1.0\n")
        self.write("Makefile", b"all:\n\t@true\ntest:\n\t@true\n")
        for name in (
            "inc/boiler_config.h", "src/main.c", "bsp/bsp.h",
            "tests/example.c", "tests/stubs/avr/io.h", "lib/vendor.h",
            "OldCode/old.c",
        ):
            self.write(name, HEADER + b"#define FORMAT_VERSION 1\n")
        self.write("inc/foreign.h", HEADER.replace(b"Anton Chernov", b"Other Author"))
        self.git("add", ".")
        self.write("src/untracked.c", HEADER)

    def git(self, *args):
        return subprocess.run(
            ["git", "-C", str(self.root), *args],
            check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    def snapshot(self):
        return {
            path: path.read_bytes()
            for path in self.root.rglob("*")
            if path.is_file() and ".git" not in path.relative_to(self.root).parts
        }

    def test_scope_and_header_only_replacement(self):
        updates = release.prepare_updates(self.root, "1.2.3")
        self.assertEqual(
            {str(path.relative_to(self.root)) for path in updates},
            {"VERSION", "inc/boiler_config.h", "src/main.c", "bsp/bsp.h",
             "tests/example.c", "tests/stubs/avr/io.h"},
        )
        for path, content in updates.items():
            if path.name == "VERSION":
                self.assertEqual(content, b"1.2.3\n")
            else:
                self.assertEqual(content, path.read_bytes().replace(b"0.1.0", b"1.2.3"))

    def test_success_and_skip_build(self):
        updates = release.prepare_updates(self.root, "1.2.3")
        with patch.object(release.subprocess, "run") as run:
            release.update_and_build(self.root, updates, ["all", "test"], True)
        run.assert_not_called()
        for path, content in updates.items():
            self.assertEqual(path.read_bytes(), content)
            self.assertFalse(release.backup_path(path).exists())

    def test_default_build_and_tests(self):
        updates = release.prepare_updates(self.root, "1.2.3")
        with patch.object(release.subprocess, "run") as run:
            release.update_and_build(self.root, updates, ["all", "test"], False)
        run.assert_called_once_with(
            ["make", "-B", "all", "test"], cwd=self.root, check=True,
        )

    def test_build_failure_restores_all_files(self):
        originals = self.snapshot()
        self.write("Makefile", b"all:\n\t@false\n")
        originals[self.root / "Makefile"] = (self.root / "Makefile").read_bytes()
        result = subprocess.run(
            ["python3", str(ROOT / "make_release.py"), "1.2.3", str(self.root),
             "--make-args", "all"],
            text=True, capture_output=True,
        )
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("Build command: ['make', '-B', 'all']", result.stdout)
        self.assertIn("Original source files restored.", result.stderr)
        self.assertIn("Release update failed:", result.stderr)
        self.assertEqual(self.snapshot(), originals)

    def test_readback_failure_restores_all_files(self):
        originals = self.snapshot()
        updates = release.prepare_updates(self.root, "1.2.3")
        read_bytes = Path.read_bytes

        def incorrect_read(path):
            if path == self.root / "VERSION":
                return b"corrupted"
            return read_bytes(path)

        errors = io.StringIO()
        with patch.object(Path, "read_bytes", incorrect_read), redirect_stderr(errors):
            with self.assertRaisesRegex(ValueError, "verification failed"):
                release.update_and_build(self.root, updates, [], True)
        self.assertIn("Original source files restored.", errors.getvalue())
        self.assertEqual(self.snapshot(), originals)

    def test_existing_backup_blocks_all_writes(self):
        updates = release.prepare_updates(self.root, "1.2.3")
        self.write("src/main.c.release.bak", b"keep")
        originals = self.snapshot()
        with self.assertRaises(FileExistsError):
            release.update_and_build(self.root, updates, [], True)
        self.assertEqual(self.snapshot(), originals)

    def test_crlf_is_preserved(self):
        path = self.write("src/main.c", HEADER.replace(b"\n", b"\r\n"))
        updates = release.prepare_updates(self.root, "1.2.3")
        self.assertEqual(updates[path], path.read_bytes().replace(b"0.1.0", b"1.2.3"))

    def test_invalid_headers_fail_before_writing(self):
        for header in (
            HEADER.replace(b" * @version 0.1.0\n", b""),
            HEADER.replace(b" * @version 0.1.0\n", b" * @version 0.1.0\n" * 2),
        ):
            with self.subTest(header=header):
                self.write("src/main.c", header)
                originals = self.snapshot()
                with self.assertRaisesRegex(ValueError, "exactly one"):
                    release.prepare_updates(self.root, "1.2.3")
                self.assertEqual(self.snapshot(), originals)

    def test_missing_owned_config_fails(self):
        self.write("inc/boiler_config.h", b"/* no project header */\n")
        with self.assertRaisesRegex(ValueError, "Missing project-owned"):
            release.prepare_updates(self.root, "1.2.3")

    def test_symlinks_are_rejected(self):
        for name in ("src/main.c", "VERSION"):
            with self.subTest(name=name):
                path = self.root / name
                original = path.read_bytes()
                path.unlink()
                path.symlink_to(self.root / "lib/vendor.h")
                with self.assertRaisesRegex(ValueError, "symlink"):
                    release.prepare_updates(self.root, "1.2.3")
                path.unlink()
                path.write_bytes(original)

    def test_invalid_version_file_fails(self):
        self.write("VERSION", b"01.2.3\n")
        with self.assertRaisesRegex(ValueError, "Invalid VERSION"):
            release.prepare_updates(self.root, "1.2.3")

    def test_cli_validates_version_and_default_targets(self):
        result = subprocess.run(
            ["python3", str(ROOT / "make_release.py"), "1.2.3", str(self.root)],
            text=True, capture_output=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("['make', '-B', 'all', 'test']", result.stdout)
        self.assertEqual((self.root / "VERSION").read_bytes(), b"1.2.3\n")
        result = subprocess.run(
            ["python3", str(ROOT / "make_release.py"), "01.2.3", str(self.root)],
            text=True, capture_output=True,
        )
        self.assertEqual(result.returncode, 2)


if __name__ == "__main__":
    unittest.main()
