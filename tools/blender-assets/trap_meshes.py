# Builds the seven own trap meshes (alarm, fear, gas, lightning, fire burst, frost, trigger) in a background Blender.
# Every trap stands on a flat plate with the footprint of the spike trap (1.05 x 1.05, plate top at 0.07) so that
# placement and look-up on the tile stay the same; the details on it are small and never wider than the plate.
# The meshes are static (no skeleton, none of these traps plays a clip) and use plain colour materials that are
# written to materials/scripts/TrapTypes.material by this script.
#
#   blender --background --python trap_meshes.py -- <repo> <work folder> [<blend folder>]
#
# Writes <work folder>/<Name>.mesh.xml for every mesh (convert with  OgreXMLConverter -q <xml> <repo>/models/<Name>.mesh)
# and, when a blend folder is given, one <blend folder>/blenderTrap<Name>.blend per model.
import math
import os
import sys
import traceback

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import odp_ogre_io as oo

HALF = 0.525
PLATE_TOP = 0.07

# name: (ambient/diffuse rgb, specular strength, emissive rgb)
MATERIALS = {
    "TrapStone": ((0.30, 0.29, 0.28), 0.05, (0.0, 0.0, 0.0)),
    "TrapDarkStone": ((0.13, 0.12, 0.13), 0.05, (0.0, 0.0, 0.0)),
    "TrapIron": ((0.22, 0.22, 0.24), 0.35, (0.0, 0.0, 0.0)),
    "TrapBrass": ((0.68, 0.50, 0.16), 0.60, (0.0, 0.0, 0.0)),
    "TrapCopper": ((0.62, 0.32, 0.16), 0.55, (0.0, 0.0, 0.0)),
    "TrapBone": ((0.82, 0.78, 0.66), 0.10, (0.0, 0.0, 0.0)),
    "TrapGasMetal": ((0.30, 0.36, 0.20), 0.30, (0.0, 0.0, 0.0)),
    "TrapGasDark": ((0.07, 0.10, 0.05), 0.0, (0.02, 0.05, 0.01)),
    "TrapFrostStone": ((0.52, 0.62, 0.70), 0.30, (0.0, 0.0, 0.0)),
    "TrapIce": ((0.62, 0.84, 0.97), 0.80, (0.10, 0.22, 0.32)),
    "TrapGlowRed": ((0.60, 0.05, 0.04), 0.20, (0.85, 0.10, 0.06)),
    "TrapGlowGreen": ((0.30, 0.80, 0.30), 0.20, (0.35, 0.95, 0.35)),
    "TrapGlowBlue": ((0.50, 0.70, 1.00), 0.20, (0.45, 0.70, 1.00)),
    "TrapGlowOrange": ((0.90, 0.35, 0.05), 0.0, (1.00, 0.40, 0.06)),
    "TrapGlowViolet": ((0.62, 0.35, 0.95), 0.20, (0.60, 0.30, 0.95)),
}

parts_log = {}


# ---------------------------------------------------------------- geometry helpers
def material(name):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    rgb = MATERIALS[name][0]
    mat.diffuse_color = (rgb[0], rgb[1], rgb[2], 1.0)
    return mat


def slot_of(obj, name):
    names = [m.name for m in obj.data.materials]
    if name not in names:
        obj.data.materials.append(material(name))
        names.append(name)
    return names.index(name)


