#!/usr/bin/env python3
"""Add folded flight feathers and articulated wing unfolding to exported chicken assets.

Uses the project's existing quaternion authoring helpers; no Blender or game build is required.
Input and output are separate XML files so original authoring exports are preserved.
"""
import argparse
import copy
import math
from pathlib import Path
import re
import xml.etree.ElementTree as ET

import gen_walk_hurt as rig


FEATHER_COUNT = 6
WING_UNFOLD_DEGREES = 72.0
FLIGHT_CLIPS = ('RoofFlight', 'Flutter', 'Mount', 'Tread', 'Dismount')


# Return a smooth unfold envelope that agrees at consecutive mating clip boundaries.
def unfold(clip, time, length):
    progress = time / length
    if clip == 'Tread':
        return 1.0
    if clip == 'Mount':
        return rig.smooth(progress)
    if clip == 'Dismount':
        return 1.0 - rig.smooth(progress)
    return rig.smooth(progress / 0.15) * rig.smooth((1.0 - progress) / 0.15)


# Change only the two distal wing tracks; the existing shoulder strokes and body tracks stay intact.
def author_clips(source, destination):
    skeleton = rig.Skeleton(str(source))
    result = skeleton.text
    for clip in FLIGHT_CLIPS:
        length, tracks = skeleton.clips[clip]
        replacements = {}
        for bone, direction in (('Wing_L', -1.0), ('Wing_R', 1.0)):
            keys = []
            for time, translation, rotation in tracks[bone]:
                angle = math.radians(WING_UNFOLD_DEGREES * direction * unfold(clip, time, length))
                delta = skeleton.frame_rot(bone, rig.qaxis(rig.Z_AXIS, angle))
                keys.append((time, translation, rig.qmul(delta, rotation)))
            track_xml = rig.anim_xml(clip, length, [(bone, keys)])
            replacements[bone] = re.search(r'<track\b.*?</track>', track_xml, re.S).group(0)
        pattern = r'(<animation name="' + re.escape(clip) + r'"[^>]*>)(.*?)(</animation>)'
        match = re.search(pattern, result, re.S)
        assert match is not None, clip
        body = match.group(2)
        for bone, replacement in replacements.items():
            track_pattern = r'<track bone="' + re.escape(bone) + r'"[^>]*>.*?</track>'
            old = re.search(track_pattern, body, re.S)
            assert old is not None, (clip, bone)
            body = body[:old.start()] + replacement + body[old.end():]
        result = result[:match.start(2)] + body + result[match.end(2):]
    assert not destination.exists(), destination
    destination.write_text(result, encoding='utf-8')


# Append one closed feather prism with full distal-wing influence and existing feather texture coordinates.
def add_feather(buffers, faces, assignments, index, side, feather, root_uv, tip_uv, bone):
    root_x = side * (0.041 + feather * 0.003)
    root_y = 0.027 + feather * 0.006
    tip_x = side * (0.050 + feather * 0.012)
    tip_y = 0.163 - feather * 0.006
    width = 0.020
    outline = [(root_x - side * width * 0.2, root_y),
               (root_x + side * width * 0.2, root_y),
               (tip_x + side * width * 0.5, tip_y - 0.015),
               (tip_x, tip_y),
               (tip_x - side * width * 0.5, tip_y - 0.015)]
    for layer in range(2):
        height = 0.105 + layer * 0.002 + feather * 0.0004
        for point, (x, y) in enumerate(outline):
            uv = root_uv if point < 2 else tip_uv
            for buffer in buffers:
                vertex = ET.SubElement(buffer, 'vertex')
                if buffer.get('positions') == 'true':
                    ET.SubElement(vertex, 'position', x=str(x), y=str(y), z=str(height))
                if buffer.get('normals') == 'true':
                    ET.SubElement(vertex, 'normal', x='0', y='0', z=str(-1 if layer == 0 else 1))
                if int(buffer.get('texture_coords', '0')) > 0:
                    for channel in uv:
                        ET.SubElement(vertex, 'texcoord', **channel)
            ET.SubElement(assignments, 'vertexboneassignment', vertexindex=str(index + layer * 5 + point),
                          boneindex=str(bone), weight='1')
    triangles = [(0, 2, 1), (0, 3, 2), (0, 4, 3), (5, 6, 7), (5, 7, 8), (5, 8, 9)]
    for edge in range(5):
        following = (edge + 1) % 5
        triangles.extend([(edge, following, following + 5), (edge, following + 5, edge + 5)])
    for triangle in triangles:
        ET.SubElement(faces, 'face', v1=str(index + triangle[0]), v2=str(index + triangle[1]),
                      v3=str(index + triangle[2]))


# Add complete folded feather fans without changing existing geometry, skinning or material assignments.
def author_mesh(source, skeleton_path, destination):
    skeleton = ET.parse(skeleton_path).getroot()
    bones = {bone.get('name'): int(bone.get('id')) for bone in skeleton.find('bones')}
    tree = ET.parse(source)
    mesh = tree.getroot()
    geometry = mesh.find('sharedgeometry')
    buffer = geometry.find('vertexbuffer')
    buffers = geometry.findall('vertexbuffer')
    original_count = len(buffer)
    original_vertices = copy.deepcopy(list(buffer))
    assignments = mesh.find('boneassignments')
    faces = mesh.find('submeshes')[0].find('faces')
    uv_buffer = buffers[1]
    root_uv = [coordinate.attrib for coordinate in uv_buffer[97].findall('texcoord')]
    tip_uv = [coordinate.attrib for coordinate in uv_buffer[94].findall('texcoord')]
    for side, bone in ((1, bones['Wing_L']), (-1, bones['Wing_R'])):
        for feather in range(FEATHER_COUNT):
            add_feather(buffers, faces, assignments, len(buffer), side, feather, root_uv, tip_uv, bone)
    geometry.set('vertexcount', str(len(buffer)))
    faces.set('count', str(len(faces)))
    assert all(ET.tostring(old) == ET.tostring(new)
               for old, new in zip(original_vertices, list(buffer)[:original_count]))
    assert not destination.exists(), destination
    tree.write(destination, encoding='utf-8', xml_declaration=True)


# Author a candidate skeleton and rooster mesh into an explicitly selected output directory.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('skeleton', type=Path)
    parser.add_argument('mesh', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    author_clips(args.skeleton, args.output / 'Chicken.skeleton.xml')
    author_mesh(args.mesh, args.skeleton, args.output / args.mesh.name)
    print('Authored unfolding wing clips and', FEATHER_COUNT * 2, 'fully skinned flight feathers.')


if __name__ == '__main__':
    main()
