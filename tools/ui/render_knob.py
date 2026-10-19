"""
Renders Bouquet's knob: a glazed porcelain dial with a fluted rim and a brass indicator,
seen from straight above (the plugin rotates it), lit by the same sky as the flowers.

    python render_knob.py --out knob.png [--size 512]
"""

import argparse
import math
import os
import sys

import bpy
import bmesh

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "flowers"))
import render_flowers as rf  # noqa: E402  (shares the sky and lighting)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", type=int, default=512)
    ap.add_argument("--samples", type=int, default=64)
    ap.add_argument("--frames", type=int, default=64)
    args = ap.parse_args(argv)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    rf.setup_render(args.size, args.size, args.samples)
    rf.add_lights()
    scene = bpy.context.scene

    # Body: a porcelain cylinder with fine knurling round its side and a softly domed top.
    bm = bmesh.new()
    segs, rings = 192, 14
    verts = []
    for ring in range(rings + 1):
        t = ring / rings
        row = []
        for k in range(segs):
            a = 2 * math.pi * k / segs
            side = t < 0.8
            r = 1.0 + (0.012 * math.cos(a * 48) if side and 0.12 < t < 0.7 else 0.0)
            if not side:                                   # the shoulder rolls over into the top
                u = (t - 0.8) / 0.2
                r *= math.cos(u * math.pi * 0.5) * 0.12 + 0.88
            z = 0.42 * min(t, 0.8) / 0.8 + (0.06 * math.sin((t - 0.8) / 0.2 * math.pi * 0.5) if not side else 0.0)
            row.append(bm.verts.new((r * math.cos(a), r * math.sin(a), z)))
        verts.append(row)
    for ring in range(rings):
        for k in range(segs):
            k2 = (k + 1) % segs
            bm.faces.new((verts[ring][k], verts[ring][k2], verts[ring + 1][k2], verts[ring + 1][k]))
    top = bm.verts.new((0, 0, 0.52))
    for k in range(segs):
        bm.faces.new((verts[rings][k], verts[rings][(k + 1) % segs], top))
    mesh = bpy.data.meshes.new("knob")
    bm.to_mesh(mesh)
    bm.free()
    for p in mesh.polygons:
        p.use_smooth = True
    knob = bpy.data.objects.new("knob", mesh)
    scene.collection.objects.link(knob)
    sub = knob.modifiers.new("subd", "SUBSURF")
    sub.levels = sub.render_levels = 2

    porcelain, bsdf, _ = rf.principled("porcelain")
    bsdf.inputs["Base Color"].default_value = (0.86, 0.85, 0.82, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.4
    bsdf.inputs["Subsurface Weight"].default_value = 0.2
    bsdf.inputs["Subsurface Radius"].default_value = (0.05, 0.05, 0.05)
    bsdf.inputs["Coat Weight"].default_value = 1.0
    bsdf.inputs["Coat Roughness"].default_value = 0.05
    knob.data.materials.append(porcelain)

    brass, b, _ = rf.principled("brass")
    b.inputs["Base Color"].default_value = (0.8, 0.62, 0.34, 1.0)
    b.inputs["Metallic"].default_value = 1.0
    b.inputs["Roughness"].default_value = 0.25
    # A thin brass ring at the foot, and a brass pip on the top marking the value.
    bpy.ops.mesh.primitive_torus_add(major_radius=1.0, minor_radius=0.03, location=(0, 0, 0.02))
    bpy.context.active_object.data.materials.append(brass)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.07, location=(0.0, 0.66, 0.49))
    pip = bpy.context.active_object
    pip.scale = (1.0, 1.0, 0.45)
    pip.data.materials.append(brass)
    for poly in pip.data.polygons:
        poly.use_smooth = True

    # Seen from in front and above, as it sits on the panel; frames turn the knob itself.
    ap_angle = math.radians(32.0)
    cam = bpy.data.cameras.new("cam")
    cam.type = "ORTHO"
    cam.ortho_scale = 2.5
    ob = bpy.data.objects.new("cam", cam)
    ob.location = (0.0, -5.0 * math.sin(ap_angle), 0.25 + 5.0 * math.cos(ap_angle))
    ob.rotation_euler = (ap_angle, 0.0, 0.0)
    scene.collection.objects.link(ob)
    scene.camera = ob

    scene.view_settings.exposure = -0.9
    # A filmstrip: the knob turned from -135 to +135 degrees.
    from PIL import Image
    frames = []
    for i in range(args.frames):
        angle = math.radians(135.0 - 270.0 * i / max(1, args.frames - 1))
        for o in (knob, pip):
            o.rotation_euler = (0.0, 0.0, angle)
        pip.location = (-0.66 * math.sin(angle), 0.66 * math.cos(angle), 0.49)
        path = args.out + f".{i}.png"
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        frames.append(path)
    sheet = Image.new("RGBA", (args.size * len(frames), args.size))
    for i, p in enumerate(frames):
        sheet.paste(Image.open(p).convert("RGBA"), (i * args.size, 0))
        os.remove(p)
    sheet.save(args.out, optimize=True)


if __name__ == "__main__":
    main()
