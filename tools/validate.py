#!/usr/bin/env python3
"""Repository-owned gameplay-validation command for Mandate of War."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import math
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
    "vehicle_movement_smoke",
    "control_point_skirmish_smoke",
    "reinforcement_delivery_smoke",
    "strategic_zoom_transition",
    "oak_grove_showcase",
    "airfield_fighter_ferry",
    "road_construction",
    "ui_design_system_showcase",
}
PERFORMANCE_SCENARIOS = {"simulation_scale_1000", "combat_benchmark_2000"}
CONTRACT_SCENARIOS = {
    "native_extension_smoke": ("test.gd", "RtsExtension smoke test passed"),
    "gdextension_boundary": ("test_gdextension_boundary.gd", "GDEXTENSION_BOUNDARY checks="),
    "hud_bridge": ("test_hud_bridge.gd", "HUD_BRIDGE checks="),
    "build_catalog": ("test_build_catalog.gd", "BUILD_CATALOG checks=6 failures=0"),
    "skirmish_presentation": ("test_skirmish.gd", "GODOT_SKIRMISH_PRESENTATION"),
    "native_skirmish_match_to_result": ("test_native_skirmish.gd", "GODOT_NATIVE_SKIRMISH"),
    "terrain_contract": ("test_goal10_terrain.gd", "GODOT_GOAL10_TERRAIN"),
    "visual_registry": ("test_visual_registry.gd", "VISUAL_REGISTRY"),
    "visual_spawn_bridge": ("test_visual_spawn_bridge.gd", "VISUAL_SPAWN_BRIDGE"),
    "visual_presentation_policy": ("test_visual_presentation_policy.gd", "VISUAL_PRESENTATION_POLICY"),
    "visual_pack_compatibility": ("test_visual_pack_compatibility.gd", "VISUAL_PACK_COMPATIBILITY"),
    "visual_asset_validator": ("test_visual_asset_validator.gd", "VISUAL_ASSET_VALIDATOR checks="),
    "native_visual_ids": ("test_native_visual_ids.gd", "NATIVE_VISUAL_IDS"),
    "engineer_movement_presentation": ("test_engineer_movement_presentation.gd", "ENGINEER_MOVEMENT_PRESENTATION"),
    "order_overlay_renderer": ("test_order_overlay_renderer.gd", "order_overlay_renderer: PASS"),
    "unit_visual_root": ("test_unit_visual_root.gd", "UNIT_VISUAL_ROOT"),
    "reference_model_load": ("test_reference_model_load.gd", "REFERENCE_MODEL_LOAD checks="),
    "map_editor_model": ("test_map_editor_model.gd", "Map editor model assertions passed"),
    "map_editor_export": ("test_map_editor_export.gd", "Map editor native export assertions passed"),
    "map_editor_ui": ("test_map_editor_ui.gd", "MAP_EDITOR_UI"),
}


def execution_errors(output: str, returncode: int, sentinel: str | None = None) -> list[str]:
    errors = [line.strip() for line in output.splitlines()
              if "SCRIPT ERROR:" in line or "VALIDATION FAIL" in line
              or line.startswith("ERROR:")]
    if returncode != 0:
        errors.append(f"process exited {returncode}")
    if sentinel and not any(line.startswith(sentinel) for line in output.splitlines()):
        errors.append(f"missing completion sentinel: {sentinel}")
    if any(int(count) > 0 for count in re.findall(r"\bfailures=(\d+)", output)):
        errors.append("harness reported failed checks")
    return errors


def isolated_environment(artifact_dir: Path) -> dict[str, str]:
    env = os.environ.copy()
    for name in ("UNIT_COUNT", "RTS_PROFILE_FRAMES", "RTS_PROTOTYPE_VISUALS", "RTS_PROTOTYPE_VISUAL_LIMIT",
                 "RTS_AUTO_START_SKIRMISH", "RTS_AUTO_START_NATIVE_SKIRMISH",
                 "RTS_NATIVE_FAST_FORWARD_RESULT", "RTS_CAPTURE_NATIVE_SCREENSHOT"):
        env.pop(name, None)
    # Godot user:// must not overwrite the player's latest replay or stats.
    if sys.platform.startswith("linux"):
        env["XDG_DATA_HOME"] = str(artifact_dir / "user-data")
        env["XDG_CONFIG_HOME"] = str(artifact_dir / "user-config")
        env["XDG_CACHE_HOME"] = str(artifact_dir / "user-cache")
    return env


def run_contract(args: argparse.Namespace, artifact_dir: Path, started_at: str) -> int:
    script, sentinel = CONTRACT_SCENARIOS[args.scenario]
    started = time.monotonic()
    command = [str(godot_executable()), "--headless", "--path", str(GODOT_PROJECT),
               "--script", f"res://{script}", "--log-file", str(artifact_dir / "engine-godot.log")]
    try:
        completed = subprocess.run(command, cwd=ROOT, env=isolated_environment(artifact_dir),
                                   text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   timeout=args.timeout)
        output = completed.stdout
        errors = execution_errors(output, completed.returncode, sentinel)
    except subprocess.TimeoutExpired as exc:
        output = exc.stdout or b""
        if isinstance(output, bytes):
            output = output.decode("utf-8", errors="replace")
        errors = [f"harness exceeded {args.timeout:g} second deadline", *execution_errors(output, 1, sentinel)]
    (artifact_dir / "engine.log").write_text(output, encoding="utf-8")
    sys.stdout.write(output)
    if args.force_failure:
        errors.append("intentional failure requested")
    report = failure_report(args, artifact_dir, started_at, "")
    report.update(status="FAIL" if errors else "PASS", errors=errors,
                  durationMs=int((time.monotonic() - started) * 1000),
                  assertions=[{"id": "harness.completed", "status": "FAIL" if errors else "PASS",
                               "message": f"{script}: assertions completed without engine errors"}],
                  metrics={"harness": script, "evidenceTier": "contract"},
                  artifacts=[{"kind": "log", "path": "engine.log"}],
                  warnings=["Contract harness; individual checks are in engine.log. No rendered evidence captured."])
    write_report(artifact_dir / "report.json", report)
    return 1 if errors else 0


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
    env = isolated_environment(artifact_dir)
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
        args.resolution,
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

    try:
        completed = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=args.timeout)
    except subprocess.TimeoutExpired as exc:
        output = exc.stdout or b""
        if isinstance(output, bytes):
            output = output.decode("utf-8", errors="replace")
        (artifact_dir / "engine.log").write_text(output, encoding="utf-8")
        report = failure_report(args, artifact_dir, started_at, f"scenario exceeded {args.timeout:g} second deadline")
        report["artifacts"] = [{"kind": "log", "path": "engine.log"}]
        write_report(artifact_dir / "report.json", report)
        return 124
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
    schema_errors = validate_report(report, args.scenario, artifact_dir.name)
    if schema_errors:
        invalid_report = failure_report(args, artifact_dir, started_at, "; ".join(schema_errors))
        write_report(artifact_dir / "invalid-report.json", report)
        write_report(report_path, invalid_report)
        return 5
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
    engine_errors = execution_errors(completed.stdout, completed.returncode)
    for artifact in report["artifacts"]:
        path = artifact_dir / artifact["path"]
        if not path.is_file() or path.stat().st_size == 0:
            engine_errors.append(f"missing or empty artifact: {artifact['path']}")
    if args.require_screenshots and not any(a["kind"] == "screenshot" for a in report["artifacts"]):
        engine_errors.append("requested screenshots are absent")
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
        if metadata.get("resolution") != "1280x720" or metadata.get("fps") != 30.0 or not (metadata.get("frames") or metadata.get("durationSeconds")):
            report["status"] = "FAIL"
            report["errors"].append("requested video has no verifiable 1280x720/30 FPS frame or duration metadata")
            write_report(report_path, report)
            return 6
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
    if not isinstance(report, dict):
        return ["report must be an object"]
    required = {"schemaVersion", "scenarioId", "runId", "status", "seed", "startedAt", "durationMs", "assertions", "metrics", "artifacts", "errors"}
    errors = [f"report missing required field: {name}" for name in sorted(required - report.keys())]
    if report.get("scenarioId") != scenario:
        errors.append("report scenarioId does not match invocation")
    if report.get("runId") != expected_run_id:
        errors.append("report runId does not match artifact directory")
    if report.get("status") not in ("PASS", "FAIL"):
        errors.append("report status must be PASS or FAIL")
    if not isinstance(report.get("assertions"), list) or not report.get("assertions"):
        errors.append("report must contain at least one assertion")
    else:
        seen = set()
        for assertion in report["assertions"]:
            if not isinstance(assertion, dict):
                errors.append("assertion must be an object")
                continue
            identifier = assertion.get("id")
            if not isinstance(identifier, str) or not identifier or identifier in seen:
                errors.append("assertion IDs must be non-empty unique strings")
            else:
                seen.add(identifier)
            if assertion.get("status") not in ("PASS", "FAIL") or not isinstance(assertion.get("message"), str):
                errors.append("assertion requires PASS/FAIL status and message")
            if report.get("status") == "PASS" and assertion.get("status") != "PASS":
                errors.append("PASS report contains an unsuccessful assertion")
    if type(report.get("schemaVersion")) is not int or report.get("schemaVersion") != 1:
        errors.append("unsupported schemaVersion")
    if type(report.get("seed")) is not int:
        errors.append("seed must be an integer")
    duration = report.get("durationMs")
    if type(duration) not in (int, float) or not math.isfinite(duration) or duration < 0:
        errors.append("durationMs must be finite and non-negative")
    if not isinstance(report.get("startedAt"), str) or not report.get("startedAt"):
        errors.append("startedAt must be a non-empty string")
    if not isinstance(report.get("metrics"), dict):
        errors.append("metrics must be an object")
    if not isinstance(report.get("errors"), list) or any(not isinstance(e, str) for e in report.get("errors", [])):
        errors.append("errors must be an array of strings")
    elif report.get("status") == "PASS" and report["errors"]:
        errors.append("PASS report contains errors")
    if not isinstance(report.get("artifacts"), list):
        errors.append("artifacts must be an array")
    else:
        for artifact in report["artifacts"]:
            if not isinstance(artifact, dict):
                errors.append("artifact must be an object")
                continue
            path = artifact.get("path")
            if not isinstance(artifact.get("kind"), str) or not isinstance(path, str) or not path or "\\" in path or ":" in path or Path(path).is_absolute() or ".." in Path(path).parts:
                errors.append("artifact requires a kind and a relative path within the run directory")
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
    if args.scenario == "combat_benchmark_2000":
        return run_combat_performance(args, artifact_dir, started_at)
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
        completed = subprocess.run([str(executable), "1000", "100", "100"], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=args.timeout)
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


def run_combat_performance(args: argparse.Namespace, artifact_dir: Path, started_at: str) -> int:
    executable = ROOT / "build" / "rts_combat_benchmark"
    if not executable.is_file():
        report = failure_report(args, artifact_dir, started_at, f"benchmark executable not found: {executable}; run the Release build first")
        write_report(artifact_dir / "report.json", report)
        return 2
    started = time.monotonic()
    completed = subprocess.run([str(executable), "2000", "100"], cwd=ROOT, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               timeout=args.timeout)
    output = completed.stdout
    (artifact_dir / "engine.log").write_text(output, encoding="utf-8")
    sys.stdout.write(output)
    values = {
        key: int(match.group(1)) for key, match in {
            "initialUnits": re.search(r"total initial units: (\\d+)", output),
            "finalUnits": re.search(r"total final units: (\\d+)", output),
            "unitsDestroyed": re.search(r"units destroyed: (\\d+)", output),
            "projectilesFired": re.search(r"projectiles fired: (\\d+)", output),
        }.items() if match
    }
    latency = re.search(r"cache-hit tick avg/p50/p95/max: ([0-9.]+) / ([0-9.]+) / ([0-9.]+) / ([0-9.]+)", output)
    if latency:
        values.update(cacheHitAverageMs=float(latency.group(1)), cacheHitP50Ms=float(latency.group(2)),
                      cacheHitP95Ms=float(latency.group(3)), cacheHitMaxMs=float(latency.group(4)))
    state = re.search(r"initial/final state hash: (\\d+) / (\\d+)", output)
    if state:
        values.update(initialStateHash=state.group(1), finalStateHash=state.group(2))
    required = (values.get("initialUnits") == 4000 and values.get("unitsDestroyed", 0) > 0
                and values.get("projectilesFired", 0) > 0
                and values.get("finalUnits", 4000) < values.get("initialUnits", 0)
                and values.get("initialStateHash") != values.get("finalStateHash"))
    passed = completed.returncode == 0 and required and not args.force_failure
    errors = []
    if completed.returncode:
        errors.append(f"combat benchmark exited {completed.returncode}")
    if not required:
        errors.append("combat workload lacks required active-combat state evolution")
    if args.force_failure:
        errors.append("intentional failure requested")
    report = {
        "schemaVersion": 1, "scenarioId": args.scenario, "runId": artifact_dir.name,
        "status": "PASS" if passed else "FAIL", "seed": args.seed, "startedAt": started_at,
        "durationMs": int((time.monotonic() - started) * 1000),
        "assertions": [{"id": "combat.active-workload", "status": "PASS" if passed else "FAIL",
                        "message": "2,000-vs-2,000 workload has combat, destruction, and state evolution within its benchmark gate"}],
        "metrics": values, "artifacts": [{"kind": "log", "path": "engine.log"}],
        "warnings": ["simulation benchmark only; this is not rendering FPS evidence"], "errors": errors,
    }
    write_report(artifact_dir / "report.json", report)
    return 0 if passed else 1


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
    parser.add_argument("--resolution", default="1280x720", help="render resolution WIDTHxHEIGHT for a gameplay scenario")
    parser.add_argument("--rendered", action="store_true", help="run with the active graphical display and capture screenshot checkpoints")
    parser.add_argument("--require-screenshots", action="store_true", help="fail if rendered screenshots cannot be captured")
    parser.add_argument("--update-baseline", action="store_true", help="explicitly replace visual baselines from this successful run")
    parser.add_argument("--visual-threshold", type=float, help="explicit normalized-similarity threshold; required when accepting a baseline")
    parser.add_argument("--force-failure", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--timeout", type=float, default=120.0, help="per-process deadline in seconds")
    args = parser.parse_args()
    scenarios = sorted(GAMEPLAY_SCENARIOS | PERFORMANCE_SCENARIOS | CONTRACT_SCENARIOS.keys())
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("--timeout must be positive and finite")
    if not re.fullmatch(r"[1-9]\d{2,4}x[1-9]\d{2,4}", args.resolution):
        parser.error("--resolution must be WIDTHxHEIGHT, with each dimension between 100 and 99999")
    if args.list:
        print("\n".join(scenarios))
        return 0
    if args.scenario in {"all", "contracts"}:
        if args.record or args.rendered or args.update_baseline:
            parser.error("run rendered, recording, and baseline workflows one scenario at a time")
        result = 0
        for scenario in sorted(CONTRACT_SCENARIOS) if args.scenario == "contracts" else scenarios:
            command = [sys.executable, str(Path(__file__).resolve()), scenario, "--seed", str(args.seed), "--timeout", str(args.timeout)]
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
    if args.scenario in CONTRACT_SCENARIOS.keys() | PERFORMANCE_SCENARIOS and (args.record or args.rendered or args.require_screenshots or args.update_baseline):
        parser.error("rendered evidence requires a gameplay scenario with capture checkpoints")
    if args.visual_threshold is not None and not 0.0 <= args.visual_threshold <= 1.0:
        parser.error("--visual-threshold must be between 0 and 1")
    current_run = run_id()
    artifact_dir = ARTIFACT_ROOT / args.scenario / current_run
    artifact_dir.mkdir(parents=True)
    started_at = dt.datetime.now(dt.timezone.utc).isoformat()
    try:
        if args.scenario in CONTRACT_SCENARIOS:
            result = run_contract(args, artifact_dir, started_at)
        elif args.scenario in PERFORMANCE_SCENARIOS:
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
