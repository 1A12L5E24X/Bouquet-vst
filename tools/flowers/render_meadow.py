"""
Renders the soft meadow below the keyboard in Bouquet's photoreal mode: a field of fine
grass seen from just above it, running back to a hazy horizon, with a shallow depth of
field so it reads as atmosphere rather than detail. The horizon sits behind the keys,
and the strip fades out above it, so nothing rises behind the keyboard.

Usage (Blender's Python module, `pip install bpy`):
    python render_meadow.py --out ../../Bouquet/assets/flowers/meadow.png
"""

import argparse
import math
import os
import random
import sys

import bpy
import numpy as np
from mathutils import Vector
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import render_flowers as rf  # noqa: E402

EYE = 0.25                # camera height above the field, metres
BASE = 2.0                # distance to where the keyboard stands, metres (in focus)
HFOV = math.radians(46.0)


def build(rng, density):
    # The ground, with grass from just below the frame to a little behind the keyboard.
    bpy.ops.mesh.primitive_plane_add(size=1.0, location=(0.0, 20.0, 0.0))
    plane = bpy.context.active_object
    plane.scale = (100.0, 50.0, 1.0)
    plane.data.materials.append(rf.simple_material("turf", (0.07, 0.10, 0.03), roughness=0.9, texture=0.8))

    blades = []
    y0, y1 = 1.2, BASE + 1.2
    half = y1 * math.tan(HFOV / 2) + 0.1
    for _ in range(int(2 * half * (y1 - y0) * density)):
        x = rng.uniform(-half, half)
        y = rng.uniform(y0, y1)
        if abs(x) > y * math.tan(HFOV / 2) + 0.08:
            continue
        a = rng.uniform(0, 2 * math.pi)
        lean = rng.uniform(0.1, 0.7)
        d = Vector((math.cos(a) * lean, math.sin(a) * lean * 0.6, 1.0))
        blades.append((Vector((x, y, 0.0)), d, rng.uniform(0.015, 0.035), rng.uniform(0.0007, 0.0013),
                       rng.uniform(0.2, 0.9), rng.uniform(-0.6, 0.6), rng.random()))
    print(f"{len(blades)} blades", flush=True)
    rf.grass_object("grass", blades, rf.grass_material("grass", 0))


def add_camera(width, height, base_row, fstop):
    """Points the camera so the ground at BASE lands on `base_row` (fraction from the top)."""
    cam = bpy.data.cameras.new("cam")
    cam.sensor_fit = "HORIZONTAL"
    cam.sensor_width = 36.0
    cam.lens = 18.0 / math.tan(HFOV / 2)
    cam.clip_end = 300.0
    cam.dof.use_dof = True
    cam.dof.focus_distance = math.hypot(EYE, BASE)
    cam.dof.aperture_fstop = fstop
    vhalf = math.atan(math.tan(HFOV / 2) * height / width)
    depression = math.atan(EYE / BASE)
    pitch = depression - (base_row - 0.5) * 2 * vhalf
    ob = bpy.data.objects.new("cam", cam)
    ob.location = (0.0, 0.0, EYE)
    ob.rotation_euler = (math.radians(90.0) - pitch, 0.0, 0.0)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.scene.camera = ob


def fade(path, start, end, sides):
    """Fades the strip out above `start` (fraction of the height, from the top) and,
    by `sides`, towards its left and right edges, so it melts into the sky."""
    im = Image.open(path).convert("RGBA")
    a = np.asarray(im, np.float32)
    h, w = a.shape[:2]
    v = np.clip((np.arange(h, dtype=np.float32)[:, None] / h - start) / (end - start), 0, 1)
    v = v * v * (3 - 2 * v)
    u = np.abs(np.arange(w, dtype=np.float32)[None, :] / w - 0.5) * 2
    s = 1.0 - sides * np.clip((u - 0.8) / 0.2, 0, 1) ** 2
    a[..., 3] *= v * s
    Image.fromarray(a.astype(np.uint8)).save(path, optimize=True)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", default="2320x200")
    ap.add_argument("--samples", type=int, default=48)
    ap.add_argument("--density", type=float, default=26000.0, help="blades per square metre")
    ap.add_argument("--base", type=float, default=0.52, help="where the keyboard stands, as a fraction from the top")
    ap.add_argument("--fstop", type=float, default=2.0, help="lower: blurrier foreground")
    ap.add_argument("--fade", type=float, nargs=2, default=[0.28, 0.5], help="fade-in band, fractions from the top")
    ap.add_argument("--sides", type=float, default=0.6, help="how much the strip fades towards its ends")
    args = ap.parse_args(argv)
    width, height = (int(v) for v in args.size.split("x"))

    bpy.ops.wm.read_factory_settings(use_empty=True)
    rf.setup_render(width, height, args.samples)
    rf.clear_scene()
    rf.add_lights()
    build(random.Random(3), args.density)
    add_camera(width, height, args.base, args.fstop)
    out = os.path.abspath(args.out)
    bpy.context.scene.render.filepath = out
    bpy.ops.render.render(write_still=True)
    fade(out, args.fade[0], args.fade[1], args.sides)


if __name__ == "__main__":
    main()
