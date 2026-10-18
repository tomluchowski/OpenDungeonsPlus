#!/usr/bin/env python3
"""Retime one skeleton clip without changing its transforms or unrelated binary chunks."""
import argparse
from pathlib import Path
import struct

from splice_animation_chunks import ANIMATION, chunks


# Scale the selected clip's length and key times while preserving all other serialized bytes.
def retime(data, name, length):
    assert length > 0.0
    header, blocks = chunks(data)
    result = [header]
    found = False
    for kind, clip, original in blocks:
        if kind != ANIMATION or clip != name:
            result.append(original)
            continue
        block = bytearray(original)
        offset = block.index(b'\n', 6) + 1
        previous = struct.unpack_from('<f', block, offset)[0]
        assert previous > 0.0
        factor = length / previous
        struct.pack_into('<f', block, offset, length)
        offset += 4
        while offset < len(block):
            kind, size = struct.unpack_from('<HI', block, offset)
            assert size >= 6 and offset + size <= len(block)
            if kind == 0x4100:
                key = offset + 8
                while key < offset + size:
                    key_kind, key_size = struct.unpack_from('<HI', block, key)
                    assert key_kind == 0x4110 and key_size in (38, 50)
                    assert key + key_size <= offset + size
                    time = struct.unpack_from('<f', block, key + 6)[0]
                    struct.pack_into('<f', block, key + 6, time * factor)
                    key += key_size
            offset += size
        result.append(bytes(block))
        found = True
    assert found, name
    return b''.join(result)


# Write a separate retimed candidate; replacing an existing asset is an explicit caller action.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('target', type=Path)
    parser.add_argument('clip')
    parser.add_argument('length', type=float)
    args = parser.parse_args()
    assert not args.target.exists(), args.target
    args.target.write_bytes(retime(args.source.read_bytes(), args.clip, args.length))


if __name__ == '__main__':
    main()
