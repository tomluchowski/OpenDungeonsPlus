# Builds the hatchery meshes in Blender from the hen mesh: chick, rooster (comb, wattle, tail) and the egg meshes.
# Needs the hen armature "HenArm" and mesh "HenMesh" imported with odp_ogre_io.
import math
import bpy
import bmesh
from mathutils import Vector

COL = None


def col():
    return bpy.data.collections["Hatchery_Assets"]


def material(name, color=None):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    if color is not None:
        m.diffuse_color = color
    return m


def dominant_groups(obj):
    names = {g.index: g.name for g in obj.vertex_groups}
    res = {}
    for v in obj.data.vertices:
        if v.groups:
            g = max(v.groups, key=lambda x: x.weight)
            res[v.index] = names[g.group]
    return res


def scale_group(obj, dom, groups, factor, pivot):
    piv = Vector(pivot)
    for v in obj.data.vertices:
        if dom.get(v.index) in groups:
            v.co = piv + (v.co - piv) * factor if isinstance(factor, float) else piv + Vector(
                (c * f for c, f in zip((v.co - piv), factor)))


def copy_hen(name):
    hen = bpy.data.objects["HenMesh"]
    obj = hen.copy()
    obj.data = hen.data.copy()
    obj.data.name = name
    obj.name = name
    col().objects.link(obj)
    return obj


def add_part(obj, bm_fn, material_name, weights):
    """Adds geometry made by bm_fn(bm) to obj as a new material slot, all new vertices get weights
    (dict bone -> weight) or a callable vertex -> dict."""
    me = obj.data
    names = [m.name for m in me.materials]
    if material_name in names:
        slot = names.index(material_name)
    else:
        me.materials.append(material(material_name))
        slot = len(me.materials) - 1
    bm = bmesh.new()
    bm.from_mesh(me)
    before = len(bm.verts)
    old_faces = set(bm.faces)
    bm_fn(bm)
    for f in bm.faces:
        if f not in old_faces:
            f.material_index = slot
    bm.to_mesh(me)
    bm.free()
    me.update()
    for v in me.vertices:
        if v.index >= before:
            w = weights(v) if callable(weights) else weights
            for bone, wt in w.items():
                obj.vertex_groups[bone].add([v.index], wt, "REPLACE")
    return slot


def ribbon(bm, pts, widths, normal_axis=(1, 0, 0)):
    """A flat strip along points pts with half widths, lying across normal_axis."""
    side = Vector(normal_axis)
    rows = []
    for p, w in zip(pts, widths):
        p = Vector(p)
        rows.append((bm.verts.new(p - side * w), bm.verts.new(p + side * w)))
    for a, b in zip(rows, rows[1:]):
        bm.faces.new((a[0], a[1], b[1], b[0]))
    return rows


def build_chick():
    obj = copy_hen("ChickMesh")
    dom = dominant_groups(obj)
    head_base = (0.0, -0.100, 0.135)
    scale_group(obj, dom, {"Head"}, 1.5, head_base)
    scale_group(obj, dom, {"NeckBone"}, (1.0, 1.0, 0.8), (0.0, -0.07, 0.12))
    scale_group(obj, dom, {"Wing_L", "Wing_R", "Shoulder_L", "Shoulder_R"}, 0.7, (0.0, 0.03, 0.10))
    scale_group(obj, dom, {"TailFeather_L", "TailFeather_R"}, 0.55, (0.0, 0.03, 0.10))
    obj.data.materials[0] = material("ChickenChick")
    obj.data.update()
    return obj


