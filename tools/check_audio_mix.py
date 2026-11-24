#!/usr/bin/env python3
"""Assert non-silent audition segments and unclipped PCM from capture_audio_mix.gd."""
import argparse
import array
import json
import math
from pathlib import Path
import sys
import wave


def db(value):
    return round(20 * math.log10(max(value, 1e-9)), 2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wav", type=Path)
    path = parser.parse_args().wav
    metadata = json.loads(path.with_suffix(".json").read_text())
    with wave.open(str(path)) as stream:
        assert stream.getsampwidth() == 2, "Capture must use PCM16"
        rate, channels = stream.getframerate(), stream.getnchannels()
        samples = array.array("h", stream.readframes(stream.getnframes()))
    if sys.byteorder != "little":
        samples.byteswap()
    assert metadata["capture_discarded_frames"] == 0, "Meter must not drop audio frames"
    segments = []
    for index, cue in enumerate(metadata["cues"]):
        start = int(cue["seconds"] * rate) * channels
        end_seconds = metadata["cues"][index + 1]["seconds"] if index + 1 < len(metadata["cues"]) else metadata["duration"]
        segment = samples[start:int(end_seconds * rate) * channels]
        peak = max((abs(value) / 32768 for value in segment), default=0)
        assert peak > 0.0001, f"Audition segment is silent: {cue}"
        rms = math.sqrt(sum((value / 32768) ** 2 for value in segment) / len(segment))
        segments.append({**cue, "peak_dbfs": db(peak), "rms_dbfs": db(rms)})
    peak = max(abs(value) for value in samples) / 32768
    clipped = sum(abs(value) >= 32767 for value in samples)
    assert clipped == 0, "Captured mix clips PCM16"
    assert peak < 10 ** (-1 / 20) + 0.001, "Mix exceeds its -1 dBFS ceiling"
    result = {
        "status": "PASS", "checks": len(segments) + 4,
        "rate": rate, "channels": channels, "duration": len(samples) / (rate * channels),
        "peak_dbfs": db(peak), "clipped_samples": clipped,
        "cue_timing": "AudioEffectCapture frame count on Master, not wall time",
        "segments": segments,
    }
    path.with_name("levels.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"AUDIO_MIX checks={result['checks']} failures=0 peak_dbfs={db(peak)} clipped_samples={clipped}")


if __name__ == "__main__":
    main()
