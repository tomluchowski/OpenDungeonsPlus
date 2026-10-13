"""Open the existing coop doorway and map its ridge to the existing roof atlas.

Run in background Blender with the canonical coop scene already open:
    blender --background <blend> --python coop_depth.py -- <current mesh XML> <output XML>
The shipped XML is changed surgically; the current Blender scene is updated independently.
Its skeleton, door clip and all geometry outside the doorway/ridge/interior remain intact.
"""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET


def doorway(points):
    return all(0.56 < x < 0.61 and abs(y) < 0.14 and 0.20 < z < 0.52 for x, y, z in points)


def ridge(point):
    x, y, z = point
    return 0.12 < x < 0.48 and abs(y) < 0.071 and 0.954 < z < 0.976


def interior(points):
    return all(0.035 < x < 0.56 and abs(y) < 0.30 and 0.19 < z < 0.56 for x, y, z in points)


def roof_uv(point):
    x, y, z = point
    return 0.08 + 0.33 * (x - 0.13) / 0.34, 0.08 + 0.30 * (y + 0.07) / 0.14


def patch_xml(source, target):
    tree = ET.parse(source)
    root = tree.getroot()
    buffers = root.findall('sharedgeometry/vertexbuffer')
    positions = buffers[0].findall('vertex/position')
    points = [tuple(float(p.get(c)) for c in ('x', 'y', 'z')) for p in positions]
    uvs = buffers[1].findall('vertex/texcoord')
    weights = {int(w.get('vertexindex')): int(w.get('boneindex'))
               for w in root.findall('boneassignments/vertexboneassignment')}
    submeshes = root.find('submeshes')
    lining = ET.SubElement(submeshes, 'submesh', material='ChickenCoopInterior', usesharedvertices='true',
                           use32bitindexes='false', operationtype='triangle_list')
    lining_faces = ET.SubElement(lining, 'faces', count='0')
    removed = 0
    for submesh in list(submeshes):
        if submesh.get('material') != 'ChickenCoop':
            continue
        faces = submesh.find('faces')
        for face in list(faces):
            indices = [int(face.get(k)) for k in ('v1', 'v2', 'v3')]
            ps = [points[i] for i in indices]
            if not all(weights[i] == 0 for i in indices):
                continue  # animated door is never cut or remapped
            if doorway(ps):
                faces.remove(face)
                removed += 1
            elif interior(ps):
                faces.remove(face)
                lining_faces.append(face)
        faces.set('count', str(len(faces)))
    lining_faces.set('count', str(len(lining_faces)))
    ET.SubElement(lining, 'boneassignments')
    remapped = 0
    for i, point in enumerate(points):
        if weights[i] != 1 and ridge(point):
            u, v = roof_uv(point)
            uvs[i].set('u', format(u, '.8g'))
            uvs[i].set('v', format(v, '.8g'))
            remapped += 1
    tree.write(target, encoding='utf-8', xml_declaration=True)
    return removed, len(lining_faces), remapped


def patch_blend():
    import bpy
    import bmesh
    obj = bpy.data.objects['CoopMesh']
    me = obj.data
    root_group = obj.vertex_groups['Root'].index
    material_names = [m.name for m in me.materials]
    wood = material_names.index('ChickenCoop')
    lining = bpy.data.materials.get('ChickenCoopInterior') or bpy.data.materials.new('ChickenCoopInterior')
    lining.diffuse_color = (0.22, 0.18, 0.14, 1.0)
    if lining.name not in material_names:
        me.materials.append(lining)
        material_names.append(lining.name)
    lining_slot = material_names.index(lining.name)
    bm = bmesh.new()
    bm.from_mesh(me)
    deform = bm.verts.layers.deform.active
    uv = bm.loops.layers.uv.active
    plugs = []
    for face in bm.faces:
        if face.material_index != wood:
            continue
        ps = [tuple(v.co) for v in face.verts]
        rooted = all(v[deform].get(root_group, 0.0) > 0.99 for v in face.verts)
        if rooted and doorway(ps):
            plugs.append(face)
        elif rooted and interior(ps):
            face.material_index = lining_slot
        if all(ridge(p) for p in ps):
            for loop in face.loops:
                u, v = roof_uv(tuple(loop.vert.co))
                loop[uv].uv = (u, 1.0 - v)
    bmesh.ops.delete(bm, geom=plugs, context='FACES_ONLY')
    bm.to_mesh(me)
    bm.free()
    me.update()
    # Keep pre-existing backup files intact; this updates only the canonical model file.
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_mainfile()
    return len(plugs)


def main():
    args = sys.argv[sys.argv.index('--') + 1:]
    result = patch_xml(args[0], args[1])
    assert result[0] == 136 and result[1] > 0 and result[2] > 0, result
    removed = patch_blend()
    Path(args[1] + '.result.txt').write_text('XML removed/lining/ridge: %s; canonical removed: %s\n' % (result, removed))


if __name__ == '__main__':
    try:
        main()
    except Exception:
        import traceback
        args = sys.argv[sys.argv.index('--') + 1:]
        Path(args[1] + '.error.txt').write_text(traceback.format_exc())
        raise
