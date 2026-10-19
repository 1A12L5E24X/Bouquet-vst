"""
Renders the grassy hilltop the keyboard sits on in Bouquet's photoreal mode: the crest
of a field seen from just below, its grass backlit against the sky and falling away
to either side. Rendered on a transparent background, so the plugin's sky shows
through above it.

Usage (Blender's Python module, `pip install bpy`):
    python render_hill.py --out ../../Bouquet/assets/flowers/hill.png [--size 2320x440] [--samples 48]
"""

import argparse
import math
import os
import random
import sys

import bpy
from PIL import Image, ImageEnhance
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import render_flowers as rf  # noqa: E402

RX, RY = 11.0, 7.0          # how quickly the hill falls away to the sides and towards us
CAM_DIST = 8.0
FRAME_WIDTH = 5.0           # metres across the frame at the crest


def height(x, y):
    return -x * x / (2 * RX) - y * y / (2 * RY)


def patchiness(x, y):
    """Slow variation across the field: lusher and drier patches."""
    return 0.5 + 0.25 * math.sin(x * 1.7 + 0.6) * math.sin(y * 2.3 - 1.1) + 0.25 * math.sin(x * 4.1 - y * 3.3)


def build_hill(rng, density, grass_scale=1.0, seed_rate=1.0):
    soil = rf.simple_material("soil", (0.07, 0.09, 0.03), roughness=0.95, texture=0.6)
    # The ground under the grass, so no sky shows through it.
    nu, nv, x0, x1, y0, y1 = 160, 120, -4.5, 4.5, -6.5, 2.5
    verts = [Vector((x0 + (x1 - x0) * i / nu, y0 + (y1 - y0) * j / nv, 0.0)) for j in range(nv + 1) for i in range(nu + 1)]
    for v in verts:
        v.z = height(v.x, v.y) - 0.01
    faces = [(j * (nu + 1) + i, j * (nu + 1) + i + 1, (j + 1) * (nu + 1) + i + 1, (j + 1) * (nu + 1) + i)
             for j in range(nv) for i in range(nu)]
    mesh = bpy.data.meshes.new("ground")
    mesh.from_pydata(verts, [], faces)
    ob = bpy.data.objects.new("ground", mesh)
    bpy.context.scene.collection.objects.link(ob)
    ob.data.materials.append(soil)

    # Grass, in clumps: denser and taller near the crest, where it is seen against the sky.
    blades = []
    seeds = []
    y = y0 + 0.4
    while y < 1.6:
        dist = CAM_DIST + y
        half = FRAME_WIDTH / 2 * dist / CAM_DIST + 0.4
        # Rows are spaced so they stay roughly even on screen.
        step = 0.012 * dist / CAM_DIST
        n = int(half * 2 * density * step * (1.0 + max(0.0, -y) * 0.35))
        for _ in range(n):
            x = rng.uniform(-half, half)
            yy = y + rng.uniform(0, step)
            p = patchiness(x, yy)
            near_crest = math.exp(-(yy * yy) / 0.6)
            h = rng.uniform(0.05, 0.11) * (0.8 + 0.5 * p) * (1.0 + 0.35 * near_crest) * grass_scale
            a = rng.uniform(0, 2 * math.pi)
            lean = rng.uniform(0.1, 0.7)
            d = Vector((math.cos(a) * lean, math.sin(a) * lean * 0.7, 1.0))
            tone = min(1.0, max(0.0, rng.random() * 0.7 + (1.0 - p) * 0.5))
            blades.append((Vector((x, yy, height(x, yy))), d, h, rng.uniform(0.0012, 0.0024) * (1 + h * 4),
                           rng.uniform(0.1, 0.8), rng.uniform(-0.6, 0.6), tone))
            if rng.random() < 0.004 * (0.4 + p) * seed_rate:
                seeds.append((x, yy))
        y += step
    print(f"{len(blades)} blades, {len(seeds)} seed heads", flush=True)
    rf.grass_object("grass", blades, rf.grass_material("grass", 0))

    seedmat = rf.simple_material("seed", (0.58, 0.50, 0.28), roughness=0.6, texture=0.4)
    for k, (x, yy) in enumerate(seeds):
        base = Vector((x, yy, height(x, yy)))
        top = base + Vector((rng.uniform(-0.02, 0.02), 0, rng.uniform(0.13, 0.22) * grass_scale))
        rf.tube_along(f"culm{k}", [base, (base + top) / 2 + Vector((0.006, 0, 0)), top], [0.0012, 0.001, 0.0008],
                      seedmat, sides=5)
        rf.spheres(f"head{k}", [(top + Vector((0, 0, 0.006 + j * 0.0045)), 0.0028) for j in range(6)], 0.0028, seedmat)


def add_camera(width, height_px, eye, shift):
    cam = bpy.data.cameras.new("cam")
    cam.sensor_fit = "HORIZONTAL"
    cam.sensor_width = 36.0
    cam.lens = 36.0 * CAM_DIST / FRAME_WIDTH
    cam.shift_y = shift
    cam.clip_end = 100.0
    ob = bpy.data.objects.new("cam", cam)
    ob.location = (0.0, -CAM_DIST, eye)
    ob.rotation_euler = (math.radians(90.0), 0.0, 0.0)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.scene.camera = ob


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", default="2320x440")
    ap.add_argument("--samples", type=int, default=48)
    ap.add_argument("--density", type=float, default=2600.0, help="blades per square metre at the crest")
    ap.add_argument("--eye", type=float, default=-0.05, help="camera height relative to the crest")
    ap.add_argument("--shift", type=float, default=-0.04)
    ap.add_argument("--grass", type=float, default=1.0, help="grass height, relative")
    ap.add_argument("--soften", type=float, default=0.0, help="0..1: mute the colour and veil it in haze")
    ap.add_argument("--seeds", type=float, default=1.0, help="how many seed heads, relative")
    ap.add_argument("--rx", type=float, default=None, help="how gently the crest falls away to the sides")
    args = ap.parse_args(argv)
    width, height_px = (int(v) for v in args.size.split("x"))

    bpy.ops.wm.read_factory_settings(use_empty=True)
    rf.setup_render(width, height_px, args.samples)
    rf.clear_scene()
    rf.add_lights()
    if args.rx is not None:
        globals()['RX'] = args.rx
    build_hill(random.Random(11), args.density, args.grass, args.seeds)
    add_camera(width, height_px, args.eye, args.shift)
    bpy.context.scene.render.filepath = os.path.abspath(args.out)
    bpy.ops.render.render(write_still=True)
    if args.soften > 0:
        soften(bpy.context.scene.render.filepath, args.soften)


def soften(path, amount):
    """Mutes the grass so it sits quietly under the keys: less saturated, a little hazy."""
    im = Image.open(path).convert("RGBA")
    alpha = im.getchannel("A")
    rgb = ImageEnhance.Color(im.convert("RGB")).enhance(1.0 - 0.35 * amount)
    haze = Image.new("RGB", im.size, (206, 218, 230))
    rgb = Image.blend(rgb, haze, 0.22 * amount)
    rgb.putalpha(alpha)
    rgb.save(path, optimize=True)


if __name__ == "__main__":
    main()
