"""Failure-path tests for the acceptance gate; no engine dependency required."""
import argparse
import copy
import json
import io
from contextlib import redirect_stdout
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import validate


class ValidationTests(unittest.TestCase):
    def report(self):
        return dict(schemaVersion=1, scenarioId="test", runId="run", status="PASS",
                    seed=123, startedAt="2026-09-13T00:00:00Z", durationMs=1,
                    assertions=[dict(id="moves", status="PASS", message="unit moved")],
                    metrics={}, artifacts=[], errors=[])

    def test_valid_report(self):
        self.assertEqual(validate.validate_report(self.report(), "test", "run"), [])

    def test_false_pass_and_malformed_reports_rejected(self):
        mutations = [
            {"assertions": [dict(id="moves", status="FAIL", message="unit did not move")]},
            {"assertions": []}, {"assertions": [None]},
            {"assertions": [dict(id="moves", status="PASS", message="a")] * 2},
            {"errors": ["engine crashed"]}, {"errors": None},
            {"durationMs": float("nan")}, {"durationMs": -1},
            {"schemaVersion": 99}, {"metrics": []}, {"seed": True},
            {"artifacts": [{"kind": "screenshot", "path": "../outside.png"}]},
            {"artifacts": [None]}, {"scenarioId": "other"}, {"runId": "other"},
        ]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                report = self.report()
                report.update(copy.deepcopy(mutation))
                self.assertTrue(validate.validate_report(report, "test", "run"))
        for value in (None, [], "PASS", 42):
            self.assertTrue(validate.validate_report(value, "test", "run"))

    def test_harness_requires_completion_and_clean_engine(self):
        self.assertFalse(validate.execution_errors("DONE checks=4 failures=0\n", 0, "DONE"))
        for output, code in [("", 0), ("DONE", 1), ("SCRIPT ERROR: broken\nDONE", 0),
                             ("ERROR: missing library\nDONE", 0), ("DONE failures=1", 0)]:
            self.assertTrue(validate.execution_errors(output, code, "DONE"))

    def test_contract_timeout_writes_failure_and_partial_log(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory)
            args = argparse.Namespace(scenario="unit_visual_root", seed=1, timeout=0.01, force_failure=False)
            with patch.object(validate, "godot_executable", return_value=Path("godot")), \
                 patch.object(validate.subprocess, "run", side_effect=subprocess.TimeoutExpired("godot", .01, output=b"SCRIPT ERROR: Assertion failed\n")), redirect_stdout(io.StringIO()):
                self.assertNotEqual(validate.run_contract(args, target, "now"), 0)
            report = json.loads((target / "report.json").read_text())
            self.assertEqual(report["status"], "FAIL")
            self.assertIn("deadline", report["errors"][0])
            self.assertIn("Assertion failed", (target / "engine.log").read_text())

    def test_gameplay_exit_and_report_disagreement_cannot_pass(self):
        for exit_code, mutation in [(1, {}), (0, {"assertions": [dict(id="bad", status="FAIL", message="broken")]}),
                                    (0, {"artifacts": [{"kind": "screenshot", "path": "missing.png"}]})]:
            with self.subTest(exit_code=exit_code, mutation=mutation), tempfile.TemporaryDirectory() as directory:
                target = Path(directory)
                args = argparse.Namespace(scenario="test", seed=1, timeout=1, force_failure=False,
                                          require_screenshots=False, record=False, rendered=False, update_baseline=False)
                report = self.report()
                report["runId"] = target.name
                report.update(mutation)
                (target / "report.json").write_text(json.dumps(report))
                (target / "engine-godot.log").write_text("Godot test fixture")
                with patch.object(validate, "godot_executable", return_value=Path("godot")), \
                     patch.object(validate.subprocess, "run", return_value=subprocess.CompletedProcess([], exit_code, "engine output\n")), redirect_stdout(io.StringIO()):
                    self.assertNotEqual(validate.run_gameplay(args, target, "now"), 0)
                self.assertEqual(json.loads((target / "report.json").read_text())["status"], "FAIL")

    def test_harness_environment_does_not_inherit_player_capture_or_profile(self):
        with patch.dict(validate.os.environ, {"RTS_NATIVE_FAST_FORWARD_RESULT": "1", "RTS_PROFILE_FRAMES": "50"}):
            env = validate.isolated_environment(Path("/tmp/isolated-test"))
        self.assertNotIn("RTS_NATIVE_FAST_FORWARD_RESULT", env)
        self.assertNotIn("RTS_PROFILE_FRAMES", env)


if __name__ == "__main__":
    unittest.main()
