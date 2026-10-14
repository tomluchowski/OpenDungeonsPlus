# Ogre XML (.mesh.xml / .skeleton.xml) import and export for Blender 4.x/5.x.
#
# Own, minimal tool written for this project (the OGRE add-on of Blender 5 can no longer
# create or read animation curves). Coordinates are kept as they are in the XML files, so a
# round trip changes nothing. Bone frames in Blender are the Ogre bone frames.
#
# Usage inside Blender:  import odp_ogre_io as oo
#   arm = oo.import_skeleton(path_skeleton_xml, "Name")
#   obj = oo.import_mesh(path_mesh_xml, arm, "Name", texture_path)
#   oo.export_skeleton_xml(arm, out_path)         (all actions of the armature are written)
#   oo.export_mesh_xml(obj, arm, out_path, "Name.skeleton")
import math
import os
import xml.etree.ElementTree as ET

import bpy
import bmesh
from mathutils import Matrix, Quaternion, Vector

FPS = 24
BONE_LEN = 0.02


def _quat(rot_el):
    if rot_el is None:
        return Quaternion((1, 0, 0, 0))
    angle = float(rot_el.get("angle", 0))
    ax = rot_el.find("axis")
    axis = Vector((float(ax.get("x")), float(ax.get("y")), float(ax.get("z"))))
    if abs(angle) < 1e-9 or axis.length < 1e-9:
        return Quaternion((1, 0, 0, 0))
    return Quaternion(axis.normalized(), angle)


def _vec(el, default=(0, 0, 0)):
    if el is None:
        return Vector(default)
    return Vector((float(el.get("x")), float(el.get("y")), float(el.get("z"))))


def _fmt(v):
    s = ("%.7g" % v)
    return "0" if s in ("-0", "0") else s


def read_skeleton(path):
    root = ET.parse(path).getroot()
    bones = []
    for b in root.find("bones").findall("bone"):
        bones.append({
            "id": int(b.get("id")), "name": b.get("name"),
            "pos": _vec(b.find("position")), "rot": _quat(b.find("rotation")),
            "scale": _vec(b.find("scale"), (1, 1, 1)),
        })
    bones.sort(key=lambda d: d["id"])
    parents = {}
    bh = root.find("bonehierarchy")
    if bh is not None:
        for p in bh.findall("boneparent"):
            parents[p.get("bone")] = p.get("parent")
    anims = []
    an = root.find("animations")
    if an is not None:
        for a in an.findall("animation"):
            tracks = []
            for t in a.find("tracks").findall("track"):
                keys = []
                for k in t.find("keyframes").findall("keyframe"):
                    sc = k.find("scale")
                    keys.append((float(k.get("time")), _vec(k.find("translate")), _quat(k.find("rotate")),
                                 _vec(sc, (1, 1, 1))))
                tracks.append((t.get("bone"), keys))
            anims.append((a.get("name"), float(a.get("length")), tracks))
    return bones, parents, anims


def _set_active(obj):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)


def import_skeleton(path, name, with_actions=True, collection=None):
    bones, parents, anims = read_skeleton(path)
    arm_data = bpy.data.armatures.new(name)
    arm = bpy.data.objects.new(name, arm_data)
    (collection or bpy.context.scene.collection).objects.link(arm)
    _set_active(arm)
    bpy.ops.object.mode_set(mode="EDIT")
    world = {}
    by_name = {b["name"]: b for b in bones}

    def world_of(n):
        if n in world:
            return world[n]
        b = by_name[n]
        local = Matrix.Translation(b["pos"]) @ b["rot"].to_matrix().to_4x4()
        par = parents.get(n)
        world[n] = (world_of(par) @ local) if par else local
        return world[n]

    for b in bones:
        world_of(b["name"])
    pending = list(bones)
    created = set()
    while pending:
        rest = []
        for b in pending:
            par = parents.get(b["name"])
            if par and par not in created:
                rest.append(b)
                continue
            eb = arm_data.edit_bones.new(b["name"])
            eb.head = (0, 0, 0)
            eb.tail = (0, BONE_LEN, 0)
            eb.matrix = world[b["name"]]
            eb.length = BONE_LEN
            if par:
                eb.parent = arm_data.edit_bones[par]
            created.add(b["name"])
        pending = rest
    bpy.ops.object.mode_set(mode="OBJECT")
    arm["ogre_bone_order"] = [b["name"] for b in bones]
    arm.rotation_mode = "QUATERNION"
    if with_actions:
        for (aname, length, tracks) in anims:
            _make_action(arm, aname, length, tracks)
    return arm


