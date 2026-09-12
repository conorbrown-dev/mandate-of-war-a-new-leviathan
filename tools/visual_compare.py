#!/usr/bin/env python3
"""Tolerance-based PNG comparison using ffmpeg for portable decoding."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


def dimensions(path: Path) -> tuple[int, int]:
    completed = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries", "stream=width,height", "-of", "csv=p=0:s=x", str(path)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip())
    width, height = completed.stdout.strip().split("x")
    return int(width), int(height)


def pixels(path: Path) -> bytes:
    completed = subprocess.run(["ffmpeg", "-v", "error", "-i", str(path), "-f", "rawvideo", "-pix_fmt", "rgb24", "-"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.decode(errors="replace"))
    return completed.stdout


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", type=Path)
    parser.add_argument("actual", type=Path)
    parser.add_argument("diff", type=Path)
    parser.add_argument("--threshold", type=float, default=0.96)
    args = parser.parse_args()
    if dimensions(args.reference) != dimensions(args.actual):
        print("VISUAL FAIL: dimensions differ", file=sys.stderr)
        return 2
    expected = pixels(args.reference)
    actual = pixels(args.actual)
    if len(expected) != len(actual) or not expected:
        print("VISUAL FAIL: decoded buffers differ or are empty", file=sys.stderr)
        return 2
    normalized_difference = sum(abs(a - b) for a, b in zip(expected, actual)) / (len(actual) * 255.0)
    similarity = 1.0 - normalized_difference
    args.diff.parent.mkdir(parents=True, exist_ok=True)
    width, height = dimensions(args.actual)
    diff_pixels = bytes(abs(a - b) for a, b in zip(expected, actual))
    subprocess.run(["ffmpeg", "-v", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{width}x{height}", "-i", "-", "-frames:v", "1", "-y", str(args.diff)], input=diff_pixels, check=True)
    print(f"similarity={similarity:.6f} threshold={args.threshold:.6f} diff={args.diff}")
    return 0 if similarity >= args.threshold else 1


if __name__ == "__main__":
    raise SystemExit(main())
