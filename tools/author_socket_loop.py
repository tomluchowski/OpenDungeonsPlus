#!/usr/bin/env python3
"""Close a weapon socket's accumulated idle rotation smoothly without changing the creature's pose tracks."""
import argparse
from pathlib import Path
import re

import numpy as np

import fix_walk_feet as feet
from author_loop_seams import rotation_velocity


# Remove the incomplete turn over the entire cycle, then match its endpoint pose and angular velocity.
def author(source, target, clip_name, bone):
    skeleton = feet.Skel(str(source))
    clip = skeleton.clips[clip_name]
    track = clip.tracks[bone]
    correction = feet.qmul(track.Q[0], feet.qconj(track.Q[-1]))
    angle, axis = feet.q2aa(correction)
    progress = (track.t - track.t[0]) / (track.t[-1] - track.t[0])
    rotations = feet.qmul(feet.rv2q(progress[:, None] * angle * axis), track.Q)
    times, translation, scale = track.t, track.T, track.S
    entering = rotation_velocity(rotations[0], rotations[1], times[1] - times[0])
    leaving = rotation_velocity(rotations[-2], rotations[-1], times[-1] - times[-2])
    tangent = (entering + leaving) * 0.5
    first = progress ** 3 - 2.0 * progress ** 2 + progress
    last = progress ** 3 - progress ** 2
    adjustment = clip.length * (first[:, None] * (tangent - entering) + last[:, None] * (tangent - leaving))
    rotations = feet.qmul(rotations, feet.rv2q(adjustment))
    rotations[-1] = rotations[0]
    rotations[1] = feet.qmul(rotations[0], feet.rv2q(tangent * (times[1] - times[0])))
    rotations[-2] = feet.qmul(rotations[0], feet.rv2q(-tangent * (times[-1] - times[-2])))
    # Keep the authored rotation path but distribute its angular travel evenly, avoiding a new fast corrective spin.
    travel = np.array([np.linalg.norm(rotation_velocity(rotations[index], rotations[index + 1], 1.0))
                       for index in range(len(times) - 1)])
    assert np.all(travel > 1e-9)
    times = np.concatenate(([0.0], np.cumsum(travel))) * clip.length / travel.sum()
    keys = [feet.key_xml(time, translation[index], rotations[index], scale[index], track.has_scale)
            for index, time in enumerate(times)]
    replacement = '<track bone="' + bone + '"><keyframes>\n' + ''.join(keys) + '</keyframes></track>'
    text = source.read_text(encoding='utf-8')
    clip_match = re.search(r'<animation name="' + re.escape(clip_name) + r'".*?</animation>', text, re.S)
    assert clip_match is not None
    changed, count = re.subn(r'<track bone="' + re.escape(bone) + r'".*?</track>', replacement,
                            clip_match.group(0), count=1, flags=re.S)
    assert count == 1
    assert not target.exists(), target
    target.write_text(text[:clip_match.start()] + changed + text[clip_match.end():], encoding='utf-8')


# Write a separate candidate for one explicitly selected active socket track.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('target', type=Path)
    parser.add_argument('clip')
    parser.add_argument('bone')
    args = parser.parse_args()
    author(args.source, args.target, args.clip, args.bone)


if __name__ == '__main__':
    main()