def _make_action(arm, aname, length, tracks):
    act = bpy.data.actions.new(aname)
    act["ogre_length"] = length
    slot = act.slots.new(id_type="OBJECT", name=arm.name)
    layer = act.layers.new("Layer")
    strip = layer.strips.new(type="KEYFRAME")
    cb = strip.channelbag(slot, ensure=True)
    for (bname, keys) in tracks:
        pb = arm.pose.bones.get(bname)
        if pb is None:
            continue
        bq = pb.bone.matrix_local.to_quaternion()
        if pb.parent:
            bq = (pb.parent.bone.matrix_local.inverted() @ pb.bone.matrix_local).to_quaternion()
        n = len(keys)
        frames = [k[0] * FPS for k in keys]
        locs, rots, scs = [], [], []
        prev = None
        for (_t, tr, q, sc) in keys:
            if prev is not None and prev.dot(q) < 0:
                q = Quaternion((-q.w, -q.x, -q.y, -q.z))
            prev = q
            locs.append(bq.inverted() @ tr)
            rots.append(q)
            scs.append(sc)
        gname = bname
        for ch, (path, size, vals) in enumerate((
                ("location", 3, locs), ("rotation_quaternion", 4, rots), ("scale", 3, scs))):
            for i in range(size):
                dp = 'pose.bones["%s"].%s' % (bname, path)
                fc = cb.fcurves.new(dp, index=i, group_name=gname)
                fc.keyframe_points.add(n)
                co = []
                for f, v in zip(frames, vals):
                    co.extend((f, v[i]))
                fc.keyframe_points.foreach_set("co", co)
                for kp in fc.keyframe_points:
                    kp.interpolation = "LINEAR"
                fc.update()
    return act


def assign_action(arm, act):
    if arm.animation_data is None:
        arm.animation_data_create()
    arm.animation_data.action = act
    if act.slots:
        arm.animation_data.action_slot = act.slots[0]


def _channelbag(act):
    for layer in act.layers:
        for strip in layer.strips:
            for slot in act.slots:
                cb = strip.channelbag(slot)
                if cb is not None:
                    return cb
    return None


def new_action(arm, name, length):
    """Empty action with slot, layer and strip, ready for keyframes via add_key()."""
    act = bpy.data.actions.new(name)
    act["ogre_length"] = length
    slot = act.slots.new(id_type="OBJECT", name=arm.name)
    layer = act.layers.new("Layer")
    strip = layer.strips.new(type="KEYFRAME")
    strip.channelbag(slot, ensure=True)
    return act


def add_pose_key(arm, act, bone, t, loc=None, rot=None, scale=None):
    """Add one key at time t (seconds). loc is in the Ogre sense (parent space offset added to the
    bind position), rot is the extra rotation in the bone frame (Ogre keyframe rotation)."""
    pb = arm.pose.bones[bone]
    cb = _channelbag(act)
    bq = (pb.parent.bone.matrix_local.inverted() @ pb.bone.matrix_local).to_quaternion() if pb.parent \
        else pb.bone.matrix_local.to_quaternion()
    frame = t * FPS
    vals = {}
    vals["location"] = (bq.inverted() @ Vector(loc)) if loc is not None else Vector((0, 0, 0))
    vals["rotation_quaternion"] = Quaternion(rot) if rot is not None else Quaternion((1, 0, 0, 0))
    vals["scale"] = Vector(scale) if scale is not None else Vector((1, 1, 1))
    for path, size in (("location", 3), ("rotation_quaternion", 4), ("scale", 3)):
        for i in range(size):
            dp = 'pose.bones["%s"].%s' % (bone, path)
            fc = None
            for f in cb.fcurves:
                if f.data_path == dp and f.array_index == i:
                    fc = f
                    break
            if fc is None:
                fc = cb.fcurves.new(dp, index=i, group_name=bone)
            kp = fc.keyframe_points.insert(frame, vals[path][i], options={"FAST"})
            kp.interpolation = "LINEAR"
            fc.update()


