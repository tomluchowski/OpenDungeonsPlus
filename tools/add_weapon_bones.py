#!/usr/bin/env python3
"""Add the three right-hand weapon mounts and verify unchanged skeleton data."""
import copy
import math
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = Path(r"C:\Users\mario\GitHub\OpenDungeonsPlus-Private\anim-work\armament")
CONVERTER = Path(r"C:\Users\mario\od-deps\build\ogre\bin\release\OgreXMLConverter.exe")
TARGETS = {"skeleton": "itemRightHand", "lich": "itemRightHand", "PitDemon": "Hand_R"}


# Run the asset converter with closed stdin, a time limit and a private log.
def convert(source, target, label, binary=False):
    command = [str(CONVERTER), "-q"]
    if binary:
        command.append("-o")
    command.extend((str(source), str(target)))
    with (PRIVATE / "logs" / (label + ".log")).open("w", encoding="utf-8") as log:
        subprocess.run(command, cwd=PRIVATE / "logs", stdin=subprocess.DEVNULL,
                       stdout=log, stderr=subprocess.STDOUT, timeout=600, check=True)


# Compare complete XML trees, allowing only converter float rounding up to 1e-5.
def compare(before, after, path="skeleton"):
    if before.tag != after.tag or before.attrib.keys() != after.attrib.keys():
        raise ValueError("Structure changed at " + path)
    # Rotation axes at angle zero may be canonicalized by the converter.
    if before.tag in ("rotate", "rotation"):
        # Convert axis-angle XML to its equivalent unit quaternion.
        def quaternion(element):
            angle = float(element.attrib["angle"])
            axis = element.find("axis")
            vector = [float(axis.attrib[key]) for key in ("x", "y", "z")]
            length = math.sqrt(sum(value * value for value in vector))
            scale = math.sin(angle / 2) / length if length else 0
            return [math.cos(angle / 2)] + [value * scale for value in vector]
        old_rotation, new_rotation = quaternion(before), quaternion(after)
        difference = min(max(abs(a - sign * b) for a, b in zip(old_rotation, new_rotation))
                         for sign in (1, -1))
        if difference > 1e-5:
            raise ValueError("Rotation changed at " + path)
        if len(before) != 1 or len(after) != 1 or before[0].tag != "axis" or after[0].tag != "axis":
            raise ValueError("Rotation structure changed at " + path)
        return
    for name, value in before.attrib.items():
        other = after.attrib[name]
        if value == other:
            continue
        try:
            equal = abs(float(value) - float(other)) <= 1e-5
        except ValueError:
            equal = False
        if not equal:
            raise ValueError("Value changed at " + path + "/@" + name)
    if (before.text or "").strip() != (after.text or "").strip() or len(before) != len(after):
        raise ValueError("Content changed at " + path)
    for index, (old, new) in enumerate(zip(before, after)):
        compare(old, new, path + "/" + old.tag + "[" + str(index) + "]")


# Insert an XML fragment without rewriting any existing source line.
def append_fragment(text, container, fragment):
    pattern = r"(?m)^([ \t]*)</" + container + r">"
    match = re.search(pattern, text)
    if not match:
        raise ValueError("Missing closing element: " + container)
    indent = match.group(1) + "    "
    ET.indent(fragment, space="    ", level=0)
    rendered = ET.tostring(fragment, encoding="unicode", short_empty_elements=True)
    rendered = "\n".join(indent + line for line in rendered.splitlines()) + "\n"
    return text[:match.start()] + rendered + text[match.start():]


# Build exactly the specified identity mount or the scaled reference mount.
def weapon_bone(name, original, reference):
    ids = {int(bone.attrib["id"]) for bone in original.find("bones")}
    bone = ET.Element("bone", {"id": str(max(ids) + 1), "name": "Weapon_R"})
    if name == "PitDemon":
        position = copy.deepcopy(reference.find("position"))
        for axis in ("x", "y", "z"):
            position.set(axis, format(float(position.attrib[axis]) * 1.288, ".12g"))
        bone.append(position)
        bone.append(copy.deepcopy(reference.find("rotation")))
    else:
        ET.SubElement(bone, "position", {"x": "0", "y": "0", "z": "0"})
        rotation = ET.SubElement(bone, "rotation", {"angle": "0"})
        ET.SubElement(rotation, "axis", {"x": "1", "y": "0", "z": "0"})
    return bone


# Verify the sole added bone, its parent, and every original bone and animation.
def verify(original, result, expected, parent):
    new_bones = result.find("bones")
    mounts = [bone for bone in new_bones if bone.attrib["name"] == "Weapon_R"]
    if len(mounts) != 1:
        raise ValueError("Expected exactly one new weapon bone")
    compare(expected, mounts[0], "Weapon_R")
    new_bones.remove(mounts[0])
    links = result.find("bonehierarchy")
    mounts = [link for link in links if link.attrib["bone"] == "Weapon_R"]
    if len(mounts) != 1 or mounts[0].attrib != {"bone": "Weapon_R", "parent": parent}:
        raise ValueError("Weapon parent differs")
    links.remove(mounts[0])
    compare(original, result)


# Back up original binaries, append mounts and verify each binary round trip.
def main():
    for folder in ("orig", "xml", "logs"):
        (PRIVATE / folder).mkdir(parents=True, exist_ok=True)
    reference_xml = PRIVATE / "xml" / "Adventurer.skeleton.xml"
    convert(ROOT / "models" / "Adventurer.skeleton", reference_xml, "bone-Adventurer-export")
    reference = ET.parse(reference_xml).find("bones/bone[@name='Weapon_R']")
    if reference is None:
        raise ValueError("Reference weapon bone missing")
    summaries = []
    for name, parent in TARGETS.items():
        target = ROOT / "models" / (name + ".skeleton")
        backup = PRIVATE / "orig" / target.name
        if not backup.exists():
            shutil.copy2(target, backup)
            if backup.read_bytes() != target.read_bytes():
                raise ValueError("Backup verification failed: " + name)
        original_xml = PRIVATE / "xml" / (name + ".original.skeleton.xml")
        convert(backup, original_xml, "bone-" + name + "-original-export")
        text = original_xml.read_text(encoding="utf-8")
        original = ET.fromstring(text)
        if original.find("bones/bone[@name='Weapon_R']") is not None:
            raise ValueError("Original already has a weapon bone: " + name)
        if original.find("bones/bone[@name='" + parent + "']") is None:
            raise ValueError("Required parent missing: " + name)
        bone = weapon_bone(name, original, reference)
        modified = append_fragment(text, "bones", copy.deepcopy(bone))
        link = ET.Element("boneparent", {"bone": "Weapon_R", "parent": parent})
        modified = append_fragment(modified, "bonehierarchy", link)
        output_xml = PRIVATE / "xml" / (name + ".skeleton.xml")
        output_xml.write_text(modified, encoding="utf-8", newline="\n")
        convert(output_xml, target, "bone-" + name + "-convert", binary=True)
        roundtrip = PRIVATE / "xml" / (name + ".roundtrip.skeleton.xml")
        convert(target, roundtrip, "bone-" + name + "-roundtrip")
        verify(original, ET.parse(roundtrip).getroot(), bone, parent)
        summary = (name + ": PASS; only Weapon_R added; parent=" + parent
                   + "; all original bones, parents, rest values, tracks and keys unchanged within 1e-5 (rotations compared as quaternions)")
        summaries.append(summary)
        print(summary)
    (PRIVATE / "logs" / "weapon-bone-verification.log").write_text("\n".join(summaries) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
