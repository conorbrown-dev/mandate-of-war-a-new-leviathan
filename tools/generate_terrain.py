#!/usr/bin/env python3
"""Generate Broken Strait's heightfield and presentation masks.

Writes godot/project/scenarios/terrain.json (320x320 heights in meters, in the
format documented in src/simulation/terrain.hpp) and terrain_masks.png (640x640
RGBA). Output depends only on SEED.

Mask channels:
  R  river water, 0 to 1
  G  forest density, 0 to 1
  B  baked light, where 255 is fully lit (ambient occlusion and sun shadow)
  A  large-scale ground color variation
"""

from __future__ import annotations

import argparse
import heapq
import json
import math
import random
import struct
import sys
import time
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCENARIOS = ROOT / "godot" / "project" / "scenarios"

SEED = 640641
N = 320
MASK_N = 640
WORLD = 40000.0
CELL = WORLD / (N - 1)
SEA_LEVEL = -20.0
# Toward the sun, from the DirectionalLight3D rotation in main.tscn (-38, -28).
SUN = (-0.370, 0.616, 0.696)

# Gameplay sites that must stay level: (x, z, half_x, half_z).
FLAT_ZONES = [
    (-12500.0, 0.0, 1500.0, 1700.0),
    (12500.0, 0.0, 1500.0, 1700.0),
    (-16500.0, -1500.0, 700.0, 700.0),
    (16500.0, -1500.0, 700.0, 700.0),
    (-15500.0, -9000.0, 700.0, 700.0),
    (-10500.0, 11000.0, 700.0, 700.0),
    (10500.0, -11000.0, 700.0, 700.0),
    (15500.0, 9000.0, 700.0, 700.0),
]
FLAT_MARGIN = 2300.0


def smoothstep(a: float, b: float, x: float) -> float:
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)


def coast_wobble(z: float) -> float:
    """Keep in step with HeightMap.presentation_land_mask()."""
    return (850.0 * math.sin(z / 2600.0 + 1.1) + 430.0 * math.sin(z / 1130.0 + 0.4)
            + 220.0 * math.sin(z / 510.0 + 2.3))


class Noise:
    def __init__(self, seed: int) -> None:
        rng = random.Random(seed)
        self.values = [rng.random() for _ in range(65536)]

    def value(self, x: float, y: float) -> float:
        x0 = math.floor(x)
        y0 = math.floor(y)
        tx = x - x0
        ty = y - y0
        tx = tx * tx * (3.0 - 2.0 * tx)
        ty = ty * ty * (3.0 - 2.0 * ty)
        v = self.values
        a = v[(x0 * 6271 + y0 * 19349) & 65535]
        b = v[((x0 + 1) * 6271 + y0 * 19349) & 65535]
        c = v[(x0 * 6271 + (y0 + 1) * 19349) & 65535]
        d = v[((x0 + 1) * 6271 + (y0 + 1) * 19349) & 65535]
        top = a + (b - a) * tx
        return top + (c + (d - c) * tx - top) * ty

    def fbm(self, x: float, y: float, octaves: int) -> float:
        total = 0.0
        amplitude = 0.5
        norm = 0.0
        for octave in range(octaves):
            total += self.value(x + octave * 17.3, y - octave * 9.1) * amplitude
            norm += amplitude
            amplitude *= 0.5
            x *= 2.03
            y *= 2.03
        return total / norm

    def ridged(self, x: float, y: float, octaves: int) -> float:
        total = 0.0
        amplitude = 0.5
        norm = 0.0
        weight = 1.0
        for octave in range(octaves):
            signal = 1.0 - abs(2.0 * self.value(x + octave * 31.7, y + octave * 11.9) - 1.0)
            signal *= signal * weight
            weight = min(1.0, signal * 2.0)
            total += signal * amplitude
            norm += amplitude
            amplitude *= 0.5
            x *= 2.1
            y *= 2.1
        return total / norm


def world(index: int) -> float:
    return (index / (N - 1) - 0.5) * WORLD


def zone_distance(x: float, z: float, zone: tuple[float, float, float, float]) -> float:
    """Distance beyond a rounded zone, with an irregular edge."""
    cx, cz, hx, hz = zone
    dx = (x - cx) / hx
    dz = (z - cz) / hz
    radius = math.hypot(dx, dz)
    angle = math.atan2(dz, dx)
    edge = 1.0 + 0.22 * math.sin(angle * 3.0 + cx * 0.001) + 0.13 * math.sin(angle * 7.0 + cz * 0.002)
    return max(0.0, radius - edge) * min(hx, hz)


