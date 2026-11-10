#!/usr/bin/env python3
"""Rebuild the two short gameplay cues from the retained CC0 firearm originals."""

import hashlib
import json
from pathlib import Path
import struct
import wave

ROOT = Path(__file__).resolve().parents[1] / "godot/project/audio"
EDITS = [("ak_single", "C_28P", 0.59, 1.14), ("smg_burst", "G_24P", 0.16, 0.57)]


def main():
    manifest = json.loads((ROOT / "manifest.json").read_text())
    (ROOT / "edited").mkdir(exist_ok=True)
    for name, source, start, end in EDITS:
        source_path = f"res://audio/free_firearms/{source}.wav"
        original = next(entry for entry in manifest["files"] if entry["path"] == source_path)
        path = ROOT / "free_firearms" / f"{source}.wav"
        assert hashlib.sha256(path.read_bytes()).hexdigest() == original["sha256"]
        with wave.open(str(path)) as stream:
            assert (stream.getframerate(), stream.getnchannels(), stream.getsampwidth()) == (96000, 2, 3)
            stream.setpos(round(start * 96000))
            data = stream.readframes(round((end - start) * 96000))
        # Four-sample box filter and stereo average before reducing 96 to 24 kHz.
        values = []
        for offset in range(0, len(data) - 24, 24):
            total = sum(int.from_bytes(data[at:at + 3], "little", signed=True)
                        for at in range(offset, offset + 24, 3))
            values.append(total / (8 * 8388608))
        gain = 10 ** (-3 / 20) / max(abs(value) for value in values)
        pcm = bytearray()
        for index, value in enumerate(values):
            fade = min(1, index / 120, (len(values) - 1 - index) / 1200)
            pcm.extend(struct.pack("<h", round(value * gain * fade * 32767)))
        output = ROOT / "edited" / f"{name}.wav"
        with wave.open(str(output), "wb") as stream:
            stream.setnchannels(1)
            stream.setsampwidth(2)
            stream.setframerate(24000)
            stream.writeframes(pcm)
        entry = {
            **original,
            "path": f"res://audio/edited/{name}.wav",
            "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
            "edits": f"Trim {start:.2f}–{end:.2f} s from {source}.wav; average stereo to mono; "
                     "four-sample box-filter downsample 96 to 24 kHz; normalize peak to -3 dBFS; "
                     "5 ms fade-in, 50 ms fade-out; PCM16.",
            "derived_from": source_path,
        }
        manifest["files"] = [existing for existing in manifest["files"] if existing["path"] != entry["path"]]
        manifest["files"].append(entry)
    (ROOT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
