#!/usr/bin/env python3
"""Draws the forged emblems of the game interface into gui/ODIcons.png and gui/ODIcons.imageset.

Every room, trap, spell, worker and category emblem is rendered from height-field layers by
tools/forged_emblem.py (bump lighting from the top left, forged bronze, gold, iron, gem and ember
materials, cavity shading and cast shadows, 4x4 supersampling). The motifs live in
tools/forged_motifs.py. Slot emblems (128 px) and the mini emblems (64 px) fill a square tile with a dark
stone well and a fine bronze edge line, the small symbols stand on their own. The image names of the imageset do not
change, so no layout entry has to move.

Usage: python tools/generate_forged_icons.py            rebuilds the atlas and the imageset
       python tools/generate_forged_icons.py --sheet DIRECTORY [--only NAME ...] [--scale N] [--columns N]
                                                       writes a contact sheet and touches nothing else
"""
import multiprocessing
import os
import re
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from forged_emblem import Canvas  # noqa: E402
import forged_motifs as fm  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ATLAS = os.path.join(ROOT, "gui", "ODIcons.png")
IMAGESET = os.path.join(ROOT, "gui", "ODIcons.imageset")


def render_icon(name):
    spec = fm.ICONS[name]
    c = Canvas(cells=spec["cells"], ss=4, extent=spec.get("extent", (1.0, 1.0)), seed=sum(ord(ch) for ch in name) % 997 + 1)
    if spec["kind"] is not None:
        fm.tile(c, spec["kind"], spec.get("rivets", True))
        c.glow_r = fm.TILE_HALF
    snap = c.snapshot()
    c.set_zoom(spec.get("zoom", 1.0))
    spec["draw"](c)
    c.set_zoom(1.0)
    if spec["kind"] is not None:
        c.restore_outside(snap, fm.TILE_HALF)
    if spec["kind"] is not None:
        fm.tile_vignette(c)
        return c.render()
    return c.render(drop=(0.04, 0.06, 0.04, 0.7))


def contact_sheet(names, path, scale=1, columns=8):
    with multiprocessing.Pool() as pool:
        tiles = pool.map(render_icon, names)
    cell = max(t.shape[0] for t in tiles)
    rows = (len(tiles) + columns - 1) // columns
    pad = 8
    sheet = Image.new("RGBA", (columns * (cell + pad) + pad, rows * (cell + pad) + pad), (44, 34, 28, 255))
    for i, t in enumerate(tiles):
        im = Image.fromarray(np.round(t).astype(np.uint8), "RGBA")
        sheet.alpha_composite(im, (pad + (i % columns) * (cell + pad), pad + (i // columns) * (cell + pad)))
    if scale != 1:
        sheet = sheet.resize((sheet.width * scale, sheet.height * scale), Image.LANCZOS)
    sheet.save(path)


SWATCHES = ("GoldButton", "LavaButton", "RockButton", "WaterButton", "DirtButton", "ClaimedButton", "GemButton")
HEADER = ('<Imageset autoScaled="false" imagefile="ODIcons.png" name="OpenDungeonsIcons" '
          'nativeHorzRes="800" nativeVertRes="600" version="2">')
SMALL_ROW = ("GoldCoin", "TerritoryIcon", "ManaIcon", "OptionsIcon", "CreaturesIcon", "HelpIcon", "LoadIcon", "SaveIcon",
             "AbortIcon", "CheckIcon", "ObjectivesIcon", "SkillIcon", "SeatIcon", "CogIcon", "HourglassIcon", "HammerAnvilIcon")
SECOND_ROW = ("CameraIcon", "MenuReturn", "MapLightButton")
PLAY_ROW = ("PlayIcon",)   # after the seven terrain swatches of the second row


def read_swatches():
    """The terrain swatches are photographs and stay as they are: cut them from the current atlas."""
    text = open(IMAGESET, encoding="utf-8").read()
    atlas = Image.open(ATLAS).convert("RGBA")
    tiles = {}
    for name in SWATCHES:
        m = re.search(r'<Image height="(\d+)" name="%s" width="(\d+)" xPos="(\d+)" yPos="(\d+)"' % name, text)
        h, w, x, y = (int(m.group(i)) for i in (1, 2, 3, 4))
        tiles[name] = atlas.crop((x, y, x + w, y + h))
    return tiles


def build_atlas():
    swatches = read_swatches()
    names = list(fm.ICONS)
    with multiprocessing.Pool() as pool:
        tiles = dict(zip(names, pool.map(render_icon, names)))
    atlas = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    entries = []

    def put(name, image, x, y):
        atlas.paste(image, (x, y))
        entries.append((name, image.width, image.height, x, y))

    def to_image(name):
        return Image.fromarray(np.round(tiles[name]).astype(np.uint8), "RGBA")

    for i, name in enumerate(SMALL_ROW):
        put(name, to_image(name), 64 * i, 0)
    for i, name in enumerate(SECOND_ROW):
        put(name, to_image(name), 64 * i, 64)
    for i, name in enumerate(PLAY_ROW):
        put(name, to_image(name), 192 + 64 * len(SWATCHES) + 64 * i, 64)
    for i, name in enumerate(SWATCHES):
        put(name, swatches[name], 192 + 64 * i, 64)
    big = [n for n in names if fm.ICONS[n]["cells"] == 128]
    assert len(big) <= 56, len(big)
    for i, name in enumerate(big):
        put(name, to_image(name), 128 * (i % 8), 128 + 128 * (i // 8))
    atlas.save(ATLAS, optimize=True)
    lines = [HEADER]
    for name, w, h, x, y in entries:
        lines.append('    <Image height="%d" name="%s" width="%d" xPos="%d" yPos="%d" />' % (h, name, w, x, y))
    lines.append("</Imageset>")
    with open(IMAGESET, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote %d images to %s" % (len(entries), ATLAS))


def main():
    args = sys.argv[1:]
    if "--sheet" not in args:
        build_atlas()
        return
    names = list(fm.ICONS)
    if "--only" in args:
        names = []
        for a in args[args.index("--only") + 1:]:
            if a.startswith("--"):
                break
            names.append(a)
    out = args[args.index("--sheet") + 1] if "--sheet" in args else "."
    os.makedirs(out, exist_ok=True)
    scale = int(args[args.index("--scale") + 1]) if "--scale" in args else 1
    contact_sheet(names, os.path.join(out, "sheet.png"), scale=scale, columns=int(args[args.index("--columns") + 1]) if "--columns" in args else 6)


if __name__ == "__main__":
    main()
