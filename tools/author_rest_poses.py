#!/usr/bin/env python3
"""Author separate articulated rest candidates with actual mesh and final sole-clearance measurements.

Candidates still require visual review; a small IK residual alone does not certify the resulting pose.
"""
import argparse
import copy
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np

import fix_walk_feet as feet
from fix_walk_feet_rigs import RIGS
import gen_walk_hurt as rig


SIT_LENGTH = 12.0
LIE_LENGTH = 14.0
POSE_SAMPLES = 97
TRANSITION_SHARE = 0.12
MAX_CONTACT_ERROR_RATIO = 0.01
MAX_BLEND_WEIGHTS = 4
FOOT_ORIENTATION_WEIGHT = 50.0
PRONE_BODIES = ('Dragon', 'PitDemon', 'Lizardman')
BREATH_AMOUNT = 0.008
BREATH_SECONDS = 4.0


# Read shared or per-submesh geometry and its original skin influences for contact measurements.
def mesh_data(path):
    mesh = ET.parse(path).getroot()
    vertices = []
    faces = []
    weights = []
    shared = mesh.find('sharedgeometry')
    if shared is not None:
        append_geometry(shared, mesh.find('boneassignments'), vertices, weights)
    for submesh in mesh.find('submeshes'):
        offset = 0
        if submesh.get('usesharedvertices') != 'true':
            offset = len(vertices)
            append_geometry(submesh.find('geometry'), submesh.find('boneassignments'), vertices, weights)
        for face in submesh.find('faces'):
            faces.append(([offset + int(face.get(key)) for key in ('v1', 'v2', 'v3')], submesh.get('material')))
    return np.array(vertices), faces, weights


# Append one vertex buffer and its local-index bone assignments without changing source geometry.
def append_geometry(geometry, assignments, vertices, weights):
    offset = len(vertices)
    for buffer in geometry.findall('vertexbuffer'):
        if buffer.get('positions') != 'true':
            continue
        for vertex in buffer:
            position = vertex.find('position')
            vertices.append([float(position.get(axis)) for axis in 'xyz'])
            weights.append([])
    if assignments is not None:
        for assignment in assignments:
            weights[offset + int(assignment.get('vertexindex'))].append(
                (int(assignment.get('boneindex')), float(assignment.get('weight'))))
    for index in range(offset, len(vertices)):
        # Match Ogre's strongest-four influence selection and normalization for diagnostics.
        selected = sorted(weights[index], key=influence_weight, reverse=True)[:MAX_BLEND_WEIGHTS]
        total = sum(weight for bone, weight in selected)
        assert all(math.isfinite(weight) and weight >= 0.0 for bone, weight in selected)
        assert not selected or total > 0.0
        weights[index] = [(bone, weight / total) for bone, weight in selected]


# Return an influence's weight for Ogre-compatible strongest-first selection.
def influence_weight(influence):
    return influence[1]


# Measure final visible sole clearance independently of the IK solver's earlier effector residual.
def sole_lift(skeleton, original, posed, weights, height, name):
    config = RIGS.get(name)
    if config is None:
        return None
    worst = 0.0
    for foot in config['feet']:
        bones = {skeleton.idx[bone] for unit in foot['units'] for bone in unit['chain']}
        # Include the actual toes and attached boot armour, which may carry the visible sole under child bones.
        for index in skeleton.topo:
            if skeleton.parent[index] in bones:
                bones.add(index)
        bones.update(skeleton.idx[bone] for vertex in foot.get('sole', []) for bone in vertex['w'])
        indices = [index for index, assignments in enumerate(weights)
                   if sum(weight for bone, weight in assignments if bone in bones) >= 0.5]
        assert indices, (name, 'No visible foot vertices')
        lowest = float(original[0, indices, 2].min())
        sole = [index for index in indices if original[0, index, 2] <= lowest + 0.02 * height]
        clearance = posed[:, sole, 2].min(axis=1) - lowest
        worst = max(worst, float(clearance.max() / height))
    return worst


# Skin all requested poses so ground contact is measured on visible vertices rather than bone pivots.
def skin_vertices(skeleton, local, vertices, weights):
    bind_local = [(skeleton.rest_pos[index][None], skeleton.rest_q[index][None], skeleton.rest_s[index][None])
                  for index in range(len(skeleton.names))]
    bind = skeleton.fk(bind_local)
    pose = skeleton.fk(local)
    count = local[0][0].shape[0]
    result = np.zeros((count, len(vertices), 3))
    influences = [[] for name in skeleton.names]
    for index, assignments in enumerate(weights):
        for bone, weight in assignments or [(skeleton.topo[0], 1.0)]:
            influences[bone].append((index, weight))
    for bone, assignments in enumerate(influences):
        if not assignments:
            continue
        indices = np.array([index for index, weight in assignments])
        amounts = np.array([weight for index, weight in assignments])
        bind_position, bind_rotation, bind_scale = bind[bone]
        position, rotation, scale = pose[bone]
        points = (vertices[indices] - bind_position[0]) @ bind_rotation[0] / bind_scale[0]
        moved = np.einsum('nij,nvj->nvi', rotation, scale[:, None] * points[None]) + position[:, None]
        result[:, indices] += moved * amounts[None, :, None]
    return result


# Use smooth entry and exit with a stable rest interval between them.
def envelope(times, length):
    progress = times / length
    entering = np.clip(progress / TRANSITION_SHARE, 0.0, 1.0)
    leaving = np.clip((1.0 - progress) / TRANSITION_SHARE, 0.0, 1.0)
    entering = entering * entering * (3.0 - 2.0 * entering)
    leaving = leaving * leaving * (3.0 - 2.0 * leaving)
    return entering * leaving


