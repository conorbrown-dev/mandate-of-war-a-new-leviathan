#!/usr/bin/env python3
"""Builds original 3D models for units that used to share a donor model.

Swarm Tank, Assault Vehicle, Outrider Scout Bike, Pathfinder Scout Car and the
three airlifters (Skybridge, Packhorse, Hauler). They are generated from the
measurements below with no third-party geometry or textures, so the output is
reproducible and carries no licence obligation. They reuse the glTF writer in
generate_power_models.py. Run from the repository root:

    python3 tools/generate_unit_models.py

Writes <name>.glb and <name>.meta.json (bounds and triangle count, which the
visual asset validator reads) into godot/project/assets/source_3d/units/.
Units are metres, +Y is up, a vehicle's nose points toward -Z (Godot's forward),
its centre line is x = 0 and its wheels or gear rest on y = 0.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_power_models as base  # noqa: E402

OUTPUT = Path(__file__).resolve().parent.parent / "godot" / "project" / "assets" / "source_3d" / "units"

base.MATERIALS.update({
    # Mass Warfare: olive drab with orange markings.
    "mass_hull": ((0.30, 0.33, 0.20), 0.15, 0.82),
    "mass_dark": ((0.19, 0.21, 0.14), 0.2, 0.8),
    "mass_mark": ((0.88, 0.45, 0.10), 0.0, 0.7),
    # Elite Precision: pale blue-grey.
    "elite_hull": ((0.55, 0.60, 0.65), 0.3, 0.6),
    "elite_dark": ((0.26, 0.30, 0.35), 0.35, 0.6),
    "elite_mark": ((0.18, 0.55, 0.85), 0.0, 0.5),
    # Industrial/Experimental: sand with safety yellow.
    "ind_hull": ((0.62, 0.55, 0.40), 0.2, 0.8),
    "ind_dark": ((0.30, 0.27, 0.20), 0.25, 0.8),
    "ind_mark": ((0.92, 0.75, 0.10), 0.0, 0.65),
    "rubber": ((0.08, 0.08, 0.08), 0.0, 0.95),
    "optic": ((0.10, 0.18, 0.22), 0.6, 0.25),
    "metal": ((0.35, 0.36, 0.37), 0.7, 0.5),
})

Model = base.Model


def solid(model, material, points) -> None:
    """Eight points (a lower ring, then the matching upper ring) as a closed solid
    whose faces all point outward, whatever order the ring is given in."""
    centre = tuple(sum(point[axis] for point in points) / 8.0 for axis in range(3))
    faces = [(0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    for face in faces:
        a, b, c, d = (points[index] for index in face)
        normal = base.cross(base.sub(b, a), base.sub(c, a))
        middle = tuple((a[axis] + b[axis] + c[axis] + d[axis]) / 4.0 - centre[axis] for axis in range(3))
        if sum(normal[axis] * middle[axis] for axis in range(3)) < 0:
            b, d = d, b
        model.quad(material, a, b, c, d)


def wedge_hull(model, material, x0, x1, z_front, z_back, y_low, y_high, nose_drop, nose_run) -> None:
    """A hull block whose front top edge slopes down by nose_drop over nose_run."""
    lower = [(x0, y_low, z_front), (x1, y_low, z_front), (x1, y_low, z_back), (x0, y_low, z_back)]
    upper = [(x0, y_high - nose_drop, z_front), (x1, y_high - nose_drop, z_front), (x1, y_high, z_back), (x0, y_high, z_back)]
    # The slope reaches full height nose_run behind the nose: split into two solids.
    z_break = z_front + nose_run
    mid_low = [(x0, y_low, z_break), (x1, y_low, z_break)]
    mid_up = [(x0, y_high, z_break), (x1, y_high, z_break)]
    solid(model, material, [lower[0], lower[1], mid_low[1], mid_low[0],
                            upper[0], upper[1], mid_up[1], mid_up[0]])
    solid(model, material, [(x0, y_low, z_break), (x1, y_low, z_break), lower[2], lower[3],
                            mid_up[0], mid_up[1], upper[2], upper[3]])


def wheel(model, x, y, z, radius, width, tyre="rubber", hub="metal") -> None:
    half = width / 2
    model.cylinder(tyre, (x - half, y, z), (x + half, y, z), radius, radius, 10)
    model.cylinder(hub, (x - half - 0.01, y, z), (x + half + 0.01, y, z), radius * 0.45, radius * 0.45, 8)


def barrel(model, material, z_start, z_end, y, radius) -> None:
    model.cylinder(material, (0, y, z_start), (0, y, z_end), radius, radius * 0.9, 8)


# ---------------------------------------------------------------- Mass Warfare

def swarm_tank():
    """A cheap, low, fast light tank: a wedge hull, small turret, long thin gun."""
    m = Model()
    for side in (-1, 1):
        x = side * 1.2
        m.box("mass_dark", (x, 0.55, 0.0), (0.7, 0.9, 5.4))  # track run
        m.box("rubber", (x, 0.28, 0.0), (0.74, 0.5, 5.5))
        for z in (-2.2, -1.1, 0.0, 1.1, 2.2):
            m.cylinder("metal", (x + side * 0.37, 0.4, z), (x + side * 0.41, 0.4, z), 0.34, 0.34, 8)
        m.cylinder("metal", (x + side * 0.37, 0.75, -2.7), (x + side * 0.41, 0.75, -2.7), 0.3, 0.3, 8)
        m.cylinder("metal", (x + side * 0.37, 0.75, 2.7), (x + side * 0.41, 0.75, 2.7), 0.3, 0.3, 8)
        m.box("mass_dark", (x, 1.02, 0.0), (0.82, 0.12, 5.2))  # track guard
    wedge_hull(m, "mass_hull", -1.0, 1.0, -2.8, 2.6, 0.55, 1.4, 0.55, 1.5)
    m.box("mass_hull", (0.0, 1.0, 2.7), (2.0, 0.8, 0.3))  # engine deck rear
    solid(m, "mass_hull", [(-0.85, 1.4, -0.7), (0.85, 1.4, -0.7), (0.85, 1.4, 1.5), (-0.85, 1.4, 1.5),
                           (-0.65, 1.95, -0.5), (0.65, 1.95, -0.5), (0.7, 1.95, 1.3), (-0.7, 1.95, 1.3)])
    m.box("mass_dark", (0.0, 1.62, -0.78), (0.5, 0.34, 0.2))  # gun mantlet
    barrel(m, "mass_dark", -0.8, -3.6, 1.62, 0.07)
    m.box("optic", (0.4, 1.98, -0.1), (0.22, 0.14, 0.2))  # sight
    m.cylinder("metal", (-0.35, 1.95, 0.5), (-0.35, 2.55, 0.5), 0.02, 0.02, 5)  # antenna
    m.box("mass_mark", (0.0, 1.43, -1.3), (1.5, 0.04, 0.35))  # orange recognition stripe
    return m, "Swarm Tank"


def assault_vehicle():
    """An eight-wheeled assault vehicle with a turret autocannon and missile pods."""
    m = Model()
    axle = (-3.0, -1.0, 1.0, 3.0)
    for z in axle:
        for side in (-1, 1):
            wheel(m, side * 1.4, 0.55, z, 0.55, 0.5)
    solid(m, "mass_dark", [(-1.2, 0.5, -3.9), (1.2, 0.5, -3.9), (1.2, 0.5, 3.9), (-1.2, 0.5, 3.9),
                           (-1.2, 0.95, -3.9), (1.2, 0.95, -3.9), (1.2, 0.95, 3.9), (-1.2, 0.95, 3.9)])
    wedge_hull(m, "mass_hull", -1.5, 1.5, -4.2, 4.0, 0.9, 2.35, 0.75, 2.0)
    m.box("mass_dark", (0.0, 2.38, 1.8), (2.4, 0.1, 1.6))  # roof hatches
    solid(m, "mass_hull", [(-1.0, 2.35, -1.3), (1.0, 2.35, -1.3), (1.0, 2.35, 0.9), (-1.0, 2.35, 0.9),
                           (-0.8, 3.0, -1.1), (0.8, 3.0, -1.1), (0.85, 3.0, 0.8), (-0.85, 3.0, 0.8)])
    barrel(m, "mass_dark", -1.1, -3.4, 2.7, 0.09)
    m.box("mass_dark", (0.0, 2.7, -1.15), (0.5, 0.4, 0.3))
    for side in (-1, 1):  # missile pods on the turret flanks
        m.box("metal", (side * 1.05, 2.8, 0.0), (0.3, 0.3, 0.9))
        m.box("mass_mark", (side * 1.05, 2.8, -0.47), (0.24, 0.24, 0.04))
    m.box("optic", (0.0, 3.1, -0.2), (0.4, 0.2, 0.3))
    m.box("mass_mark", (0.0, 1.6, -3.0), (2.0, 0.04, 0.5))
    return m, "Assault Vehicle"


def scout_bike():
    """A light scout cycle: two wheels, a fairing and a sensor mast."""
    m = Model()
    for z in (-0.85, 0.85):
        m.cylinder("rubber", (-0.07, 0.36, z), (0.07, 0.36, z), 0.36, 0.36, 12)
        m.cylinder("metal", (-0.08, 0.36, z), (0.08, 0.36, z), 0.14, 0.14, 8)
    m.beam("mass_dark", (0.0, 0.4, 0.85), (0.0, 0.7, 0.0), 0.1)  # frame
    m.beam("mass_dark", (0.0, 0.7, 0.0), (0.0, 0.95, -0.7), 0.1)
    m.beam("metal", (0.0, 0.95, -0.7), (0.0, 0.4, -0.85), 0.08)  # fork
    solid(m, "mass_hull", [(-0.2, 0.55, -0.55), (0.2, 0.55, -0.55), (0.2, 0.55, 0.35), (-0.2, 0.55, 0.35),
                           (-0.15, 0.95, -0.5), (0.15, 0.95, -0.5), (0.18, 0.95, 0.3), (-0.18, 0.95, 0.3)])
    m.box("mass_dark", (0.0, 1.0, 0.55), (0.28, 0.1, 0.7))  # seat
    m.box("metal", (0.0, 1.1, -0.7), (0.7, 0.05, 0.05))  # handlebar
    m.cylinder("metal", (0.0, 0.95, 0.75), (0.0, 1.9, 0.75), 0.025, 0.02, 6)  # mast
    m.cylinder("optic", (0.0, 1.9, 0.75), (0.0, 2.05, 0.75), 0.11, 0.11, 8)
    m.box("mass_mark", (0.0, 0.97, -0.05), (0.32, 0.03, 0.4))
    return m, "Outrider Scout Bike"


# --------------------------------------------------------------- Elite Precision

def scout_car():
    """A four-wheel recon car with a faceted low-signature body and a sensor mast."""
    m = Model()
    for z in (-1.5, 1.5):
        for side in (-1, 1):
            wheel(m, side * 1.05, 0.5, z, 0.5, 0.4)
    solid(m, "elite_dark", [(-0.9, 0.45, -2.4), (0.9, 0.45, -2.4), (0.9, 0.45, 2.4), (-0.9, 0.45, 2.4),
                            (-0.9, 0.8, -2.4), (0.9, 0.8, -2.4), (0.9, 0.8, 2.4), (-0.9, 0.8, 2.4)])
    wedge_hull(m, "elite_hull", -1.05, 1.05, -2.5, 2.4, 0.75, 1.55, 0.5, 1.2)
    solid(m, "elite_hull", [(-0.85, 1.55, -0.3), (0.85, 1.55, -0.3), (0.85, 1.55, 1.7), (-0.85, 1.55, 1.7),
                            (-0.65, 2.0, 0.0), (0.65, 2.0, 0.0), (0.7, 2.0, 1.5), (-0.7, 2.0, 1.5)])
    m.box("optic", (0.0, 1.8, -0.28), (1.2, 0.3, 0.05))  # windscreen
    m.cylinder("metal", (0.0, 2.0, 1.3), (0.0, 3.0, 1.3), 0.04, 0.03, 6)  # sensor mast
    m.cylinder("optic", (0.0, 3.0, 1.3), (0.0, 3.35, 1.3), 0.17, 0.17, 10)
    m.box("elite_mark", (0.0, 1.57, -1.5), (1.4, 0.03, 0.3))
    m.beam("metal", (0.55, 1.6, 1.8), (0.55, 2.4, 2.1), 0.03)  # whip antenna
    return m, "Pathfinder Scout Car"


# ------------------------------------------------------------------- Airlifters

def fuselage(m, material, z_nose, z_tail, y, radius, nose_len, tail_len, tail_rise=0.0) -> None:
    """A cylindrical body with a tapered nose and tail along -Z..+Z."""
    z_a, z_b = z_nose + nose_len, z_tail - tail_len
    m.cylinder(material, (0, y, z_a), (0, y, z_b), radius, radius, 12)
    m.cylinder(material, (0, y, z_nose), (0, y, z_a), radius * 0.28, radius, 12)
    m.cylinder(material, (0, y + tail_rise, z_tail), (0, y, z_b), radius * 0.22, radius, 12)


def wing(m, material, side, y_root, z_root_front, z_root_back, span, z_tip_front, z_tip_back, thick, dihedral=0.0) -> None:
    x0, x1 = 0.0, side * span
    y_tip = y_root + dihedral
    solid(m, material, [(x0, y_root - thick, z_root_front), (x1, y_tip - thick * 0.5, z_tip_front),
                        (x1, y_tip - thick * 0.5, z_tip_back), (x0, y_root - thick, z_root_back),
                        (x0, y_root + thick, z_root_front), (x1, y_tip + thick * 0.5, z_tip_front),
                        (x1, y_tip + thick * 0.5, z_tip_back), (x0, y_root + thick, z_root_back)])


def tail_surfaces(m, material, y, z_front, z_back, span, fin_height, z_fin_front, z_fin_back, fin_base=None) -> None:
    base_y = y if fin_base is None else fin_base
    for side in (-1, 1):
        wing(m, material, side, y, z_front, z_back, span, z_front + 1.2, z_back, 0.06)
    solid(m, material, [(-0.08, base_y, z_fin_front), (0.08, base_y, z_fin_front), (0.08, base_y, z_fin_back), (-0.08, base_y, z_fin_back),
                        (-0.05, base_y + fin_height, z_fin_front + 1.6), (0.05, base_y + fin_height, z_fin_front + 1.6),
                        (0.05, base_y + fin_height, z_fin_back), (-0.05, base_y + fin_height, z_fin_back)])


def gear(m, y_body, spread, z_positions, wheel_radius=0.55) -> None:
    for z in z_positions:
        for side in (-1, 1):
            m.beam("metal", (side * spread, y_body, z), (side * spread, wheel_radius, z), 0.12)
            wheel(m, side * spread, wheel_radius, z, wheel_radius, 0.35)


def skybridge():
    """Elite: a sleek twin-engine high-wing transport with a T-tail."""
    m = Model()
    fuselage(m, "elite_hull", -13.0, 13.0, 3.2, 1.8, 5.0, 8.0, 0.9)
    m.box("elite_mark", (0.0, 3.2, -6.0), (3.7, 0.12, 2.0))
    m.box("optic", (0.0, 4.2, -10.2), (1.8, 0.4, 1.4))  # cockpit glazing
    for side in (-1, 1):
        wing(m, "elite_hull", side, 4.8, -3.0, 3.0, 14.5, 0.0, 2.8, 0.22, 0.8)
        m.cylinder("elite_dark", (side * 5.2, 3.9, -4.8), (side * 5.2, 3.9, -1.0), 0.62, 0.55, 10)  # engine
        m.cylinder("optic", (side * 5.2, 3.9, -4.9), (side * 5.2, 3.9, -4.8), 0.3, 0.45, 10)  # intake
        m.beam("elite_dark", (side * 5.2, 4.5, -2.6), (side * 5.2, 4.8, -1.4), 0.2)  # pylon
    tail_surfaces(m, "elite_hull", 7.8, 8.5, 12.6, 4.4, 3.9, 8.2, 12.8, fin_base=3.7)
    m.box("elite_dark", (0.0, 1.5, 6.5), (2.2, 0.8, 4.0))  # belly fairing
    gear(m, 1.6, 1.2, (-7.0, 2.0, 5.0))
    return m, "Skybridge Airlifter"


def packhorse():
    """Mass: a rugged four-propeller high-wing hauler with a rear ramp."""
    m = Model()
    fuselage(m, "mass_hull", -11.0, 12.0, 3.0, 2.1, 3.5, 6.5, 1.5)
    m.box("mass_dark", (0.0, 1.55, 8.5), (2.6, 0.12, 3.5))  # ramp
    m.box("optic", (0.0, 4.15, -8.9), (2.4, 0.5, 1.0))
    m.box("mass_mark", (0.0, 3.0, -2.0), (4.3, 0.1, 1.2))
    for side in (-1, 1):
        wing(m, "mass_hull", side, 5.1, -2.5, 2.5, 15.5, -1.2, 2.2, 0.25)
        for x in (side * 4.6, side * 9.6):
            m.cylinder("mass_dark", (x, 4.55, -4.2), (x, 4.55, -1.0), 0.55, 0.5, 10)  # nacelle
            m.cylinder("metal", (x, 4.55, -4.5), (x, 4.55, -4.2), 0.18, 0.28, 8)  # spinner
            for angle in (0.0, math.pi / 2):
                blade_x, blade_y = math.cos(angle) * 2.0, math.sin(angle) * 2.0
                m.beam("metal", (x - blade_x, 4.55 - blade_y, -4.55), (x + blade_x, 4.55 + blade_y, -4.55), 0.12)
    tail_surfaces(m, "mass_hull", 4.7, 9.0, 12.6, 5.0, 4.8, 8.8, 12.9)
    gear(m, 1.6, 1.7, (-5.0, 3.0, 6.0), 0.65)
    return m, "Packhorse Airlifter"


def hauler():
    """Industrial: a bulky four-jet freighter with a deep hold and twin fins."""
    m = Model()
    fuselage(m, "ind_hull", -17.0, 17.0, 4.2, 2.9, 6.0, 9.0, 1.2)
    m.box("ind_mark", (0.0, 4.2, -6.0), (5.9, 0.15, 2.6))
    m.box("optic", (0.0, 5.7, -13.5), (3.2, 0.5, 1.3))
    m.box("ind_dark", (0.0, 2.0, 0.0), (4.4, 0.9, 20.0))  # deep belly fairing
    for side in (-1, 1):
        wing(m, "ind_hull", side, 3.2, -2.5, 4.5, 20.0, 5.0, 8.0, 0.32, 1.4)
        for x in (side * 6.6, side * 12.4):
            m.cylinder("ind_dark", (x, 2.3, -0.5 + abs(x) * 0.25), (x, 2.3, 4.2 + abs(x) * 0.25), 0.9, 0.8, 10)
            m.cylinder("optic", (x, 2.3, -0.6 + abs(x) * 0.25), (x, 2.3, -0.5 + abs(x) * 0.25), 0.5, 0.8, 10)
        solid(m, "ind_hull", [(side * 3.0, 6.2, 12.0), (side * 3.2, 6.2, 14.4), (side * 3.2, 6.2, 16.8), (side * 3.0, 6.2, 16.8),
                              (side * 3.0, 11.0, 15.0), (side * 3.2, 11.0, 15.4), (side * 3.2, 11.0, 16.8), (side * 3.0, 11.0, 16.8)])
        wing(m, "ind_hull", side, 6.4, 13.0, 17.0, 5.5, 14.0, 17.2, 0.1)
    gear(m, 2.2, 2.2, (-9.0, 1.0, 4.0, 7.0), 0.8)
    return m, "Hauler Airlifter"



# ------------------------------------------------------------------ Cargo ships

def cargo_ship(prefix):
    """A freighter: flared bow, aft deckhouse and funnel, stacked containers."""
    hull, dark, mark = f"{prefix}_hull", f"{prefix}_dark", f"{prefix}_mark"
    m = Model()
    # Hull below the waterline is dark; the topsides carry the faction paint.
    solid(m, dark, [(-6.0, 0.0, -34.0), (6.0, 0.0, -34.0), (6.0, 0.0, 38.0), (-6.0, 0.0, 38.0),
                    (-7.0, 3.0, -36.0), (7.0, 3.0, -36.0), (7.0, 3.0, 38.0), (-7.0, 3.0, 38.0)])
    solid(m, hull, [(-7.0, 3.0, -36.0), (7.0, 3.0, -36.0), (7.0, 3.0, 38.0), (-7.0, 3.0, 38.0),
                    (-7.2, 6.0, -42.0), (7.2, 6.0, -42.0), (7.2, 6.0, 38.0), (-7.2, 6.0, 38.0)])
    # The bow rises to a sharp stem.
    solid(m, hull, [(-7.0, 3.0, -36.0), (7.0, 3.0, -36.0), (0.4, 3.0, -48.0), (-0.4, 3.0, -48.0),
                    (-7.2, 6.0, -42.0), (7.2, 6.0, -42.0), (0.4, 6.8, -49.0), (-0.4, 6.8, -49.0)])
    m.box(dark, (0.0, 6.05, -8.0), (13.8, 0.1, 80.0))  # main deck
    # Deckhouse, bridge wing and funnel aft.
    m.box(hull, (0.0, 9.5, 28.0), (11.0, 7.0, 9.0))
    m.box(hull, (0.0, 14.0, 27.0), (12.4, 2.4, 6.0))
    m.box("optic", (0.0, 14.1, 24.0), (11.0, 1.2, 0.2))
    m.cylinder(mark, (0.0, 13.0, 31.5), (0.0, 19.0, 31.5), 1.6, 1.3, 10)
    m.box(mark, (0.0, 9.5, 23.4), (11.1, 1.0, 0.2))
    # Container stacks forward of the deckhouse, in three paints.
    colours = (hull, mark, dark)
    for row, z in enumerate((-30.0, -21.0, -12.0, -3.0, 6.0, 15.0)):
        for column, x in enumerate((-4.2, 0.0, 4.2)):
            for tier in range(2 if (row + column) % 3 else 3):
                m.box(colours[(row + column + tier) % 3], (x, 6.1 + 1.3 + tier * 2.6, z), (3.6, 2.5, 8.0))
    # Two deck cranes.
    for z in (-26.0, 2.0):
        m.beam("metal", (-6.0, 6.1, z), (-6.0, 15.0, z), 0.5)
        m.beam("metal", (-6.0, 15.0, z), (0.0, 16.0, z), 0.4)
    return m, "Cargo Ship"


def elite_cargo_ship():
    model, _ = cargo_ship("elite")
    return model, "Seaspan Freighter"


def mass_cargo_ship():
    model, _ = cargo_ship("mass")
    return model, "Dockhorse Freighter"


def industrial_cargo_ship():
    model, _ = cargo_ship("ind")
    return model, "Bulkhead Freighter"



def patrol_boat(prefix):
    """A small patrol boat: a sharp hull, wheelhouse, a forward gun and a radar mast."""
    hull, dark, mark = f"{prefix}_hull", f"{prefix}_dark", f"{prefix}_mark"
    m = Model()
    solid(m, dark, [(-2.2, 0.0, -8.0), (2.2, 0.0, -8.0), (2.4, 0.0, 9.0), (-2.4, 0.0, 9.0),
                    (-2.8, 1.4, -9.0), (2.8, 1.4, -9.0), (3.0, 1.4, 9.5), (-3.0, 1.4, 9.5)])
    solid(m, hull, [(-2.8, 1.4, -9.0), (2.8, 1.4, -9.0), (3.0, 1.4, 9.5), (-3.0, 1.4, 9.5),
                    (-2.9, 2.4, -7.0), (2.9, 2.4, -7.0), (3.0, 2.4, 9.5), (-3.0, 2.4, 9.5)])
    solid(m, hull, [(-2.8, 1.4, -9.0), (2.8, 1.4, -9.0), (0.2, 1.4, -14.0), (-0.2, 1.4, -14.0),
                    (-2.9, 2.4, -7.0), (2.9, 2.4, -7.0), (0.2, 2.2, -13.0), (-0.2, 2.2, -13.0)])
    m.box(hull, (0.0, 4.0, 2.5), (4.6, 3.0, 5.0))  # wheelhouse
    m.box("optic", (0.0, 4.4, -0.05), (4.4, 1.0, 0.1))
    m.box(dark, (0.0, 5.6, 2.5), (4.8, 0.2, 5.2))  # roof
    m.cylinder(dark, (0.0, 2.4, -5.5), (0.0, 3.1, -5.5), 1.0, 0.9, 10)  # gun mount
    m.box(dark, (0.0, 3.4, -5.5), (1.2, 0.8, 1.8))
    m.cylinder("metal", (0.0, 3.4, -6.4), (0.0, 3.4, -9.4), 0.12, 0.1, 6)  # gun barrel
    m.cylinder("metal", (0.0, 5.6, 3.5), (0.0, 8.0, 3.5), 0.06, 0.05, 5)  # mast
    m.box("optic", (0.0, 8.2, 3.5), (1.6, 0.3, 0.2))  # radar bar
    m.box(mark, (0.0, 2.45, 6.5), (5.0, 0.05, 1.6))  # recognition stripe aft
    m.box(dark, (0.0, 3.0, 7.5), (2.4, 1.2, 1.5))  # rear deck housing
    return m, "Patrol Boat"


def elite_patrol_boat():
    return patrol_boat("elite")[0], "Breakwater Patrol Boat"


def mass_patrol_boat():
    return patrol_boat("mass")[0], "Pike Patrol Boat"


def industrial_patrol_boat():
    return patrol_boat("ind")[0], "Sentinel Patrol Boat"


def port():
    """A quay with a warehouse, two container gantries and mooring bollards.
    The quay edge is at +X (the water side); the dock runs along Z."""
    m = Model()
    m.box("concrete", (-30.0, 0.5, 0.0), (100.0, 1.0, 120.0))  # apron
    m.box("concrete", (22.0, 0.0, 0.0), (6.0, 2.0, 120.0))  # quay wall
    m.box("metal", (-60.0, 5.0, -30.0), (24.0, 10.0, 50.0))  # warehouse
    m.box("ind_dark", (-60.0, 10.3, -30.0), (25.0, 0.6, 51.0))
    m.box("metal", (-60.0, 4.0, 35.0), (18.0, 8.0, 30.0))
    for z in (-35.0, 25.0):  # container gantries
        for side in (-1, 1):
            m.beam("ind_mark", (side * 7.0 + 10.0, 1.0, z), (side * 7.0 + 10.0, 22.0, z), 0.9)
        m.beam("ind_mark", (3.0, 22.0, z), (30.0, 22.0, z), 1.0)
    for z in range(-50, 60, 20):
        m.box("ind_dark", (16.5, 3.0, float(z)), (1.2, 0.8, 1.2))  # bollards
    for index, z in enumerate((-20.0, 0.0, 14.0, 40.0)):
        m.box((hull := ("mass_hull", "elite_hull", "ind_hull", "mass_mark"))[index], (-20.0, 2.8, z), (6.0, 2.5, 8.0))
    return m, "Port"

MODELS = {
    "mass_swarm_tank": swarm_tank,
    "mass_assault_vehicle": assault_vehicle,
    "mass_scout_bike": scout_bike,
    "elite_scout_car": scout_car,
    "elite_airlifter": skybridge,
    "mass_airlifter": packhorse,
    "industrial_airlifter": hauler,
    "elite_cargo_ship": elite_cargo_ship,
    "mass_cargo_ship": mass_cargo_ship,
    "industrial_cargo_ship": industrial_cargo_ship,
    "elite_patrol_boat": elite_patrol_boat,
    "mass_patrol_boat": mass_patrol_boat,
    "industrial_patrol_boat": industrial_patrol_boat,
    "port": port,
}


def bounds_of(model):
    points = [p for group in model.triangles.values() for triangle in group for p in triangle]
    low = [min(p[axis] for p in points) for axis in range(3)]
    high = [max(p[axis] for p in points) for axis in range(3)]
    return low, high


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    catalog = {}
    for name, build in MODELS.items():
        model, title = build()
        model.write(OUTPUT / f"{name}.glb")
        low, high = bounds_of(model)
        meta = {
            "category": "air" if "airlifter" in name else "naval" if ("cargo_ship" in name or "patrol_boat" in name) else "structure" if name == "port" else "ground",
            "godot_forward": "-Z",
            "godot_up": "+Y",
            "bounds_m": {"x": round(high[0] - low[0], 3), "y": round(high[1] - low[1], 3), "z": round(high[2] - low[2], 3)},
            "min_y": round(low[1], 3),
            "triangles_all_exported_meshes": model.triangle_count(),
            "collision_recommendation": "Use simple BoxShape3D/CapsuleShape3D or custom RTS footprint data; do not use render mesh collision per unit.",
            "source_pipeline": "Original model generated by tools/generate_unit_models.py",
            "title": title,
        }
        (OUTPUT / f"{name}.meta.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
        catalog[name] = {"title": title, "triangles": model.triangle_count(), "bounds_m": meta["bounds_m"]}
    for name, entry in catalog.items():
        print(f"{name:22s} {entry['triangles']:6d} triangles  {entry['bounds_m']}")


if __name__ == "__main__":
    main()
