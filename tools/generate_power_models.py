#!/usr/bin/env python3
"""Builds the power grid's 3D models as binary glTF (.glb) files.

These are original models made for Mandate of War: a wooden distribution pole,
a steel lattice transmission pylon and a power station. They are generated
from the measurements below, with no third-party geometry or textures, so the
output is reproducible and free of licence obligations. Run from the repository
root:

    python3 tools/generate_power_models.py

It writes the three .glb files and power_models.json, which tells the game
where each model's conductors attach. Units are metres, +Y is up, and a line
runs along the model's Z axis (crossarms lie along X).
"""

from __future__ import annotations

import json
import math
import struct
from pathlib import Path

OUTPUT = Path(__file__).resolve().parent.parent / "godot" / "project" / "assets" / "source_3d" / "power"

# name: (base colour RGB, metallic, roughness)
MATERIALS = {
    "wood": ((0.36, 0.25, 0.16), 0.0, 0.92),
    "wood_dark": ((0.24, 0.17, 0.11), 0.0, 0.9),
    "steel": ((0.56, 0.59, 0.60), 0.75, 0.48),
    "steel_dark": ((0.30, 0.33, 0.35), 0.7, 0.55),
    "ceramic": ((0.74, 0.78, 0.76), 0.0, 0.35),
    "concrete": ((0.58, 0.57, 0.54), 0.0, 0.95),
    "cladding": ((0.42, 0.48, 0.52), 0.25, 0.7),
    "roof": ((0.25, 0.28, 0.30), 0.3, 0.75),
    "stack_red": ((0.66, 0.16, 0.12), 0.0, 0.8),
    "stack_white": ((0.86, 0.86, 0.83), 0.0, 0.8),
    "transformer": ((0.33, 0.40, 0.36), 0.5, 0.6),
    "glass": ((0.62, 0.78, 0.84), 0.1, 0.2),
}


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def mul(a, scale):
    return (a[0] * scale, a[1] * scale, a[2] * scale)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def length(a):
    return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])


def unit(a):
    size = length(a)
    return (a[0] / size, a[1] / size, a[2] / size) if size > 1e-9 else (0.0, 1.0, 0.0)


def frame(direction):
    """Two unit vectors square to `direction` and to each other."""
    axis = unit(direction)
    helper = (0.0, 1.0, 0.0) if abs(axis[1]) < 0.9 else (1.0, 0.0, 0.0)
    side = unit(cross(helper, axis))
    return side, cross(axis, side)


