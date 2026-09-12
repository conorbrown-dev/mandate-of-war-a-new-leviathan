#!/usr/bin/env python3
"""Repository-owned gameplay-validation command for Mandate of War."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
GODOT_PROJECT = ROOT / "godot" / "project"
ARTIFACT_ROOT = ROOT / "validation" / "artifacts"
BASELINE_ROOT = ROOT / "validation" / "baselines"
GAMEPLAY_SCENARIOS = {
    "basic_selection_move",
    "strategic_zoom_transition",
    "airfield_fighter_ferry",
}
PERFORMANCE_SCENARIOS = {"simulation_scale_1000"}


def godot_executable() -> Path:
    configured = os.environ.get("GODOT_BIN")
    candidates = [
        Path(configured) if configured else None,
        ROOT / "Godot_v4.7.2-stable_linux.x86_64",
        Path(shutil.which("godot4") or "") if shutil.which("godot4") else None,
        Path(shutil.which("godot") or "") if shutil.which("godot") else None,
    ]
    for candidate in candidates:
        if candidate and candidate.is_file():
            return candidate
    raise FileNotFoundError("Godot executable not found; set GODOT_BIN or install godot4")


def run_id() -> str:
    return dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")


def write_report(path: Path, report: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def run_gameplay(args: argparse.Namespace, artifact_dir: Path, started_at: str) -> int:
    godot = godot_executable()
    env = os.environ.copy()
    env.update(
        {
            "MANDATE_VALIDATION_SCENARIO": args.scenario,
            "MANDATE_VALIDATION_ARTIFACT_DIR": str(artifact_dir),
            "MANDATE_VALIDATION_RUN_ID": artifact_dir.name,
            "MANDATE_VALIDATION_SEED": str(args.seed),
            "MANDATE_VALIDATION_STARTED_AT": started_at,
            "MANDATE_VALIDATION_FORCE_FAILURE": "1" if args.force_failure else "0",
            "MANDATE_VALIDATION_REQUIRE_SCREENSHOTS": "1" if args.require_screenshots else "0",
        }
    )
    command = [
        str(godot),
        "--path",
        str(GODOT_PROJECT),
        "--script",
        "res://validation/validation_runner.gd",
        "--resolution",
        "1280x720",
        "--fixed-fps",
        "30",
        "--log-file",
        str(artifact_dir / "engine-godot.log"),
    ]
    rendered_run = args.rendered or args.record or args.require_screenshots or args.update_baseline
    if rendered_run and not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
        report = failure_report(args, artifact_dir, started_at, "rendered evidence requested but no graphical display is available")
        write_report(artifact_dir / "report.json", report)
        return 2
    if args.record:
        video_dir = artifact_dir / "video"
        video_dir.mkdir(parents=True, exist_ok=True)
        command += ["--write-movie", str(video_dir / f"{args.scenario}.avi")]
        env["MANDATE_VALIDATION_RECORD"] = "1"
    elif not rendered_run:
        command.insert(1, "--headless")

    completed = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (artifact_dir / "engine.log").write_text(completed.stdout, encoding="utf-8")
    sys.stdout.write(completed.stdout)
    report_path = artifact_dir / "report.json"
    if not report_path.exists():
        report = failure_report(args, artifact_dir, started_at, "validation runner did not produce report.json")
        report["errors"].append(f"Godot exit code: {completed.returncode}")
        write_report(report_path, report)
        return completed.returncode or 3
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        report = failure_report(args, artifact_dir, started_at, f"invalid report.json: {exc}")
        write_report(report_path, report)
        return 4
    report.setdefault("artifacts", []).extend(
        [
            {"kind": "log", "path": "engine.log"},
            {"kind": "godotLog", "path": "engine-godot.log"},
        ]
    )
    schema_errors = validate_report(report, args.scenario, artifact_dir.name)
    if schema_errors:
        report["status"] = "FAIL"
        report.setdefault("errors", []).extend(schema_errors)
        write_report(report_path, report)
        return 5
    engine_errors = [line.strip() for line in completed.stdout.splitlines() if "SCRIPT ERROR:" in line or "VALIDATION FAIL" in line]
    if engine_errors:
        report["status"] = "FAIL"
        report.setdefault("errors", []).extend(engine_errors)
        write_report(report_path, report)
        return completed.returncode or 1
    if args.record:
        video_path = artifact_dir / "video" / f"{args.scenario}.avi"
        if not video_path.is_file() or video_path.stat().st_size == 0:
            report["status"] = "FAIL"
            report.setdefault("errors", []).append("requested video artifact is missing or empty")
            write_report(report_path, report)
            return 6
        metadata = probe_video(video_path)
        report.setdefault("artifacts", []).append(
            {
                "kind": "video",
                "path": str(video_path.relative_to(artifact_dir)),
                "bytes": video_path.stat().st_size,
                "resolution": metadata.get("resolution", "1280x720"),
                "fps": metadata.get("fps", 30.0),
                "frames": metadata.get("frames"),
                "durationSeconds": metadata.get("durationSeconds"),
                "seed": args.seed,
                "scenarioId": args.scenario,
            }
        )
        write_report(report_path, report)
    if completed.returncode != 0 or report.get("status") != "PASS":
        return completed.returncode or 1
    return apply_visual_baselines(args, artifact_dir, report)


def validate_report(report: dict, scenario: str, expected_run_id: str) -> list[str]:
    required = {"schemaVersion", "scenarioId", "runId", "status", "seed", "startedAt", "durationMs", "assertions", "metrics", "artifacts", "errors"}
    errors = [f"report missing required field: {name}" for name in sorted(required - report.keys())]
    if report.get("scenarioId") != scenario:
        errors.append("report scenarioId does not match invocation")
    if report.get("runId") != expected_run_id:
        errors.append("report runId does not match artifact directory")
    if report.get("status") not in {"PASS", "FAIL"}:
        errors.append("report status must be PASS or FAIL")
    if not isinstance(report.get("assertions"), list) or not report.get("assertions"):
        errors.append("report must contain at least one assertion")
    return errors


def probe_video(path: Path) -> dict:
    completed = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries", "stream=width,height,avg_frame_rate,nb_frames,duration", "-of", "json", str(path)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        return {}
    try:
        stream = json.loads(completed.stdout)["streams"][0]
        numerator, denominator = str(stream.get("avg_frame_rate", "0/1")).split("/")
        return {
            "resolution": f"{stream.get('width')}x{stream.get('height')}",
            "fps": float(numerator) / max(float(denominator), 1.0),
            "frames": int(stream["nb_frames"]) if str(stream.get("nb_frames", "")).isdigit() else None,
            "durationSeconds": float(stream["duration"]) if stream.get("duration") not in (None, "N/A") else None,
        }
    except (KeyError, IndexError, TypeError, ValueError, json.JSONDecodeError):
        return {}


def failure_report(args: argparse.Namespace, artifact_dir: Path, started_at: str, reason: str) -> dict:
    return {
        "schemaVersion": 1,
        "scenarioId": args.scenario,
        "runId": artifact_dir.name,
        "status": "FAIL",
        "seed": args.seed,
        "startedAt": started_at,
        "durationMs": 0,
        "assertions": [],
        "metrics": {},
        "artifacts": [],
        "warnings": [],
        "errors": [reason],
    }


def run_performance(args: argparse.Namespace, artifact_dir: Path, started_at: str) -> int:
    executable = ROOT / "build" / "rts_scale_benchmark"
    if not executable.is_file():
        report = failure_report(args, artifact_dir, started_at, f"benchmark executable not found: {executable}; run the Release build first")
        write_report(artifact_dir / "report.json", report)
        return 2
    runs = []
    assertions = []
    errors = []
    started = time.monotonic()
    log_parts = []
    for index in range(3):
        completed = subprocess.run([str(executable), "1000", "100", "100"], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        log_parts.append(f"RUN {index + 1}\n{completed.stdout}")
        csv_rows = [line for line in completed.stdout.splitlines() if re.match(r"^\d+,\d+,\d+,", line)]
        values = csv_rows[-1].split(",") if csv_rows else []
        parsed = {}
        if len(values) == 15:
            parsed = {
                "entityCount": int(values[0]),
                "measurementTicks": int(values[1]),
                "uniqueDestinations": int(values[2]),
                "commandEnqueueMs": float(values[3]),
                "coldFieldGenerationMs": float(values[4]),
                "simulationMsAvg": float(values[5]),
                "simulationMsP50": float(values[6]),
                "simulationMsP95": float(values[7]),
                "simulationMsMax": float(values[8]),
                "snapshotMsAvg": float(values[9]),
                "movedUnits": int(values[10]),
                "rssMiB": float(values[11]),
                "rssDeltaMiB": float(values[12]),
                "initialStateHash": values[13],
                "finalStateHash": values[14],
            }
        elif completed.returncode == 0:
            errors.append(f"benchmark run {index + 1} did not emit the expected CSV metrics")
        runs.append({"run": index + 1, "exitCode": completed.returncode, **parsed})
        if completed.returncode != 0:
            errors.append(f"benchmark run {index + 1} exited {completed.returncode}")
    (artifact_dir / "engine.log").write_text("\n".join(log_parts), encoding="utf-8")
    status = "FAIL" if errors or args.force_failure else "PASS"
    tick_averages = [run["simulationMsAvg"] for run in runs if "simulationMsAvg" in run]
    variance = {
        "simulationMsAvgMean": statistics.fmean(tick_averages) if tick_averages else None,
        "simulationMsAvgStdDev": statistics.pstdev(tick_averages) if len(tick_averages) > 1 else 0.0 if tick_averages else None,
        "simulationMsAvgRange": max(tick_averages) - min(tick_averages) if tick_averages else None,
    }
    assertions.append({"id": "benchmark.three-successful-runs", "status": status, "message": "three 1,000-unit benchmark runs completed with structured metrics"})
    moved_ok = len(runs) == 3 and all(run.get("movedUnits") == 1000 for run in runs)
    assertions.append({"id": "benchmark.all-units-moved", "status": "PASS" if moved_ok else "FAIL", "message": "all benchmark units moved in every run"})
    if not moved_ok:
        status = "FAIL"
        errors.append("not every benchmark run moved all 1,000 units")
    report = {
        "schemaVersion": 1,
        "scenarioId": args.scenario,
        "runId": artifact_dir.name,
        "status": status,
        "seed": args.seed,
        "startedAt": started_at,
        "durationMs": int((time.monotonic() - started) * 1000),
        "assertions": assertions,
        "metrics": {"entityCount": 1000, "warmupTicks": 100, "measurementTicks": 100, "runs": runs, "variance": variance, "methodology": "100 warm-up ticks and 100 measured cache-hit ticks per repository benchmark invocation; cold flow-field generation is reported separately"},
        "artifacts": [{"kind": "log", "path": "engine.log"}],
        "warnings": ["simulation benchmark only; this is not rendering FPS evidence"],
        "errors": errors + (["intentional failure requested"] if args.force_failure else []),
    }
    write_report(artifact_dir / "report.json", report)
    return 0 if status == "PASS" else 1


def apply_visual_baselines(args: argparse.Namespace, artifact_dir: Path, report: dict) -> int:
    screenshots = [a for a in report.get("artifacts", []) if a.get("kind") == "screenshot"]
    manifest_path = BASELINE_ROOT / "manifest.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {"schemaVersion": 1, "baselines": []}
    except json.JSONDecodeError as exc:
        report["status"] = "FAIL"
        report.setdefault("errors", []).append(f"invalid visual baseline manifest: {exc}")
        write_report(artifact_dir / "report.json", report)
        return 8
    if args.update_baseline:
        if not screenshots:
            report["status"] = "FAIL"
            report.setdefault("errors", []).append("baseline update requested but the scenario produced no screenshots")
            write_report(artifact_dir / "report.json", report)
            return 7
        if args.visual_threshold is None:
            report["status"] = "FAIL"
            report.setdefault("errors", []).append("baseline update requires an explicit --visual-threshold established from repeat captures")
            write_report(artifact_dir / "report.json", report)
            return 8
        destination = BASELINE_ROOT / args.scenario
        destination.mkdir(parents=True, exist_ok=True)
        retained = [entry for entry in manifest.get("baselines", []) if entry.get("scenarioId") != args.scenario]
        for artifact in screenshots:
            source = artifact_dir / artifact["path"]
            shutil.copy2(source, destination / source.name)
            retained.append(
                {
                    "scenarioId": args.scenario,
                    "checkpoint": artifact.get("checkpoint"),
                    "file": f"{args.scenario}/{source.name}",
                    "threshold": args.visual_threshold,
                }
            )
        manifest = {"schemaVersion": 1, "baselines": retained, "status": "active"}
        write_report(manifest_path, manifest)
        print(f"Updated {len(screenshots)} visual baseline(s) in {destination}")
        return 0
    comparator = ROOT / "tools" / "visual_compare.py"
    for artifact in screenshots:
        actual = artifact_dir / artifact["path"]
        baseline = BASELINE_ROOT / args.scenario / actual.name
        if not baseline.exists():
            continue
        manifest_entry = next(
            (
                entry
                for entry in manifest.get("baselines", [])
                if entry.get("scenarioId") == args.scenario and entry.get("checkpoint") == artifact.get("checkpoint")
            ),
            None,
        )
        threshold = args.visual_threshold if args.visual_threshold is not None else manifest_entry.get("threshold") if manifest_entry else None
        if threshold is None:
            report["status"] = "FAIL"
            report.setdefault("errors", []).append(f"baseline exists without a calibrated threshold for {artifact.get('checkpoint')}")
            write_report(artifact_dir / "report.json", report)
            return 8
        diff = artifact_dir / "diffs" / actual.name
        completed = subprocess.run(
            [sys.executable, str(comparator), str(baseline), str(actual), str(diff), "--threshold", str(threshold)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        sys.stdout.write(completed.stdout)
        score_match = re.search(r"similarity=([0-9.]+)", completed.stdout)
        comparison = {
            "checkpoint": artifact.get("checkpoint"),
            "baseline": str(baseline.relative_to(ROOT)),
            "actual": str(actual.relative_to(artifact_dir)),
            "diff": str(diff.relative_to(artifact_dir)),
            "threshold": threshold,
            "similarity": float(score_match.group(1)) if score_match else None,
            "status": "PASS" if completed.returncode == 0 else "FAIL",
        }
        report.setdefault("metrics", {}).setdefault("visualComparisons", []).append(comparison)
        report.setdefault("artifacts", []).append({"kind": "visualDiff", "path": str(diff.relative_to(artifact_dir)), "checkpoint": artifact.get("checkpoint"), "threshold": threshold})
        if completed.returncode != 0:
            report["status"] = "FAIL"
            report.setdefault("errors", []).append(f"visual comparison failed for {artifact.get('checkpoint', actual.name)}")
            write_report(artifact_dir / "report.json", report)
            return completed.returncode
    write_report(artifact_dir / "report.json", report)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Run a deterministic Mandate gameplay-validation scenario")
    parser.add_argument("scenario", nargs="?", help="scenario ID")
    parser.add_argument("--list", action="store_true", help="list scenario IDs")
    parser.add_argument("--seed", type=int, default=640640)
    parser.add_argument("--record", action="store_true", help="record fixed-step video; requires a graphical display")
    parser.add_argument("--rendered", action="store_true", help="run with the active graphical display and capture screenshot checkpoints")
    parser.add_argument("--require-screenshots", action="store_true", help="fail if rendered screenshots cannot be captured")
    parser.add_argument("--update-baseline", action="store_true", help="explicitly replace visual baselines from this successful run")
    parser.add_argument("--visual-threshold", type=float, help="explicit normalized-similarity threshold; required when accepting a baseline")
    parser.add_argument("--force-failure", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    scenarios = sorted(GAMEPLAY_SCENARIOS | PERFORMANCE_SCENARIOS)
    if args.list:
        print("\n".join(scenarios))
        return 0
    if args.scenario == "all":
        if args.record or args.rendered or args.update_baseline:
            parser.error("run rendered, recording, and baseline workflows one scenario at a time")
        result = 0
        for scenario in scenarios:
            command = [sys.executable, str(Path(__file__).resolve()), scenario, "--seed", str(args.seed)]
            if args.require_screenshots:
                command.append("--require-screenshots")
            if args.force_failure:
                command.append("--force-failure")
            completed = subprocess.run(command, cwd=ROOT)
            if completed.returncode != 0:
                result = completed.returncode
        return result
    if args.scenario not in scenarios:
        parser.error(f"unknown scenario {args.scenario!r}; use --list or 'all'")
    if args.visual_threshold is not None and not 0.0 <= args.visual_threshold <= 1.0:
        parser.error("--visual-threshold must be between 0 and 1")
    current_run = run_id()
    artifact_dir = ARTIFACT_ROOT / args.scenario / current_run
    artifact_dir.mkdir(parents=True)
    started_at = dt.datetime.now(dt.timezone.utc).isoformat()
    try:
        if args.scenario in PERFORMANCE_SCENARIOS:
            result = run_performance(args, artifact_dir, started_at)
        else:
            result = run_gameplay(args, artifact_dir, started_at)
    except (OSError, subprocess.SubprocessError) as exc:
        report = failure_report(args, artifact_dir, started_at, f"validation execution failed: {exc}")
        write_report(artifact_dir / "report.json", report)
        result = 10
    print(f"Report: {artifact_dir / 'report.json'}")
    return result


if __name__ == "__main__":
    raise SystemExit(main())