def zone_weight(x: float, z: float) -> float:
    best = 0.0
    for zone in FLAT_ZONES:
        best = max(best, 1.0 - smoothstep(0.0, FLAT_MARGIN, zone_distance(x, z, zone)))
    return best


def blur(field: list[float], passes: int) -> list[float]:
    out = field[:]
    for _ in range(passes):
        horizontal = out[:]
        for j in range(N):
            row = j * N
            for i in range(1, N - 1):
                horizontal[row + i] = (out[row + i - 1] + 2.0 * out[row + i] + out[row + i + 1]) * 0.25
        out = horizontal[:]
        for j in range(1, N - 1):
            row = j * N
            for i in range(N):
                out[row + i] = (horizontal[row + i - N] + 2.0 * horizontal[row + i] + horizontal[row + i + N]) * 0.25
    return out


def base_heights(noise: Noise) -> list[float]:
    h = [0.0] * (N * N)
    for j in range(N):
        z = world(j)
        wobble = coast_wobble(z)
        for i in range(N):
            x = world(i)
            warp_x = x + 2600.0 * (noise.fbm(x / 7000.0 + 13.0, z / 7000.0, 3) - 0.5) * 2.0
            warp_z = z + 2600.0 * (noise.fbm(x / 7000.0, z / 7000.0 + 29.0, 3) - 0.5) * 2.0
            continent = noise.fbm(warp_x / 15000.0 + 3.0, warp_z / 15000.0 + 7.0, 3)
            ridge = noise.ridged(warp_x / 8500.0, warp_z / 8500.0, 6)
            hills = noise.fbm(warp_x / 3100.0 + 5.0, warp_z / 3100.0 + 2.0, 6)
            mountains = smoothstep(0.43, 0.56, continent)
            foothills = smoothstep(0.26, 0.48, continent)
            land = 55.0 + 1050.0 * foothills * hills ** 1.6
            land += mountains * 2750.0 * ridge ** 1.15
            ramp = smoothstep(5600.0, 11500.0, abs(x) + wobble)
            # The theater is an island pair; land sinks into open sea at its outer edge.
            edge = max(abs(x) + coast_wobble(z + 900.0) * 0.5, abs(z) + coast_wobble(x + 2100.0) * 0.5)
            ramp *= 1.0 - smoothstep(16800.0, 19400.0, edge)
            h[j * N + i] = SEA_LEVEL + (land - SEA_LEVEL) * ramp
    return h


def apply_flat_zones(h: list[float]) -> None:
    """Level gameplay sites on slight rises so drainage flows around them."""
    smooth = blur(h, 10)
    for zone in FLAT_ZONES:
        cx, cz, hx, hz = zone
        i = min(N - 1, max(0, round((cx / WORLD + 0.5) * (N - 1))))
        j = min(N - 1, max(0, round((cz / WORLD + 0.5) * (N - 1))))
        target = min(smooth[j * N + i] + 14.0, 340.0)
        reach = FLAT_MARGIN + 1.5 * max(hx, hz)
        for row in range(max(0, int((cz - reach) / CELL + N / 2) - 1), min(N, int((cz + reach) / CELL + N / 2) + 2)):
            z = world(row)
            for column in range(max(0, int((cx - reach) / CELL + N / 2) - 1), min(N, int((cx + reach) / CELL + N / 2) + 2)):
                x = world(column)
                weight = 1.0 - smoothstep(0.0, FLAT_MARGIN, zone_distance(x, z, zone))
                index = row * N + column
                h[index] = h[index] + (target - h[index]) * weight


