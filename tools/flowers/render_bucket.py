"""
Renders the watering bucket for Bouquet's preset garden: an old galvanised pail with a
rolled rim, two pressed ribs and a wire handle, half full of water, seen from above at
the same high angle as the lawn. It is rendered upright and tipping over towards the
right in a few steps, the water always lying level inside it and draining as it tips,
so the garden can pour it on a flower.

Writes bucket.png (the poses side by side, RGBA) and bucket.json (frame size, each
pose's tilt and fill, and in pixels the base it stands on and the lip it pours from).

Usage (Blender's Python module, `pip install bpy`):
    python render_bucket.py --out ../../Bouquet/assets/flowers
"""

import argparse
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import render_flowers as rf  # noqa: E402

SIZE = 256                       # pixels per pose
ORTHO = 0.56                     # metres across the frame
ELEVATION = math.radians(58.0)   # how far above the horizon the camera looks down
H = 0.27                         # height of the pail, metres
R_BASE, R_TOP = 0.105, 0.14      # its radius at the base and at the rim
# (tilt in degrees, how full the water is 0..1)
POSES = [(0.0, 0.72), (14.0, 0.68), (32.0, 0.56), (52.0, 0.42), (72.0, 0.26), (90.0, 0.12)]


def radius_at(z):
    return R_BASE + (R_TOP - R_BASE) * z / H