# Apply a world-axis joint rotation in the bone's actual animated parent frame.
def rotate_joint(skeleton, local, bone, axis, angles):
    index = skeleton.idx[bone]
    parent = skeleton.parent[index]
    vectors = np.asarray(axis)
    if vectors.ndim == 1:
        vectors = np.tile(vectors, (len(angles), 1))
    assert vectors.shape == (len(angles), 3)
    if parent >= 0:
        world = skeleton.fk(local)
        vectors = np.einsum('nji,nj->ni', world[parent][1], vectors)
    position, rotation, scale = local[index]
    local[index] = (position, feet.qmul(feet.rv2q(vectors * angles[:, None]), rotation), scale)


# Lower an arm segment along the standing body's side before rotating the whole body into lying.
def lower_arm(skeleton, local, chain, amount):
    for bone, child in zip(chain, chain[1:]):
        world = skeleton.fk(local)
        direction = world[skeleton.idx[child]][0] - world[skeleton.idx[bone]][0]
        direction /= np.linalg.norm(direction, axis=1)[:, None]
        target = np.tile(np.array([0.0, 0.0, -1.0]), (len(amount), 1))
        axis = np.cross(direction, target)
        norm = np.linalg.norm(axis, axis=1)
        angle = np.arctan2(norm, np.einsum('ni,ni->n', direction, target))
        for frame in range(len(amount)):
            if norm[frame] < 1e-8:
                axis[frame] = rig.X_AXIS
            else:
                axis[frame] /= norm[frame]
        rotate_joint(skeleton, local, bone, axis, angle * amount)


