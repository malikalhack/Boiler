import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github/workflows/version-bump-on-development.yml"
CODE = textwrap.dedent(WORKFLOW.read_text().split("        run: |\n", 1)[1])
HEADER = b"/**\n * @version 0.1.0\n * @authors Anton Chernov\n */\n"


class VersionWorkflowTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="boiler-version-test-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.remote = self.root / "remote.git"
        self.work = self.root / "work"
        self.git("init", "--bare", str(self.remote))
        self.git("clone", str(self.remote), str(self.work))
        self.git("-C", str(self.work), "switch", "-c", "development")
        self.git("-C", str(self.work), "config", "user.name", "Workflow test")
        self.git("-C", str(self.work), "config", "user.email", "test@example.invalid")
        self.write("VERSION", b"0.1.0\n")
        self.write("Makefile", b"all:\n\t@true\n")
        for name in ("inc/boiler_config.h", "src/main.c", "bsp/bsp.h", "tests/test.c"):
            self.write(name, HEADER)
        shutil.copy2(ROOT / "make_release.py", self.work / "make_release.py")
        self.commit("Initial")
        self.before = self.git("-C", str(self.work), "rev-parse", "HEAD")
        self.git("-C", str(self.work), "push", "origin", "development")
        self.write("feature", b"feature\n")
        self.commit("Feature")
        self.after = self.git("-C", str(self.work), "rev-parse", "HEAD")
        self.git("-C", str(self.work), "push", "origin", "development")
        self.responses = {}
        self.api_error = False
        self.event = {
            "before": self.before, "after": self.after,
            "commits": [{"id": self.after, "message": "Feature"}],
            "head_commit": {"id": self.after, "message": "Feature"},
        }
        binary = self.root / "bin"
        binary.mkdir()
        gh = binary / "gh"
        gh.write_text(
            f"#!{sys.executable}\n"
            "import json, os, sys\n"
            "from pathlib import Path\n"
            "with Path(os.environ['API_LOG']).open('a') as log:\n"
            "    log.write(sys.argv[-1] + '\\n')\n"
            "if os.environ['API_ERROR'] == '1':\n"
            "    sys.exit('API unavailable')\n"
            "data = json.loads(Path(os.environ['API_RESPONSES']).read_text())\n"
            "print(json.dumps(data.get(sys.argv[-1], [[]])))\n"
        )
        gh.chmod(0o755)
        self.env = {
            **os.environ,
            "PATH": str(binary) + os.pathsep + os.environ["PATH"],
            "GITHUB_EVENT_PATH": str(self.root / "event.json"),
            "REPO": "test/repo",
            "API_LOG": str(self.root / "api.log"),
            "API_RESPONSES": str(self.root / "responses.json"),
            "PYTHONDONTWRITEBYTECODE": "1",
        }

    def git(self, *args):
        return subprocess.check_output(
            ["git", *args], text=True, stderr=subprocess.PIPE,
        ).strip()

    def write(self, name, content):
        path = self.work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)

    def commit(self, message):
        self.git("-C", str(self.work), "add", ".")
        self.git("-C", str(self.work), "commit", "--allow-empty", "-m", message)
        if hasattr(self, "event"):
            sha = self.git("-C", str(self.work), "rev-parse", "HEAD")
            commit = {"id": sha, "message": message}
            self.event["after"] = sha
            self.event["commits"].append(commit)
            self.event["head_commit"] = commit

    def run_workflow(self, expected=0):
        (self.root / "event.json").write_text(json.dumps(self.event))
        (self.root / "responses.json").write_text(json.dumps(self.responses))
        result = subprocess.run(
            [sys.executable, "-c", CODE], cwd=self.work,
            env={**self.env, "API_ERROR": "1" if self.api_error else "0"},
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        return result

    def pull_request(self, **overrides):
        return {
            "number": 7, "merged_at": "2026-11-05T00:00:00Z",
            "base": {"ref": "development", "repo": {"full_name": "test/repo"}},
            "merge_commit_sha": self.after, "title": "Feature", "body": "",
            **overrides,
        }

    def associate(self, *pull_requests, sha=None):
        self.responses[f"repos/test/repo/commits/{sha or self.after}/pulls"] = [
            list(pull_requests)
        ]

    def assert_version(self, version):
        self.assertEqual((self.work / "VERSION").read_text(), version + "\n")

    def test_direct_push_updates_headers_once_and_rerun_is_idempotent(self):
        self.run_workflow()
        self.assert_version("0.1.1")
        for name in ("inc/boiler_config.h", "src/main.c", "bsp/bsp.h", "tests/test.c"):
            self.assertIn(b"@version 0.1.1", (self.work / name).read_bytes())
        head = self.git("-C", str(self.work), "rev-parse", "HEAD")
        self.assertIn(
            f"Version-Bump-For-Push: {self.after}",
            self.git("-C", str(self.work), "log", "-1", "--format=%B"),
        )
        self.assertEqual(self.git("-C", str(self.work), "status", "--porcelain"), "")
        self.run_workflow()
        self.assertEqual(self.git("-C", str(self.work), "rev-parse", "HEAD"), head)

    def test_merge_push_increments_once(self):
        self.associate(self.pull_request())
        self.run_workflow()
        self.assert_version("0.1.1")
        self.assertEqual(
            self.git("-C", str(self.work), "rev-list", "--count", f"{self.after}..HEAD"),
            "1",
        )

    def test_pr_title_body_and_commit_summary_skip(self):
        for fields in (
            {"title": "Feature [SKIP CI]"},
            {"body": "Skip this release: [skip ci]"},
            {"body": "## Commits\n- `abcdef0` Docs [skip ci]"},
        ):
            with self.subTest(fields=fields):
                self.associate(self.pull_request(**fields))
                result = self.run_workflow()
                self.assertIn("version update skipped", result.stdout)
                self.assert_version("0.1.0")

    def test_unmerged_unrelated_or_wrong_base_pr_does_not_skip(self):
        self.associate(
            self.pull_request(merged_at=None, title="[skip ci]"),
            self.pull_request(number=8, merge_commit_sha=self.before, title="[skip ci]"),
            self.pull_request(number=9, base={"ref": "master"}, title="[skip ci]"),
            self.pull_request(number=10, base={
                "ref": "development", "repo": {"full_name": "other/repo"},
            }, title="[skip ci]"),
        )
        self.run_workflow()
        self.assert_version("0.1.1")

    def test_multi_commit_push_increments_once(self):
        self.write("second", b"second\n")
        self.commit("Second feature")
        self.event["after"] = self.git("-C", str(self.work), "rev-parse", "HEAD")
        self.git("-C", str(self.work), "push", "origin", "development")
        self.run_workflow()
        self.assert_version("0.1.1")
        self.assertEqual(len((self.root / "api.log").read_text().splitlines()), 2)

    def test_earlier_commit_or_merged_pr_in_push_can_skip(self):
        self.write("second", b"second\n")
        self.commit("Second feature")
        self.event["after"] = self.git("-C", str(self.work), "rev-parse", "HEAD")
        self.git("-C", str(self.work), "push", "origin", "development")
        self.associate(self.pull_request(title="[skip ci]"))
        self.run_workflow()
        self.assert_version("0.1.0")
        self.responses = {}
        self.commit("Docs [skip ci]")
        self.commit("Last feature")
        self.event["after"] = self.git("-C", str(self.work), "rev-parse", "HEAD")
        self.git("-C", str(self.work), "push", "origin", "development")
        self.api_error = True
        self.run_workflow()
        self.assert_version("0.1.0")

    def test_automatic_version_commit_does_not_recurse(self):
        self.run_workflow()
        self.event = {
            "before": self.after,
            "after": self.git("-C", str(self.work), "rev-parse", "HEAD"),
            "commits": [{
                "id": self.git("-C", str(self.work), "rev-parse", "HEAD"),
                "message": self.git("-C", str(self.work), "log", "-1", "--format=%B"),
            }],
        }
        self.api_error = True
        result = self.run_workflow()
        self.assertIn("[skip ci] found", result.stdout)
        self.assert_version("0.1.1")

    def test_api_failure_prevents_bump(self):
        self.api_error = True
        self.run_workflow(expected=1)
        self.assert_version("0.1.0")
        self.assertEqual(self.git("-C", str(self.work), "status", "--porcelain"), "")

    def test_new_branch_event(self):
        self.event["before"] = "0" * 40
        self.run_workflow()
        self.assert_version("0.1.1")

    def test_force_push_with_missing_before_commit(self):
        old_sha = self.after
        self.git("-C", str(self.work), "commit", "--amend", "-m", "Amended feature")
        sha = self.git("-C", str(self.work), "rev-parse", "HEAD")
        self.git("-C", str(self.work), "push", "--force", "origin", "development")
        fresh = self.root / "fresh"
        self.git("clone", "--no-local", "--branch", "development", str(self.remote), str(fresh))
        self.work = fresh
        with self.assertRaises(subprocess.CalledProcessError):
            self.git("-C", str(self.work), "cat-file", "-e", old_sha)
        self.event = {
            "before": old_sha, "after": sha,
            "commits": [{"id": sha, "message": "Amended feature"}],
            "head_commit": {"id": sha, "message": "Amended feature"},
            "forced": True,
        }
        self.run_workflow()
        self.assert_version("0.1.1")

    def test_missing_before_commit_does_not_bypass_skip(self):
        self.event["before"] = "f" * 40
        self.event["commits"][0]["message"] = "Feature [SKIP CI]"
        self.api_error = True
        result = self.run_workflow()
        self.assertIn("[skip ci] found", result.stdout)
        self.assert_version("0.1.0")

    def test_empty_commit_list_uses_event_head(self):
        self.event["commits"] = []
        self.run_workflow()
        self.assert_version("0.1.1")

    def test_missing_event_head_fails_explicitly(self):
        self.event["commits"] = []
        self.event["head_commit"] = None
        result = self.run_workflow(expected=1)
        self.assertIn("Push event is missing its head commit.", result.stderr)
        self.assert_version("0.1.0")

    def test_push_rejection_retries_without_double_increment(self):
        hook = self.remote / "hooks/pre-receive"
        flag = self.root / "rejected"
        hook.write_text(
            f"#!{sys.executable}\n"
            "from pathlib import Path\n"
            "import sys\n"
            f"flag = Path({str(flag)!r})\n"
            "if not flag.exists():\n"
            "    flag.touch()\n"
            "    sys.exit(1)\n"
        )
        hook.chmod(0o755)
        result = self.run_workflow()
        self.assertIn("Push attempt 1 failed", result.stdout)
        self.assert_version("0.1.1")


if __name__ == "__main__":
    unittest.main()
