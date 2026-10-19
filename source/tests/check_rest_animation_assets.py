#!/usr/bin/env python3
"""Check every configured creature's serialized joint-based rest poses without compiling the game."""
import math
from pathlib import Path
import re
import sys

import numpy as np


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import author_rest_poses as author
import fix_walk_feet as feet
from check_animation_assets import export


# Map the configured reaction strength to the authored entry curve, independently of creature playback speed.
def sample_time(progress, strength, length):
    if strength >= 1.0:
        return length * progress
    entering = min(1.0, max(0.0, progress / author.TRANSITION_SHARE))
    leaving = min(1.0, max(0.0, (1.0 - progress) / author.TRANSITION_SHARE))
    amount = entering * entering * (3.0 - 2.0 * entering)
    amount *= leaving * leaving * (3.0 - 2.0 * leaving) * strength
    inverse = 0.5 - math.sin(math.asin(max(-1.0, min(1.0, 1.0 - 2.0 * amount))) / 3.0)
    return length * author.TRANSITION_SHARE * inverse


# Reject floating or penetrating geometry, allowing the same contact bound plus serializer precision.
def check_floor(points, floor, height):
    error = np.abs(points[:, :, 2].min(axis=1) - floor) / height
    assert float(error.max()) <= author.MAX_CONTACT_ERROR_RATIO + 2e-5, ('floor contact', float(error.max()))


# Require every original rest track while verifying the three new unweighted mounts remain passive.
def rest_track_names(skeleton, skeleton_name, weighted):
    required = set(skeleton.names)
    parent = {'skeleton.skeleton': 'itemRightHand', 'lich.skeleton': 'itemRightHand',
              'PitDemon.skeleton': 'Hand_R'}.get(skeleton_name)
    if parent is not None:
        assert 'Weapon_R' in skeleton.idx, (skeleton_name, 'missing passive weapon mount')
        mount = skeleton.idx['Weapon_R']
        assert mount not in weighted, (skeleton_name, 'passive mount deforms creature')
        assert skeleton.names[skeleton.parent[mount]] == parent, (skeleton_name, 'wrong weapon parent')
        assert all('Weapon_R' not in clip.tracks for clip in skeleton.clips.values()), (skeleton_name, 'animated passive mount')
        required.remove('Weapon_R')
    return required


