#!/usr/bin/env python3
"""Developer-only: render the radio catalog's lines to clips with ElevenLabs.

The shipped game never runs this and never talks to ElevenLabs. It plays the
clips listed in godot/project/audio/radio/voice_manifest.json and shows text
for any line that has none.

Typical use:
  tools/generate_radio_voice.py                      # dry run: lines, cache state, characters
  tools/generate_radio_voice.py --generate           # render what is missing, for audition
  tools/generate_radio_voice.py --approve-all        # after listening: put clips in the game
  tools/generate_radio_voice.py --reprocess          # rebuild clips from the kept masters

The API key comes from ELEVENLABS_API_KEY in the environment or the ignored
.env file at the repository root. Voice ids come from the catalog, or from
ELEVENLABS_VOICE_<VOICE_SET> (for example ELEVENLABS_VOICE_ELITE_COMMAND).
Only catalog text is ever sent: never game state or player data.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import sys
import time
import urllib.error
import urllib.request
import wave

ROOT = Path(__file__).resolve().parents[1]
RADIO_DIR = ROOT / "godot" / "project" / "audio" / "radio"
API_URL = "https://api.elevenlabs.io/v1/text-to-speech/{voice_id}?output_format={output_format}"
OUTPUT_FORMAT = "pcm_24000"  # raw signed 16-bit little-endian mono
SAMPLE_RATE = 24000
PROCESSING_VERSION = 1
DEFAULT_BUDGET = 5000
CARDINALS = [("n", "north"), ("ne", "northeast"), ("e", "east"), ("se", "southeast"),
             ("s", "south"), ("sw", "southwest"), ("w", "west"), ("nw", "northwest")]
TOKEN = re.compile(r"\{([a-z_]+)\}")
LICENSE_NOTE = ("Generated with ElevenLabs text-to-speech under the generating account's plan. "
                "Commercial use needs a paid plan and a voice the account has rights to; "
                "confirm both before shipping (https://elevenlabs.io/terms-of-use).")


class GeneratorError(Exception):
    """A problem the developer can act on; printed without a traceback."""


def read_env_file(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    if not path.is_file():
        return values
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        name, value = line.split("=", 1)
        values[name.strip()] = value.strip().strip("'\"")
    return values


def load_catalog(path: Path) -> dict:
    try:
        catalog = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise GeneratorError(f"cannot read catalog {path}: {error}") from error
    if catalog.get("schema_version") != 1:
        raise GeneratorError("catalog schema_version must be 1")
    return catalog


def expand_lines(catalog: dict, events: set[str] | None = None, voice_sets: set[str] | None = None,
                 include_parked: bool = False) -> list[dict]:
    """Every clip the catalog asks for. Ids match RadioAnnouncer.clip_id."""
    lines = []
    for event in catalog.get("events", []):
        if events and event["id"] not in events:
            continue
        if event.get("status", "active") != "active" and not include_parked:
            continue
        for voice_set, variants in sorted(event.get("lines", {}).items()):
            if voice_sets and voice_set not in voice_sets:
                continue
            if voice_set not in catalog.get("voice_sets", {}):
                raise GeneratorError(f"{event['id']} uses unknown voice set {voice_set}")
            for index, template in enumerate(variants):
                tokens = set(TOKEN.findall(template))
                # A spoken line may only vary by compass point: eight clips. A
                # bearing, range or grid would need hundreds, so those stay in
                # the text detail.
                if tokens - {"cardinal"}:
                    raise GeneratorError(
                        f"{event['id']} ({voice_set} v{index + 1}) speaks {sorted(tokens - {'cardinal'})}; "
                        "only {cardinal} may be voiced")
                base = f"{voice_set}/{event['id']}.v{index + 1}"
                spoken = [(f"{base}.{code}", template.replace("{cardinal}", word)) for code, word in CARDINALS] \
                    if tokens else [(base, template)]
                for clip_id, text in spoken:
                    lines.append({"id": clip_id, "event": event["id"], "voice_set": voice_set,
                                  "variant": index + 1, "text": text})
    return lines


def voice_config(catalog: dict, voice_set: str, env: dict[str, str]) -> dict:
    config = dict(catalog["voice_sets"][voice_set].get("elevenlabs", {}))
    override = env.get("ELEVENLABS_VOICE_" + voice_set.upper())
    if override:
        config["voice_id"] = override
    config.setdefault("model_id", "eleven_multilingual_v2")
    config.setdefault("voice_settings", {})
    return config


def settings_hash(text: str, config: dict, radio: bool) -> str:
    payload = {"text": text, "voice_id": config.get("voice_id", ""), "model_id": config["model_id"],
               "voice_settings": config["voice_settings"], "output_format": OUTPUT_FORMAT,
               "processing": PROCESSING_VERSION, "radio": radio}
    return hashlib.sha256(json.dumps(payload, sort_keys=True).encode("utf-8")).hexdigest()


def synthesize(text: str, config: dict, api_key: str, retries: int = 5, sleep=time.sleep) -> bytes:
    """One ElevenLabs request. Retries rate limits and server errors."""
    body = json.dumps({"text": text, "model_id": config["model_id"],
                       "voice_settings": config["voice_settings"]}).encode("utf-8")
    request = urllib.request.Request(
        API_URL.format(voice_id=config["voice_id"], output_format=OUTPUT_FORMAT), data=body, method="POST",
        headers={"xi-api-key": api_key, "Content-Type": "application/json", "Accept": "application/octet-stream"})
    for attempt in range(retries):
        try:
            with urllib.request.urlopen(request, timeout=120) as response:
                return response.read()
        except urllib.error.HTTPError as error:
            detail = error.read().decode("utf-8", errors="replace")[:300]
            if error.code in (429, 500, 502, 503, 504) and attempt + 1 < retries:
                wait = float(error.headers.get("Retry-After") or 2 ** attempt)
                print(f"  ElevenLabs returned {error.code}; retrying in {wait:.0f} s", file=sys.stderr)
                sleep(min(wait, 60.0))
                continue
            hint = {401: "check ELEVENLABS_API_KEY", 402: "the account is out of credits or the plan lacks this feature",
                    404: "check the voice id", 422: "the request was rejected; check the voice settings"}.get(error.code, "")
            raise GeneratorError(f"ElevenLabs error {error.code}{' (' + hint + ')' if hint else ''}: {detail}") from error
        except urllib.error.URLError as error:
            if attempt + 1 < retries:
                sleep(2 ** attempt)
                continue
            raise GeneratorError(f"cannot reach ElevenLabs: {error.reason}") from error
    raise GeneratorError("ElevenLabs kept failing")


def to_samples(pcm: bytes) -> list[float]:
    count = len(pcm) // 2
    return [value / 32768.0 for value in struct.unpack(f"<{count}h", pcm[:count * 2])]


def biquad(samples: list[float], kind: str, frequency: float, q: float = 0.707) -> list[float]:
    omega = 2.0 * math.pi * frequency / SAMPLE_RATE
    alpha = math.sin(omega) / (2.0 * q)
    cosine = math.cos(omega)
    if kind == "highpass":
        b0, b1, b2 = (1 + cosine) / 2, -(1 + cosine), (1 + cosine) / 2
    else:
        b0, b1, b2 = (1 - cosine) / 2, 1 - cosine, (1 - cosine) / 2
    a0, a1, a2 = 1 + alpha, -2 * cosine, 1 - alpha
    out = []
    x1 = x2 = y1 = y2 = 0.0
    for x in samples:
        y = (b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1, y2, y1 = x1, x, y1, y
        out.append(y)
    return out


def process(pcm: bytes, radio: bool = True) -> bytes:
    """Trim dead air, optionally band-limit like a radio set, level to -3 dBFS."""
    samples = to_samples(pcm)
    if not samples:
        raise GeneratorError("ElevenLabs returned no audio")
    threshold = 10 ** (-45 / 20)
    loud = [index for index, value in enumerate(samples) if abs(value) > threshold]
    if not loud:
        raise GeneratorError("the returned audio is silent")
    pad = int(0.04 * SAMPLE_RATE)
    samples = samples[max(0, loud[0] - pad):min(len(samples), loud[-1] + pad)]
    if radio:
        # Subtle: a voice band of about 300 Hz to 3.4 kHz and light saturation.
        samples = biquad(biquad(samples, "highpass", 300.0), "lowpass", 3400.0)
        samples = [math.tanh(value * 1.6) for value in samples]
    peak = max(abs(value) for value in samples) or 1.0
    gain = 10 ** (-3 / 20) / peak
    fade_in, fade_out = int(0.005 * SAMPLE_RATE), int(0.03 * SAMPLE_RATE)
    out = bytearray()
    last = len(samples) - 1
    for index, value in enumerate(samples):
        fade = min(1.0, index / fade_in, (last - index) / fade_out)
        out.extend(struct.pack("<h", max(-32767, min(32767, round(value * gain * fade * 32767)))))
    return bytes(out)


def write_wav(path: Path, pcm: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as stream:
        stream.setnchannels(1)
        stream.setsampwidth(2)
        stream.setframerate(SAMPLE_RATE)
        stream.writeframes(pcm)


def read_wav(path: Path) -> bytes:
    with wave.open(str(path)) as stream:
        if (stream.getframerate(), stream.getnchannels(), stream.getsampwidth()) != (SAMPLE_RATE, 1, 2):
            raise GeneratorError(f"{path} is not 24 kHz mono PCM16")
        return stream.readframes(stream.getnframes())


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Workspace:
    """Where masters, audition copies, approved clips and the manifest live."""

    def __init__(self, radio_dir: Path):
        self.radio_dir = radio_dir
        self.catalog_path = radio_dir / "radio_catalog.json"
        self.manifest_path = radio_dir / "voice_manifest.json"
        self.masters = radio_dir / "masters"
        self.audition = radio_dir / "audition"
        self.clips = radio_dir / "clips"

    def load_manifest(self) -> dict:
        if self.manifest_path.is_file():
            return json.loads(self.manifest_path.read_text(encoding="utf-8"))
        return {"schema_version": 1, "generator": "tools/generate_radio_voice.py", "license": LICENSE_NOTE,
                "clips": [], "pending": []}

    def save_manifest(self, manifest: dict) -> None:
        manifest["clips"].sort(key=lambda clip: clip["id"])
        manifest["pending"].sort(key=lambda clip: clip["id"])
        self.manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    def keep_out_of_godot(self, folder: Path) -> None:
        folder.mkdir(parents=True, exist_ok=True)
        (folder / ".gdignore").touch()


def find(manifest: dict, clip_id: str):
    for group in ("clips", "pending"):
        for clip in manifest[group]:
            if clip["id"] == clip_id:
                return group, clip
    return None, None


def plan(workspace: Workspace, catalog: dict, lines: list[dict], env: dict[str, str], radio: bool, force: bool) -> list[dict]:
    manifest = workspace.load_manifest()
    for line in lines:
        config = voice_config(catalog, line["voice_set"], env)
        line["config"] = config
        line["hash"] = settings_hash(line["text"], config, radio)
        group, existing = find(manifest, line["id"])
        current = existing is not None and existing.get("settings_hash") == line["hash"] and \
            (workspace.radio_dir / existing["file"]).is_file()
        line["state"] = "render" if force or not current else ("approved" if group == "clips" else "awaiting approval")
    return lines


def print_plan(lines: list[dict], budget: int) -> int:
    characters = sum(len(line["text"]) for line in lines if line["state"] == "render")
    for line in lines:
        print(f"  [{line['state']:>17}] {line['id']}: {line['text']}")
    to_render = sum(1 for line in lines if line["state"] == "render")
    print(f"{len(lines)} lines; {to_render} to render, {characters} characters to send (budget {budget}).")
    return characters


def generate(workspace: Workspace, catalog: dict, lines: list[dict], api_key: str, radio: bool, transport=synthesize) -> int:
    manifest = workspace.load_manifest()
    workspace.keep_out_of_godot(workspace.masters)
    workspace.keep_out_of_godot(workspace.audition)
    rendered = 0
    for line in lines:
        if line["state"] != "render":
            continue
        config = line["config"]
        print(f"  rendering {line['id']}")
        pcm = transport(line["text"], config, api_key)
        master = workspace.masters / (line["id"] + ".wav")
        write_wav(master, pcm)
        audition = workspace.audition / (line["id"] + ".wav")
        write_wav(audition, process(pcm, radio))
        entry = {
            "id": line["id"], "event": line["event"], "voice_set": line["voice_set"], "variant": line["variant"],
            "text": line["text"], "file": str(audition.relative_to(workspace.radio_dir)),
            "master": str(master.relative_to(workspace.radio_dir)),
            "voice_id": config["voice_id"], "model_id": config["model_id"], "voice_settings": config["voice_settings"],
            "output_format": OUTPUT_FORMAT, "settings_hash": line["hash"],
            "master_sha256": sha256_file(master), "sha256": sha256_file(audition),
            "processing": {"version": PROCESSING_VERSION, "radio": radio,
                           "steps": "trim below -45 dBFS with 40 ms pad; "
                                    + ("300 Hz high-pass, 3.4 kHz low-pass, tanh saturation; " if radio else "")
                                    + "peak -3 dBFS; 5 ms fade-in, 30 ms fade-out; mono PCM16 24 kHz"},
            "source": "ElevenLabs text-to-speech",
        }
        # A re-render replaces any earlier copy and must be approved again.
        for group in ("clips", "pending"):
            for old in [clip for clip in manifest[group] if clip["id"] == line["id"]]:
                if group == "clips":
                    (workspace.radio_dir / old["file"]).unlink(missing_ok=True)
                manifest[group].remove(old)
        manifest["pending"].append(entry)
        # Saved after every clip, so an interrupted run keeps what it paid for.
        workspace.save_manifest(manifest)
        rendered += 1
    return rendered


def reprocess(workspace: Workspace, radio: bool) -> int:
    manifest = workspace.load_manifest()
    count = 0
    for group in ("clips", "pending"):
        for clip in manifest[group]:
            master = workspace.radio_dir / clip["master"]
            if not master.is_file():
                raise GeneratorError(f"master missing for {clip['id']}: {master}")
            write_wav(workspace.radio_dir / clip["file"], process(read_wav(master), radio))
            clip["sha256"] = sha256_file(workspace.radio_dir / clip["file"])
            clip["processing"]["radio"] = radio
            count += 1
    workspace.save_manifest(manifest)
    return count


def approve(workspace: Workspace, ids: set[str] | None) -> int:
    """Move auditioned clips into the game and list them for the announcer."""
    manifest = workspace.load_manifest()
    moved = 0
    for clip in list(manifest["pending"]):
        if ids is not None and clip["id"] not in ids:
            continue
        target = workspace.clips / (clip["id"] + ".wav")
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(workspace.radio_dir / clip["file"]), str(target))
        clip["file"] = str(target.relative_to(workspace.radio_dir))
        clip["path"] = "res://audio/radio/" + clip["file"]
        manifest["pending"].remove(clip)
        manifest["clips"].append(clip)
        moved += 1
    if ids:
        missing = ids - {clip["id"] for clip in manifest["clips"]}
        if missing:
            raise GeneratorError(f"not awaiting approval: {sorted(missing)}")
    workspace.save_manifest(manifest)
    return moved


def main(argv: list[str] | None = None, transport=synthesize, environ: dict[str, str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--radio-dir", type=Path, default=RADIO_DIR, help="folder holding radio_catalog.json")
    parser.add_argument("--generate", action="store_true", help="call ElevenLabs; without this nothing is sent")
    parser.add_argument("--event", action="append", help="only this event id (repeatable)")
    parser.add_argument("--voice-set", action="append", help="only this voice set (repeatable)")
    parser.add_argument("--include-parked", action="store_true", help="also lines for alerts not wired up yet")
    parser.add_argument("--force", action="store_true", help="render again even if the cached clip matches")
    parser.add_argument("--max-characters", type=int, default=DEFAULT_BUDGET,
                        help=f"refuse to send more than this many characters (default {DEFAULT_BUDGET})")
    parser.add_argument("--no-radio", action="store_true", help="skip the radio band-limit; level and trim only")
    parser.add_argument("--reprocess", action="store_true", help="rebuild clips from masters; no API call")
    parser.add_argument("--approve", action="append", metavar="CLIP_ID", help="put an auditioned clip in the game")
    parser.add_argument("--approve-all", action="store_true", help="put every auditioned clip in the game")
    args = parser.parse_args(argv)
    env = dict(read_env_file(ROOT / ".env"))
    env.update(os.environ if environ is None else environ)
    workspace = Workspace(args.radio_dir)
    radio = not args.no_radio
    try:
        if args.approve or args.approve_all:
            print(f"approved {approve(workspace, None if args.approve_all else set(args.approve))} clips")
            return 0
        if args.reprocess:
            print(f"reprocessed {reprocess(workspace, radio)} clips from their masters")
            return 0
        catalog = load_catalog(workspace.catalog_path)
        lines = expand_lines(catalog, set(args.event) if args.event else None,
                             set(args.voice_set) if args.voice_set else None, args.include_parked)
        if not lines:
            raise GeneratorError("no lines match that selection")
        plan(workspace, catalog, lines, env, radio, args.force)
        characters = print_plan(lines, args.max_characters)
        if not args.generate:
            print("Dry run: nothing was sent. Add --generate to render.")
            return 0
        if characters > args.max_characters:
            raise GeneratorError(f"{characters} characters exceeds --max-characters {args.max_characters}; "
                                 "narrow with --event/--voice-set or raise the budget")
        pending = [line for line in lines if line["state"] == "render"]
        if not pending:
            print("Everything is cached; nothing to render.")
            return 0
        api_key = env.get("ELEVENLABS_API_KEY", "")
        if not api_key:
            raise GeneratorError("ELEVENLABS_API_KEY is not set (environment or .env at the repository root)")
        unvoiced = sorted({line["voice_set"] for line in pending if not line["config"].get("voice_id")})
        if unvoiced:
            raise GeneratorError("no ElevenLabs voice id for: " + ", ".join(unvoiced)
                                 + " (set it in the catalog or ELEVENLABS_VOICE_<VOICE_SET>)")
        count = generate(workspace, catalog, lines, api_key, radio, transport)
        print(f"Rendered {count} clips to {workspace.audition}. Listen, then --approve-all or --approve CLIP_ID.")
        return 0
    except GeneratorError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