def zinc_material():
    """Galvanised steel: a crystalline spangle in the zinc, dulled and spotted with age."""
    mat, bsdf, nt = rf.principled("zinc")
    bsdf.inputs["Metallic"].default_value = 1.0
    coord = nt.nodes.new("ShaderNodeTexCoord")
    spangle = nt.nodes.new("ShaderNodeTexVoronoi")
    spangle.inputs["Scale"].default_value = 38.0
    nt.links.new(coord.outputs["Object"], spangle.inputs["Vector"])
    grime = nt.nodes.new("ShaderNodeTexNoise")
    grime.inputs["Scale"].default_value = 9.0
    grime.inputs["Detail"].default_value = 8.0
    nt.links.new(coord.outputs["Object"], grime.inputs["Vector"])

    # Roughness: each spangle crystal a little different, the grime duller still.
    rough = nt.nodes.new("ShaderNodeMapRange")
    rough.inputs["To Min"].default_value = 0.22
    rough.inputs["To Max"].default_value = 0.42
    nt.links.new(spangle.outputs["Distance"], rough.inputs["Value"])
    dull = nt.nodes.new("ShaderNodeMath")
    dull.operation = "MULTIPLY_ADD"
    dull.inputs[1].default_value = 0.3
    nt.links.new(grime.outputs["Fac"], dull.inputs[0])
    nt.links.new(rough.outputs["Result"], dull.inputs[2])
    nt.links.new(dull.outputs["Value"], bsdf.inputs["Roughness"])

    # Colour: cool grey zinc, the crystals catching slightly different tones, darker patina in places.
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.35
    ramp.color_ramp.elements[0].color = (0.36, 0.37, 0.37, 1.0)
    ramp.color_ramp.elements[1].position = 0.75
    ramp.color_ramp.elements[1].color = (0.72, 0.74, 0.75, 1.0)
    nt.links.new(grime.outputs["Fac"], ramp.inputs["Fac"])
    crystals = nt.nodes.new("ShaderNodeMix")
    crystals.data_type = "RGBA"
    crystals.blend_type = "MULTIPLY"
    crystals.inputs["Factor"].default_value = 0.18
    nt.links.new(ramp.outputs["Color"], crystals.inputs[6])
    nt.links.new(spangle.outputs["Color"], crystals.inputs[7])
    nt.links.new(crystals.outputs[2], bsdf.inputs["Base Color"])

    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.08
    bump.inputs["Distance"].default_value = 0.0004
    nt.links.new(spangle.outputs["Distance"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


def water_material():
    mat, bsdf, nt = rf.principled("water")
    # Rainwater in an old pail: a little green-grey, bright where it mirrors the sky.
    bsdf.inputs["Base Color"].default_value = (0.32, 0.45, 0.47, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.02
    bsdf.inputs["IOR"].default_value = 1.33
    bsdf.inputs["Transmission Weight"].default_value = 0.35
    bsdf.inputs["Coat Weight"].default_value = 1.0
    bsdf.inputs["Coat Roughness"].default_value = 0.0
    # A few ripples on the surface.
    wave = nt.nodes.new("ShaderNodeTexWave")
    wave.wave_type = "RINGS"
    wave.inputs["Scale"].default_value = 6.0
    wave.inputs["Distortion"].default_value = 3.0
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.12
    nt.links.new(wave.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


def pail_profile():
    """(radius, height) up the outside wall, with two pressed ribs."""
    pts = []
    steps = 40
    for i in range(steps + 1):
        z = H * i / steps
        r = radius_at(z)
        for rib in (0.22, 0.62):
            r += 0.004 * math.exp(-((z / H - rib) / 0.025) ** 2)
        pts.append((r, z))
    return pts


def build_pail(transform, fill):
    zinc = zinc_material()
    wall = 0.0012
    # Outside wall, inside wall (slightly smaller), and the bottom.
    outside = pail_profile()
    inside = [(r - wall, z) for r, z in reversed(outside)]
    rf.lathe("wall", outside + inside, 96, zinc, transform)
    rf.lathe("bottom", [(0.0, 0.004), (R_BASE - wall, 0.004), (R_BASE + 0.003, 0.0), (R_BASE + 0.003, -0.006)], 96, zinc, transform)

    # The rolled rim: a torus of wire wrapped in the sheet.
    rim = []
    for k in range(13):
        a = 2 * math.pi * k / 12
        rim.append((R_TOP + 0.003 + 0.0045 * math.cos(a), H + 0.0045 * math.sin(a)))
    rf.lathe("rim", rim, 96, zinc, transform)

    # Ears riveted on either side, and the wire handle laid back over the rim.
    for side in (-1, 1):
        ear_c = Vector((side * (R_TOP + 0.002), 0.0, H - 0.03))
        rf.spheres(f"ear{side}", [(transform @ ear_c, 0.012)], 0.012, zinc, subdivisions=3,
                   squash=Matrix.Diagonal((0.35, 1.0, 1.2, 1.0)))
    handle = []
    for i in range(25):
        t = i / 24
        a = math.pi * t
        # Resting on the rim, lying back away from the camera.
        x = -math.cos(a) * (R_TOP + 0.01)
        y = math.sin(a) * 0.18 * 0.55
        z = H - 0.03 + math.sin(a) * 0.18 * 0.85
        handle.append(transform @ Vector((x, y, z)))
    rf.tube_along("handle", handle, [0.0028] * len(handle), zinc, sides=10)
    grip = handle[9:16]
    rf.tube_along("grip", grip, [0.0075] * len(grip), rf.simple_material("wood", (0.33, 0.2, 0.11), roughness=0.6, texture=0.4), sides=14)

    # The water: the inside of the pail below a level plane in the world.
    water_level_local = max(0.01, fill * H)
    tilt_up = transform.to_3x3() @ Vector((0, 0, 1))
    centre = transform @ Vector((0.0, 0.0, water_level_local))
    inner = []
    for i in range(25):
        z = H * i / 24
        inner.append((radius_at(z) - wall - 0.0005, z))
    body = rf.lathe("water", [(0.0, 0.005)] + inner + [(0.0, H)], 96, water_material(), transform)
    # Cut it with a big box whose top is the world-level water surface through `centre`.
    level = centre.z if tilt_up.z > 0.2 else centre.z
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0.0, 0.0, level - 1.0))
    cutter = bpy.context.active_object
    cutter.scale = (4.0, 4.0, 2.0)
    mod = body.modifiers.new("level", "BOOLEAN")
    mod.operation = "INTERSECT"
    mod.object = cutter
    mod.solver = "EXACT"
    cutter.hide_render = True
    cutter.hide_viewport = True
    return body


def pivot_transform(tilt_deg):
    """Tips the pail over its right-hand bottom edge, towards the right of the picture."""
    a = math.radians(tilt_deg)
    pivot = Vector((R_BASE + 0.003, 0.0, 0.0))
    return Matrix.Translation(pivot) @ Matrix.Rotation(a, 4, "Y") @ Matrix.Translation(-pivot)


def camera(look_at):
    """Looks down at `look_at` (the pail's middle, so every pose stays in frame)."""
    cam = bpy.data.cameras.new("cam")
    cam.type = "ORTHO"
    cam.ortho_scale = ORTHO
    ob = bpy.data.objects.new("cam", cam)
    bpy.context.scene.collection.objects.link(ob)
    d = 2.0
    ob.location = look_at + Vector((0.0, -math.cos(ELEVATION) * d, math.sin(ELEVATION) * d))
    ob.rotation_euler = (math.radians(90.0) - ELEVATION, 0.0, 0.0)
    bpy.context.scene.camera = ob
    return ob


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=".")
    parser.add_argument("--samples", type=int, default=96)
    args = parser.parse_args([a for a in sys.argv[1:] if a != "--"])
    os.makedirs(args.out, exist_ok=True)

    scene = bpy.context.scene
    poses = []
    paths = []
    for i, (tilt, fill) in enumerate(POSES):
        rf.clear_scene()
        rf.setup_render(SIZE, SIZE, args.samples)
        rf.add_lights()
        t = pivot_transform(tilt)
        cam = camera(t @ Vector((0.0, 0.0, H * 0.5)))
        build_pail(t, fill)

        # Where it stands, and the lowest point of its rim (the lip water pours over).
        base = t @ Vector((0.0, 0.0, 0.0))
        lip = t @ Vector((R_TOP + 0.007, 0.0, H))
        poses.append({
            "tilt": tilt, "fill": fill,
            "base": [round(c, 2) for c in rf.project(scene, cam, base, SIZE, SIZE)],
            "lip": [round(c, 2) for c in rf.project(scene, cam, lip, SIZE, SIZE)],
        })
        path = os.path.join(args.out, f"_bucket_{i}.png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        paths.append(path)
        print(f"bucket pose {i}", flush=True)

    sheet = Image.new("RGBA", (SIZE * len(paths), SIZE))
    for i, p in enumerate(paths):
        sheet.paste(Image.open(p).convert("RGBA"), (i * SIZE, 0))
        os.remove(p)
    sheet.save(os.path.join(args.out, "bucket.png"), optimize=True)
    with open(os.path.join(args.out, "bucket.json"), "w") as f:
        json.dump({"size": SIZE, "poses": poses}, f, indent=1)


if __name__ == "__main__":
    main()
