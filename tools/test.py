#!/usr/bin/env python3
"""Cross-platform entry point for Mandate's native and Godot tests."""

from __future__ import annotations

import subprocess
import sys

from validate import ROOT, GAMEPLAY_SCENARIOS, PERFORMANCE_SCENARIOS, CONTRACT_SCENARIOS


def run(command: list[str]) -> int:
    return subprocess.run(command, cwd=ROOT).returncode


def main() -> int:
    area = sys.argv[1] if len(sys.argv) > 1 else "all"
    scenarios = GAMEPLAY_SCENARIOS | PERFORMANCE_SCENARIOS | CONTRACT_SCENARIOS.keys()
    if area in scenarios or area == "validation":
        scenario = "all" if area == "validation" else area
        return run([sys.executable, str(ROOT / "tools" / "validate.py"), scenario, *sys.argv[2:]])
    if area == "framework":
        return run([sys.executable, "-m", "unittest", "discover", "-s", "tools/tests", "-v"])
    if area not in {"all", "native", "godot"}:
        print("usage: tools/test [all|native|godot|<scenario-id>]", file=sys.stderr)
        return 2
    if run(["cmake", "-S", str(ROOT), "-B", str(ROOT / "build"), "-DCMAKE_BUILD_TYPE=Release"]) != 0:
        return 1
    if run(["cmake", "--build", str(ROOT / "build")]) != 0:
        return 1
    if area in {"all", "native"} and run(["ctest", "--test-dir", str(ROOT / "build"), "--output-on-failure"]) != 0:
        return 1
    if area == "all":
        if run([sys.executable, "-m", "unittest", "discover", "-s", "tools/tests", "-v"]) != 0:
            return 1
        if run([sys.executable, str(ROOT / "scripts/validate_faction_prototype_mapping.py")]) != 0:
            return 1
    if area == "godot":
        return run([sys.executable, str(ROOT / "tools/validate.py"), "contracts"])
    if area == "all" and run([sys.executable, str(ROOT / "tools" / "validate.py"), "all"]) != 0:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