class Builder:
    def __init__(self, name):
        self.name = name
        self.mesh = bpy.data.meshes.new(name)
        self.obj = bpy.data.objects.new(name, self.mesh)
        bpy.context.scene.collection.objects.link(self.obj)
        self.bm = bmesh.new()

    def _finish(self, geom, mat_name, smooth):
        idx = slot_of(self.obj, mat_name)
        faces = [g for g in geom if isinstance(g, bmesh.types.BMFace)]
        for f in faces:
            f.material_index = idx
            f.smooth = smooth and abs(f.normal.z) < 0.99
        return geom

    def box(self, center, size, mat_name, rot_z=0.0):
        res = bmesh.ops.create_cube(self.bm, size=1.0)
        verts = res["verts"]
        bmesh.ops.scale(self.bm, vec=Vector(size), verts=verts)
        if rot_z:
            bmesh.ops.rotate(self.bm, cent=(0, 0, 0), matrix=Matrix.Rotation(rot_z, 3, "Z"), verts=verts)
        bmesh.ops.translate(self.bm, vec=Vector(center), verts=verts)
        faces = set(f for v in verts for f in v.link_faces)
        self._finish(list(faces), mat_name, False)

    def cone(self, center, r_bottom, r_top, height, mat_name, segments=12, smooth=True, caps=True):
        """Cylinder or cone standing on center (bottom of the part)."""
        res = bmesh.ops.create_cone(self.bm, cap_ends=caps, cap_tris=False, segments=segments,
                                    radius1=r_bottom, radius2=r_top, depth=height)
        verts = res["verts"]
        bmesh.ops.translate(self.bm, vec=Vector((center[0], center[1], center[2] + height * 0.5)), verts=verts)
        faces = set(f for v in verts for f in v.link_faces)
        self._finish(list(faces), mat_name, smooth)

    def sphere(self, center, radii, mat_name, segments=10, rings=6):
        res = bmesh.ops.create_uvsphere(self.bm, u_segments=segments, v_segments=rings, radius=1.0)
        verts = res["verts"]
        bmesh.ops.scale(self.bm, vec=Vector(radii), verts=verts)
        bmesh.ops.translate(self.bm, vec=Vector(center), verts=verts)
        faces = set(f for v in verts for f in v.link_faces)
        self._finish(list(faces), mat_name, True)

    def torus(self, center, major, minor, mat_name, segments=14, ring=6):
        verts = []
        for i in range(segments):
            a = 2.0 * math.pi * i / segments
            row = []
            for j in range(ring):
                b = 2.0 * math.pi * j / ring
                r = major + minor * math.cos(b)
                row.append(self.bm.verts.new((center[0] + r * math.cos(a), center[1] + r * math.sin(a),
                                              center[2] + minor * math.sin(b))))
            verts.append(row)
        faces = []
        for i in range(segments):
            n = (i + 1) % segments
            for j in range(ring):
                m = (j + 1) % ring
                faces.append(self.bm.faces.new((verts[i][j], verts[n][j], verts[n][m], verts[i][m])))
        self._finish(faces, mat_name, True)

    def pyramid(self, center, radius, height, mat_name, segments=6, tilt=(0.0, 0.0), twist=0.0):
        """A crystal: a prism with a pointed tip, tilted by tilt (radians around x and y)."""
        ring_bottom, ring_mid = [], []
        for i in range(segments):
            a = twist + 2.0 * math.pi * i / segments
            ring_bottom.append(self.bm.verts.new((radius * math.cos(a), radius * math.sin(a), 0.0)))
            ring_mid.append(self.bm.verts.new((radius * math.cos(a), radius * math.sin(a), height * 0.62)))
        tip = self.bm.verts.new((0.0, 0.0, height))
        faces = []
        for i in range(segments):
            n = (i + 1) % segments
            faces.append(self.bm.faces.new((ring_bottom[i], ring_bottom[n], ring_mid[n], ring_mid[i])))
            faces.append(self.bm.faces.new((ring_mid[i], ring_mid[n], tip)))
        faces.append(self.bm.faces.new(list(reversed(ring_bottom))))
        geom = ring_bottom + ring_mid + [tip]
        rot = Matrix.Rotation(tilt[1], 3, "Y") @ Matrix.Rotation(tilt[0], 3, "X")
        bmesh.ops.rotate(self.bm, cent=(0, 0, 0), matrix=rot, verts=geom)
        bmesh.ops.translate(self.bm, vec=Vector(center), verts=geom)
        self._finish(faces, mat_name, False)

    def plate(self, mat_name="TrapStone", border=None):
        """The common base: a flat slab with the spike trap footprint, optionally with a raised frame."""
        self.box((0, 0, PLATE_TOP * 0.5 - 0.0), (2 * HALF, 2 * HALF, PLATE_TOP), mat_name)
        if border:
            b_mat, width, h = border
            span = 2 * HALF
            for sign in (-1, 1):
                self.box((sign * (HALF - width * 0.5), 0, PLATE_TOP + h * 0.5), (width, span, h), b_mat)
                self.box((0, sign * (HALF - width * 0.5), PLATE_TOP + h * 0.5), (span - 2 * width, width, h), b_mat)

    def finish(self):
        self.bm.normal_update()
        self.bm.to_mesh(self.mesh)
        self.bm.free()
        self.mesh.update()
        return self.obj


