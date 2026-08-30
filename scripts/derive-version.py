#!/usr/bin/env python3
"""Derive Moonlight build versions from CI_VERSION or Git tags.

Version precedence:
  1. CI_VERSION environment variable (explicit CI/release override)
  2. `git describe --tags --long --dirty` from the source tree
  3. app/version.txt fallback for source archives without Git metadata

The script intentionally emits separate display/artifact/numeric forms because
Windows file versions and Apple bundle versions must remain numeric, while
application/about-box versions can include Git metadata.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
from pathlib import Path


GIT_DESCRIBE_RE = re.compile(
    r"^v?(?P<base>\d+\.\d+\.\d+)"
    r"(?P<prerelease>(?:[.-][0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?)"
    r"-(?P<count>\d+)-g(?P<sha>[0-9a-fA-F]+)"
    r"(?P<dirty>-dirty)?$"
)


def run_git(source_root: Path, *args: str) -> str | None:
    try:
        return subprocess.check_output(
            ["git", *args],
            cwd=source_root,
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def read_fallback_version(source_root: Path) -> str:
    version_file = source_root / "app" / "version.txt"
    try:
        return version_file.read_text(encoding="utf-8").strip()
    except OSError:
        return "0.0.0"


def split_numeric_base(version: str) -> tuple[int, int, int]:
    match = re.search(r"(\d+)\.(\d+)\.(\d+)", version)
    if not match:
        return (0, 0, 0)
    return tuple(int(part) for part in match.groups())  # type: ignore[return-value]


def clamp_u16(value: int) -> int:
    return max(0, min(value, 65535))


def normalize_explicit_version(version: str) -> str:
    if re.match(r"^v\d", version):
        return version[1:]
    return version


def derive(source_root: Path) -> dict[str, str]:
    ci_version = os.environ.get("CI_VERSION", "").strip()
    fallback = read_fallback_version(source_root)

    if ci_version:
        display = normalize_explicit_version(ci_version)
        base = ".".join(str(part) for part in split_numeric_base(display))
        major, minor, patch = split_numeric_base(base)
        return {
            "base": base,
            "display": display,
            "artifact": display.replace("+", "-"),
            "numeric": f"{major}.{minor}.{patch}.0",
            "describe": ci_version,
            "commit": run_git(source_root, "rev-parse", "--short=8", "HEAD") or "unknown",
        }

    describe = run_git(
        source_root,
        "describe",
        "--tags",
        "--long",
        "--dirty",
        "--match",
        "v[0-9]*",
        "--match",
        "[0-9]*",
    )
    commit = run_git(source_root, "rev-parse", "--short=8", "HEAD") or "unknown"

    if describe:
        match = GIT_DESCRIBE_RE.fullmatch(describe)
        if match:
            base = match.group("base")
            version = base + (match.group("prerelease") or "")
            count = int(match.group("count"))
            sha = match.group("sha")
            dirty = bool(match.group("dirty"))
            major, minor, patch = split_numeric_base(base)

            if count == 0 and not dirty:
                display = version
            else:
                display = f"{version}+{count}.g{sha[:8]}"
                if dirty:
                    display += ".dirty"

            return {
                "base": base,
                "display": display,
                "artifact": display.replace("+", "-"),
                "numeric": f"{major}.{minor}.{patch}.{clamp_u16(count)}",
                "describe": describe,
                "commit": commit,
            }

    major, minor, patch = split_numeric_base(fallback)
    return {
        "base": fallback,
        "display": fallback,
        "artifact": fallback.replace("+", "-"),
        "numeric": f"{major}.{minor}.{patch}.0",
        "describe": fallback,
        "commit": commit,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-root",
        default=str(Path(__file__).resolve().parents[1]),
        help="Moonlight Qt source root (defaults to this script's parent repo)",
    )
    parser.add_argument(
        "--field",
        choices=("base", "display", "artifact", "numeric", "describe", "commit"),
        default="display",
        help="Single field to print",
    )
    parser.add_argument(
        "--format",
        choices=("plain", "env", "qmake"),
        default="plain",
        help="Output format for all fields (plain prints only --field)",
    )
    args = parser.parse_args()

    source_root = Path(args.source_root).resolve()
    info = derive(source_root)

    if args.format == "env":
        for key in ("base", "display", "artifact", "numeric", "describe", "commit"):
            print(f"MOONLIGHT_{key.upper()}={info[key]}")
    elif args.format == "qmake":
        print(f"MOONLIGHT_VERSION = {info['display']}")
        print(f"MOONLIGHT_ARTIFACT_VERSION = {info['artifact']}")
        print(f"MOONLIGHT_NUMERIC_VERSION = {info['numeric']}")
        print(f"MOONLIGHT_GIT_COMMIT = {info['commit']}")
    else:
        print(info[args.field])

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
