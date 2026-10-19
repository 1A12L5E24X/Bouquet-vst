"""
Renders the grass the keyboard sits in, in Bouquet's photoreal mode: a bed of meadow
grass beneath the keys and banked up at either end, with blades growing up over the
keyboard's lower edge. Nothing rises behind the keys: the ground stops just behind
them, so the flowers seem to grow out of it from behind the keyboard.

The keyboard itself is a holdout (transparent) in the render. The plugin draws the
keys first and this image over them, so the grass in front overlaps the keys.

The camera is orthographic, one millimetre per window pixel, so the render lines up
with the window exactly. Rendered at twice the size for Retina screens.

Usage (Blender's Python module, `pip install bpy`):
    python render_bed.py --out ../../Bouquet/assets/flowers/bed.png
"""

import argparse
import math
import os
import random
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import render_flowers as rf  # noqa: E402

PITCH = math.radians(10.0)       # the camera looks down a little
FWD = Vector((0.0, math.cos(PITCH), -math.sin(PITCH)))
UP = Vector((0.0, math.sin(PITCH), math.cos(PITCH)))


def screen_v(p):
    """Height of a point on screen, in metres (1 mm = 1 window pixel)."""
    return p.dot(UP)


def ground_height(x, half_keys, half_frame):  # noqa: ARG001
    """The ground banks up around the ends of the keyboard, then falls away."""
    d = abs(x) - half_keys
    if d < -0.02:
        return 0.0
    return 0.03 * math.exp(-((d - 0.016) / 0.03) ** 2)


def holdout_material():
    mat = bpy.data.materials.new("keys")
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    hold = nt.nodes.new("ShaderNodeHoldout")
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(hold.outputs[0], out.inputs["Surface"])
    return mat


def build(rng, args):
    w_px, h_px = args.window
    kx0, ky0, kx1, ky1 = args.keys            # keyboard rectangle in window pixels
    contact_px = ky1 - args.sink               # where the ground meets the keys on screen
    mm = 0.001
    half_keys = (kx1 - kx0) / 2 * mm
    half_frame = w_px / 2 * mm
    cx = (kx0 + kx1) / 2 * mm - half_frame     # keyboard centre, relative to the window centre

    # The keyboard: a holdout plane facing the camera, standing in the ground at y = 0.
    v_bottom, v_top = -(ky1 - contact_px) * mm - 0.05, (contact_px - ky0) * mm
    corners = [Vector((cx - half_keys, 0, 0)) + UP * v_bottom, Vector((cx + half_keys, 0, 0)) + UP * v_bottom,
               Vector((cx + half_keys, 0, 0)) + UP * v_top, Vector((cx - half_keys, 0, 0)) + UP * v_top]
    mesh = bpy.data.meshes.new("keys")
    mesh.from_pydata(corners, [], [(0, 1, 2, 3)])
    keys = bpy.data.objects.new("keys", mesh)
    bpy.context.scene.collection.objects.link(keys)
    keys.data.materials.append(holdout_material())

    # The ground: from below the frame up to just behind the keys.
    y_front = -(h_px - contact_px) * mm / math.sin(PITCH) - 0.25
    y_back = args.behind * mm
    nu, nv = 240, 60
    verts, faces = [], []
    for j in range(nv + 1):
        y = y_front + (y_back - y_front) * j / nv
        for i in range(nu + 1):
            x = -half_frame - 0.03 + (2 * half_frame + 0.06) * i / nu
            verts.append(Vector((x, y, ground_height(x - cx, half_keys, half_frame) - 0.004)))
    for j in range(nv):
        for i in range(nu):
            a = j * (nu + 1) + i
            faces.append((a, a + 1, a + nu + 2, a + nu + 1))
    mesh = bpy.data.meshes.new("ground")
    mesh.from_pydata(verts, [], faces)
    ground = bpy.data.objects.new("ground", mesh)
    bpy.context.scene.collection.objects.link(ground)
    ground.data.materials.append(rf.simple_material("soil", (0.07, 0.08, 0.03), roughness=0.95, texture=0.6))

    blades = []
    area = (2 * half_frame + 0.06) * (y_back - y_front)
    for _ in range(int(area * args.density)):
        x = rng.uniform(-half_frame - 0.03, half_frame + 0.03)
        y = rng.uniform(y_front, y_back)
        z = ground_height(x - cx, half_keys, half_frame)
        h = rng.uniform(0.007, 0.016) * args.grass
        if y > 0.0:
            h *= 0.6                               # behind the keys: never above them
        elif y > -0.03 and abs(x - cx) < half_keys:
            h *= 0.45 + 0.55 * (-y / 0.03)         # just in front: only tips over the keys' lower edge
        a = rng.uniform(0, 2 * math.pi)
        lean = rng.uniform(0.15, 0.8)
        d = Vector((math.cos(a) * lean, math.sin(a) * lean * 0.6, 1.0))
        blades.append((Vector((x, y, z)), d, h, rng.uniform(0.0006, 0.0011), rng.uniform(0.2, 0.9),
                       rng.uniform(-0.6, 0.6), rng.random()))
    print(f"{len(blades)} blades", flush=True)
    rf.grass_object("grass", blades, rf.grass_material("grass", 0))

    # Orthographic camera: the window's width across the frame, centred on the strip rendered.
    cam = bpy.data.cameras.new("cam")
    cam.type = "ORTHO"
    cam.ortho_scale = w_px * mm
    cam.sensor_fit = "HORIZONTAL"
    cam.clip_end = 50.0
    ob = bpy.data.objects.new("cam", cam)
    strip_mid = (args.top + h_px) / 2
    ob.location = UP * ((contact_px - strip_mid) * mm) - FWD * 5.0
    ob.rotation_euler = (math.radians(90.0) - PITCH, 0.0, 0.0)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.scene.camera = ob


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--scale", type=int, default=2, help="render pixels per window pixel")
    ap.add_argument("--samples", type=int, default=32)
    ap.add_argument("--window", type=int, nargs=2, default=[1160, 860])
    ap.add_argument("--keys", type=int, nargs=4, default=[44, 768, 1116, 832], help="x0 y0 x1 y1 in window pixels")
    ap.add_argument("--top", type=int, default=730, help="the strip rendered runs from here to the window's bottom")
    ap.add_argument("--sink", type=int, default=10, help="how far the keys sit down into the ground, in pixels")
    ap.add_argument("--behind", type=float, default=10.0, help="how far the ground runs behind the keys, in mm")
    ap.add_argument("--grass", type=float, default=1.0, help="grass height, relative")
    ap.add_argument("--density", type=float, default=90000.0, help="blades per square metre")
    args = ap.parse_args(argv)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    w_px, h_px = args.window
    rf.setup_render(w_px * args.scale, (h_px - args.top) * args.scale, args.samples)
    rf.clear_scene()
    rf.add_lights()
    build(random.Random(5), args)
    bpy.context.scene.render.filepath = os.path.abspath(args.out)
    bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    main()