def build_rooster():
    obj = copy_hen("RoosterMesh")
    dom = dominant_groups(obj)
    scale_group(obj, dom, {"Head"}, 1.12, (0.0, -0.100, 0.135))
    scale_group(obj, dom, {"Breast_L", "Breast_R", "Wishbone"}, (1.12, 1.0, 1.1), (0.0, 0.0, 0.10))
    obj.data.materials[0] = material("ChickenRooster")

    # comb: five rounded lobes standing on top of the head (the head looks along -y)
    def comb(bm):
        outline = [(-0.158, 0.168), (-0.152, 0.198), (-0.143, 0.176), (-0.136, 0.206), (-0.126, 0.178),
                   (-0.118, 0.203), (-0.110, 0.173), (-0.103, 0.186), (-0.100, 0.160)]
        half = 0.0085
        left = [bm.verts.new((-half, y, z)) for y, z in outline]
        right = [bm.verts.new((half, y, z)) for y, z in outline]
        # fan faces on both sides need a base line, build as strip between outline and the head top line
        base_l = [bm.verts.new((-half, y, 0.158)) for y, z in outline]
        base_r = [bm.verts.new((half, y, 0.158)) for y, z in outline]
        for i in range(len(outline) - 1):
            bm.faces.new((left[i], left[i + 1], base_l[i + 1], base_l[i]))
            bm.faces.new((right[i + 1], right[i], base_r[i], base_r[i + 1]))
            bm.faces.new((left[i], right[i], right[i + 1], left[i + 1]))
        bm.faces.new((left[0], base_l[0], base_r[0], right[0]))
        bm.faces.new((right[-1], base_r[-1], base_l[-1], left[-1]))

    add_part(obj, comb, "ChickenRoosterComb", {"Head": 1.0})

    # wattle: a small drop under the beak
    def wattle(bm):
        pts = [(-0.152, 0.122), (-0.148, 0.108), (-0.145, 0.092), (-0.142, 0.104), (-0.138, 0.121)]
        half = 0.004
        a = [bm.verts.new((-half, y, z)) for y, z in pts]
        b = [bm.verts.new((half, y, z)) for y, z in pts]
        for i in range(len(pts) - 1):
            bm.faces.new((a[i], a[i + 1], b[i + 1], b[i]))
        bm.faces.new((a[0], b[0], b[-1], a[-1]))

    add_part(obj, wattle, "ChickenRoosterComb", {"Head": 1.0})

    # tail: long curved feathers that rise behind the body, broad in the vertical plane
    def tail(bm):
        specs = [(-0.05, 0.20, 0.018), (-0.026, 0.25, 0.022), (0.0, 0.29, 0.024), (0.026, 0.25, 0.022), (0.05, 0.20, 0.018)]
        for spread, top, w in specs:
            pts = []
            for k in range(8):
                t = k / 7.0
                y = 0.05 + 0.21 * t + 0.08 * t * t
                z = 0.105 + (top - 0.105) * math.sin(t * 1.25) / math.sin(1.25) - 0.06 * t * t
                pts.append(Vector((spread * t, y, z)))
            rows = []
            for k, p in enumerate(pts):
                tan = (pts[min(k + 1, 7)] - pts[max(k - 1, 0)]).normalized()
                nrm = Vector((0.0, -tan.z, tan.y)).normalized()
                t = k / 7.0
                half = w * (1.0 - 0.45 * t) if k < 7 else w * 0.15
                rows.append((bm.verts.new(p - nrm * half), bm.verts.new(p + nrm * half)))
            for a_, b_ in zip(rows, rows[1:]):
                bm.faces.new((a_[0], a_[1], b_[1], b_[0]))

    def tail_weights(v):
        if v.co.x < -0.01:
            return {"TailFeather_R": 0.6, "Hip": 0.4}
        if v.co.x > 0.01:
            return {"TailFeather_L": 0.6, "Hip": 0.4}
        return {"Hip": 1.0}

    add_part(obj, tail, "ChickenRoosterTail", tail_weights)
    obj.data.update()
    return obj


def build_all():
    for n in ("ChickMesh", "RoosterMesh"):
        o = bpy.data.objects.get(n)
        if o is not None:
            m = o.data
            bpy.data.objects.remove(o, do_unlink=True)
            bpy.data.meshes.remove(m)
    c = build_chick()
    r = build_rooster()
    c.location = (0, 0, 0)
    return c, r