# Place resting hands on the nearest thigh after the legs have reached their sitting pose.
def rest_hands(skeleton, local, base, author_rig, legs, height, amount, vertices, weights):
    errors = []
    legs = [leg for leg in legs if len(leg.ids) >= 2]
    assert legs, 'Hand support needs the actual thigh chains, not independent foot controls'
    for item in author_rig:
        arms = list(item.get('hang', []))
        if item.get('press'):
            arms.append(item['press'])
        for chain in arms:
            assert len(chain) >= 2, chain
            world = skeleton.fk(local)
            shoulder = world[skeleton.idx[chain[0]]][0]
            nearest = int(np.argmin([abs(world[leg.ids[0]][0][len(amount) // 2, 0] -
                                        shoulder[len(amount) // 2, 0]) for leg in legs]))
            leg = legs[nearest]
            hip = world[leg.ids[0]][0]
            knee = world[leg.ids[-1] if len(leg.ids) == 2 else leg.ids[-2]][0]
            current = world[skeleton.idx[chain[-1]]][0]
            lengths = np.array([np.linalg.norm(world[skeleton.idx[child]][0] - world[skeleton.idx[bone]][0], axis=1)
                                for bone, child in zip(chain, chain[1:])])
            maximum = lengths.sum(axis=0)
            minimum = np.maximum(0.0, 2.0 * lengths.max(axis=0) - maximum)
            target = None
            chosen = np.zeros(len(amount), dtype=bool)
            candidates = []
            for share in (0.4, 0.45, 0.5, 0.55, 0.6, 0.65, 0.7, 0.75, 0.8, 0.35, 0.3, 0.25, 0.2):
                candidate = hip * (1.0 - share) + knee * share
                candidate[:, 1] -= 0.02 * height
                candidate[:, 2] += 0.04 * height
                candidate = current + (candidate - current) * amount[:, None]
                radius = np.linalg.norm(candidate - shoulder, axis=1)
                valid = (radius >= minimum - 1e-8) & (radius <= maximum + 1e-8)
                candidates.append((candidate, valid))
            # Keep one reachable thigh point throughout breathing instead of switching support at a reach limit.
            stable = next((candidate for candidate, valid in candidates if np.all(valid)), None)
            if stable is not None:
                candidates = [(stable, np.ones(len(amount), dtype=bool))]
            for candidate, valid in candidates:
                valid = valid & ~chosen
                if target is None:
                    target = candidate.copy()
                target[valid] = candidate[valid]
                chosen |= valid
            if not np.all(chosen):
                # Short arms may rest against the actual abdomen instead of reaching an impossible thigh point.
                torso = {skeleton.idx[bone] for bone in item.get('body', []) + item.get('spine', [])}
                indices = [index for index, row in enumerate(weights)
                           if sum(weight for bone, weight in row if bone in torso) >= 0.5]
                assert indices, (chain, 'No visible torso support')
                points = skin_vertices(skeleton, local, vertices[indices], [weights[index] for index in indices])
                for frame in np.flatnonzero(~chosen):
                    candidates = points[frame].copy()
                    candidates[:, 1] -= 0.015 * height
                    possible = current[frame] + (candidates - current[frame]) * amount[frame]
                    radius = np.linalg.norm(possible - shoulder[frame], axis=1)
                    valid = (radius >= minimum[frame] - 1e-8) & (radius <= maximum[frame] + 1e-8)
                    valid &= candidates[:, 1] <= hip[frame, 1]
                    valid &= candidates[:, 2] <= shoulder[frame, 2]
                    assert np.any(valid), (chain, 'No reachable body support', int(frame))
                    available = np.flatnonzero(valid)
                    closest = available[np.argmin(np.linalg.norm(possible[valid] - target[frame], axis=1))]
                    target[frame] = possible[closest]
            unit = feet.Unit(skeleton, {'chain': chain, 'eff': [0.0, 0.0, 0.0]})
            initial = [local[index] for index in unit.ids]
            original_rotation = skeleton.fk(base)[unit.ids[-1]][1]
            parent = world[unit.par] if unit.par >= 0 else None
            delta, error, solved = feet.solve_unit(unit, initial, parent, target, original_rotation,
                                                  height, wo=0.5, wr=2.0, iters=60)
            for index, result in zip(unit.ids, solved):
                local[index] = result
            errors.append(float(error.max() / height))
    return max(errors, default=0.0)


# Keep the visible hand and any embedded held object horizontal instead of pointing their tips through the floor.
def level_hand(skeleton, local, chain, vertices, weights, amount):
    bone = skeleton.idx[chain[-1]]
    descendants = {bone}
    for index in skeleton.topo:
        if skeleton.parent[index] in descendants:
            descendants.add(index)
    selected = [index for index, row in enumerate(weights)
                if sum(weight for joint, weight in row if joint in descendants) >= 0.9]
    if len(selected) < 3:
        return
    world = skeleton.fk(local)
    points = skin_vertices(skeleton, local, vertices[selected], [weights[index] for index in selected])
    centre = points.mean(axis=1)
    centred = points - centre[:, None, :]
    covariance = np.einsum('nvi,nvj->nij', centred, centred)
    values, vectors = np.linalg.eigh(covariance)
    direction = vectors[:, :, -1]
    direction[np.einsum('ni,ni->n', direction, centre - world[bone][0]) < 0.0] *= -1.0
    target = np.tile(np.array([0.0, -1.0, 0.0]), (len(amount), 1))
    axis = np.cross(direction, target)
    norm = np.linalg.norm(axis, axis=1)
    angle = np.arctan2(norm, np.einsum('ni,ni->n', direction, target))
    axis[norm < 1e-8] = rig.X_AXIS
    axis[norm >= 1e-8] /= norm[norm >= 1e-8, None]
    rotate_joint(skeleton, local, chain[-1], axis, angle * amount)


# Preserve the actual weighted shin orientation where the original mesh has no ankle influences.
def level_shin_feet(skeleton, local, base):
    original = [None] * len(skeleton.names)
    current = [None] * len(skeleton.names)
    for index in skeleton.topo:
        parent = skeleton.parent[index]
        original[index] = base[index][1] if parent < 0 else feet.qmul(original[parent], base[index][1])
        current[index] = local[index][1] if parent < 0 else feet.qmul(current[parent], local[index][1])
        if skeleton.names[index] in ('LeftLeg', 'RightLeg'):
            position, rotation, scale = local[index]
            inverse = current[parent].copy()
            inverse[:, 1:] *= -1.0
            rotation = feet.qmul(inverse, original[index])
            local[index] = (position, rotation, scale)
            current[index] = original[index]


# Raise a separate cloth or armour joint only enough to prevent its weighted surface from entering the floor.
def raise_joint_surface(skeleton, local, vertices, weights, bone, floor):
    parent = skeleton.parent[bone]
    assert parent >= 0
    descendants = {bone}
    for index in skeleton.topo:
        if skeleton.parent[index] in descendants:
            descendants.add(index)
    influence = np.array([sum(weight for index, weight in row if index in descendants) for row in weights])
    selected = influence > 1e-8
    if not np.any(selected):
        return
    points = skin_vertices(skeleton, local, vertices, weights)
    lift = np.maximum(0.0, (floor - points[:, selected, 2]) / influence[selected]).max(axis=1)
    world = skeleton.fk(local)
    offset = np.zeros((len(lift), 3))
    offset[:, 2] = lift
    offset = np.einsum('nji,nj->ni', world[parent][1], offset) / world[parent][2]
    position, rotation, scale = local[bone]
    local[bone] = (position + offset, rotation, scale)


# Gather a hanging cape onto the floor through its lower cloth joints while keeping its shoulder attachment.
def drape_cape(skeleton, local, base, vertices, weights):
    floor = skin_vertices(skeleton, base, vertices, weights)[0, :, 2].min()
    capes = [index for index in skeleton.topo if skeleton.names[index].lower().startswith('cape')]
    for bone in capes:
        if skeleton.parent[bone] in capes:
            raise_joint_surface(skeleton, local, vertices, weights, bone, floor)


# Lay an embedded long tool across the lap using its actual weighted geometry and current hand frame.
def fold_tool(skeleton, local, vertices, weights, amount):
    if 'Pick' not in skeleton.idx:
        return
    index = skeleton.idx['Pick']
    selected = [vertex for vertex, assignments in enumerate(weights)
                if sum(weight for bone, weight in assignments if bone == index) >= 0.9]
    assert len(selected) >= 3
    points = vertices[selected]
    values, axes = np.linalg.eigh(np.cov(points.T))
    secondary = axes[:, -2]
    direction = axes[:, -1]
    if direction[0] < 0.0:
        direction = -direction
    bind = skeleton.fk([(skeleton.rest_pos[bone][None], skeleton.rest_q[bone][None], skeleton.rest_s[bone][None])
                        for bone in range(len(skeleton.names))])
    direction = bind[index][1][0].T @ direction / bind[index][2][0]
    secondary = bind[index][1][0].T @ secondary / bind[index][2][0]
    world = skeleton.fk(local)
    parent = skeleton.parent[index]
    assert parent >= 0
    anchor = world[parent][0].copy()
    position, rotation, scale = world[index]
    grip = np.einsum('nji,nj->ni', rotation, anchor - position) / scale
    current = np.einsum('nij,nj->ni', rotation, scale * direction)
    current /= np.linalg.norm(current, axis=1)[:, None]
    target = np.tile(np.array([1.0, 0.0, 0.0]), (len(amount), 1))
    axis = np.cross(current, target)
    length = np.linalg.norm(axis, axis=1)
    axis[length < 1e-8] = np.array([0.0, 1.0, 0.0])
    length[length < 1e-8] = 1.0
    axis /= length[:, None]
    turning = np.clip(amount / 0.15, 0.0, 1.0)
    turning = turning * turning * (3.0 - 2.0 * turning)
    angle = np.arccos(np.clip(current[:, 0], -1.0, 1.0)) * turning
    rotate_joint(skeleton, local, 'Pick', axis, angle)
    position, rotation, scale = skeleton.fk(local)[index]
    flat = np.einsum('nij,nj->ni', rotation, scale * secondary)
    roll = -np.arctan2(flat[:, 2], flat[:, 1])
    roll = (roll + math.pi * 0.5) % math.pi - math.pi * 0.5
    rotate_joint(skeleton, local, 'Pick', rig.X_AXIS, roll * turning)
    world = skeleton.fk(local)
    position, rotation, scale = world[index]
    desired = anchor - np.einsum('nij,nj->ni', rotation, scale * grip)
    offset = np.einsum('nji,nj->ni', world[parent][1], desired - position) / world[parent][2]
    position, rotation, scale = local[index]
    local[index] = (position + offset, rotation, scale)


# Fold the hovering insect's four wings back along its body using their actual weighted surface centres.
def fold_hover_wings(skeleton, local, base, vertices, weights, amount):
    world = skeleton.fk(base)
    points = skin_vertices(skeleton, base, vertices, weights)[0]
    for name in ('Wing1_L', 'Wing2_L', 'Wing1_R', 'Wing2_R'):
        bone = skeleton.idx[name]
        selected = [index for index, assignments in enumerate(weights)
                    if sum(weight for joint, weight in assignments if joint == bone) >= 0.5]
        assert selected, name
        direction = points[selected].mean(axis=0) - world[bone][0][0]
        angle = math.pi * 0.5 - math.atan2(direction[1], direction[0])
        angle = (angle + math.pi) % (2.0 * math.pi) - math.pi
        rotate_joint(skeleton, local, name, rig.Z_AXIS, angle * 0.85 * amount)


# Translate each root in world space while keeping the actor's game position untouched.
def move_roots(skeleton, local, offset):
    for index in skeleton.topo:
        if skeleton.parent[index] < 0:
            position, rotation, scale = local[index]
            local[index] = (position + offset, rotation, scale)


# Rotate every root around the same origin so separately rooted armour remains attached to the body.
def tilt_body(skeleton, local, angles):
    vectors = np.zeros((len(angles), 3))
    vectors[:, 0] = angles
    quaternion = feet.rv2q(vectors)
    matrix = feet.q2m(quaternion)
    for index in skeleton.topo:
        if skeleton.parent[index] < 0:
            position, rotation, scale = local[index]
            local[index] = (np.einsum('nij,nj->ni', matrix, position), feet.qmul(quaternion, rotation), scale)


# Bend a joint's actual child segment toward a measured direction without changing its length.
def aim_joint(skeleton, local, bone, child, target, amount):
    world = skeleton.fk(local)
    direction = world[skeleton.idx[child]][0] - world[skeleton.idx[bone]][0]
    direction /= np.linalg.norm(direction, axis=1)[:, None]
    target = target / np.linalg.norm(target, axis=1)[:, None]
    axis = np.cross(direction, target)
    norm = np.linalg.norm(axis, axis=1)
    angle = np.arctan2(norm, np.einsum('ni,ni->n', direction, target))
    axis[norm < 1e-8] = rig.X_AXIS
    axis[norm >= 1e-8] /= norm[norm >= 1e-8, None]
    rotate_joint(skeleton, local, bone, axis, angle * amount)


# Fold both membranous wings through their shoulder, elbow and finger joints before resting.
def fold_beast_wings(skeleton, local, vertices, weights, names, amount):
    for name in names:
        bone = skeleton.idx[name]
        descendants = {bone}
        for index in skeleton.topo:
            if skeleton.parent[index] in descendants:
                descendants.add(index)
        selected = [index for index, row in enumerate(weights)
                    if sum(weight for joint, weight in row if joint in descendants) >= 0.5]
        assert selected, name
        points = skin_vertices(skeleton, local, vertices[selected], [weights[index] for index in selected])
        world = skeleton.fk(local)
        direction = points.mean(axis=1) - world[bone][0]
        yaw = math.pi * 0.5 - np.arctan2(direction[:, 1], direction[:, 0])
        yaw = (yaw + math.pi) % (2.0 * math.pi) - math.pi
        rotate_joint(skeleton, local, name, rig.Z_AXIS, yaw * amount)
        rotate_joint(skeleton, local, name, rig.X_AXIS, np.radians(-15.0 * amount))
        elbow = name.replace('Wing_1', 'Wing_2')
        hand = name.replace('Wing_1', 'Wing_3')
        tip = name.replace('Wing_1', 'Wing_4')
        world = skeleton.fk(local)
        upper = world[skeleton.idx[elbow]][0] - world[bone][0]
        aim_joint(skeleton, local, elbow, hand, -upper, 0.8 * amount)
        if tip in skeleton.idx:
            aim_joint(skeleton, local, hand, tip, upper, 0.6 * amount)


# Relax limbless bodies through their own spine joints without borrowing a death pose.
def limbless_rest(skeleton, base, amount, name, lying):
    local = copy.deepcopy(base)
    if name == 'Slime':
        amount = amount * (1.8 if lying else 1.0)
        world = skeleton.fk(base)
        for bone, compression in (('slime_base', 0.15), ('slime_mid', 0.2), ('slime_head', 0.12)):
            index = skeleton.idx[bone]
            position, rotation, scale = local[index]
            vertical = int(np.argmax(np.abs(world[index][1][0, 2])))
            relaxed = np.ones_like(scale)
            relaxed += compression * 0.25 * amount[:, None]
            relaxed[:, vertical] = 1.0 - compression * amount
            local[index] = (position, rotation, scale * relaxed)
        rotate_joint(skeleton, local, 'slime_head', rig.X_AXIS, np.radians(10.0 * amount))
    elif name == 'LavaSpawn':
        for bone, degrees in (('spine', 50.0 if lying else 20.0), ('chest', 15.0 if lying else -10.0), ('neck', -25.0 if lying else 12.0),
                              ('arm1.L', -25.0), ('arm1.R', -25.0),
                              ('arm2.L', 45.0), ('arm2.R', 45.0)):
            rotate_joint(skeleton, local, bone, rig.X_AXIS, np.radians(degrees * amount))
        for bone, degrees in (('tail1', 25.0), ('tail2', 35.0)):
            rotate_joint(skeleton, local, bone, rig.Z_AXIS, np.radians(degrees * amount))
    elif name == 'Wyvern':
        for bone, degrees in (('Spine_1', 15.0), ('Spine_2', -10.0), ('Head', 15.0)):
            rotate_joint(skeleton, local, bone, rig.X_AXIS, np.radians(degrees * amount))
        for suffix, sign in (('.L', 1.0), ('.R', -1.0)):
            for bone, degrees in (('Arm', 25.0), ('Forearm', -45.0), ('Hand', 35.0)):
                rotate_joint(skeleton, local, bone + suffix, rig.Z_AXIS, np.radians(sign * degrees * amount))
        tilt_body(skeleton, local, np.radians((55.0 if lying else 25.0) * amount))
    else:
        raise AssertionError(('Missing anatomical rest recipe', name))
    return local


# Build leg chains for the two small insect rigs from their real visible foot vertices.
def insect_config(skeleton, base, vertices, weights, name):
    chains = []
    if name == 'CaveHornet':
        for side in ('L', 'R'):
            for number in range(1, 4):
                chains.append(['Leg' + str(number) + joint + '_' + side for joint in ('up', 'mid', 'low')])
    elif name == 'SmallSpider':
        for side in ('l', 'r'):
            for number in range(1, 5):
                tip = 'leg4-2_' + side + '.001' if number == 4 else 'leg' + str(number) + '-3_' + side
                chains.append(['leg' + str(number) + '-1_' + side, 'leg' + str(number) + '-2_' + side, tip])
    else:
        return None
    world = skeleton.fk(base)
    points = skin_vertices(skeleton, base, vertices, weights)[0]
    result = {'feet': []}
    for chain in chains:
        bone = skeleton.idx[chain[-1]]
        indices = [index for index, assignments in enumerate(weights)
                   if sum(weight for joint, weight in assignments if joint == bone) >= 0.5]
        assert indices, (name, chain)
        lowest = min(points[index, 2] for index in indices)
        sole = points[[index for index in indices if points[index, 2] <= lowest + 1e-5]].mean(axis=0)
        position, rotation, scale = world[bone]
        effector = (rotation[0].T @ (sole - position[0]) / scale[0]).tolist()
        result['feet'].append({'units': [{'chain': chain, 'eff': effector}]})
    return result


# Add the existing sleep breathing cue to a real torso joint before solving foot and hand contact.
def breathe(skeleton, local, author_rig, motion, name):
    special = {'Adventurer': 'C3', 'CaveHornet': 'Body2', 'Spider': 'Body2', 'Kreatur': 'body2',
               'Tentacle': 'Body2', 'Slime': 'slime_mid', 'LavaSpawn': 'chest', 'Wyvern': 'Spine_1',
               'Kobold': 'TorsoUpper', 'Rat': 'Backbone', 'Scarab': 'Bone.001'}
    if name in special:
        bone = special[name]
    else:
        candidates = [bone for item in author_rig for bone in item.get('spine', [])]
        chest = [bone for bone in candidates if any(part in bone.lower() for part in ('chest', 'breast'))]
        spine = [bone for bone in candidates if 'spine' in bone.lower()]
        assert chest or spine, (name, 'No breathing torso joint')
        bone = (chest or spine)[0]
    index = skeleton.idx[bone]
    position, rotation, scale = local[index]
    local[index] = (position, rotation, scale * (1.0 + motion[:, None]))


# Bend grounded legs through the project's IK and fold humanoid arms toward the knees.
def sitting(skeleton, author_rig, base, amount, height, name, vertices, weights, lying=False, breath=None):
    config = RIGS.get(name) or insect_config(skeleton, base, vertices, weights, name)
    if config is None:
        local = limbless_rest(skeleton, base, amount, name, lying)
        if breath is not None:
            breathe(skeleton, local, author_rig, breath, name)
        return local, None
    humanoid = isinstance(author_rig, list) and all(item['kind'] == 'biped' for item in author_rig)
    if humanoid or name == 'Rat':
        # Keep the complete foot level; solving an extra toe joint can hide an inverted ankle behind a fixed toe.
        config = copy.deepcopy(config)
        original_world = skeleton.fk(base)
        for foot in config['feet']:
            for spec in foot['units']:
                if len(spec['chain']) > 3 and ('toe' in spec['chain'][-1].lower() or name == 'Rat' or
                                               'ankle' in spec['chain'][-2].lower()):
                    toe = skeleton.idx[spec['chain'][-1]]
                    position, rotation, scale = original_world[toe]
                    point = position[0] + rotation[0] @ (scale[0] * np.array(spec['eff']))
                    spec['chain'].pop()
                    position, rotation, scale = original_world[skeleton.idx[spec['chain'][-1]]]
                    spec['eff'] = (rotation[0].T @ (point - position[0]) / scale[0]).tolist()
    if name in ('DarkElf', 'Elf', 'Defender', 'Goblin'):
        # These walk rigs stop at the shin; resting needs the ankle to keep the sole level while the knee bends.
        config = copy.deepcopy(config)
        original_world = skeleton.fk(base)
        for foot in config['feet']:
            for spec in foot['units']:
                end = spec['chain'][-1]
                ankle = end.replace('Leg', 'Foot')
                if end.endswith('Leg') and ankle in skeleton.idx:
                    shin = skeleton.idx[end]
                    position, rotation, scale = original_world[shin]
                    point = position[0] + rotation[0] @ (scale[0] * np.array(spec['eff']))
                    position, rotation, scale = original_world[skeleton.idx[ankle]]
                    spec['chain'].append(ankle)
                    spec['eff'] = (rotation[0].T @ (point - position[0]) / scale[0]).tolist()
    if name == 'NatureMonster':
        # The feet are separate weighted branches; bending the thighs must reach their real ankle positions too.
        original_world = skeleton.fk(base)
        for side in ('L', 'R'):
            chain = ['Thigh_' + side, 'Leg_' + side]
            ankle = original_world[skeleton.idx['Foot_' + side]][0][0]
            position, rotation, scale = original_world[skeleton.idx[chain[-1]]]
            effector = (rotation[0].T @ (ankle - position[0]) / scale[0]).tolist()
            config['feet'].append({'units': [{'chain': chain, 'eff': effector, 'hold': False}]})
    if name == 'Troll':
        for foot in config['feet']:
            for spec in foot['units']:
                if len(spec['chain']) > 1:
                    spec['hold'] = False
    local = copy.deepcopy(base)
    original = skeleton.fk(base)
    units = [feet.Unit(skeleton, spec) for foot in config['feet'] for spec in foot['units']]
    ground = min(float((original[unit.ids[-1]][0] + np.einsum('nij,nj->ni', original[unit.ids[-1]][1],
                         original[unit.ids[-1]][2] * unit.eff_off))[0, 2]) for unit in units)
    hip_units = [unit for unit in units if len(unit.ids) >= 2] or units
    hip = min(float(original[unit.ids[0]][0][0, 2]) for unit in hip_units)
    clearance = max(0.0, hip - ground)
    if name == 'Spider':
        # This rig places its leg pivots below its visible feet; measure the actual torso instead.
        points = skin_vertices(skeleton, base, vertices, weights)[0]
        body = {skeleton.idx[bone] for bone in ('Body', 'Body2', 'Body.002')}
        indices = [index for index, assignments in enumerate(weights)
                   if sum(weight for bone, weight in assignments if bone in body) >= 0.5]
        assert indices, 'No visible spider torso'
        clearance = max(0.0, float(points[indices, 2].min() - points[:, 2].min()))
        assert clearance > 0.0, 'Spider resting must visibly lower the torso'
    drop = clearance * (0.75 if lying and name in PRONE_BODIES else
                        (0.45 if humanoid else (0.75 if lying else 0.55)))
    offset = np.zeros((len(amount), 3))
    offset[:, 2] = -drop * amount
    move_roots(skeleton, local, offset)
    if humanoid:
        # Seed the anatomical knee bend before solving, preventing the symmetric backward-knee solution.
        for unit in units:
            if len(unit.ids) < 2:
                continue
            for index, degrees in zip(unit.ids[:3], (-80.0, 110.0, -30.0)):
                rotate_joint(skeleton, local, skeleton.names[index], rig.X_AXIS, np.radians(degrees * amount))
    if isinstance(author_rig, list):
        for item in author_rig:
            if item.get('head') in skeleton.idx:
                degrees = 8.0 if humanoid and name not in PRONE_BODIES else -8.0
                rotate_joint(skeleton, local, item['head'], rig.X_AXIS, np.radians(degrees * amount))
            for chain in item.get('tail', []):
                rotate_joint(skeleton, local, chain[0], rig.X_AXIS, np.radians(25.0 * amount))
            if name == 'Tentacle':
                for bone, degrees in (('Body1', 30.0 if lying else 10.0),
                                      ('Body2', 35.0 if lying else 15.0), ('Body3', -20.0 if lying else -5.0)):
                    rotate_joint(skeleton, local, bone, rig.X_AXIS, np.radians(degrees * amount))
            if lying and name in PRONE_BODIES:
                spine = ('Spine_0', 'Spine_1', 'Spine_2', 'Spine_3', 'Spine_4') if name == 'Lizardman' else ('Spine_1', 'Spine_2', 'Spine_3')
                for bone in spine:
                    rotate_joint(skeleton, local, bone, rig.X_AXIS, np.radians(60.0 * amount / len(spine)))
                rotate_joint(skeleton, local, 'Head', rig.X_AXIS, np.radians(-60.0 * amount))
            if name in ('Dragon', 'PitDemon') and item.get('wings'):
                fold_beast_wings(skeleton, local, vertices, weights, item['wings'], amount)
            if not humanoid:
                continue
            arms = list(item.get('hang', []))
            if item.get('press'):
                arms.append(item['press'])
            for chain in arms:
                if lying and name not in PRONE_BODIES:
                    lower_arm(skeleton, local, chain, amount * 10.0)
                    continue
                arm_amount = 0.0 * amount if lying else amount
                rotate_joint(skeleton, local, chain[0], rig.X_AXIS, np.radians(-20.0 * arm_amount))
                if len(chain) > 1:
                    rotate_joint(skeleton, local, chain[1], rig.X_AXIS, np.radians(-65.0 * arm_amount))
    cape_depth = {}
    for index in skeleton.topo:
        bone = skeleton.names[index]
        parent = skeleton.parent[index]
        if bone.lower().startswith('cape'):
            cape_depth[index] = cape_depth.get(parent, -1) + 1
    if name == 'Scarab':
        for bone in ('Bone.001_L', 'Bone.001_R'):
            rotate_joint(skeleton, local, bone, rig.X_AXIS, np.radians(-40.0 * amount))
    if name == 'CaveHornet':
        rotate_joint(skeleton, local, 'Head', rig.X_AXIS, np.radians(-15.0 * amount))
        for side in ('L', 'R'):
            for number, degrees in ((1, -20.0), (2, -25.0), (3, 15.0)):
                rotate_joint(skeleton, local, 'Proboscis' + str(number) + '_' + side,
                             rig.X_AXIS, np.radians(degrees * amount))
        fold_hover_wings(skeleton, local, base, vertices, weights, amount)
    if breath is not None:
        breathe(skeleton, local, author_rig, breath, name)
    if units:
        # Resolve torso/head clearance before planting the feet, rather than lifting planted feet afterwards.
        leg_bones = {index for unit in units for index in unit.ids}
        if humanoid:
            for item in author_rig:
                for chain in list(item.get('hang', [])) + ([item['press']] if item.get('press') else []):
                    leg_bones.update(skeleton.idx[bone] for bone in chain)
            leg_bones.update(cape_depth)
        for index in skeleton.topo:
            if skeleton.parent[index] in leg_bones:
                leg_bones.add(index)
        torso = [index for index, assignments in enumerate(weights)
                 if sum(weight for bone, weight in assignments if bone not in leg_bones) >= 0.9]
        assert torso, (name, 'No independently weighted torso for ground clearance')
        original_points = skin_vertices(skeleton, base, vertices, weights)
        posed_points = skin_vertices(skeleton, local, vertices, weights)
        correction = np.zeros((len(amount), 3))
        correction[:, 2] = np.maximum(0.0, original_points[0, :, 2].min() -
                                     posed_points[:, torso, 2].min(axis=1))
        move_roots(skeleton, local, correction)
    for placement in range(4):
        errors = []
        for unit in units:
            original_position, original_rotation, original_scale = original[unit.ids[-1]]
            target = original_position + np.einsum('nij,nj->ni', original_rotation, original_scale * unit.eff_off)
            if humanoid:
                target[:, 1] -= height * 0.2 * amount
            if name == 'CaveHornet':
                target[:, 2] += (original_points[0, :, 2].min() - target[0, 2]) * amount
            world = skeleton.fk(local)
            parent_world = world[unit.par] if unit.par >= 0 else None
            initial = [local[index] for index in unit.ids]
            delta = None
            for stiffness in (4.0, 1.5, 0.6):
                delta, error, solved = feet.solve_unit(unit, initial, parent_world, target, original_rotation,
                                                      height, wo=FOOT_ORIENTATION_WEIGHT, wr=stiffness, iters=60, delta0=delta)
            for index, solved_local in zip(unit.ids, solved):
                local[index] = solved_local
            errors.append(float(error.max() / height))
        if lying and humanoid and name not in PRONE_BODIES:
            # This temporary upright pose is rotated onto its back next; a held blade may safely extend downward here.
            break
        # Mixed skin weights can put a bent limb below its planted tip; raise its body and replant the feet.
        posed_points = skin_vertices(skeleton, local, vertices, weights)
        rigid = [index for index, row in enumerate(weights)
                 if sum(weight for bone, weight in row
                        if skeleton.names[bone].lower().startswith('cape') or skeleton.names[bone] == 'Pick') < 0.1]
        penetration = np.maximum(0.0, original_points[0, :, 2].min() - posed_points[:, rigid, 2].min(axis=1))
        if placement == 3 or penetration.max() < height * 1e-4:
            break
        correction = np.zeros((len(amount), 3))
        correction[:, 2] = penetration
        move_roots(skeleton, local, correction)
    error = max(errors)
    assert error <= MAX_CONTACT_ERROR_RATIO, (name, 'foot contact error', error)
    if name in ('DarkElf', 'Elf'):
        level_shin_feet(skeleton, local, base)
    if humanoid and not lying:
        hand_error = rest_hands(skeleton, local, base, author_rig, units, height, amount, vertices, weights)
        assert hand_error <= MAX_CONTACT_ERROR_RATIO, (name, 'hand support error', hand_error)
        for item in author_rig:
            for chain in list(item.get('hang', [])) + ([item['press']] if item.get('press') else []):
                level_hand(skeleton, local, chain, vertices, weights, amount)
        drape_cape(skeleton, local, base, vertices, weights)
        if name == 'RunelordDwarf':
            floor = skin_vertices(skeleton, base, vertices, weights)[0, :, 2].min()
            for bone in ('a_4.L', 'a_4.R'):
                raise_joint_surface(skeleton, local, vertices, weights, skeleton.idx[bone], floor)
    if not lying:
        fold_tool(skeleton, local, vertices, weights, amount)
    return local, error


# Close a pose exactly on the original idle frame and serialize only its new animation block.
def clip_xml(skeleton, local, base, times, name):
    lines = ['<animation name="' + name + '" length="' + str(float(times[-1])) + '"><tracks>']
    for index, bone in enumerate(skeleton.names):
        position, rotation, scale = local[index]
        for frame in (0, len(times) - 1):
            position[frame] = base[index][0][frame]
            rotation[frame] = base[index][1][frame]
            scale[frame] = base[index][2][frame]
        translation = position - skeleton.rest_pos[index]
        key_rotation = feet.qmul(np.tile(feet.qconj(skeleton.rest_q[index]), (len(times), 1)), rotation)
        key_scale = scale / skeleton.rest_s[index]
        lines.append('<track bone="' + bone + '"><keyframes>')
        for frame, time in enumerate(times):
            lines.append(feet.key_xml(time, translation[frame], key_rotation[frame], key_scale[frame], True))
        lines.append('</keyframes></track>')
    lines.append('</tracks></animation>')
    return '\n'.join(lines)


# Distinguish a visible anatomical branch from a separate unweighted authoring-control rig.
def rig_visible(skeleton, item, weighted):
    branches = {skeleton.idx[bone] for bone in item.get('body', []) + item.get('spine', []) + [item['head']]
                if bone is not None}
    for index in skeleton.topo:
        if skeleton.parent[index] in branches:
            branches.add(index)
    return bool(branches & weighted)


# Author and measure both complete idle rest cycles without changing existing clips or geometry.
def author(source, mesh, destination):
    skeleton = feet.Skel(str(source))
    author_skeleton = rig.Skeleton(str(source))
    name = source.name.removesuffix('.skeleton.xml')
    body_rig = rig.config(name, author_skeleton)
    vertices, faces, weights = mesh_data(mesh)
    if isinstance(body_rig, list):
        weighted = {bone for row in weights for bone, weight in row if weight > 0.0}
        body_rig = [item for item in body_rig if rig_visible(skeleton, item, weighted)]
        assert body_rig, (name, 'No visible anatomical rig')
    text = source.read_text(encoding='utf-8')
    report = {'model': name, 'clips': {}}
    for clip, length in (('AmbientSit', SIT_LENGTH), ('AmbientLie', LIE_LENGTH)):
        assert clip not in skeleton.clips, (name, clip)
        times = np.linspace(0.0, length, POSE_SAMPLES)
        base = skeleton.locals_at(skeleton.clips['Idle'], np.zeros(len(times)))
        points = skin_vertices(skeleton, base, vertices, weights)
        height = float(np.ptp(points[0, :, 2]))
        assert height > 0.0
        amount = envelope(times, length)
        breath = BREATH_AMOUNT * np.sin(times * (2.0 * math.pi / BREATH_SECONDS)) * amount
        error = None
        floor_correction = np.zeros(len(times))
        if clip == 'AmbientSit':
            local, error = sitting(skeleton, body_rig, base, amount, height, name, vertices, weights, breath=breath)
        else:
            humanoid = isinstance(body_rig, list) and all(item['kind'] == 'biped' for item in body_rig) and name not in PRONE_BODIES
            local, error = sitting(skeleton, body_rig, base, amount * (0.1 if humanoid else 1.0),
                                   height, name, vertices, weights, lying=True, breath=breath)
            if humanoid:
                tilt_body(skeleton, local, np.radians(-90.0 * amount))
            fold_tool(skeleton, local, vertices, weights, amount)
            lying_points = skin_vertices(skeleton, local, vertices, weights)
            offset = np.zeros((len(times), 3))
            offset[:, 2] = points[0, :, 2].min() - lying_points[:, :, 2].min(axis=1)
            offset[:, :2] = points[0, :, :2].mean(axis=0) - lying_points[:, :, :2].mean(axis=1)
            move_roots(skeleton, local, offset)
            floor_correction += offset[:, 2]
        # Preserve the original visible floor level for every transition frame, including limbless rigs.
        posed = skin_vertices(skeleton, local, vertices, weights)
        offset = np.zeros((len(times), 3))
        offset[:, 2] = points[0, :, 2].min() - posed[:, :, 2].min(axis=1)
        move_roots(skeleton, local, offset)
        floor_correction += offset[:, 2]
        authored = clip_xml(skeleton, local, base, times, clip)
        text = text.replace('</animations>', authored + '\n</animations>')
        final_points = skin_vertices(skeleton, local, vertices, weights)
        sole_error = sole_lift(skeleton, points, final_points, weights, height, name)
        if clip == 'AmbientSit' and sole_error is not None:
            assert sole_error <= MAX_CONTACT_ERROR_RATIO, (name, 'visible sole clearance', sole_error)
        endpoint_error = float(np.abs(final_points[[0, -1]] - points[[0, -1]]).max() / height)
        assert endpoint_error < 1e-5, (name, 'entry/exit pose mismatch', endpoint_error)
        report['clips'][clip] = {'samples': len(times), 'ik_effector_error_ratio': error,
                                 'max_visible_sole_lift_ratio': sole_error,
                                 'endpoint_error_ratio': endpoint_error,
                                 'max_vertex_step_ratio': float(np.linalg.norm(np.diff(final_points, axis=0), axis=2).max() / height),
                                 'max_floor_correction_ratio': float(abs(floor_correction).max() / height)}
    assert not destination.exists(), destination
    destination.write_text(text, encoding='utf-8')
    return report


# Write candidates and their measured contact report to an explicit output directory.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('names', nargs='+')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    reports = []
    for name in args.names:
        mesh_name = 'TentacleGreen' if name == 'Tentacle' else name
        report = author(args.input / (name + '.skeleton.xml'), args.input / (mesh_name + '.mesh.xml'),
                        args.output / (name + '.skeleton.xml'))
        reports.append(report)
        print(name, report['clips'], flush=True)
    report_path = args.output / 'contact-report.json'
    assert not report_path.exists(), report_path
    report_path.write_text(json.dumps(reports, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