def _action_tracks(arm, act):
    """-> {bone: [(time, T, Q, S)]} using bind-space conversion; keys at union of channel key times."""
    cb = _channelbag(act)
    per_bone = {}
    for fc in cb.fcurves:
        dp = fc.data_path
        if not dp.startswith('pose.bones["'):
            continue
        bname = dp[len('pose.bones["'):dp.index('"]')]
        per_bone.setdefault(bname, []).append(fc)
    order = arm.get("ogre_bone_order", [])
    result = []
    for bname in [b for b in order if b in per_bone] + [b for b in per_bone if b not in order]:
        pb = arm.pose.bones.get(bname)
        if pb is None:
            continue
        bq = (pb.parent.bone.matrix_local.inverted() @ pb.bone.matrix_local).to_quaternion() if pb.parent \
            else pb.bone.matrix_local.to_quaternion()
        fcs = {(f.data_path.rsplit(".", 1)[1], f.array_index): f for f in per_bone[bname]}
        frames = sorted({round(kp.co.x, 5) for f in per_bone[bname] for kp in f.keyframe_points})
        keys = []
        for fr in frames:
            def ev(path, i, dflt):
                f = fcs.get((path, i))
                return f.evaluate(fr) if f else dflt
            loc = Vector((ev("location", 0, 0), ev("location", 1, 0), ev("location", 2, 0)))
            q = Quaternion((ev("rotation_quaternion", 0, 1), ev("rotation_quaternion", 1, 0),
                            ev("rotation_quaternion", 2, 0), ev("rotation_quaternion", 3, 0)))
            q.normalize()
            sc = Vector((ev("scale", 0, 1), ev("scale", 1, 1), ev("scale", 2, 1)))
            keys.append((fr / FPS, bq @ loc, q, sc))
        result.append((bname, keys))
    return result


def _quat_xml(q, tag, indent):
    q = q.copy()
    q.normalize()
    angle = q.angle
    axis = q.axis
    if abs(angle) < 1e-7:
        angle, axis = 0.0, Vector((1, 0, 0))
    return '%s<%s angle="%s">\n%s\t<axis x="%s" y="%s" z="%s" />\n%s</%s>\n' % (
        indent, tag, _fmt(angle), indent, _fmt(axis.x), _fmt(axis.y), _fmt(axis.z), indent, tag)


def export_skeleton_xml(arm, path, only_actions=None):
    order = list(arm.get("ogre_bone_order", []))
    for b in arm.data.bones:
        if b.name not in order:
            order.append(b.name)
    out = ['<?xml version="1.0"?>\n<skeleton blendmode="average">\n\t<bones>\n']
    for i, name in enumerate(order):
        b = arm.data.bones[name]
        local = (b.parent.matrix_local.inverted() @ b.matrix_local) if b.parent else b.matrix_local
        t = local.to_translation()
        q = local.to_quaternion()
        out.append('\t\t<bone id="%d" name="%s">\n\t\t\t<position x="%s" y="%s" z="%s" />\n' % (
            i, name, _fmt(t.x), _fmt(t.y), _fmt(t.z)))
        out.append(_quat_xml(q, "rotation", "\t\t\t"))
        out.append("\t\t</bone>\n")
    out.append("\t</bones>\n\t<bonehierarchy>\n")
    for name in order:
        b = arm.data.bones[name]
        if b.parent:
            out.append('\t\t<boneparent bone="%s" parent="%s" />\n' % (name, b.parent.name))
    out.append("\t</bonehierarchy>\n\t<animations>\n")
    acts = [a for a in bpy.data.actions if a.get("ogre_length") is not None]
    if only_actions is not None:
        acts = [a for a in acts if a.name in only_actions]
    acts.sort(key=lambda a: a.name)
    for act in acts:
        out.append('\t\t<animation name="%s" length="%s">\n\t\t\t<tracks>\n' % (act.name, _fmt(act["ogre_length"])))
        for bname, keys in _action_tracks(arm, act):
            out.append('\t\t\t\t<track bone="%s">\n\t\t\t\t\t<keyframes>\n' % bname)
            for (t, tr, q, sc) in keys:
                out.append('\t\t\t\t\t\t<keyframe time="%s">\n' % _fmt(t))
                out.append('\t\t\t\t\t\t\t<translate x="%s" y="%s" z="%s" />\n' % (_fmt(tr.x), _fmt(tr.y), _fmt(tr.z)))
                out.append(_quat_xml(q, "rotate", "\t\t\t\t\t\t\t"))
                if abs(sc.x - 1) > 1e-6 or abs(sc.y - 1) > 1e-6 or abs(sc.z - 1) > 1e-6:
                    out.append('\t\t\t\t\t\t\t<scale x="%s" y="%s" z="%s" />\n' % (_fmt(sc.x), _fmt(sc.y), _fmt(sc.z)))
                out.append("\t\t\t\t\t\t</keyframe>\n")
            out.append("\t\t\t\t\t</keyframes>\n\t\t\t\t</track>\n")
        out.append("\t\t\t</tracks>\n\t\t</animation>\n")
    out.append("\t</animations>\n</skeleton>\n")
    with open(path, "w", newline="\n") as f:
        f.write("".join(out))