# ---------------------------------------------------------------- the seven models
def build_alarm():
    b = Builder("Alarm")
    b.plate("TrapStone", ("TrapIron", 0.05, 0.03))
    top = PLATE_TOP
    # iron mounting ring and a brass bell: dome, flared rim, clapper, and a red lamp on a short post beside it
    b.cone((0, 0, top), 0.30, 0.26, 0.04, "TrapIron", 16)
    b.cone((0, 0, top + 0.04), 0.20, 0.17, 0.07, "TrapBrass", 16)
    b.cone((0, 0, top + 0.11), 0.17, 0.10, 0.09, "TrapBrass", 16)
    b.sphere((0, 0, top + 0.20), (0.10, 0.10, 0.07), "TrapBrass", 12, 6)
    b.cone((0, 0, top + 0.26), 0.02, 0.02, 0.03, "TrapIron", 6)
    b.sphere((0, 0, top + 0.05), (0.035, 0.035, 0.035), "TrapIron", 8, 4)
    b.cone((0.36, -0.30, top), 0.045, 0.035, 0.12, "TrapIron", 8)
    b.sphere((0.36, -0.30, top + 0.16), (0.075, 0.075, 0.09), "TrapGlowRed", 10, 6)
    b.cone((-0.36, 0.30, top), 0.045, 0.035, 0.12, "TrapIron", 8)
    b.sphere((-0.36, 0.30, top + 0.16), (0.075, 0.075, 0.09), "TrapGlowRed", 10, 6)
    return b.finish()


def build_fear():
    b = Builder("Fear")
    b.plate("TrapDarkStone", ("TrapIron", 0.05, 0.025))
    top = PLATE_TOP
    # a skull in the middle: cranium, jaw, two hollow eyes with a glow, nose slit and teeth
    b.sphere((0, 0, top + 0.13), (0.20, 0.20, 0.16), "TrapBone", 14, 8)
    b.box((0, -0.06, top + 0.045), (0.20, 0.17, 0.07), "TrapBone")
    for sign in (-1, 1):
        b.sphere((sign * 0.075, -0.165, top + 0.15), (0.055, 0.03, 0.06), "TrapDarkStone", 8, 5)
        b.sphere((sign * 0.075, -0.19, top + 0.15), (0.028, 0.02, 0.030), "TrapGlowGreen", 6, 4)
    b.box((0, -0.19, top + 0.095), (0.03, 0.03, 0.055), "TrapDarkStone")
    for i in range(5):
        b.box((-0.07 + i * 0.035, -0.158, top + 0.025), (0.022, 0.03, 0.05), "TrapBone")
    # four bone studs in the corners
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cone((sx * 0.38, sy * 0.38, top), 0.045, 0.03, 0.10, "TrapBone", 6)
            b.sphere((sx * 0.38, sy * 0.38, top + 0.11), (0.05, 0.05, 0.045), "TrapBone", 8, 4)
    return b.finish()


def build_gas():
    b = Builder("Gas")
    b.plate("TrapStone")
    top = PLATE_TOP
    # a square vent: raised metal frame, dark recess, seven bars, a short pipe stub at the back
    frame = 0.07
    outer = 0.42
    b.box((0, 0, top + 0.035), (2 * outer, 2 * outer, 0.07), "TrapGasDark")
    for sign in (-1, 1):
        b.box((sign * (outer - frame * 0.5), 0, top + 0.06), (frame, 2 * outer, 0.12), "TrapGasMetal")
        b.box((0, sign * (outer - frame * 0.5), top + 0.06), (2 * outer - 2 * frame, frame, 0.12), "TrapGasMetal")
    for i in range(7):
        y = -0.30 + i * 0.10
        b.box((0, y, top + 0.075), (2 * outer - 2 * frame, 0.045, 0.045), "TrapGasMetal")
    b.cone((0.33, 0.33, top + 0.12), 0.075, 0.075, 0.10, "TrapGasMetal", 10)
    b.torus((0.33, 0.33, top + 0.23), 0.06, 0.018, "TrapGasMetal", 10, 5)
    return b.finish()