def erode(h: list[float], droplets: int, rng: random.Random) -> None:
    """Droplet erosion, which cuts gullies and deposits sediment in valleys."""
    for _ in range(droplets):
        x = rng.uniform(1.0, N - 2.0)
        y = rng.uniform(1.0, N - 2.0)
        dir_x = dir_y = 0.0
        speed = 1.0
        water = 1.0
        sediment = 0.0
        for _step in range(48):
            ix = int(x)
            iy = int(y)
            if ix < 1 or iy < 1 or ix >= N - 2 or iy >= N - 2:
                break
            fx = x - ix
            fy = y - iy
            k = iy * N + ix
            a, b, c, d = h[k], h[k + 1], h[k + N], h[k + N + 1]
            if a <= SEA_LEVEL + 1.0:
                break
            gx = (b - a) * (1.0 - fy) + (d - c) * fy
            gy = (c - a) * (1.0 - fx) + (d - b) * fx
            dir_x = dir_x * 0.25 - gx * 0.75
            dir_y = dir_y * 0.25 - gy * 0.75
            length = math.hypot(dir_x, dir_y)
            if length < 1e-6:
                break
            dir_x /= length
            dir_y /= length
            nx = x + dir_x
            ny = y + dir_y
            nix = int(nx)
            niy = int(ny)
            if nix < 1 or niy < 1 or nix >= N - 2 or niy >= N - 2:
                break
            old = a * (1 - fx) * (1 - fy) + b * fx * (1 - fy) + c * (1 - fx) * fy + d * fx * fy
            nfx = nx - nix
            nfy = ny - niy
            m = niy * N + nix
            new = (h[m] * (1 - nfx) * (1 - nfy) + h[m + 1] * nfx * (1 - nfy)
                   + h[m + N] * (1 - nfx) * nfy + h[m + N + 1] * nfx * nfy)
            delta = new - old
            capacity = max(-delta, 0.6) * speed * water * 7.0
            if sediment > capacity or delta > 0.0:
                amount = min(delta, sediment) if delta > 0.0 else (sediment - capacity) * 0.3
                sediment -= amount
                sign = 1.0
            else:
                amount = min((capacity - sediment) * 0.3, -delta)
                sediment += amount
                sign = -1.0
            h[k] += sign * amount * (1 - fx) * (1 - fy)
            h[k + 1] += sign * amount * fx * (1 - fy)
            h[k + N] += sign * amount * (1 - fx) * fy
            h[k + N + 1] += sign * amount * fx * fy
            speed = math.sqrt(max(0.0, speed * speed - delta * 0.12))
            speed = min(speed, 8.0)
            water *= 0.97
            x = nx
            y = ny


def flow_accumulation(h: list[float]) -> tuple[list[int], list[float], list[int]]:
    """Fill pits outward from the sea, then route every land cell downhill."""
    visited = [False] * (N * N)
    receiver = [-1] * (N * N)
    order: list[int] = []
    heap: list[tuple[float, int]] = []
    for index, value in enumerate(h):
        i = index % N
        j = index // N
        edge = i in (0, N - 1) or j in (0, N - 1)
        if value <= SEA_LEVEL + 2.0 or edge:
            heapq.heappush(heap, (value, index))
            visited[index] = True
    neighbours = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]
    while heap:
        level, index = heapq.heappop(heap)
        order.append(index)
        i = index % N
        j = index // N
        for di, dj in neighbours:
            a = i + di
            b = j + dj
            if a < 0 or b < 0 or a >= N or b >= N:
                continue
            other = b * N + a
            if visited[other]:
                continue
            visited[other] = True
            receiver[other] = index
            heapq.heappush(heap, (max(level + 0.01, h[other]), other))
    accumulation = [1.0] * (N * N)
    for index in reversed(order):
        target = receiver[index]
        if target >= 0:
            accumulation[target] += accumulation[index]
    return receiver, accumulation, order


def carve_rivers(h: list[float]) -> list[tuple[int, int, float]]:
    """Cut valleys along major drainage and return (cell, receiver, strength)."""
    receiver, accumulation, order = flow_accumulation(h)
    threshold = 170.0
    reaches: list[tuple[int, int, float]] = []
    valley = [0.0] * (N * N)
    for index in reversed(order):
        flow = accumulation[index]
        target = receiver[index]
        if flow < threshold or target < 0 or h[index] <= SEA_LEVEL + 2.0:
            continue
        i = index % N
        j = index // N
        if zone_weight(world(i), world(j)) > 0.4:
            continue
        strength = min(1.0, math.sqrt(flow / threshold) / 6.0)
        reaches.append((index, target, strength))
        depth = 7.0 + 28.0 * strength
        reach = 2 + int(strength * 3.0)
        for dj in range(-reach, reach + 1):
            for di in range(-reach, reach + 1):
                a = i + di
                b = j + dj
                if a < 0 or b < 0 or a >= N or b >= N:
                    continue
                distance = math.hypot(di, dj) / (reach + 1.0)
                if distance < 1.0:
                    k = b * N + a
                    valley[k] = max(valley[k], depth * (1.0 - distance) ** 1.5)
    for index in range(N * N):
        if valley[index] > 0.0:
            h[index] = max(h[index] - valley[index], min(h[index], SEA_LEVEL + 3.0))
    # Beds only ever descend, so water never appears to run uphill.
    for index, target, _strength in reaches:
        h[target] = min(h[target], h[index] - 0.3)
    return reaches