def import_mesh(path, arm, name, texture_path=None, collection=None):
    root = ET.parse(path).getroot()
    order = list(arm.get("ogre_bone_order", [])) if arm else []

    def read_geom(g):
        pos, nor, uv = [], [], []
        for vb in g.findall("vertexbuffer"):
            for v in vb.findall("vertex"):
                p = v.find("position")
                if p is not None:
                    pos.append(_vec(p))
                n = v.find("normal")
                if n is not None:
                    nor.append(_vec(n))
                t = v.find("texcoord")
                if t is not None:
                    uv.append((float(t.get("u")), float(t.get("v"))))
        return pos, nor, uv

    def read_assign(el):
        res = {}
        if el is None:
            return res
        for a in el.findall("vertexboneassignment"):
            res.setdefault(int(a.get("vertexindex")), []).append((int(a.get("boneindex")), float(a.get("weight"))))
        return res

    verts, norms, uvs, faces, fmat, weights, matnames = [], [], [], [], [], {}, []
    shared = root.find("sharedgeometry")
    spos = snor = suv = None
    base_shared = 0
    if shared is not None:
        spos, snor, suv = read_geom(shared)
        base_shared = 0
        verts += spos
        norms += snor
        uvs += suv
        for vi, lst in read_assign(root.find("boneassignments")).items():
            weights[vi] = lst
    for sm in root.find("submeshes").findall("submesh"):
        mname = sm.get("material")
        if mname not in matnames:
            matnames.append(mname)
        mi = matnames.index(mname)
        if sm.get("usesharedvertices") == "true":
            off = base_shared
        else:
            off = len(verts)
            g = sm.find("geometry")
            p, n, u = read_geom(g)
            verts += p
            norms += n
            uvs += u
            for vi, lst in read_assign(sm.find("boneassignments")).items():
                weights[off + vi] = lst
        for f in sm.find("faces").findall("face"):
            faces.append((off + int(f.get("v1")), off + int(f.get("v2")), off + int(f.get("v3"))))
            fmat.append(mi)
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in verts], [], faces)
    me.update()
    if len(uvs) == len(verts):
        layer = me.uv_layers.new(name="UVMap")
        for li, loop in enumerate(me.loops):
            u, v = uvs[loop.vertex_index]
            layer.data[li].uv = (u, 1.0 - v)
    if len(norms) == len(verts):
        try:
            me.normals_split_custom_set_from_vertices([tuple(n) for n in norms])
        except Exception:
            pass
    for mname in matnames:
        mat = bpy.data.materials.get(mname) or bpy.data.materials.new(mname)
        me.materials.append(mat)
        if texture_path and os.path.exists(texture_path) and mname == matnames[0]:
            mat.use_nodes = True
            nt = mat.node_tree
            tex = nt.nodes.new("ShaderNodeTexImage")
            tex.image = bpy.data.images.load(texture_path, check_existing=True)
            bsdf = nt.nodes.get("Principled BSDF")
            if bsdf:
                nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    for pi, mi in enumerate(fmat):
        me.polygons[pi].material_index = mi
    obj = bpy.data.objects.new(name, me)
    (collection or bpy.context.scene.collection).objects.link(obj)
    if arm is not None:
        for bn in order:
            obj.vertex_groups.new(name=bn)
        for vi, lst in weights.items():
            for (bi, w) in lst:
                if bi < len(order):
                    obj.vertex_groups[order[bi]].add([vi], w, "ADD")
        mod = obj.modifiers.new("Armature", "ARMATURE")
        mod.object = arm
        obj.parent = arm
    return obj


def _round_key(vals, nd=5):
    return tuple(round(v, nd) for v in vals)