class Model:
    def __init__(self) -> None:
        self.triangles: dict[str, list[tuple]] = {}

    def triangle(self, material: str, a, b, c) -> None:
        """Counter-clockwise seen from outside, with a flat normal."""
        self.triangles.setdefault(material, []).append((a, b, c))

    def quad(self, material: str, a, b, c, d) -> None:
        self.triangle(material, a, b, c)
        self.triangle(material, a, c, d)

    def hexahedron(self, material: str, corners) -> None:
        """corners: the four of one end, then the matching four of the other,
        each wound counter-clockwise seen from the second end looking back."""
        a, b, c, d, e, f, g, h = corners
        self.quad(material, a, d, c, b)
        self.quad(material, e, f, g, h)
        self.quad(material, a, b, f, e)
        self.quad(material, b, c, g, f)
        self.quad(material, c, d, h, g)
        self.quad(material, d, a, e, h)

    def box(self, material: str, center, size) -> None:
        hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
        x, y, z = center
        self.hexahedron(material, [
            (x - hx, y - hy, z - hz), (x + hx, y - hy, z - hz), (x + hx, y - hy, z + hz), (x - hx, y - hy, z + hz),
            (x - hx, y + hy, z - hz), (x + hx, y + hy, z - hz), (x + hx, y + hy, z + hz), (x - hx, y + hy, z + hz),
        ])

    def beam(self, material: str, start, end, thickness: float) -> None:
        """A square-section member from one point to another."""
        axis = sub(end, start)
        side, up = frame(axis)
        half = thickness / 2
        ring = [add(mul(side, sx * half), mul(up, sy * half)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        self.hexahedron(material, [add(start, offset) for offset in ring] + [add(end, offset) for offset in ring])

    def cylinder(self, material: str, start, end, start_radius: float, end_radius: float, segments: int = 10) -> None:
        axis = sub(end, start)
        side, up = frame(axis)
        lower, upper = [], []
        for index in range(segments):
            angle = 2 * math.pi * index / segments
            direction = add(mul(side, math.cos(angle)), mul(up, math.sin(angle)))
            lower.append(add(start, mul(direction, start_radius)))
            upper.append(add(end, mul(direction, end_radius)))
        for index in range(segments):
            following = (index + 1) % segments
            self.quad(material, lower[index], lower[following], upper[following], upper[index])
            self.triangle(material, start, lower[following], lower[index])
            self.triangle(material, end, upper[index], upper[following])

    def triangle_count(self) -> int:
        return sum(len(group) for group in self.triangles.values())

    def write(self, path: Path) -> None:
        names = sorted(self.triangles)
        buffer = bytearray()
        views, accessors, primitives = [], [], []
        for material_index, name in enumerate(names):
            positions, normals = [], []
            for a, b, c in self.triangles[name]:
                normal = unit(cross(sub(b, a), sub(c, a)))
                positions.extend((a, b, c))
                normals.extend((normal, normal, normal))
            low = [min(point[axis] for point in positions) for axis in range(3)]
            high = [max(point[axis] for point in positions) for axis in range(3)]
            for values, bounds in ((positions, (low, high)), (normals, None)):
                offset = len(buffer)
                for value in values:
                    buffer += struct.pack("<3f", *value)
                views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(buffer) - offset, "target": 34962})
                accessor = {"bufferView": len(views) - 1, "componentType": 5126, "count": len(values), "type": "VEC3"}
                if bounds:
                    accessor["min"], accessor["max"] = bounds
                accessors.append(accessor)
            primitives.append({"attributes": {"POSITION": len(accessors) - 2, "NORMAL": len(accessors) - 1},
                               "material": material_index, "mode": 4})
        document = {
            "asset": {"version": "2.0", "generator": "Mandate of War tools/generate_power_models.py",
                      "copyright": "Original model for Mandate of War, generated procedurally. No third-party content."},
            "scene": 0,
            "scenes": [{"nodes": [0]}],
            "nodes": [{"mesh": 0, "name": path.stem}],
            "meshes": [{"name": path.stem, "primitives": primitives}],
            "materials": [{"name": name, "pbrMetallicRoughness": {
                "baseColorFactor": [*MATERIALS[name][0], 1.0],
                "metallicFactor": MATERIALS[name][1], "roughnessFactor": MATERIALS[name][2]}} for name in names],
            "buffers": [{"byteLength": len(buffer)}],
            "bufferViews": views,
            "accessors": accessors,
        }
        text = json.dumps(document, separators=(",", ":")).encode("utf-8")
        text += b" " * (-len(text) % 4)
        buffer += b"\0" * (-len(buffer) % 4)
        total = 12 + 8 + len(text) + 8 + len(buffer)
        path.write_bytes(struct.pack("<4sII", b"glTF", 2, total) + struct.pack("<I4s", len(text), b"JSON") + text +
                         struct.pack("<I4s", len(buffer), b"BIN\0") + bytes(buffer))


