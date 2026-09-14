#!/usr/bin/env python3
"""Small, documented command surface for a clean Mandate of War checkout."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
PROJECT = ROOT / "godot" / "project"


def run(command: list[str]) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=ROOT, check=True)


def godot() -> str:
    configured = os.environ.get("GODOT_BIN")
    candidates = [
        configured,
        str(ROOT / "Godot_v4.7.2-stable_linux.x86_64"),
        shutil.which("godot4"),
        shutil.which("godot"),
    ]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return candidate
    raise SystemExit(
        "Godot 4.7.2 was not found. Set GODOT_BIN to a Godot 4.7.2 executable; "
        "see docs/DEVELOPMENT_WORKFLOW.md."
    )


def configure() -> None:
    run(["cmake", "-S", str(ROOT), "-B", str(BUILD), "-DCMAKE_BUILD_TYPE=Release"])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("configure", "build", "smoke", "test", "run"))
    args = parser.parse_args()
    if shutil.which("cmake") is None:
        raise SystemExit("cmake 3.20 or later is required but was not found on PATH.")
    if args.command == "configure":
        configure()
    elif args.command == "build":
        configure()
        run(["cmake", "--build", str(BUILD)])
    elif args.command == "smoke":
        configure()
        run(["cmake", "--build", str(BUILD)])
        run([sys.executable, str(ROOT / "tools" / "validate.py"), "native_extension_smoke"])
    elif args.command == "test":
        run([sys.executable, str(ROOT / "tools" / "test.py"), "native"])
    else:
        configure()
        run(["cmake", "--build", str(BUILD)])
        run([godot(), "--editor", "--path", str(PROJECT)])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
