#!/usr/bin/env python
# /// script
# ///

import re
import subprocess
from pathlib import Path

submodule_macro = {
    "moonlight-common-c/moonlight-common-c": "commit1",
    "qmdnsengine/qmdnsengine": "commit2",
    "app/SDL_GameControllerDB": "commit3",
    "h264bitstream/h264bitstream": "commit4",
    "moonlight-common-c/moonlight-common-c/enet": "commit5",
    "moonlight-common-c/moonlight-common-c/nanors": "commit6",
}
spec_file = Path(__file__).parent / "moonlight-qt.spec"


def get_submodules() -> dict[str, str]:
    submodule_status_lines = subprocess.run(
        [
            "git",
            "submodule",
            "status",
            "--recursive",
        ],
        check=True,
        text=True,
        capture_output=True,
    ).stdout.splitlines()
    if any(line.startswith("-") for line in submodule_status_lines):
        subprocess.check_call(
            [
                "git",
                "submodule",
                "update",
                "--recursive",
            ]
        )
        results = get_submodules()
        subprocess.check_call(
            [
                "git",
                "submodule",
                "deinit",
                "--all",
            ]
        )
        return results
    results = {}
    for line in submodule_status_lines:
        sha, path, ref = line.strip().split(" ")
        results[path] = sha
    return results


def patch_specfile(submodules: dict[str, str]) -> None:
    spec = spec_file.read_text()
    for path, sha in submodules.items():
        commit_tag = submodule_macro[path]
        spec = re.sub(rf"(%global\s*{commit_tag}\s*)\w+", rf"\g<1>{sha}", spec)
    spec_file.write_text(spec)


if __name__ == "__main__":
    submodules = get_submodules()
    if set(submodules.keys()) != set(submodule_macro.keys()):
        print("submodule lists do not match")
        exit(1)
    patch_specfile(submodules)