def export_mesh_xml(obj, arm, path, skeleton_name, smooth=True):
    """Export one mesh object (rest pose, modifiers applied except armature) as Ogre XML. Material
    slots become submeshes. Vertices are split per (position, normal, uv)."""
    order = list(arm.get("ogre_bone_order", [])) if arm else []
    if arm is not None:
        for b in arm.data.bones:
            if b.name not in order:
                order.append(b.name)
    saved_pose = arm.data.pose_position if arm else None
    saved_mods = []
    if arm is not None:
        arm.data.pose_position = "REST"
    for m in obj.modifiers:
        if m.type == "ARMATURE":
            saved_mods.append((m, m.show_viewport))
            m.show_viewport = False
    dg = bpy.context.evaluated_depsgraph_get()
    dg.update()
    ev = obj.evaluated_get(dg)
    me = bpy.data.meshes.new_from_object(ev, depsgraph=dg)
    for (m, s) in saved_mods:
        m.show_viewport = s
    if arm is not None:
        arm.data.pose_position = saved_pose
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.to_mesh(me)
    bm.free()
    me.calc_loop_triangles()
    uv = me.uv_layers.active
    corner_normals = me.corner_normals
    groups = {g.index: g.name for g in obj.vertex_groups}
    src_obj_mesh = obj.data
    # weights come from the evaluated mesh vertices: use obj.data vertex weights when the vertex
    # count is unchanged, otherwise from the evaluated copy's deform layer.
    dvert_layer = None
    bm2 = None
    vkey = {}
    out_pos, out_nor, out_uv, out_w = [], [], [], []
    sub_faces = {}
    deform = me.vertices
    for tri in me.loop_triangles:
        face = []
        for li in tri.loops:
            vi = me.loops[li].vertex_index
            p = me.vertices[vi].co
            n = corner_normals[li].vector
            u = tuple(uv.data[li].uv) if uv else (0.0, 0.0)
            if smooth:
                n = me.vertices[vi].normal
            key = (vi, _round_key(n, 4), _round_key(u, 5))
            idx = vkey.get(key)
            if idx is None:
                idx = len(out_pos)
                vkey[key] = idx
                out_pos.append(p.copy())
                out_nor.append(n.copy())
                out_uv.append(u)
                ws = []
                for g in me.vertices[vi].groups:
                    gname = groups.get(g.group)
                    if gname in order and g.weight > 1e-4:
                        ws.append((order.index(gname), g.weight))
                ws.sort(key=lambda x: -x[1])
                ws = ws[:4]
                tot = sum(w for _, w in ws)
                out_w.append([(b, w / tot) for b, w in ws] if tot > 0 else [])
            face.append(idx)
        sub_faces.setdefault(tri.material_index, []).append(face)
    lines = ['<?xml version="1.0"?>\n<mesh>\n']
    lines.append('\t<sharedgeometry vertexcount="%d">\n\t\t<vertexbuffer positions="true" normals="true">\n' % len(out_pos))
    for p, n in zip(out_pos, out_nor):
        lines.append('\t\t\t<vertex>\n\t\t\t\t<position x="%s" y="%s" z="%s" />\n\t\t\t\t<normal x="%s" y="%s" z="%s" />\n\t\t\t</vertex>\n' % (
            _fmt(p.x), _fmt(p.y), _fmt(p.z), _fmt(n.x), _fmt(n.y), _fmt(n.z)))
    lines.append('\t\t</vertexbuffer>\n\t\t<vertexbuffer texture_coord_dimensions_0="float2" texture_coords="1">\n')
    for u in out_uv:
        lines.append('\t\t\t<vertex>\n\t\t\t\t<texcoord u="%s" v="%s" />\n\t\t\t</vertex>\n' % (_fmt(u[0]), _fmt(1.0 - u[1])))
    lines.append("\t\t</vertexbuffer>\n\t</sharedgeometry>\n\t<submeshes>\n")
    for mi in sorted(sub_faces):
        mname = obj.material_slots[mi].material.name if mi < len(obj.material_slots) and obj.material_slots[mi].material else "BaseWhite"
        lines.append('\t\t<submesh material="%s" usesharedvertices="true" use32bitindexes="false" operationtype="triangle_list">\n\t\t\t<faces count="%d">\n' % (mname, len(sub_faces[mi])))
        for f in sub_faces[mi]:
            lines.append('\t\t\t\t<face v1="%d" v2="%d" v3="%d" />\n' % tuple(f))
        lines.append("\t\t\t</faces>\n\t\t\t<boneassignments />\n\t\t</submesh>\n")
    lines.append("\t</submeshes>\n")
    if arm is not None:
        lines.append('\t<skeletonlink name="%s" />\n\t<boneassignments>\n' % skeleton_name)
        for vi, ws in enumerate(out_w):
            for b, w in ws:
                lines.append('\t\t<vertexboneassignment vertexindex="%d" boneindex="%d" weight="%s" />\n' % (vi, b, _fmt(w)))
        lines.append("\t</boneassignments>\n")
    lines.append("</mesh>\n")
    with open(path, "w", newline="\n") as f:
        f.write("".join(lines))
    bpy.data.meshes.remove(me)
    return {"vertices": len(out_pos), "triangles": sum(len(v) for v in sub_faces.values()),
            "unweighted": sum(1 for w in out_w if not w)}
