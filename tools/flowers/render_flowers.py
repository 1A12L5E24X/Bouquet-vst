"""
Renders Bouquet's hyper-realistic flowers with Blender (Cycles).

Every species is modelled procedurally (petals, sepals, discs, stamens, stems, leaves),
then rendered as a sprite sequence: a bud opening to full bloom, then a visceral rot
(petals bleed, turn black and wet, mould spreads, blood runs into a pool, the head
collapses). Each species is rendered in several colourways. The plugin plays these
frames back, tinting nothing: what you see is the render.

Output (per species and colour): an atlas PNG of all frames plus a JSON file with,
for every frame, where the stem is in the sprite (so the plugin can run drips down it
and attach flies to the head).

Usage (Blender's Python module, `pip install bpy`):
    python render_flowers.py --out ../../Bouquet/assets/flowers [--species daisy] [--colour pink]
                             [--frames all|bloom|rot|N] [--size 144x216] [--samples 32]
"""

import argparse
import json
import math
import os
import random
import sys
import time

import bpy
import bmesh
from mathutils import Matrix, Vector
from bpy_extras.object_utils import world_to_camera_view

SPECIES = ["daisy", "tulip", "poppy", "cosmos", "bell", "rose"]

COLOURS = {
    "white":   (0.93, 0.91, 0.86),
    "butter":  (0.95, 0.76, 0.22),
    "apricot": (0.96, 0.52, 0.22),
    "coral":   (0.88, 0.22, 0.17),
    "crimson": (0.55, 0.03, 0.10),
    "pink":    (0.93, 0.48, 0.64),
    "lilac":   (0.62, 0.48, 0.86),
    "blue":    (0.28, 0.42, 0.85),
}

BLOOM_FRAMES = 10
ROT_FRAMES = 14
STEM_HEIGHT = 0.17
ORTHO = 0.27
HEAD = 1.7          # head scale relative to life size: sprites are small, faces must read

BLOOD = (0.30, 0.008, 0.012)
NECROTIC = (0.03, 0.008, 0.006)
STEM_GREEN = (0.16, 0.33, 0.09)
LEAF_GREEN = (0.13, 0.30, 0.07)


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3.0 - 2.0 * x)


