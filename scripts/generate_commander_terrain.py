#!/usr/bin/env python3
"""Generate the deterministic 320x320 heightfield for Broken Strait's 40 km theater."""

import math
import struct
from pathlib import Path


WIDTH = 320
HEIGHT = 320
WORLD_WIDTH = 40000.0
WORLD_HEIGHT = 40000.0
OUTPUT = Path(__file__).parents[1] / "godot/project/scenarios/terrain.bin"

# Each faction begins on an authored construction pad rather than on whichever
# part of the procedural relief happens to be closest to its roster spawn.
# The flat core comfortably contains the largest initial structure (the
# 250 x 750 m airfield); the 900 m feathered shoulder keeps the pad from
# creating a cliff or a pathfinding trap at its boundary.
START_PADS = ((-12500.0, 0.0), (12500.0, 0.0))
PAD_CORE_HALF_WIDTH = 1400.0
PAD_CORE_HALF_DEPTH = 1900.0
PAD_BLEND_DISTANCE = 900.0


def gaussian(value: float, spread: float) -> float:
    return math.exp(-0.5 * (value / spread) ** 2)


def _base_terrain_height(x: float, z: float) -> float:
    # The middle channel is water. Land relief is generated independently on
    # both sides so the strait remains a tactically meaningful separation.
    land = 1.0 if abs(x) > 5200.0 else 0.0
    side = -1.0 if x < 0.0 else 1.0
    center = side * 12500.0
    local_x = x - center

    if not land:
        return -18.0 + 3.0 * math.sin(z / 1100.0) * math.cos(x / 900.0)

    broad = 180.0 + 65.0 * math.sin(x / 1800.0) + 45.0 * math.cos(z / 2300.0)
    hills = 150.0 * (0.5 + 0.5 * math.sin((x + z * 0.7) / 2600.0))

    # Two long mountain belts frame each landmass, with foothills spreading
    # into the playable approaches.
    ridge_a = 1450.0 * gaussian(local_x - 1800.0, 1800.0) * (0.72 + 0.28 * math.cos(z / 2600.0))
    ridge_b = 1050.0 * gaussian(local_x + 3300.0, 2200.0) * (0.70 + 0.30 * math.sin(z / 3100.0))

    # A winding low valley cuts through the interior of each landmass.
    valley_axis = 900.0 * math.sin(z / 3600.0) + 500.0 * math.sin(z / 1500.0)
    valley = -420.0 * gaussian(local_x - valley_axis, 850.0)

    # Narrow ravines branch from the mountain belts. Their width is kept
    # visible at the 125 m sample spacing while remaining traversable around.
    ravine_axis = -1800.0 + 1700.0 * math.sin(z / 4400.0)
    ravine = -260.0 * gaussian(local_x - ravine_axis, 260.0)

    micro = 38.0 * math.sin(x / 430.0 + z / 710.0) + 22.0 * math.cos(z / 290.0)
    return max(8.0, broad + hills + ridge_a + ridge_b + valley + ravine + micro)


def _smoothstep(value: float) -> float:
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)


def _start_pad_weight(x: float, z: float, center_x: float, center_z: float) -> float:
    # Use a rectangular pad because the runway footprint is rectangular. The
    # max component gives it a continuous rounded-off transition on all four
    # approaches, including the diagonals.
    x_progress = max(0.0, (abs(x - center_x) - PAD_CORE_HALF_WIDTH) / PAD_BLEND_DISTANCE)
    z_progress = max(0.0, (abs(z - center_z) - PAD_CORE_HALF_DEPTH) / PAD_BLEND_DISTANCE)
    return 1.0 - _smoothstep(max(x_progress, z_progress))


START_PAD_HEIGHTS = tuple(_base_terrain_height(x, z) for x, z in START_PADS)


def terrain_height(x: float, z: float) -> float:
    base_height = _base_terrain_height(x, z)
    for (center_x, center_z), pad_height in zip(START_PADS, START_PAD_HEIGHTS):
        weight = _start_pad_weight(x, z, center_x, center_z)
        if weight > 0.0:
            base_height += (pad_height - base_height) * weight
    return base_height


values = []
for row in range(HEIGHT):
    z = (row / (HEIGHT - 1) - 0.5) * WORLD_HEIGHT
    for column in range(WIDTH):
        x = (column / (WIDTH - 1) - 0.5) * WORLD_WIDTH
        values.append(terrain_height(x, z))

OUTPUT.write_bytes(struct.pack("<%df" % len(values), *values))
print("wrote %d samples to %s (min=%.1f max=%.1f)" % (len(values), OUTPUT, min(values), max(values)))