def stamp_rivers(reaches: list[tuple[int, int, float]]) -> list[float]:
    """Draw each reach as a continuous soft line on the mask grid."""
    field = [0.0] * (MASK_N * MASK_N)
    scale = (MASK_N - 1) / (N - 1)
    for index, target, strength in reaches:
        ax, ay = (index % N) * scale, (index // N) * scale
        bx, by = (target % N) * scale, (target // N) * scale
        radius = 0.8 + 2.0 * strength
        for step in range(5):
            t = step / 4.0
            x = ax + (bx - ax) * t
            y = ay + (by - ay) * t
            span = int(radius + 2.0)
            for row in range(max(0, int(y) - span), min(MASK_N, int(y) + span + 2)):
                for column in range(max(0, int(x) - span), min(MASK_N, int(x) + span + 2)):
                    value = 1.0 - smoothstep(radius - 0.6, radius + 0.9, math.hypot(column - x, row - y))
                    k = row * MASK_N + column
                    if value > field[k]:
                        field[k] = value
    return field


def limit_slope(h: list[float], maximum: float, passes: int) -> None:
    for _ in range(passes):
        for j in range(1, N - 1):
            for i in range(1, N - 1):
                k = j * N + i
                low = min(h[k - 1], h[k + 1], h[k - N], h[k + N])
                if h[k] - low > maximum:
                    h[k] = low + maximum


def sample(field: list[float], x: float, y: float) -> float:
    x = max(0.0, min(N - 1.001, x))
    y = max(0.0, min(N - 1.001, y))
    ix = int(x)
    iy = int(y)
    fx = x - ix
    fy = y - iy
    k = iy * N + ix
    return (field[k] * (1 - fx) * (1 - fy) + field[k + 1] * fx * (1 - fy)
            + field[k + N] * (1 - fx) * fy + field[k + N + 1] * fx * fy)


def baked_light(h: list[float]) -> list[float]:
    """Combine broad occlusion with a sun shadow ray over the heightfield."""
    wide = blur(h, 6)
    light = [1.0] * (N * N)
    horizontal = math.hypot(SUN[0], SUN[2])
    step_x = SUN[0] / horizontal
    step_y = SUN[2] / horizontal
    rise = SUN[1] / horizontal * CELL
    for j in range(N):
        for i in range(N):
            k = j * N + i
            occlusion = max(0.0, min(1.0, (wide[k] - h[k]) / 150.0))
            start = h[k] + 4.0
            shadow = 0.0
            for step in range(2, 44, 2):
                ray = start + rise * step
                ground = sample(h, i + step_x * step, j + step_y * step)
                if ground > ray:
                    shadow = max(shadow, min(1.0, (ground - ray) / 60.0))
                    if shadow >= 1.0:
                        break
            light[k] = (1.0 - 0.32 * occlusion) * (1.0 - 0.52 * shadow)
    return blur(light, 1)


def build_masks(h: list[float], rivers: list[float], light: list[float], noise: Noise) -> bytearray:
    slope = [0.0] * (N * N)
    for j in range(1, N - 1):
        for i in range(1, N - 1):
            k = j * N + i
            slope[k] = math.hypot(h[k + 1] - h[k - 1], h[k + N] - h[k - N]) / (2.0 * CELL)
    scale = (N - 1) / (MASK_N - 1)
    out = bytearray(MASK_N * MASK_N * 4)
    for j in range(MASK_N):
        z = (j / (MASK_N - 1) - 0.5) * WORLD
        wobble = coast_wobble(z)
        for i in range(MASK_N):
            x = (i / (MASK_N - 1) - 0.5) * WORLD
            gx = i * scale + 2.2 * (noise.value(x / 500.0, z / 500.0) - 0.5)
            gy = j * scale + 2.2 * (noise.value(x / 500.0 + 40.0, z / 500.0) - 0.5)
            height = sample(h, gx, gy)
            mx = max(0, min(MASK_N - 1, round(gx / scale)))
            my = max(0, min(MASK_N - 1, round(gy / scale)))
            water = rivers[my * MASK_N + mx]
            clumps = noise.fbm(x / 2900.0 + 11.0, z / 2900.0 + 3.0, 5)
            suitable = (1.0 - smoothstep(0.40, 0.68, sample(slope, gx, gy)))
            suitable *= 1.0 - smoothstep(1250.0, 1850.0, height)
            suitable *= smoothstep(8200.0, 9800.0, abs(x) + wobble)
            suitable *= 1.0 - zone_weight(x, z)
            suitable *= 1.0 - water
            forest = smoothstep(0.46, 0.60, clumps) * suitable
            macro = noise.fbm(x / 1700.0 + 21.0, z / 1700.0 + 17.0, 5)
            lit = sample(light, gx, gy)
            k = (j * MASK_N + i) * 4
            out[k] = int(water * 255.0)
            out[k + 1] = int(forest * 255.0)
            out[k + 2] = int(max(0.0, min(1.0, lit)) * 255.0)
            out[k + 3] = int(max(0.0, min(1.0, (macro - 0.25) * 2.0)) * 255.0)
    return out


def png_chunk(kind: bytes, body: bytes) -> bytes:
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))