def build_lightning():
    b = Builder("Lightning")
    b.plate("TrapDarkStone", ("TrapIron", 0.05, 0.025))
    top = PLATE_TOP
    # a rod with three copper coils and a glowing blue tip on an iron foot; four ceramic insulators around it
    b.cone((0, 0, top), 0.19, 0.15, 0.05, "TrapIron", 12)
    b.cone((0, 0, top + 0.05), 0.035, 0.03, 0.26, "TrapIron", 8)
    for i, z in enumerate((0.09, 0.16, 0.23)):
        b.torus((0, 0, top + z), 0.085 - 0.012 * i, 0.022, "TrapCopper", 14, 6)
    b.sphere((0, 0, top + 0.34), (0.055, 0.055, 0.055), "TrapGlowBlue", 10, 6)
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * 0.36, sy * 0.36
            b.cone((x, y, top), 0.05, 0.05, 0.05, "TrapIron", 8)
            b.cone((x, y, top + 0.05), 0.04, 0.04, 0.05, "TrapBone", 8)
            b.sphere((x, y, top + 0.11), (0.03, 0.03, 0.03), "TrapGlowBlue", 6, 4)
    return b.finish()


def build_fireburst():
    b = Builder("Fireburst")
    b.plate("TrapDarkStone")
    top = PLATE_TOP
    # an iron grate over a bed of embers
    outer = 0.43
    b.box((0, 0, top + 0.02), (2 * outer, 2 * outer, 0.04), "TrapGlowOrange")
    for sign in (-1, 1):
        b.box((sign * (outer - 0.04), 0, top + 0.07), (0.08, 2 * outer, 0.10), "TrapIron")
        b.box((0, sign * (outer - 0.04), top + 0.07), (2 * outer - 0.16, 0.08, 0.10), "TrapIron")
    for i in range(5):
        c = -0.26 + i * 0.13
        b.box((c, 0, top + 0.085), (0.045, 2 * outer - 0.16, 0.045), "TrapIron")
        b.box((0, c, top + 0.085), (2 * outer - 0.16, 0.045, 0.045), "TrapIron")
    for sx, sy in ((-0.14, 0.10), (0.16, -0.06), (0.02, 0.22), (-0.2, -0.2)):
        b.sphere((sx, sy, top + 0.05), (0.05, 0.05, 0.03), "TrapGlowOrange", 6, 4)
    return b.finish()


def build_frost():
    b = Builder("Frost")
    b.plate("TrapFrostStone", ("TrapIce", 0.05, 0.03))
    top = PLATE_TOP
    # a cluster of ice crystals, the tall one leaning slightly
    b.pyramid((0.0, 0.0, top), 0.10, 0.34, "TrapIce", 6, (0.08, -0.06))
    b.pyramid((0.17, 0.05, top), 0.065, 0.20, "TrapIce", 6, (0.0, 0.28), 0.3)
    b.pyramid((-0.15, 0.08, top), 0.07, 0.24, "TrapIce", 6, (-0.1, -0.30), 0.6)
    b.pyramid((0.04, -0.17, top), 0.06, 0.17, "TrapIce", 6, (0.30, 0.1), 0.2)
    b.pyramid((-0.08, -0.16, top), 0.045, 0.11, "TrapIce", 6, (0.28, -0.2), 0.9)
    b.pyramid((0.30, -0.28, top), 0.04, 0.10, "TrapIce", 5, (0.2, 0.3))
    b.pyramid((-0.30, 0.30, top), 0.045, 0.12, "TrapIce", 5, (-0.25, -0.2), 0.5)
    return b.finish()