# Verify actual configured mesh coverage, native deforming-track coverage and passive mounts, partial strengths and both return poses.
def main():
    source = (ROOT / 'source/render/CreatureReactions.cpp').read_text(encoding='utf-8')
    share = float(re.search(r'ARTICULATED_TRANSITION_SHARE = ([0-9.]+);', source).group(1))
    assert share == author.TRANSITION_SHARE
    configuration = (ROOT / 'config/creatures.cfg').read_text(encoding='utf-8')
    meshes = sorted(set(re.findall(r'^\s*MeshName\s+(\S+\.mesh)\s*$', configuration, re.MULTILINE)))
    reactions = (ROOT / 'config/creatureReactions.cfg').read_text(encoding='utf-8')
    out = ROOT / 'out/rest-animation-assets'
    out.mkdir(parents=True, exist_ok=True)
    skeletons = {}
    tested = 0
    for mesh_name in meshes:
        mesh_xml = export(mesh_name, out)
        skeleton_name = mesh_xml.find('skeletonlink').get('name')
        assert (ROOT / 'models' / skeleton_name).is_file(), skeleton_name
        if skeleton_name not in skeletons:
            export(skeleton_name, out)
            skeletons[skeleton_name] = feet.Skel(str(out / (skeleton_name + '.xml')))
        skeleton = skeletons[skeleton_name]
        vertices, faces, weights = author.mesh_data(out / (mesh_name + '.xml'))
        original_local = skeleton.locals_at(skeleton.clips['Idle'], np.array([0.0]))
        original = author.skin_vertices(skeleton, original_local, vertices, weights)
        floor = float(original[0, :, 2].min())
        height = float(np.ptp(original[0, :, 2]))
        assert height > 0.0
        weighted = {bone for row in weights for bone, weight in row if weight > 0.0}
        required_tracks = rest_track_names(skeleton, skeleton_name, weighted)
        for name, kind, length, reference in (('AmbientSit', 'sit', author.SIT_LENGTH, 0.28),
                                             ('AmbientLie', 'lie', author.LIE_LENGTH, 80.0)):
            assert name in skeleton.clips, (mesh_name, name)
            clip = skeleton.clips[name]
            assert abs(clip.length - length) < 1e-5
            assert set(clip.tracks) == required_tracks, (mesh_name, 'missing rest tracks')
            for track in clip.tracks.values():
                assert np.all(np.diff(track.t) > 0.0)
                assert abs(track.t[0]) < 1e-6 and abs(track.t[-1] - length) < 1e-5
                assert np.all(np.isfinite(track.T)) and np.all(np.isfinite(track.Q)) and np.all(np.isfinite(track.S))
                assert np.all(track.S > 0.0)
            endpoints = skeleton.locals_at(clip, np.array([0.0, length]))
            endpoint_points = author.skin_vertices(skeleton, endpoints, vertices, weights)
            assert float(np.abs(endpoint_points - original).max() / height) < 2e-5, (mesh_name, name, 'idle endpoints')
            amounts = [float(value) for value in re.findall(r'^\s*Motion\s+' + kind + r'\s+\S+\s+(\S+)',
                                                            reactions, re.MULTILINE)]
            strengths = sorted({0.0, 1.0} | {min(1.0, abs(value) / reference) for value in amounts})
            times = np.array([sample_time(progress, strength, length) for strength in strengths
                              for progress in (0.0, 0.03, 0.06, 0.1, 0.5, 0.9, 0.94, 0.97, 1.0)])
            local = skeleton.locals_at(clip, times)
            points = author.skin_vertices(skeleton, local, vertices, weights)
            check_floor(points, floor, height)
            if name == 'AmbientSit':
                reference_points = np.repeat(original, len(times), axis=0)
                error = author.sole_lift(skeleton, reference_points, points, weights, height,
                                         Path(skeleton_name).stem)
                assert error is None or error <= author.MAX_CONTACT_ERROR_RATIO + 2e-5, (mesh_name, error)
            middle = skeleton.locals_at(clip, np.array([length * 0.5]))
            living = author.skin_vertices(skeleton, skeleton.locals_at(clip, np.array([length * 0.35, length * 0.55])),
                                          vertices, weights)
            assert float(np.abs(living[1] - living[0]).max() / height) > 1e-5, (mesh_name, name, 'frozen rest hold')
            hold_times = np.linspace(length * 0.15, length * 0.85, 121)
            hold_points = author.skin_vertices(skeleton, skeleton.locals_at(clip, hold_times), vertices, weights)
            check_floor(hold_points, floor, height)
            hold_step = float(np.linalg.norm(np.diff(hold_points, axis=0), axis=2).max() / height)
            assert hold_step < author.BREATH_AMOUNT, (mesh_name, name, 'rest support jump', hold_step)
            articulated = False
            for bone in weighted:
                if skeleton.parent[bone] < 0:
                    continue
                for field in (0, 1, 2):
                    if np.max(np.abs(middle[bone][field] - original_local[bone][field])) > 1e-5:
                        articulated = True
            assert articulated, (mesh_name, name, 'root-only placeholder')
            tested += 1
        lifted = original.copy()
        lifted[:, :, 2] += 0.1 * height
        try:
            check_floor(lifted, floor, height)
        except AssertionError:
            pass
        else:
            raise AssertionError('A floating-body negative control passed')
    assert len(meshes) == 33 and len(skeletons) == 32
    print('Checked', tested, 'serialized rest variants on all 33 configured meshes and 32 skeletons, including partial strengths.')


if __name__ == '__main__':
    main()
