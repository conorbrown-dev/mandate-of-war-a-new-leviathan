#!/usr/bin/env python3
"""Cross-platform entry point for Mandate's native and Godot tests."""

from __future__ import annotations

import subprocess
import sys

from validate import GODOT_PROJECT, ROOT, GAMEPLAY_SCENARIOS, PERFORMANCE_SCENARIOS, godot_executable


def run(command: list[str]) -> int:
    return subprocess.run(command, cwd=ROOT).returncode


def main() -> int:
    area = sys.argv[1] if len(sys.argv) > 1 else "all"
    scenarios = GAMEPLAY_SCENARIOS | PERFORMANCE_SCENARIOS
    if area in scenarios or area == "validation":
        scenario = "all" if area == "validation" else area
        return run([sys.executable, str(ROOT / "tools" / "validate.py"), scenario, *sys.argv[2:]])
    if area not in {"all", "native", "godot"}:
        print("usage: tools/test [all|native|godot|<scenario-id>]", file=sys.stderr)
        return 2
    if run(["cmake", "--build", str(ROOT / "build")]) != 0:
        return 1
    if area in {"all", "native"} and run(["ctest", "--test-dir", str(ROOT / "build"), "--output-on-failure"]) != 0:
        return 1
    if area in {"all", "godot"}:
        log_dir = ROOT / "validation" / "artifacts" / "test-suite"
        log_dir.mkdir(parents=True, exist_ok=True)
        command = [str(godot_executable()), "--headless", "--path", str(GODOT_PROJECT), "--script", "res://test_skirmish.gd", "--log-file", str(log_dir / "test-skirmish.log")]
        if run(command) != 0:
            return 1
    if area == "all" and run([sys.executable, str(ROOT / "tools" / "validate.py"), "all"]) != 0:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