def wooden_pole() -> tuple[Model, dict]:
    """A single-circuit timber distribution pole, 11 m above ground."""
    model = Model()
    height = 11.0
    model.cylinder("wood", (0, -1.0, 0), (0, height, 0), 0.19, 0.12, 9)
    model.box("wood_dark", (0, 10.2, 0), (2.7, 0.13, 0.15))
    for side in (-1, 1):
        model.beam("steel_dark", (side * 0.95, 10.14, 0.0), (0.0, 9.35, 0.0), 0.05)
    attach = []
    for x, base in ((-1.2, 10.27), (0.0, height), (1.2, 10.27)):
        model.cylinder("steel_dark", (x, base, 0), (x, base + 0.08, 0), 0.025, 0.025, 6)
        model.cylinder("ceramic", (x, base + 0.08, 0), (x, base + 0.17, 0), 0.085, 0.07, 8)
        model.cylinder("ceramic", (x, base + 0.17, 0), (x, base + 0.27, 0), 0.07, 0.04, 8)
        attach.append([x, round(base + 0.27, 3), 0.0])
    # A pole-mounted transformer makes the silhouette read as "power" up close.
    model.cylinder("transformer", (0.42, 8.0, 0), (0.42, 9.0, 0), 0.26, 0.26, 10)
    model.box("steel_dark", (0.2, 8.5, 0), (0.2, 0.08, 0.3))
    model.cylinder("steel_dark", (0, 2.4, 0), (0, 2.5, 0), 0.2, 0.2, 9)
    return model, {"height": height, "span": 70.0, "attach": attach}


def steel_pylon() -> tuple[Model, dict]:
    """A double-circuit lattice transmission tower, 40 m to its earth-wire peak."""
    model = Model()
    levels = [0.0, 6.0, 12.0, 17.0, 22.0, 26.0, 30.5, 35.0, 38.0]

    def half_width(y: float) -> float:
        return 4.2 - (4.2 - 1.0) * min(y, 26.0) / 26.0

    def corners(y: float):
        w = half_width(y)
        return [(-w, y, -w), (w, y, -w), (w, y, w), (-w, y, w)]

    for corner in corners(0.0):
        model.box("concrete", (corner[0], 0.0, corner[2]), (1.3, 1.2, 1.3))
    for lower, upper in zip(levels, levels[1:]):
        below, above = corners(lower), corners(upper)
        for index in range(4):
            following = (index + 1) % 4
            model.beam("steel", below[index], above[index], 0.26 if lower < 26.0 else 0.2)
            model.beam("steel", above[index], above[following], 0.12)
            model.beam("steel", below[index], above[following], 0.1)
            model.beam("steel", below[following], above[index], 0.1)
    for corner in corners(38.0):
        model.beam("steel", corner, (0.0, 40.0, 0.0), 0.14)
    attach = [[0.0, 40.0, 0.0]]
    for y, reach in ((26.0, 8.4), (30.5, 7.0), (35.0, 5.6)):
        w = half_width(y)
        for side in (-1, 1):
            tip = (side * reach, y + 0.35, 0.0)
            for z in (-w, w):
                model.beam("steel", (side * w, y, z), tip, 0.13)
                model.beam("steel", (side * w, y + 2.4, z), tip, 0.1)
            model.beam("steel", (side * w, y + 2.4, 0.0), (side * (w + reach) / 2, y + 1.4, 0.0), 0.08)
            # Suspension insulator string: stacked discs under the arm tip.
            for disc in range(8):
                top = y + 0.2 - disc * 0.27
                model.cylinder("ceramic", (side * reach, top, 0), (side * reach, top - 0.2, 0), 0.07, 0.16, 8)
            attach.append([side * reach, round(y + 0.2 - 8 * 0.27, 3), 0.0])
    return model, {"height": 40.0, "span": 350.0, "attach": attach, "earth_wire": 0}


