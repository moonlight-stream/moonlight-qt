import importlib.util
import os
import unittest
from pathlib import Path
from unittest import mock


SCRIPT_PATH = Path(__file__).resolve().parents[2] / "scripts" / "derive-version.py"
SPEC = importlib.util.spec_from_file_location("derive_version", SCRIPT_PATH)
assert SPEC is not None and SPEC.loader is not None
derive_version = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(derive_version)


class DeriveVersionTests(unittest.TestCase):
    def derive_from_git(self, describe: str) -> dict[str, str]:
        def run_git(_source_root: Path, *args: str) -> str:
            if args and args[0] == "describe":
                return describe
            if args == ("rev-parse", "--short=8", "HEAD"):
                return "feedface"
            raise AssertionError(f"Unexpected git command: {args}")

        with (
            mock.patch.dict(os.environ, {"CI_VERSION": ""}),
            mock.patch.object(derive_version, "run_git", side_effect=run_git),
            mock.patch.object(
                derive_version, "read_fallback_version", return_value="6.2.84"
            ),
        ):
            return derive_version.derive(Path("."))

    def test_exact_stable_tag(self) -> None:
        result = self.derive_from_git("v6.3.13-0-g12345678")

        self.assertEqual(result["display"], "6.3.13")
        self.assertEqual(result["artifact"], "6.3.13")
        self.assertEqual(result["numeric"], "6.3.13.0")

    def test_exact_legacy_dot_prerelease_tag(self) -> None:
        result = self.derive_from_git("v6.3.10.beta-0-g4af38f30")

        self.assertEqual(result["display"], "6.3.10.beta")
        self.assertEqual(result["artifact"], "6.3.10.beta")
        self.assertEqual(result["numeric"], "6.3.10.0")

    def test_exact_semver_prerelease_tag(self) -> None:
        result = self.derive_from_git("v6.3.14-beta.1-0-gabcdef12")

        self.assertEqual(result["display"], "6.3.14-beta.1")
        self.assertEqual(result["artifact"], "6.3.14-beta.1")
        self.assertEqual(result["numeric"], "6.3.14.0")

    def test_commits_after_prerelease_tag_keep_git_metadata(self) -> None:
        result = self.derive_from_git(
            "v6.3.14-beta.1-3-gabcdef123456-dirty"
        )

        self.assertEqual(
            result["display"], "6.3.14-beta.1+3.gabcdef12.dirty"
        )
        self.assertEqual(
            result["artifact"], "6.3.14-beta.1-3.gabcdef12.dirty"
        )
        self.assertEqual(result["numeric"], "6.3.14.3")

    def test_explicit_version_strips_tag_prefix(self) -> None:
        with (
            mock.patch.dict(os.environ, {"CI_VERSION": "v6.3.15-rc.1"}),
            mock.patch.object(derive_version, "run_git", return_value="feedface"),
            mock.patch.object(
                derive_version, "read_fallback_version", return_value="6.2.84"
            ),
        ):
            result = derive_version.derive(Path("."))

        self.assertEqual(result["display"], "6.3.15-rc.1")
        self.assertEqual(result["artifact"], "6.3.15-rc.1")
        self.assertEqual(result["numeric"], "6.3.15.0")


if __name__ == "__main__":
    unittest.main()
