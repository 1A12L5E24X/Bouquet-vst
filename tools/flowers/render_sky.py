"""
Renders the sky behind Bouquet's photoreal mode, wider than the window so the plugin
can let it drift: Blender (Cycles) renders a physically based clear daylight sky, then
fair-weather cumulus are laid over it: fractal noise on a cloud layer seen in
perspective (small and flat towards the horizon), lit by marching towards the sun
(bright tops, grey-blue undersides, glowing thin edges) and softened by haze near the
horizon.

Usage (Blender's Python module, `pip install bpy`):
    python render_sky.py --out ../../Bouquet/assets/flowers/sky.jpg [--size 3480x1720]
"""

import argparse
import math
import os
import sys

import bpy
import numpy as np
from PIL import Image

PITCH = math.radians(33.5)      # the bottom of the frame sits just above the horizon
VFOV = math.radians(64.0)


def render_clear_sky(width, height, path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 8
    scene.render.resolution_x, scene.render.resolution_y = width, height
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.exposure = -1.0

    world = bpy.data.worlds.new("sky")
    scene.world = world
    world.use_nodes = True
    nt = world.node_tree
    nt.nodes.clear()
    sky = nt.nodes.new("ShaderNodeTexSky")
    sky.sky_type = "MULTIPLE_SCATTERING"
    sky.sun_disc = False
    sky.sun_elevation = math.radians(50.0)
    sky.sun_rotation = math.radians(160.0)
    sky.air_density = 1.0
    sky.aerosol_density = 0.5
    bg = nt.nodes.new("ShaderNodeBackground")
    out = nt.nodes.new("ShaderNodeOutputWorld")
    nt.links.new(sky.outputs[0], bg.inputs["Color"])
    nt.links.new(bg.outputs[0], out.inputs[0])

    cam_data = bpy.data.cameras.new("cam")
    cam_data.sensor_fit = "VERTICAL"
    cam_data.angle = VFOV
    cam = bpy.data.objects.new("cam", cam_data)
    cam.rotation_euler = (math.radians(90.0) + PITCH, 0.0, 0.0)
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


class ValueNoise:
    """Tileable 2D value noise with smooth interpolation, sampled at any coordinates."""

    def __init__(self, seed, size=256):
        self.grid = np.random.default_rng(seed).random((size, size)).astype(np.float32)
        self.size = size

    def __call__(self, x, y):
        n = self.size
        x0, y0 = np.floor(x), np.floor(y)
        tx, ty = x - x0, y - y0
        tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
        x0, y0 = x0.astype(np.int64) % n, y0.astype(np.int64) % n
        x1, y1 = (x0 + 1) % n, (y0 + 1) % n
        g = self.grid
        a = g[y0, x0] + (g[y0, x1] - g[y0, x0]) * tx
        b = g[y1, x0] + (g[y1, x1] - g[y1, x0]) * tx
        return a + (b - a) * ty


def fbm(noises, x, y, octaves=7, gain=0.52):
    total, amp, norm = 0.0, 1.0, 0.0
    for o in range(octaves):
        f = 2.0 ** o
        total = total + noises[o % len(noises)](x * f + o * 17.3, y * f - o * 9.1) * amp
        norm += amp
        amp *= gain
    return total / norm


def clouds(width, height, seed, cover, scale):
    """Returns (colour, alpha) of the cloud layer."""
    noises = [ValueNoise(seed + i) for i in range(4)]
    aspect = width / height
    # The ray through every pixel, and where it meets a cloud layer one unit up.
    v = (np.arange(height, dtype=np.float32)[:, None] + 0.5) / height          # 0 top .. 1 bottom
    u = (np.arange(width, dtype=np.float32)[None, :] + 0.5) / width
    half = math.tan(VFOV / 2)
    up = (0.5 - v) * 2 * half
    side = (u - 0.5) * 2 * half * aspect
    elev = np.maximum(PITCH + np.arctan(up), math.radians(0.6))                 # angle above the horizon
    dist = 1.0 / np.tan(elev)                                                   # how far away the layer is
    px = np.broadcast_to(side * dist, (height, width))
    py = np.broadcast_to(dist, (height, width))

    def density_at(ox, oy):
        d = fbm(noises, px * scale + ox, py * scale * 1.6 + oy)
        return np.clip((d - cover) * 4.2, 0.0, 1.0)

    dens = density_at(0.0, 0.0)
    # March towards the sun through the layer: the more cloud in the way, the darker.
    sun = np.array([0.35, -0.55]) * 0.09
    shadow = np.zeros_like(dens)
    for k in range(1, 5):
        shadow += density_at(sun[0] * k, sun[1] * k)
    light = np.exp(-shadow * 0.9)

    lit = np.array([1.0, 0.985, 0.95], np.float32)
    shade = np.array([0.56, 0.62, 0.72], np.float32)
    colour = shade + (lit - shade) * light[..., None]
    edge = np.clip(1.0 - dens * 3.0, 0.0, 1.0) * (dens > 0)                    # thin edges glow
    colour = colour + edge[..., None] * 0.08
    alpha = 1.0 - np.exp(-dens * 3.2)
    haze = np.clip((elev - math.radians(1.0)) / math.radians(14.0), 0.0, 1.0)  # far clouds melt into haze
    return colour, alpha * (0.35 + 0.65 * haze)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", default="3480x1720")
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--cover", type=float, default=0.52, help="higher: fewer clouds")
    ap.add_argument("--scale", type=float, default=1.6, help="higher: smaller clouds")
    args = ap.parse_args(argv)
    width, height = (int(v) for v in args.size.split("x"))

    tmp = args.out + ".sky.png"
    render_clear_sky(width, height, tmp)
    sky = np.asarray(Image.open(tmp).convert("RGB"), np.float32) / 255.0
    os.remove(tmp)

    colour, alpha = clouds(width, height, args.seed, args.cover, args.scale)
    out = sky * (1.0 - alpha[..., None]) + colour * alpha[..., None]
    Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8)).save(args.out, quality=88)


if __name__ == "__main__":
    main()
