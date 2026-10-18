#!/usr/bin/env python3
"""Close confirmed loop boundaries while retaining unchanged tracks and clip durations."""
import argparse
import json
from pathlib import Path
import re

import numpy as np

import fix_walk_feet as feet


VALUE_TOLERANCE = 1e-6


# Detect a true end-key discontinuity without treating an interpolated trailing interval as a seam.
def discontinuous(track, length):
    if abs(track.t[-1] - length) > VALUE_TOLERANCE:
        return False
    same_rotation = np.array_equal(track.Q[-1], track.Q[0]) or np.array_equal(track.Q[-1], -track.Q[0])
    return (not np.array_equal(track.T[-1], track.T[0]) or
            not np.array_equal(track.S[-1], track.S[0]) or not same_rotation)


# Replace only discontinuous tracks using the project's existing smooth closure convention.
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
            times, translation, rotation, scale = feet.close_track(track, 1.0, clip.length)
            # The helper leaves sub-tolerance source noise untouched; close those endpoints exactly too.
            translation[-1] = translation[0]
            rotation[-1] = rotation[0]
            scale[-1] = scale[0]
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
