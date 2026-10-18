#!/usr/bin/env python3
"""Replace selected OGRE skeleton animation chunks without reserializing unrelated bones or clips."""
import argparse
from pathlib import Path
import struct


HEADER = 0x1000
BONE = 0x2000
ANIMATION = 0x4000
CHUNK_HEADER_BYTES = 6


# Read top-level chunks, accounting for OGRE's historical exclusion of bone names from chunk lengths.
def chunks(data):
    assert struct.unpack_from('<H', data)[0] == HEADER
    start = data.index(b'\n') + 1
    header = data[:start]
    result = []
    while start < len(data):
        kind, length = struct.unpack_from('<HI', data, start)
        assert length >= CHUNK_HEADER_BYTES, (start, kind, length)
        end = start + length
        if kind == BONE:
            end += data.index(b'\n', start + CHUNK_HEADER_BYTES) - (start + CHUNK_HEADER_BYTES) + 1
        assert end <= len(data), (start, kind, end, len(data))
        name = None
        if kind == ANIMATION:
            name = data[start + CHUNK_HEADER_BYTES:data.index(b'\n', start + CHUNK_HEADER_BYTES)].decode('utf-8')
        result.append((kind, name, data[start:end]))
        start = end
    return header, result


# Preserve every original byte except explicitly selected animation chunks, appending selected new clips once.
def splice(original, authored, selected):
    header, original_chunks = chunks(original)
    authored_header, authored_chunks = chunks(authored)
    assert header == authored_header, 'Use the same skeleton serialization version for both exports'
    authored_clips = {name: data for kind, name, data in authored_chunks if kind == ANIMATION}
    assert selected <= authored_clips.keys(), selected - authored_clips.keys()
    written = set()
    result = [header]
    for kind, name, data in original_chunks:
        if kind == ANIMATION and name in selected:
            result.append(authored_clips[name])
            written.add(name)
        else:
            result.append(data)
    for kind, name, data in authored_chunks:
        if kind == ANIMATION and name in selected and name not in written:
            result.append(data)
            written.add(name)
    assert written == selected
    return b''.join(result)


# Produce a separate candidate file so an original asset is never overwritten by this tool.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('original', type=Path)
    parser.add_argument('authored', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('clips', nargs='+')
    args = parser.parse_args()
    assert not args.output.exists(), args.output
    result = splice(args.original.read_bytes(), args.authored.read_bytes(), set(args.clips))
    args.output.write_bytes(result)
    print('Spliced', len(args.clips), 'animation chunks; unrelated original chunks preserved byte for byte.')


if __name__ == '__main__':
    main()
