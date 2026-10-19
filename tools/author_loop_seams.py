#!/usr/bin/env python3
"""Close confirmed loop boundaries while retaining unchanged tracks and clip durations."""
import argparse
import json
from pathlib import Path
import re

import numpy as np

import fix_walk_feet as feet


VALUE_TOLERANCE = 1e-6
TAIL_SHARE = 0.2
TAIL_SAMPLES = 17


# Return the local angular velocity along the shortest quaternion arc.
def rotation_velocity(first, last, duration):
    assert duration > 0.0, 'Angular velocity needs strictly increasing key times'
    difference = feet.qnorm(feet.qmul(feet.qconj(feet.qnorm(first)), feet.qnorm(last)))
    if difference[0] < 0.0:
        difference = -difference
    angle, axis = feet.q2aa(difference)
    return np.asarray(axis) * angle / duration


# Close pose and velocity through a sampled Hermite correction confined to the original closure window.
def close_smooth_track(track, length):
    tail = np.linspace(length * (1.0 - TAIL_SHARE), length, TAIL_SAMPLES)
    times = np.unique([float(feet.fmt(time)) for time in np.concatenate((track.t, tail))])
    times[-1] = length
    translation, rotation, scale = track.sample(times)
    sampled = feet.Track(times, translation, rotation, scale, track.has_scale)
    times, translation, rotation, scale = feet.close_track(sampled, 1.0, length, win=TAIL_SHARE)
    entry_time = track.t[1] - track.t[0]
    exit_time = track.t[-1] - track.t[-2]
    start_position = (track.T[1] - track.T[0]) / entry_time
    end_position = (track.T[-1] - track.T[-2]) / exit_time
    start_scale = (track.S[1] - track.S[0]) / entry_time
    end_scale = (track.S[-1] - track.S[-2]) / exit_time
    start_rotation = rotation_velocity(track.Q[0], track.Q[1], entry_time)
    end_rotation = rotation_velocity(track.Q[-2], track.Q[-1], exit_time)
    if track.t[-1] < length - VALUE_TOLERANCE:
        # Ogre holds the last key until the clip wraps; it does not interpolate that interval back to the first key.
        end_position = np.zeros(3)
        end_scale = np.zeros(3)
        end_rotation = np.zeros(3)
    correction = feet.qmul(feet.qconj(track.Q[-1]), track.Q[0])
    end_rotation = feet.q2m(correction).T @ end_rotation
    progress = np.clip((times / length - (1.0 - TAIL_SHARE)) / TAIL_SHARE, 0.0, 1.0)
    tangent = (progress ** 3 - progress ** 2) * length * TAIL_SHARE
    translation += tangent[:, None] * (start_position - end_position)
    scale += tangent[:, None] * (start_scale - end_scale)
    rotation = feet.qmul(rotation, feet.rv2q(tangent[:, None] * (start_rotation - end_rotation)))
    translation[-1] = translation[0]
    rotation[-1] = rotation[0]
    scale[-1] = scale[0]
    # Ogre interpolates linearly between keys; make the final segment's tangent exact too.
    final_step = times[-1] - times[-2]
    translation[-2] = translation[0] - start_position * final_step
    rotation[-2] = feet.qmul(rotation[0], feet.rv2q(-start_rotation * final_step))
    scale[-2] = scale[0] - start_scale * final_step
    return times, translation, rotation, scale


# Detect a loop discontinuity, including Ogre's held last key when the track ends before the clip.
def discontinuous(track, length):
    if track.t[-1] > length + VALUE_TOLERANCE:
        return False
    same_rotation = np.array_equal(track.Q[-1], track.Q[0]) or np.array_equal(track.Q[-1], -track.Q[0])
    if not np.array_equal(track.T[-1], track.T[0]) or not np.array_equal(track.S[-1], track.S[0]) or not same_rotation:
        return True
    if len(track.t) < 2:
        return False
    entry_time = track.t[1] - track.t[0]
    exit_time = track.t[-1] - track.t[-2]
    if track.t[-1] < length - VALUE_TOLERANCE:
        return (np.linalg.norm((track.T[1] - track.T[0]) / entry_time) > VALUE_TOLERANCE or
                np.linalg.norm(rotation_velocity(track.Q[0], track.Q[1], entry_time)) > VALUE_TOLERANCE or
                np.linalg.norm((track.S[1] - track.S[0]) / entry_time) > VALUE_TOLERANCE)
    return (np.linalg.norm((track.T[1] - track.T[0]) / entry_time - (track.T[-1] - track.T[-2]) / exit_time) > VALUE_TOLERANCE or
            np.linalg.norm(rotation_velocity(track.Q[0], track.Q[1], entry_time) -
                           rotation_velocity(track.Q[-2], track.Q[-1], exit_time)) > VALUE_TOLERANCE)


# Replace only discontinuous tracks, retaining the original closure window and matching its entry velocity.
def author(source, destination, names):
    skeleton = feet.Skel(str(source))
    text = source.read_text(encoding='utf-8')
    report = {}
    for name in names:
        clip = skeleton.clips[name]
        changed = []
        pattern = r'(<animation name="' + re.escape(name) + r'"[^>]*>)(.*?)(</animation>)'
        match = re.search(pattern, text, re.S)
        assert match is not None, name
        block = match.group(2)
        for bone, track in clip.tracks.items():
            if not discontinuous(track, clip.length):
                continue
            assert len(track.t) >= 2, (name, bone)
            times, translation, rotation, scale = close_smooth_track(track, clip.length)
            keys = [feet.key_xml(time, translation[index], rotation[index], scale[index], track.has_scale)
                    for index, time in enumerate(times)]
            replacement = '<track bone="' + bone + '"><keyframes>\n' + ''.join(keys) + '</keyframes></track>'
            track_pattern = r'<track bone="' + re.escape(bone) + r'"[^>]*>.*?</track>'
            block, count = re.subn(track_pattern, replacement, block, count=1, flags=re.S)
            assert count == 1, (name, bone)
            changed.append(bone)
        text = text[:match.start(2)] + block + text[match.end(2):]
        report[name] = changed
    assert not destination.exists(), destination
    destination.write_text(text, encoding='utf-8')
    return report


# Write an isolated loop candidate and list its changed tracks for review.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('clips', nargs='+')
    args = parser.parse_args()
    print(json.dumps(author(args.source, args.destination, args.clips), indent=2))


if __name__ == '__main__':
    main()
