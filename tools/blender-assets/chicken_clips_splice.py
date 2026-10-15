#!/usr/bin/env python3
"""Puts clips exported from Blender into the chicken skeleton XML, replacing clips of the same name.

    python chicken_clips_splice.py <Chicken.skeleton.xml> <clips.skeleton.xml> <out.skeleton.xml>

Both inputs are OgreXMLConverter / odp_ogre_io skeleton XML files. The bones and the hierarchy of both must be the
same. Clips that exist in the first file are replaced in place, new clips are appended; all other clips stay byte for
byte as they are. Afterwards run OgreXMLConverter -o on the output (the -o keeps every keyframe).
"""
import re
import sys
import xml.etree.ElementTree as ET


def blocks(text):
    found = {}
    for match in re.finditer(r'\t\t<animation name="([^"]+)" length="[^"]*">.*?\n\t\t</animation>\n', text, re.S):
        found[match.group(1)] = match.group(0)
    return found


def structure(path):
    root = ET.parse(path).getroot()
    bones = [(b.get("id"), b.get("name"), tuple(sorted(b.find("position").attrib.items())),
              tuple(sorted(b.find("rotation").find("axis").attrib.items())), b.find("rotation").get("angle"))
             for b in root.find("bones").findall("bone")]
    return bones, [(h.get("bone"), h.get("parent")) for h in root.find("bonehierarchy").findall("boneparent")]


def number_close(a, b):
    return abs(float(a) - float(b)) < 1e-4


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    base_path, clips_path, out_path = sys.argv[1:]
    base_bones, base_hier = structure(base_path)
    new_bones, new_hier = structure(clips_path)
    if [b[1] for b in base_bones] != [b[1] for b in new_bones] or base_hier != new_hier:
        raise SystemExit("bones or hierarchy differ")
    for b0, b1 in zip(base_bones, new_bones):
        values0 = [float(v) for _k, v in b0[2]] + [float(b0[4])] + [float(v) for _k, v in b0[3]]
        values1 = [float(v) for _k, v in b1[2]] + [float(b1[4])] + [float(v) for _k, v in b1[3]]
        if not all(number_close(x, y) for x, y in zip(values0, values1)):
            raise SystemExit("bind pose of %s differs" % b0[1])
    base_text = open(base_path, encoding="utf-8").read()
    clips_text = open(clips_path, encoding="utf-8").read()
    old = blocks(base_text)
    new = blocks(clips_text)
    text = base_text
    for name, block in new.items():
        if name in old:
            text = text.replace(old[name], block)
    added = [name for name in new if name not in old]
    if added:
        end = text.rindex("\t</animations>")
        text = text[:end] + "".join(new[name] for name in added) + text[end:]
    open(out_path, "w", encoding="utf-8", newline="\n").write(text)
    print("replaced", [n for n in new if n in old], "added", added)
    return 0


if __name__ == "__main__":
    sys.exit(main())