def mask_png(masks: bytearray) -> bytes:
    rows = bytearray()
    stride = MASK_N * 4
    for row in range(MASK_N):
        rows.append(0)
        rows.extend(masks[row * stride:(row + 1) * stride])
    return (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", struct.pack(">IIBBBBB", MASK_N, MASK_N, 8, 6, 0, 0, 0))
            + png_chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + png_chunk(b"IEND", b""))


def write_preview(path: Path, h: list[float], masks: bytearray) -> None:
    """Write an RGB overview image, used only to review a generated map."""
    rows = bytearray()
    top = max(h)
    for j in range(N):
        rows.append(0)
        for i in range(N):
            value = h[j * N + i]
            k = (j * 2 * MASK_N + i * 2) * 4
            water, forest, lit = masks[k], masks[k + 1], masks[k + 2]
            tone = max(0.0, value) / top
            if value < -5.0:
                color = (24, 52, 84)
            elif water > 110:
                color = (60, 130, 200)
            else:
                f = forest / 255.0
                color = (int((120 + 120 * tone) * (1 - f) + 40 * f),
                         int((140 + 90 * tone) * (1 - f) + 84 * f),
                         int((80 + 150 * tone) * (1 - f) + 44 * f))
            rows.extend(int(c * (0.35 + 0.65 * lit / 255.0)) for c in color)

    path.write_bytes(b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", struct.pack(">IIBBBBB", N, N, 8, 2, 0, 0, 0))
                     + png_chunk(b"IDAT", zlib.compress(bytes(rows))) + png_chunk(b"IEND", b""))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--droplets", type=int, default=140000)
    parser.add_argument("--preview", type=Path, help="also write an overview PNG here")
    parser.add_argument("--out", type=Path, default=SCENARIOS, help="output directory")
    args = parser.parse_args()
    started = time.monotonic()

    def log(message: str) -> None:
        print(f"[{time.monotonic() - started:6.1f}s] {message}", flush=True)

    noise = Noise(SEED)
    rng = random.Random(SEED + 1)
    h = base_heights(noise)
    log("base heights")
    apply_flat_zones(h)
    erode(h, args.droplets, rng)
    log("erosion")
    apply_flat_zones(h)
    limit_slope(h, 105.0, 4)
    reaches = carve_rivers(h)
    apply_flat_zones(h)
    rivers = stamp_rivers(reaches)
    log("rivers")
    light = baked_light(h)
    masks = build_masks(h, rivers, light, noise)
    log("masks")

    args.out.mkdir(parents=True, exist_ok=True)
    heightfield = {"format": "heightfield", "version": 1, "width": N, "height": N,
                   "heights": [round(value, 2) for value in h]}
    (args.out / "terrain.json").write_text(json.dumps(heightfield, separators=(",", ":")) + "\n")
    (args.out / "terrain_masks.png").write_bytes(mask_png(masks))
    if args.preview:
        write_preview(args.preview, h, masks)
    land = sorted(v for v in h if v > 0.0)
    log(f"land median {land[len(land) // 2]:.0f} m, max {land[-1]:.0f} m, "
        f"river reaches {len(reaches)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