def build_trigger():
    b = Builder("Trigger")
    b.plate("TrapDarkStone", ("TrapStone", 0.05, 0.03))
    top = PLATE_TOP
    # a raised slab with an engraved glowing rune: ring, bar, two diagonals
    b.box((0, 0, top + 0.02), (0.76, 0.76, 0.04), "TrapStone")
    glow = top + 0.045
    b.torus((0, 0, glow), 0.255, 0.014, "TrapGlowViolet", 24, 5)
    b.box((0, 0, glow), (0.025, 0.46, 0.02), "TrapGlowViolet")
    b.box((0, 0.0, glow), (0.20, 0.025, 0.02), "TrapGlowViolet", math.radians(40))
    b.box((0, 0.0, glow), (0.20, 0.025, 0.02), "TrapGlowViolet", math.radians(-40))
    b.box((0, 0.17, glow), (0.12, 0.025, 0.02), "TrapGlowViolet")
    b.box((0, -0.17, glow), (0.12, 0.025, 0.02), "TrapGlowViolet")
    b.sphere((0, 0, glow + 0.005), (0.04, 0.04, 0.02), "TrapGlowViolet", 8, 4)
    return b.finish()


MODELS = (
    ("AlarmTrap", build_alarm),
    ("FearTrap", build_fear),
    ("GasTrap", build_gas),
    ("LightningTrap", build_lightning),
    ("FireburstTrap", build_fireburst),
    ("FrostTrap", build_frost),
    ("TriggerTrap", build_trigger),
)


def material_script():
    lines = ["// Colour materials of the own trap meshes, written by tools/blender-assets/trap_meshes.py", ""]
    for name in sorted(MATERIALS):
        rgb, spec, emi = MATERIALS[name]
        lines += [
            "material %s" % name,
            "{",
            "    receive_shadows on",
            "",
            "    technique",
            "    {",
            "        pass",
            "        {",
            "            ambient %.2f %.2f %.2f 1.0" % rgb,
            "            diffuse %.2f %.2f %.2f 1.0" % rgb,
            "            specular %.2f %.2f %.2f 1.0 20.0" % (spec, spec, spec),
            "            emissive %.2f %.2f %.2f 1.0" % emi,
            "        }",
            "    }",
            "}",
            "",
        ]
    return "\n".join(lines)


def main():
    repo, work = sys.argv[sys.argv.index("--") + 1:][:2]
    rest = sys.argv[sys.argv.index("--") + 1:][2:]
    blend_dir = rest[0] if rest else None
    os.makedirs(work, exist_ok=True)
    report = open(os.path.join(work, "trap_meshes.log"), "w")
    for name, fn in MODELS:
        # a clean scene per model, so every .blend holds exactly one model
        for obj in list(bpy.data.objects):
            bpy.data.objects.remove(obj, do_unlink=True)
        for mesh in list(bpy.data.meshes):
            bpy.data.meshes.remove(mesh)
        obj = fn()
        obj.name = name
        obj.data.name = name
        stats = oo.export_mesh_xml(obj, None, os.path.join(work, name + ".mesh.xml"), None, smooth=False)
        pts = [v.co for v in obj.data.vertices]
        box = [(round(min(p[i] for p in pts), 3), round(max(p[i] for p in pts), 3)) for i in range(3)]
        report.write("%s %s bounds %s\n" % (name, stats, box))
        report.flush()
        if blend_dir:
            bpy.ops.wm.save_as_mainfile(filepath=os.path.join(blend_dir, "blenderTrap%s.blend" % name.replace("Trap", "")),
                                        copy=True)
    with open(os.path.join(repo, "materials", "scripts", "TrapTypes.material"), "w", newline="\n") as handle:
        handle.write(material_script())
    report.write("done\n")
    report.close()


try:
    main()
except Exception:
    out = sys.argv[sys.argv.index("--") + 2] if "--" in sys.argv else "."
    with open(os.path.join(out, "trap_meshes_error.txt"), "w") as handle:
        handle.write(traceback.format_exc())
