# Post-processing of exported skeleton XML: the authoring rig writes keyframe translations as offsets in the parent
# frame, Ogre applies them in the frame of the bone itself (rotated by the bind rotation). Converts the translations
# of the given clips: T_file = bind_rotation^-1 @ T_parent.
import xml.etree.ElementTree as ET
import odp_ogre_io as oo
from mathutils import Vector, Quaternion


def fix(path_in, path_out, clips):
    bones, parents, anims = oo.read_skeleton(path_in)
    rots = {b["name"]: b["rot"] for b in bones}
    text = open(path_in).read()
    root = ET.fromstring(text)
    for a in root.find("animations").findall("animation"):
        if a.get("name") not in clips:
            continue
        for t in a.find("tracks").findall("track"):
            q = rots[t.get("bone")]
            for k in t.find("keyframes").findall("keyframe"):
                tr = k.find("translate")
                v = q.inverted() @ Vector((float(tr.get("x")), float(tr.get("y")), float(tr.get("z"))))
                for c, val in zip("xyz", v):
                    s = "%.7g" % val
                    tr.set(c, "0" if s in ("-0", "0") else s)
    return root


def write(root, path_out):
    # same layout as the exporter (tabs, no declaration changes)
    ET.indent(root, space="\t")
    with open(path_out, "w", newline="\n") as f:
        f.write('<?xml version="1.0"?>\n' + ET.tostring(root, encoding="unicode") + "\n")