def mix(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


# ---------------------------------------------------------------------------- scene

def clear_scene():
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    for coll in (bpy.data.meshes, bpy.data.curves, bpy.data.materials, bpy.data.lights, bpy.data.cameras):
        for block in list(coll):
            coll.remove(block)


def setup_render(width, height, samples):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    scene.cycles.max_bounces = 6
    scene.cycles.transmission_bounces = 4
    scene.render.film_transparent = True
    scene.render.resolution_x = width
    scene.render.resolution_y = height
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.exposure = -0.35

    world = bpy.data.worlds.new("sky") if not bpy.data.worlds else bpy.data.worlds[0]
    scene.world = world
    world.use_nodes = True
    nt = world.node_tree
    nt.nodes.clear()
    bg = nt.nodes.new("ShaderNodeBackground")
    out = nt.nodes.new("ShaderNodeOutputWorld")
    env = nt.nodes.new("ShaderNodeTexEnvironment")
    # A real photographed sky (CC0, Poly Haven via pmndrs/assets) lights the flowers.
    env.image = bpy.data.images.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), "hdri", "park.exr"))
    mapping = nt.nodes.new("ShaderNodeMapping")
    coord = nt.nodes.new("ShaderNodeTexCoord")
    mapping.inputs["Rotation"].default_value = (0.0, 0.0, math.radians(120.0))
    nt.links.new(coord.outputs["Generated"], mapping.inputs["Vector"])
    nt.links.new(mapping.outputs["Vector"], env.inputs["Vector"])
    # Keep the sky's light and shadows but not its blue cast, so petal colours stay true.
    neutral = nt.nodes.new("ShaderNodeHueSaturation")
    neutral.inputs["Saturation"].default_value = 0.25
    nt.links.new(env.outputs["Color"], neutral.inputs["Color"])
    nt.links.new(neutral.outputs["Color"], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = 1.0
    nt.links.new(bg.outputs[0], out.inputs[0])


def add_lights():
    sun = bpy.data.lights.new("sun", "SUN")
    sun.energy = 2.4
    sun.angle = math.radians(4.0)
    sun.color = (1.0, 0.95, 0.86)
    ob = bpy.data.objects.new("sun", sun)
    ob.rotation_euler = (math.radians(50), math.radians(-25), math.radians(-35))
    bpy.context.scene.collection.objects.link(ob)

    # Back light: makes petals glow through, like real petals against the sun.
    rim = bpy.data.lights.new("rim", "SUN")
    rim.energy = 1.4
    rim.angle = math.radians(8.0)
    rim.color = (1.0, 0.92, 0.8)
    ob = bpy.data.objects.new("rim", rim)
    ob.rotation_euler = (math.radians(-60), math.radians(10), math.radians(160))
    bpy.context.scene.collection.objects.link(ob)


CLOSEUP = None   # (x, z, scale) to frame only the flower head, for inspecting detail


def add_camera():
    cam = bpy.data.cameras.new("cam")
    cam.type = "ORTHO"
    cam.ortho_scale = ORTHO if CLOSEUP is None else CLOSEUP[2]
    ob = bpy.data.objects.new("cam", cam)
    bpy.context.scene.collection.objects.link(ob)
    centre_z = ORTHO * 0.5 - 0.02
    tilt = math.radians(18.0)
    if CLOSEUP is not None:
        ob.location = (CLOSEUP[0], -1.0 * math.cos(tilt), CLOSEUP[1] + math.sin(tilt) * 1.0)
    else:
        ob.location = (0.0, -1.0 * math.cos(tilt), centre_z + math.sin(tilt) * 1.0)
    ob.rotation_euler = (math.radians(90.0) - tilt, 0.0, 0.0)
    bpy.context.scene.camera = ob
    return ob


# ------------------------------------------------------------------------ materials

def principled(name):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    return mat, nodes["Principled BSDF"], mat.node_tree


def petal_material(name, colour, rot, species, seed):
    mat, bsdf, nt = principled(name)
    bleed = smooth((rot - 0.05) / 0.35)
    die = smooth((rot - 0.45) / 0.4)
    wet = smooth((rot - 0.12) / 0.2) * (1.0 - smooth((rot - 0.8) / 0.15))
    base = mix(mix(colour, BLOOD, bleed * 0.92), NECROTIC, die)

    nodes = nt.nodes
    uv = nodes.new("ShaderNodeTexCoord")
    sep = nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(uv.outputs["UV"], sep.inputs[0])

    # Veins: fine bands running along the petal.
    wave = nodes.new("ShaderNodeTexWave")
    wave.wave_type = "BANDS"
    wave.bands_direction = "Y"          # lines running from the base to the tip
    wave.inputs["Scale"].default_value = 5.5
    wave.inputs["Distortion"].default_value = 7.0
    wave.inputs["Detail"].default_value = 5.0
    wave.inputs["Detail Scale"].default_value = 2.5
    wave.inputs["Detail Roughness"].default_value = 0.6
    nt.links.new(uv.outputs["UV"], wave.inputs["Vector"])

    # Veins and the fine cell texture of a petal, as relief.
    grain = nodes.new("ShaderNodeTexNoise")
    grain.inputs["Scale"].default_value = 140.0
    grain.inputs["Detail"].default_value = 4.0
    relief = nodes.new("ShaderNodeMath")
    relief.operation = "MULTIPLY_ADD"
    relief.inputs[1].default_value = 0.6
    nt.links.new(grain.outputs["Fac"], relief.inputs[2])
    nt.links.new(wave.outputs["Fac"], relief.inputs[0])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.09 + 0.4 * smooth(rot * 1.3)
    bump.inputs["Distance"].default_value = 0.0004
    nt.links.new(relief.outputs[0], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

    # Colour: deeper at the base, lighter and slightly translucent at the tip.
    ramp = nodes.new("ShaderNodeValToRGB")
    nt.links.new(sep.outputs["X"], ramp.inputs[0])
    deep = tuple(c * 0.55 for c in base)
    if species == "poppy":
        deep = mix(NECROTIC, base, 0.1)          # the dark blotch at the heart of a poppy
        ramp.color_ramp.elements[0].position = 0.16
        ramp.color_ramp.elements[1].position = 0.24
    elif species == "daisy":
        deep = mix(base, (0.95, 0.85, 0.3), 0.35) if rot < 0.3 else deep
    ramp.color_ramp.elements[0].color = (*deep, 1.0)
    ramp.color_ramp.elements[1].color = (*base, 1.0)

    veinmix = nodes.new("ShaderNodeMix")
    veinmix.data_type = "RGBA"
    veinmix.blend_type = "MULTIPLY"
    veinmix.inputs["Factor"].default_value = 0.1
    nt.links.new(ramp.outputs["Color"], veinmix.inputs[6])
    nt.links.new(wave.outputs["Color"], veinmix.inputs[7])

    # Natural unevenness of colour across the flower.
    blotch = nodes.new("ShaderNodeTexNoise")
    blotch.inputs["Scale"].default_value = 5.0
    blotch.inputs["Detail"].default_value = 3.0
    vary = nodes.new("ShaderNodeMix")
    vary.data_type = "RGBA"
    vary.blend_type = "OVERLAY"
    vary.inputs["Factor"].default_value = 0.35
    nt.links.new(veinmix.outputs[2], vary.inputs[6])
    nt.links.new(blotch.outputs["Color"], vary.inputs[7])
    colour_out = vary.outputs[2]

    # Mould: pale fuzzy patches spreading as the flower dies.
    mould = smooth((rot - 0.45) / 0.35)
    if mould > 0.0:
        noise = nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = 7.0
        noise.inputs["Detail"].default_value = 12.0
        noise.inputs["Roughness"].default_value = 0.7
        noise.noise_dimensions = "4D"
        noise.inputs["W"].default_value = seed % 100
        mramp = nodes.new("ShaderNodeValToRGB")
        mramp.color_ramp.elements[0].position = 0.68 - 0.1 * mould
        mramp.color_ramp.elements[1].position = 0.76 - 0.1 * mould
        nt.links.new(noise.outputs["Fac"], mramp.inputs[0])
        mm = nodes.new("ShaderNodeMix")
        mm.data_type = "RGBA"
        nt.links.new(mramp.outputs["Color"], mm.inputs["Factor"])
        nt.links.new(colour_out, mm.inputs[6])
        mm.inputs[7].default_value = (0.72, 0.76, 0.64, 1.0)
        colour_out = mm.outputs[2]

    nt.links.new(colour_out, bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.55 - 0.45 * wet
    bsdf.inputs["Subsurface Weight"].default_value = 0.45 * (1.0 - die)
    bsdf.inputs["Subsurface Radius"].default_value = (0.02, 0.008, 0.005)
    bsdf.inputs["Subsurface Scale"].default_value = 0.05
    bsdf.inputs["Transmission Weight"].default_value = 0.25 * (1.0 - die)
    bsdf.inputs["Sheen Weight"].default_value = 0.35 if species in ("rose", "poppy") else 0.15
    bsdf.inputs["Coat Weight"].default_value = 0.85 * wet
    bsdf.inputs["Coat Roughness"].default_value = 0.05
    try:
        bsdf.inputs["Thin Wall"].default_value = True
    except Exception:
        pass
    return mat


def simple_material(name, colour, roughness=0.5, subsurface=0.0, coat=0.0, sheen=0.0, texture=0.0, veins=False):
    mat, bsdf, nt = principled(name)
    bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
    if texture > 0.0:
        # Organic unevenness: mottled colour and a little surface relief.
        noise = nt.nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = 60.0
        noise.inputs["Detail"].default_value = 6.0
        mixc = nt.nodes.new("ShaderNodeMix")
        mixc.data_type = "RGBA"
        mixc.blend_type = "OVERLAY"
        mixc.inputs["Factor"].default_value = texture
        mixc.inputs[6].default_value = (*colour, 1.0)
        nt.links.new(noise.outputs["Color"], mixc.inputs[7])
        nt.links.new(mixc.outputs[2], bsdf.inputs["Base Color"])
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.25
        bump.inputs["Distance"].default_value = 0.0003
        height = noise.outputs["Fac"]
        if veins:
            uv = nt.nodes.new("ShaderNodeTexCoord")
            wave = nt.nodes.new("ShaderNodeTexWave")
            wave.bands_direction = "Y"
            wave.inputs["Scale"].default_value = 6.0
            wave.inputs["Distortion"].default_value = 1.5
            nt.links.new(uv.outputs["UV"], wave.inputs["Vector"])
            height = wave.outputs["Fac"]
            bump.inputs["Strength"].default_value = 0.35
        nt.links.new(height, bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Subsurface Weight"].default_value = subsurface
    bsdf.inputs["Subsurface Radius"].default_value = (0.01, 0.01, 0.004)
    bsdf.inputs["Subsurface Scale"].default_value = 0.02
    bsdf.inputs["Coat Weight"].default_value = coat
    bsdf.inputs["Sheen Weight"].default_value = sheen
    return mat


def blood_material():
    return simple_material("blood", (0.12, 0.0, 0.004), roughness=0.06, subsurface=0.4, coat=0.6)


# ------------------------------------------------------------------------- geometry

def mesh_object(name, verts, faces, uvs, material, smooth_shading=True, subdivide=1):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    if uvs is not None:
        layer = mesh.uv_layers.new(name="UVMap")
        for poly in mesh.polygons:
            for li in poly.loop_indices:
                layer.data[li].uv = uvs[mesh.loops[li].vertex_index]
    mesh.update()
    for p in mesh.polygons:
        p.use_smooth = smooth_shading
    ob = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(ob)
    ob.data.materials.append(material)
    if subdivide:
        mod = ob.modifiers.new("subd", "SUBSURF")
        mod.levels = subdivide
        mod.render_levels = subdivide
    return ob


def grid_surface(name, nu, nv, fn, material, subdivide=1):
    """fn(u, v) -> Vector, u in [0,1] along, v in [-1,1] across."""
    verts, uvs, faces = [], [], []
    for i in range(nu + 1):
        u = i / nu
        for j in range(nv + 1):
            v = -1.0 + 2.0 * j / nv
            verts.append(fn(u, v))
            uvs.append((u, (v + 1.0) * 0.5))
    row = nv + 1
    for i in range(nu):
        for j in range(nv):
            a = i * row + j
            faces.append((a, a + 1, a + row + 1, a + row))
    return mesh_object(name, verts, faces, uvs, material, subdivide=subdivide)


def frame_from(direction, up=Vector((0, 0, 1))):
    d = direction.normalized()
    side = up.cross(d)
    if side.length < 1e-5:
        side = Vector((1, 0, 0))
    side.normalize()
    normal = d.cross(side).normalized()
    return d, side, normal


def petal(name, origin, axis, azimuth, elevation, length, width, profile, cup, curl, ruffle, twist,
          material, seed, notch=False, fall=0.0, nu=14, nv=8):
    """A petal growing out of `origin` in the plane around `axis` (the flower's axis)."""
    rng = random.Random(seed)
    phase = rng.random() * 6.28
    # Horizontal direction at this azimuth, perpendicular to the axis.
    axis = axis.normalized()
    ref = Vector((1, 0, 0)) if abs(axis.x) < 0.9 else Vector((0, 1, 0))
    h1 = axis.cross(ref).normalized()
    h2 = axis.cross(h1).normalized()
    horiz = (h1 * math.cos(azimuth) + h2 * math.sin(azimuth)).normalized()
    d = (horiz * math.cos(elevation) + axis * math.sin(elevation)).normalized()
    side = axis.cross(horiz).normalized()
    normal = d.cross(side).normalized()
    drop = Vector((0, 0, -fall * 0.05)) + side * (fall * 0.012 * (rng.random() - 0.5))

    def fn(u, v):
        w = width * 0.5 * profile(u)
        x = v * w
        if notch and u > 0.85:
            x *= 1.0 + 0.0
        y = length * u
        if notch:
            y -= length * 0.08 * max(0.0, u - 0.8) * 5.0 * (1.0 - abs(v)) ** 2
        z = cup * (v * v) * w + curl * u * u * length
        z += ruffle * math.sin(v * 7.0 + u * 11.0 + phase) * u
        tw = twist * u
        xr = x * math.cos(tw) - z * math.sin(tw)
        zr = x * math.sin(tw) + z * math.cos(tw)
        return origin + d * y + side * xr + normal * zr + drop * (1.0 + u)

    ob = grid_surface(name, nu, nv, fn, material)
    # Real petals have a little thickness; their edges catch the light.
    solid = ob.modifiers.new("thickness", "SOLIDIFY")
    solid.thickness = 0.00035
    solid.offset = 0.0
    return ob


def tube_along(name, points, radii, material, sides=10):
    """A tube through `points` with per-point radius."""
    verts, faces = [], []
    n = len(points)
    for i, p in enumerate(points):
        t = (points[min(i + 1, n - 1)] - points[max(i - 1, 0)]).normalized()
        a = t.cross(Vector((1, 0, 0)))
        if a.length < 1e-4:
            a = t.cross(Vector((0, 1, 0)))
        a.normalize()
        b = t.cross(a).normalized()
        for k in range(sides):
            ang = 2 * math.pi * k / sides
            verts.append(p + (a * math.cos(ang) + b * math.sin(ang)) * radii[i])
    for i in range(n - 1):
        for k in range(sides):
            a0 = i * sides + k
            a1 = i * sides + (k + 1) % sides
            faces.append((a0, a1, a1 + sides, a0 + sides))
    return mesh_object(name, verts, faces, None, material, subdivide=0)


def spheres(name, centres, radius, material, subdivisions=2, squash=None):
    bm = bmesh.new()
    for c in centres:
        r = radius if not isinstance(c, tuple) else c[1]
        pos = c if not isinstance(c, tuple) else c[0]
        m = Matrix.Translation(pos)
        if squash is not None:
            m = m @ squash
        bmesh.ops.create_icosphere(bm, subdivisions=subdivisions, radius=r, matrix=m)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for p in mesh.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(ob)
    ob.data.materials.append(material)
    return ob


def lathe(name, profile_pts, segments, material, transform, lobes=0, lobe_depth=0.0):
    """Revolve (radius, height) points around Z; optional scalloped rim."""
    verts, faces = [], []
    rings = len(profile_pts)
    for i, (r, z) in enumerate(profile_pts):
        rim = i / (rings - 1)
        for k in range(segments):
            ang = 2 * math.pi * k / segments
            rr = r * (1.0 + lobe_depth * rim ** 3 * math.cos(lobes * ang)) if lobes else r
            zz = z + (lobe_depth * 0.5 * r * rim ** 3 * math.cos(lobes * ang) if lobes else 0.0)
            verts.append(transform @ Vector((rr * math.cos(ang), rr * math.sin(ang), zz)))
    for i in range(rings - 1):
        for k in range(segments):
            a0 = i * segments + k
            a1 = i * segments + (k + 1) % segments
            faces.append((a0, a1, a1 + segments, a0 + segments))
    return mesh_object(name, verts, faces, None, material, subdivide=1)


# ----------------------------------------------------------------------- the plant

def stem_points(rot, species, seed):
    """The stem as a polyline from the ground up; bends over as the plant rots."""
    rng = random.Random(seed * 7 + 3)
    lean = (rng.random() - 0.5) * 0.25
    pts = []
    n = 18
    droop = math.radians(78.0) * (rot ** 1.25)
    arch = math.radians(115.0) if species == "bell" else 0.0
    # Start left of centre so arching or collapsing heads stay inside the frame.
    x, z, ang = (-0.045 if species == "bell" else -0.022), 0.0, 0.0
    seg = STEM_HEIGHT / n
    for i in range(n + 1):
        pts.append(Vector((x, 0.0, z)))
        s = i / n
        ang = lean * s + (droop + arch) * s ** 3
        x += math.sin(ang) * seg
        z += math.cos(ang) * seg
    return pts


def build_plant(species, colour_name, bloom, rot, seed):
    colour = COLOURS[colour_name]
    rng = random.Random(seed)
    pts = stem_points(rot, species, seed)
    top = pts[-1]
    axis = (pts[-1] - pts[-3]).normalized()
    if species in ("daisy", "cosmos", "poppy", "rose"):
        # Open faces turn towards the viewer (and the sun), less so as the plant collapses.
        axis = (axis + Vector((0.0, -1.0, 0.15)) * (0.9 * (1.0 - smooth(rot * 1.3)))).normalized()

    stem_colour = mix(mix(STEM_GREEN, (0.18, 0.12, 0.04), smooth(rot / 0.5)), (0.04, 0.025, 0.01), smooth((rot - 0.5) / 0.4))
    stem_mat = simple_material("stem", stem_colour, roughness=0.45, subsurface=0.15, coat=0.3 * smooth((rot - 0.2) / 0.3), texture=0.25)
    radii = [0.0022 * (1.0 - 0.35 * i / (len(pts) - 1)) for i in range(len(pts))]
    tube_along("stem", pts, radii, stem_mat)

    # Leaves: lance-shaped, folded along the midrib, curling and blackening with rot.
    leaf_colour = mix(mix(LEAF_GREEN, (0.20, 0.13, 0.03), smooth(rot / 0.5)), (0.03, 0.02, 0.01), smooth((rot - 0.5) / 0.4))
    leaf_mat = simple_material("leaf", leaf_colour, roughness=0.38, subsurface=0.3, coat=0.4 * smooth((rot - 0.2) / 0.3), texture=0.3, veins=True)
    for k, (at, az) in enumerate(((0.28, 0.6), (0.48, 3.6))):
        idx = int(at * (len(pts) - 1))
        origin = pts[idx]
        length = 0.062 - 0.012 * k
        elev = math.radians(35.0 - 55.0 * smooth(rot * 1.2))
        curl = 0.1 + 0.6 * smooth(rot * 1.3)
        petal(f"leaf{k}", origin, Vector((0, 0, 1)), az + rng.uniform(-0.2, 0.2), elev, length, 0.016,
              lambda u: math.sin(math.pi * min(1.0, u * 1.02)) ** 0.8, cup=0.6 + curl, curl=-0.15 - 0.35 * rot,
              ruffle=0.0006 + 0.002 * rot, twist=0.2, material=leaf_mat, seed=seed + k * 13)

    open_ = bloom
    pmat = petal_material("petal", colour, rot, species, seed)
    shrivel = 1.0 - 0.45 * smooth(rot * 1.4)
    sag = math.radians(35.0) * smooth(rot * 1.2)
    ruffle_rot = 0.003 * smooth(rot * 1.3)

    def fallen(i, n):
        fall_at = 0.3 + 0.55 * random.Random(seed * 31 + i).random()
        return max(0.0, (rot - fall_at) / 0.12)

    disc_mat = simple_material("disc", mix(mix((0.85, 0.55, 0.05), BLOOD, smooth(rot / 0.4)), NECROTIC, smooth((rot - 0.45) / 0.4)),
                               roughness=0.7 - 0.6 * smooth((rot - 0.15) / 0.2), coat=smooth((rot - 0.15) / 0.2), texture=0.35)
    green_mat = simple_material("sepal", stem_colour, roughness=0.5, subsurface=0.15)

    def sepals(count=5, length=0.012):
        for i in range(count):
            petal(f"sepal{i}", top, axis, 2 * math.pi * i / count + 0.3, math.radians(-25.0 - 20.0 * rot), length * HEAD, 0.006 * HEAD,
                  lambda u: math.sin(math.pi * u) ** 0.9, cup=0.4, curl=-0.1, ruffle=0.0, twist=0.0,
                  material=green_mat, seed=seed + 900 + i, nu=6, nv=4)

    if species in ("daisy", "cosmos", "poppy"):
        if species == "daisy":
            n, length, width = 21, 0.030 * HEAD, 0.0075 * HEAD
            prof = lambda u: math.sin(math.pi * min(1.0, 0.05 + u * 0.93)) ** 0.5
            elev_closed, elev_open, cup = 80.0, 8.0, 0.25
        elif species == "cosmos":
            n, length, width = 8, 0.032 * HEAD, 0.017 * HEAD
            prof = lambda u: max(0.0, math.sin(math.pi * (0.08 + 0.84 * u))) ** 0.45 * (0.75 + 0.25 * u)
            elev_closed, elev_open, cup = 82.0, 12.0, 0.25
        else:
            n, length, width = 4, 0.034 * HEAD, 0.040 * HEAD
            prof = lambda u: max(0.0, math.sin(math.pi * (0.05 + 0.9 * u))) ** 0.4 * (0.8 + 0.3 * u)
            elev_closed, elev_open, cup = 86.0, 26.0, 0.6
        for i in range(n):
            f = fallen(i, n)
            if f >= 1.0:
                continue
            az = 2 * math.pi * i / n + rng.uniform(-0.06, 0.06)
            el = math.radians(elev_closed + (elev_open - elev_closed) * open_) - sag
            L = length * (0.35 + 0.65 * open_) * rng.uniform(0.92, 1.06) * (1.0 if species != "poppy" or i % 2 == 0 else 0.92)
            W = width * (0.6 + 0.4 * open_) * shrivel * rng.uniform(0.9, 1.08)
            petal(f"petal{i}", top, axis, az, el, L, W, prof, cup=cup, curl=-0.05 if species != "poppy" else 0.12,
                  ruffle=(0.0035 if species == "poppy" else 0.0004) + ruffle_rot, twist=rng.uniform(-0.15, 0.15),
                  material=pmat, seed=seed + i, notch=(species == "cosmos"), fall=f)
        if species == "poppy":
            # Seed pod with stigma rays, ringed by dark stamens.
            # Seed pod (a squat urn with a dark rayed lid), ringed by filaments with dark anthers.
            ref = Vector((1, 0, 0)) if abs(axis.x) < 0.9 else Vector((0, 1, 0))
            e1 = axis.cross(ref).normalized()
            e2 = axis.cross(e1).normalized()
            basis = Matrix((e1, e2, axis)).transposed().to_4x4()
            pod = simple_material("pod", mix((0.36, 0.44, 0.26), NECROTIC, smooth(rot / 0.6)), roughness=0.45, coat=0.25, texture=0.2)
            spheres("pod", [(top + axis * 0.0045 * HEAD, 0.0052 * HEAD)], 0.0052, pod, subdivisions=4,
                    squash=basis @ Matrix.Diagonal((1.0, 1.0, 0.8, 1.0)))
            dark = simple_material("anther", (0.025, 0.02, 0.035), roughness=0.35, texture=0.15)
            lid = top + axis * 0.0085 * HEAD
            for k in range(9):
                a = 2 * math.pi * k / 9
                tip = lid + (e1 * math.cos(a) + e2 * math.sin(a)) * 0.0042 * HEAD - axis * 0.0006 * HEAD
                tube_along(f"ray{k}", [lid + axis * 0.0004 * HEAD, tip], [0.00045 * HEAD, 0.00025 * HEAD], dark, sides=6)
            anthers = []
            frng = random.Random(seed * 3)
            for k in range(70):
                a = k * 2.39996
                rr = (0.0062 + 0.0025 * frng.random()) * HEAD
                out = (e1 * math.cos(a) + e2 * math.sin(a))
                base = top + out * 0.0045 * HEAD + axis * 0.002 * HEAD
                end = top + out * rr + axis * (0.0055 + 0.002 * frng.random()) * HEAD
                tube_along(f"filament{k}", [base, end], [0.00018 * HEAD, 0.00015 * HEAD], dark, sides=4)
                anthers.append((end, 0.00055 * HEAD))
            spheres("anthers", anthers, 0.0005, dark, subdivisions=2,
                    squash=basis @ Matrix.Diagonal((1.0, 1.0, 1.8, 1.0)))
        else:
            r = (0.0105 if species == "daisy" else 0.008) * HEAD
            florets = []
            count = 320 if species == "daisy" else 160
            for k in range(count):
                a = k * 2.39996
                d = r * math.sqrt(k / count)
                h = (0.004 * (1.0 - (d / r) ** 2) + 0.001) * HEAD
                ref = Vector((1, 0, 0)) if abs(axis.x) < 0.9 else Vector((0, 1, 0))
                e1 = axis.cross(ref).normalized()
                e2 = axis.cross(e1).normalized()
                florets.append((top + axis * h + (e1 * math.cos(a) + e2 * math.sin(a)) * d, (0.00055 + 0.00015 * rng.random()) * HEAD))
            # The unopened florets at the heart of the disc are greener.
            inner = florets[: count // 6]
            spheres("disc", florets[count // 6:], 0.0006, disc_mat)
            heart = simple_material("heart", mix(mix((0.52, 0.55, 0.10), BLOOD, smooth(rot / 0.4)), NECROTIC, smooth((rot - 0.45) / 0.4)),
                                    roughness=0.6, texture=0.4)
            spheres("heart", inner, 0.0006, heart)

    elif species == "rose":
        sepals(5, 0.016)
        n = 26
        for i in range(n):
            f = fallen(i, n) if i > n * 0.4 else 0.0
            if f >= 1.0:
                continue
            k = i / (n - 1)
            az = i * 2.39996
            closed = 88.0 - 6.0 * k
            opened = 88.0 - 80.0 * k
            el = math.radians(closed + (opened - closed) * open_) - sag * k
            L = (0.011 + 0.022 * k) * (0.6 + 0.4 * open_) * HEAD
            W = L * 1.15 * shrivel
            petal(f"petal{i}", top + axis * (0.004 * HEAD * (1.0 - k)), axis, az, el, L, W,
                  lambda u: max(0.0, math.sin(math.pi * (0.12 + 0.8 * u))) ** 0.5 * (0.85 + 0.2 * u),
                  cup=1.1 - 0.4 * k, curl=-0.25 * k * open_, ruffle=0.0003 + ruffle_rot, twist=0.1,
                  material=pmat, seed=seed + i, fall=f)

    elif species == "tulip":
        sepals(3, 0.008)
        for i in range(6):
            f = fallen(i, 6)
            if f >= 1.0:
                continue
            inner = i % 2 == 1
            az = 2 * math.pi * i / 6
            el = math.radians(88.0 - (26.0 if inner else 32.0) * open_) - sag
            L = 0.046 * HEAD * (0.8 + 0.2 * open_) * (0.97 if inner else 1.0)
            W = 0.027 * HEAD * shrivel
            petal(f"petal{i}", top - axis * 0.002, axis, az, el, L, W, lambda u: max(0.0, math.sin(math.pi * u)) ** 0.7,
                  cup=1.2, curl=0.05, ruffle=0.0004 + ruffle_rot, twist=0.0, material=pmat, seed=seed + i, fall=f)

    elif species == "bell":
        # A campanula hanging from the arched tip, five-lobed rim.
        sepals(5, 0.007)
        flare = 0.6 + 0.4 * open_
        prof = []
        for i in range(10):
            s = i / 9
            r = (0.0035 + 0.0125 * (s ** 1.6) * flare * shrivel) * HEAD
            prof.append((r, -0.032 * HEAD * s * (0.75 + 0.25 * open_)))
        # Bells hang mouth-down from the arch, swinging out a little with the stem.
        down = (axis * 0.35 + Vector((0.0, 0.0, -1.0))).normalized()
        rot_m = down.to_track_quat("-Z", "Y").to_matrix().to_4x4()
        transform = Matrix.Translation(top) @ rot_m
        lathe("bell", prof, 30, pmat, transform, lobes=5, lobe_depth=0.12 * open_)

    # --- Gore: blood beads, a trail down the stem, and a pool.
    if rot > 0.12:
        bmat = blood_material()
        beads = []
        brng = random.Random(seed * 17)
        ref = Vector((1, 0, 0)) if abs(axis.x) < 0.9 else Vector((0, 1, 0))
        e1 = axis.cross(ref).normalized()
        e2 = axis.cross(e1).normalized()
        for k in range(int(30 * smooth((rot - 0.12) / 0.4))):
            a = brng.random() * 6.283
            d = brng.random() * 0.018 * HEAD
            beads.append((top + (e1 * math.cos(a) + e2 * math.sin(a)) * d + axis * (0.004 + 0.002 * brng.random()) * HEAD,
                          (0.0007 + 0.0011 * brng.random()) * HEAD))
        if beads:
            spheres("beads", beads, 0.001, bmat)
        progress = smooth((rot - 0.15) / 0.4)
        if progress > 0.02:
            m = len(pts)
            start = m - 1
            end = max(1, int(start - progress * (m - 2)))
            trail = [pts[i] + Vector((0.0, -0.0019, 0.0)) for i in range(start, end - 1, -1)]
            if len(trail) >= 2:
                tube_along("trail", trail, [0.0011] * len(trail), bmat, sides=8)
                spheres("drop", [(trail[-1] + Vector((0, 0, -0.0015)), 0.0022)], 0.002, bmat)
        pool = smooth((rot - 0.3) / 0.45)
        if pool > 0.02:
            pr = 0.028 * pool
            prof = [(pr * s, 0.0012 * (1 - s * s)) for s in [i / 6 for i in range(7)]]
            lathe("pool", list(reversed(prof)), 28, bmat, Matrix.Translation(Vector((pts[0].x, 0.0, -0.001))))

    return pts


# ------------------------------------------------------------------------ rendering

def frame_params(i):
    if i < BLOOM_FRAMES:
        return i / (BLOOM_FRAMES - 1), 0.0
    return 1.0, (i - BLOOM_FRAMES + 1) / ROT_FRAMES


def project(scene, cam, point, width, height):
    co = world_to_camera_view(scene, cam, point)
    return [round(co.x * width, 2), round((1.0 - co.y) * height, 2)]


def render_species(out_dir, species, colour, frames, width, height, samples, seed):
    scene = bpy.context.scene
    info = {"species": species, "colour": colour, "width": width, "height": height,
            "bloomFrames": BLOOM_FRAMES, "rotFrames": ROT_FRAMES, "frames": []}
    paths = []
    for i in frames:
        clear_scene()
        add_lights()
        cam = add_camera()
        bloom, rot = frame_params(i)
        pts = build_plant(species, colour, bloom, rot, seed)
        path = os.path.join(out_dir, f"_{species}_{colour}_{i:02d}.png")
        scene.render.filepath = path
        t0 = time.time()
        bpy.ops.render.render(write_still=True)
        print(f"{species} {colour} frame {i} ({bloom:.2f}/{rot:.2f}) {time.time() - t0:.1f}s", flush=True)
        paths.append(path)
        info["frames"].append({"stem": [project(scene, cam, p, width, height) for p in pts[::3] + [pts[-1]]],
                               "head": project(scene, cam, pts[-1], width, height)})
    return paths, info


def build_atlas(paths, out_path, width, height, cols=6):
    rows = (len(paths) + cols - 1) // cols
    atlas = bpy.data.images.new("atlas", width * cols, height * rows, alpha=True)
    pixels = [0.0] * (width * cols * height * rows * 4)
    W = width * cols
    for n, p in enumerate(paths):
        img = bpy.data.images.load(p)
        src = list(img.pixels)
        cx, cy = n % cols, rows - 1 - n // cols    # Blender images start at the bottom row
        for y in range(height):
            row = (cy * height + y) * W + cx * width
            pixels[row * 4:(row + width) * 4] = src[y * width * 4:(y + 1) * width * 4]
        bpy.data.images.remove(img)
    atlas.pixels = pixels
    atlas.filepath_raw = out_path
    atlas.file_format = "PNG"
    atlas.save()
    bpy.data.images.remove(atlas)
    for p in paths:
        os.remove(p)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--species", default="all")
    ap.add_argument("--colour", default="all")
    ap.add_argument("--frames", default="all")
    ap.add_argument("--size", default="144x216")
    ap.add_argument("--samples", type=int, default=32)
    ap.add_argument("--keep", action="store_true", help="keep individual frames, skip the atlas")
    ap.add_argument("--closeup", action="store_true", help="frame only the head (inspection)")
    args = ap.parse_args(argv)

    width, height = (int(v) for v in args.size.split("x"))
    if args.closeup:
        global CLOSEUP
        CLOSEUP = (0.0, STEM_HEIGHT, 0.11)
    total = BLOOM_FRAMES + ROT_FRAMES
    if args.frames == "all":
        frames = list(range(total))
    elif args.frames == "bloom":
        frames = list(range(BLOOM_FRAMES))
    elif args.frames == "rot":
        frames = list(range(BLOOM_FRAMES, total))
    else:
        frames = [int(f) for f in args.frames.split(",")]

    os.makedirs(args.out, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    setup_render(width, height, args.samples)

    species_list = SPECIES if args.species == "all" else args.species.split(",")
    colour_list = list(COLOURS) if args.colour == "all" else args.colour.split(",")
    for si, species in enumerate(species_list):
        for colour in colour_list:
            seed = 1000 + SPECIES.index(species) * 97
            atlas = os.path.join(args.out, f"{species}_{colour}.png")
            if not args.keep and os.path.exists(atlas) and os.path.exists(os.path.join(args.out, f"{species}.json")):
                print(f"skip {species} {colour} (done)", flush=True)
                continue
            paths, info = render_species(args.out, species, colour, frames, width, height, args.samples, seed)
            if args.keep:
                continue
            build_atlas(paths, atlas, width, height)
            with open(os.path.join(args.out, f"{species}.json"), "w") as f:
                json.dump(info, f, separators=(",", ":"))


if __name__ == "__main__":
    main()