def power_station() -> tuple[Model, dict]:
    """A thermal power station with a switchyard on its +X side."""
    model = Model()
    # The pad's deep skirt keeps the yard closed to the ground on a gentle slope.
    model.box("concrete", (0, -7.6, 0), (150.0, 16.0, 96.0))
    # Boiler house, turbine hall and the administration block.
    model.box("cladding", (-48, 16.4, -16), (24.0, 32.0, 30.0))
    model.box("roof", (-48, 32.9, -16), (25.0, 1.0, 31.0))
    model.box("cladding", (-12, 10.4, -16), (48.0, 20.0, 28.0))
    model.box("roof", (-12, 20.9, -16), (49.0, 1.0, 29.0))
    model.box("roof", (-12, 22.4, -16), (40.0, 2.0, 6.0))
    model.box("glass", (-12, 14.0, -1.9), (42.0, 3.0, 0.2))
    model.box("concrete", (-52, 3.9, 24), (20.0, 7.0, 12.0))
    model.box("glass", (-52, 4.6, 30.1), (16.0, 1.6, 0.2))
    model.box("roof", (-52, 7.6, 24), (21.0, 0.4, 13.0))
    # Flue stacks in aviation red and white bands.
    for x in (-58.0, -40.0):
        base, bands = 0.4, 7
        for band in range(bands):
            lower = base + band * 10.0
            radius = lambda y: 3.3 - 1.2 * (y - base) / (bands * 10.0)
            model.cylinder("stack_red" if band % 2 == 0 else "stack_white", (x, lower, 8), (x, lower + 10.0, 8),
                           radius(lower), radius(lower + 10.0), 14)
        model.box("cladding", (x, 6.0, -0.5), (4.0, 5.0, 3.0))
    # Fuel oil and water tanks.
    for x, z, radius, height in ((-16, 22, 8.0, 10.0), (4, 22, 8.0, 10.0), (-6, 38, 5.0, 7.0)):
        model.cylinder("stack_white", (x, 0.4, z), (x, 0.4 + height, z), radius, radius, 18)
        model.cylinder("roof", (x, 0.4 + height, z), (x, 0.4 + height + 1.2, z), radius, radius * 0.25, 18)
    # Switchyard: step-up transformers, busbars and two take-off gantries.
    for z in (-24.0, 0.0, 24.0):
        model.box("transformer", (26, 3.2, z), (8.0, 5.6, 6.0))
        for fin in range(5):
            model.box("steel_dark", (21.4, 3.0, z - 2.0 + fin), (1.2, 4.4, 0.35))
        model.box("steel_dark", (26, 6.6, z), (3.0, 1.2, 2.0))
        for bushing in (-1.8, 0.0, 1.8):
            model.cylinder("ceramic", (27.5, 6.0, z + bushing), (28.6, 9.2, z + bushing), 0.28, 0.14, 8)
        model.box("concrete", (26, 0.6, z), (10.0, 0.4, 8.0))
    for x in (44.0, 60.0):
        for z in (-30.0, 30.0):
            for leg in (-0.9, 0.9):
                model.beam("steel", (x + leg, 0.4, z), (x, 16.0, z), 0.22)
            for rung in range(1, 6):
                y = rung * 2.6
                spread = 0.9 * (1.0 - (y - 0.4) / 15.6)
                model.beam("steel", (x - spread, y, z), (x + spread, y, z), 0.1)
        model.box("steel", (x, 16.0, 0.0), (0.7, 0.9, 61.0))
        for z in (-20.0, -10.0, 0.0, 10.0, 20.0):
            model.cylinder("ceramic", (x, 15.5, z), (x, 13.2, z), 0.1, 0.2, 8)
    for z in (-20.0, 0.0, 20.0):
        model.cylinder("steel", (28.6, 9.3, z), (44.0, 13.1, z), 0.06, 0.06, 6)
        model.cylinder("steel", (44.0, 13.1, z), (60.0, 13.1, z), 0.06, 0.06, 6)
    # Perimeter fence.
    for x, z, width, depth in ((0, -47.6, 149.0, 0.15), (0, 47.6, 149.0, 0.15), (-74.6, 0, 0.15, 95.0), (74.6, 0, 0.15, 95.0)):
        model.box("steel_dark", (x, 1.6, z), (width, 2.4, depth))
    return model, {"height": 70.4, "footprint": [150.0, 96.0], "terminal": [60.0, 13.1, 0.0]}


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    catalog = {"units": "metres", "line_axis": "z"}
    for name, build in (("wooden_power_pole", wooden_pole), ("steel_pylon", steel_pylon), ("power_station", power_station)):
        model, details = build()
        model.write(OUTPUT / f"{name}.glb")
        details["scene"] = f"res://assets/source_3d/power/{name}.glb"
        details["triangles"] = model.triangle_count()
        catalog[name] = details
        print(f"{name}: {model.triangle_count()} triangles")
    (OUTPUT / "power_models.json").write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
