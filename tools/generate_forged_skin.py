#!/usr/bin/env python3
"""Repaints the window skin atlas gui/ODSkin.png in the forged dungeon look.

The atlas keeps every image position of gui/ODSkin.imageset, so no look'n'feel entry has to move.
Every drawn rectangle is looked up by image name in the imageset and overwritten completely
(colour and alpha), which makes the script safe to run repeatedly. All colours and material
values live in the PALETTE and MATERIAL blocks below. Light comes from the top left, like the
procedural HUD textures in source/render/Gui.cpp.

Usage: python tools/generate_forged_skin.py [--preview DIRECTORY]
"""
import os
import re
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ATLAS = os.path.join(ROOT, "gui", "ODSkin.png")
IMAGESET = os.path.join(ROOT, "gui", "ODSkin.imageset")

# ---------------------------------------------------------------------------------------------
# PALETTE: the one place for colours (RGB 0..255). Keep in step with NAV_* in Gui.cpp.
# ---------------------------------------------------------------------------------------------
PALETTE = {
    "contour": (8, 5, 4),            # blackened outline around every part
    "iron": (58, 50, 44),            # plate face, before shading
    "iron_lit": (96, 84, 72),        # bevel facing the light
    "inset": (27, 20, 15),           # dark brown stone of wells, lists and window bodies
    "inset_lit": (44, 33, 24),
    "stone": (74, 56, 40),            # tile background; the category colour of a tab multiplies it
    "bronze_dark": (50, 26, 8),
    "bronze": (196, 136, 66),
    "bronze_bright": (226, 150, 66),
    "gold_dark": (70, 38, 10),
    "gold": (238, 190, 92),
    "gold_bright": (255, 226, 150),
    "ember_dark": (70, 22, 8),
    "ember": (214, 88, 22),
    "ember_bright": (255, 150, 52),
    "rivet": (200, 140, 72),
    "rivet_glint": (255, 236, 190),
}

# Material values shared by all parts
MATERIAL = {
    "grain": 5.0,        # amplitude of the fine iron grain
    "mottle": 6.0,       # amplitude of the large soft mottling
    "light": (-0.7071, -0.7071),
}


def col(name):
    return np.array(PALETTE[name], dtype=np.float32)


def lerp(a, b, t):
    return a + (b - a) * t


def fine_noise(h, w, seed):
    return np.random.RandomState(seed).uniform(-1.0, 1.0, (h, w)).astype(np.float32)


def soft_noise(h, w, seed, cell=6):
    gh, gw = h // cell + 3, w // cell + 3
    grid_ = np.random.RandomState(seed).uniform(-1.0, 1.0, (gh, gw)).astype(np.float32)
    ys = np.arange(h, dtype=np.float32) / cell
    xs = np.arange(w, dtype=np.float32) / cell
    y0, x0 = ys.astype(int), xs.astype(int)
    fy, fx = (ys - y0)[:, None], (xs - x0)[None, :]
    fy, fx = fy * fy * (3 - 2 * fy), fx * fx * (3 - 2 * fx)
    a = grid_[y0][:, x0]
    b = grid_[y0][:, x0 + 1]
    c = grid_[y0 + 1][:, x0]
    d = grid_[y0 + 1][:, x0 + 1]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def grid(h, w):
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    return xs + 0.5, ys + 0.5


def rounded_depth(w, h, r):
    """Distance from the border of a rounded rectangle, positive inside."""
    px, py = grid(h, w)
    qx = np.abs(px - w / 2.0) - (w / 2.0 - r)
    qy = np.abs(py - h / 2.0) - (h / 2.0 - r)
    outside = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0))
    inside = np.minimum(np.maximum(qx, qy), 0)
    return -(outside + inside - r)


def facing_of(depth):
    """Light on a bevel: 1 on edges facing the top left, -1 on edges facing away."""
    gy, gx = np.gradient(depth)
    length = np.maximum(np.hypot(gx, gy), 1e-4)
    nx, ny = -gx / length, -gy / length   # outward normal
    return nx * MATERIAL["light"][0] + ny * MATERIAL["light"][1]


def plate(w, h, r=2.0, rim=3.4, edge="bronze", face="iron", state="normal",
          seed=1, rivets=(), line=1.0):
    """Draws a forged plate: contour, metal edge line, bevel and a face.

    edge: bronze, gold, ember or none. face: iron, inset or ember.
    state: normal, hover, pressed, selected.
    """
    depth = rounded_depth(w, h, r)
    facing = facing_of(depth)
    if state == "pressed":
        facing = -facing
    out = np.zeros((h, w, 4), dtype=np.float32)
    mott = soft_noise(h, w, seed, 5) * MATERIAL["mottle"]
    grain = fine_noise(h, w, seed + 7) * MATERIAL["grain"]

    y = (np.arange(h, dtype=np.float32) + 0.5)[:, None] / h
    if face == "iron":
        img = col("iron")[None, None, :] * (1.16 - 0.34 * y[..., None])
    elif face == "inset":
        img = col("inset")[None, None, :] * (1.10 - 0.30 * y[..., None])
    else:
        t = np.clip(0.35 + 0.65 * y, 0, 1)[..., None] * (0.75 + 0.25 * soft_noise(h, w, seed + 3, 4)[..., None])
        img = lerp(col("ember_dark")[None, None, :], col("ember")[None, None, :], t * 0.78)
    img = img + (mott + grain)[..., None]
    if state == "hover" and face != "ember":
        img = img + np.clip((y - 0.25) / 0.75, 0, 1)[..., None] * (col("ember") * 0.34)[None, None, :]
    if state == "pressed":
        img = img * 0.62
    if state == "selected":
        img = img + np.clip((y - 0.15) / 0.85, 0, 1)[..., None] * (col("ember") * 0.42)[None, None, :]
    out[..., :3] = img

    line_end = 1.0 + line
    if rim > line_end:
        bevel = (depth >= line_end) & (depth < rim)
        across = np.clip((depth - line_end) / max(rim - line_end, 0.5), 0, 1)     # 0 at the edge, 1 at the face
        lit = 0.62 + facing * 0.44 * (1.0 - across) ** 0.8 - 0.05 * across
        bev = col("iron")[None, None, :] * lit[..., None] * 1.12
        bev = bev + (col("iron_lit") - col("iron"))[None, None, :] * (np.clip(facing, 0, 1) * (1.0 - across))[..., None] * 0.8
        out[..., :3] = np.where(bevel[..., None], bev + grain[..., None], out[..., :3])
    if face in ("inset", "ember"):
        inner = (depth >= rim) & (depth < rim + 1.3)
        lip = 0.55 - 0.5 * facing
        out[..., :3] = np.where(inner[..., None], out[..., :3] * lip[..., None] + 8, out[..., :3])
    if edge != "none":
        if state == "hover":
            dark, bright = col("gold_dark"), col("gold_bright")
        elif state == "selected":
            dark, bright = col("ember_dark"), col("ember_bright")
        elif edge == "gold":
            dark, bright = col("gold_dark"), col("gold")
        elif edge == "ember":
            dark, bright = col("ember_dark"), col("ember_bright")
        else:
            dark, bright = col("bronze_dark"), col("bronze")
        t = np.clip(0.42 + 0.58 * facing, 0, 1)[..., None]
        if state == "pressed":
            t = t * 0.7
        metal = lerp(dark[None, None, :], bright[None, None, :], t)
        edge_mask = (depth >= 1.0) & (depth < line_end)
        out[..., :3] = np.where(edge_mask[..., None], metal, out[..., :3])
    contour = depth < 1.0
    out[..., :3] = np.where(contour[..., None], lerp(out[..., :3], col("contour")[None, None, :], 0.86), out[..., :3])
    for (rx, ry, rr) in rivets:
        px, py = grid(h, w)
        dx, dy = px - rx, py - ry
        dist = np.hypot(dx, dy)
        z = np.sqrt(np.clip(1 - (dist / rr) ** 2, 0, 1))
        nxv, nyv = dx / rr, dy / rr
        diffuse = np.clip(-0.5 * nxv - 0.6 * nyv + 0.62 * z, 0, 1)
        halfway = np.clip(-0.26 * nxv - 0.31 * nyv + 0.91 * z, 0, 1)
        rv = col("rivet")[None, None, :] * (0.34 + 0.92 * diffuse)[..., None]
        rv = rv + col("rivet_glint")[None, None, :] * (0.85 * halfway ** 18)[..., None]
        out[..., :3] = np.where((dist < rr)[..., None], rv, out[..., :3])
        ring = ((dist >= rr) & (dist < rr + 0.8))[..., None]
        out[..., :3] = np.where(ring, out[..., :3] * 0.35, out[..., :3])
    out[..., 3] = np.clip(depth + 0.5, 0, 1)
    out[..., :3] = np.clip(out[..., :3], 0, 255)
    return out


def blend(img, colour, cover):
    colour = np.array(colour, dtype=np.float32)
    a = cover[..., None]
    rgb = colour[None, None, :] * np.ones_like(img[..., :3])
    img_a = img[..., 3:4]
    out_a = a + img_a * (1 - a)
    out_rgb = (rgb * a + img[..., :3] * img_a * (1 - a)) / np.maximum(out_a, 1e-4)
    result = img.copy()
    result[..., :3] = out_rgb
    result[..., 3:4] = out_a
    return result


def stroke(img, points, colour, width, soft=0.7):
    """Antialiased polyline stroke on top of an RGBA float image (straight alpha)."""
    h, w = img.shape[:2]
    px, py = grid(h, w)
    cover = np.zeros((h, w), dtype=np.float32)
    for (ax, ay), (bx, by) in zip(points[:-1], points[1:]):
        dx, dy = bx - ax, by - ay
        ln = max(dx * dx + dy * dy, 1e-6)
        t = np.clip(((px - ax) * dx + (py - ay) * dy) / ln, 0, 1)
        dist = np.hypot(px - (ax + t * dx), py - (ay + t * dy))
        cover = np.maximum(cover, np.clip((width / 2.0 + soft * 0.5 - dist) / soft, 0, 1))
    return blend(img, colour, cover)


def triangle(img, cx, cy, size, direction, colour):
    h, w = img.shape[:2]
    px, py = grid(h, w)
    dx, dy = px - cx, py - cy
    if direction == "up":
        u, v = dx, dy
    elif direction == "down":
        u, v = dx, -dy
    elif direction == "left":
        u, v = dy, dx
    else:
        u, v = dy, -dx
    inside = np.clip((size * 0.7 - v) / 0.8, 0, 1) * np.clip((v + size) / 0.8, 0, 1)
    inside = inside * np.clip(((v + size) * 0.95 - np.abs(u) + 0.4) / 0.8, 0, 1)
    return blend(img, colour, inside)


def gold_arrow(img, cx, cy, size, direction, bright=False):
    shadow = triangle(img, cx + 0.6, cy + 0.6, size, direction, (10, 6, 3))
    return triangle(shadow, cx, cy, size, direction, PALETTE["gold_bright"] if bright else PALETTE["gold"])


def to_uint8(img):
    return np.clip(np.round(img), 0, 255).astype(np.uint8)


def read_imageset():
    text = open(IMAGESET, encoding="utf-8").read()
    rects = {}
    for m in re.finditer(r'<Image height="(\d+)" name="([^"]+)" width="(\d+)" xPos="(\d+)" yPos="(\d+)"', text):
        rects[m.group(2)] = (int(m.group(4)), int(m.group(5)), int(m.group(3)), int(m.group(1)))
    return rects


RECTS = read_imageset()


def union(*names):
    x0 = min(RECTS[n][0] for n in names)
    y0 = min(RECTS[n][1] for n in names)
    x1 = max(RECTS[n][0] + RECTS[n][2] for n in names)
    y1 = max(RECTS[n][1] + RECTS[n][3] for n in names)
    return (x0, y0, x1 - x0, y1 - y0)


class Atlas:
    def __init__(self):
        self.img = np.array(Image.open(ATLAS).convert("RGBA"), dtype=np.float32)
        self.img[..., 3] /= 255.0   # alpha is kept in 0..1 while drawing
        self.log = []

    def put(self, rect, tile, label):
        x, y, w, h = rect
        assert tile.shape[1] == w and tile.shape[0] == h, (label, rect, tile.shape)
        self.img[y:y + h, x:x + w] = tile
        self.log.append((label, rect))

    def save(self):
        out = self.img.copy()
        # Pixels outside every named image are unused: clear them so no old artwork stays behind.
        used = np.zeros(out.shape[:2], dtype=bool)
        for (x, y, w, h) in RECTS.values():
            used[y:y + h, x:x + w] = True
        out[~used] = 0
        out[..., 3] *= 255.0
        Image.fromarray(to_uint8(out), "RGBA").save(ATLAS, optimize=True)


def sheet(rect_names, **kw):
    r = union(*rect_names)
    return r, plate(r[2], r[3], **kw)


def empty(r):
    return np.zeros((r[3], r[2], 4), dtype=np.float32)


def opaque(rgb):
    return np.concatenate([np.clip(rgb, 0, 255), np.ones(rgb.shape[:2] + (1,), dtype=np.float32)], axis=2)


def rivet(rgb, cx, cy, radius):
    """A domed bronze rivet with a dark ring, lit from the top left."""
    h, w = rgb.shape[:2]
    px, py = grid(h, w)
    dx, dy = px - cx, py - cy
    dist = np.hypot(dx, dy)
    z = np.sqrt(np.clip(1 - (dist / radius) ** 2, 0, 1))
    nxv, nyv = dx / radius, dy / radius
    diffuse = np.clip(-0.5 * nxv - 0.6 * nyv + 0.62 * z, 0, 1)
    halfway = np.clip(-0.26 * nxv - 0.31 * nyv + 0.91 * z, 0, 1)
    rv = col("rivet")[None, None, :] * (0.34 + 0.92 * diffuse)[..., None]
    rv = rv + col("rivet_glint")[None, None, :] * (0.85 * halfway ** 18)[..., None]
    rgb = np.where((dist < radius)[..., None], rv, rgb)
    ring = ((dist >= radius) & (dist < radius + 0.9))[..., None]
    return np.where(ring, rgb * 0.35, rgb)


def forged_bar(w, h, seed):
    """One segment of a forged iron bar: lit bronze edge, chamfers, an iron face with rivets and a glowing vent slot.

    The rows are laid out for h = 48 and scale with h. The left and right edge are the joints between segments.
    """
    px, py = grid(h, w)
    s = h / 48.0
    yy = py

    def band(a, b):
        return ((yy >= a * s) & (yy < b * s))[..., None]

    rgb = np.broadcast_to(col("iron")[None, None, :] * (1.16 - 0.42 * (yy / h))[..., None], (h, w, 3)).copy()
    rgb = np.where(band(0, 1.5), col("contour"), rgb)
    t = np.clip((yy - 1.5 * s) / (2.5 * s), 0, 1)[..., None]
    rgb = np.where(band(1.5, 4), lerp(col("bronze_bright"), col("bronze_dark"), t), rgb)
    t = np.clip((yy - 4 * s) / (6 * s), 0, 1)[..., None]
    rgb = np.where(band(4, 10), lerp(col("iron_lit") * 1.12, col("iron"), t), rgb)
    rgb = np.where(band(10, 11.5), col("contour") * 1.5, rgb)
    rgb = np.where(band(36.5, 38), col("contour") * 1.5, rgb)
    t = np.clip((yy - 38 * s) / (6 * s), 0, 1)[..., None]
    rgb = np.where(band(38, 44), lerp(col("iron") * 0.62, col("iron") * 0.34, t), rgb)
    rgb = np.where(band(44, 45.5), col("bronze_dark"), rgb)
    rgb = np.where(band(45.5, 48), col("contour"), rgb)
    streak = np.repeat(fine_noise(h, 1, seed + 2), w, axis=1)
    rgb = rgb + (soft_noise(h, w, seed, 6) * 5.0 + fine_noise(h, w, seed + 1) * 3.0 + streak * 3.5)[..., None]
    # vent slot in the middle of the face with a glow of embers
    cx, cy, hw, hh, rad = w / 2.0, 24.0 * s, 15.0, 4.6 * s, 2.4
    qx, qy = np.abs(px - cx) - (hw - rad), np.abs(py - cy) - (hh - rad)
    depth = -(np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - rad)
    inside = depth > 0.9
    glow = np.exp(-(((px - cx) / (hw * 0.75)) ** 2)) * np.clip(1.0 - np.abs(py - cy) / hh, 0, 1)
    slot = col("inset")[None, None, :] * 0.55 + col("ember")[None, None, :] * (0.55 * glow)[..., None]         + col("ember_bright")[None, None, :] * (0.25 * glow ** 3)[..., None]
    rgb = np.where(inside[..., None], slot, rgb)
    rim = (depth > 0.0) & (depth <= 0.9)
    lit = ((py > cy) | (px > cx + hw * 0.6))[..., None]
    rgb = np.where(rim[..., None], np.where(lit, col("bronze_dark") * 1.3, col("contour")), rgb)
    for rx in (7.5, w - 7.5):
        rgb = rivet(rgb, rx, 24.0 * s, 3.1 * s)
    rgb[:, 0, :] = lerp(rgb[:, 0, :], col("contour")[None, :], 0.85)
    rgb[:, 1, :] = np.where(((yy[:, 1] > 11.5 * s) & (yy[:, 1] < 36.5 * s))[:, None], rgb[:, 1, :] * 1.28, rgb[:, 1, :])
    rgb[:, -1, :] = lerp(rgb[:, -1, :], col("contour")[None, :], 0.85)
    return opaque(np.clip(rgb, 0, 255))


def paint(atlas):
    put = atlas.put
    # ---- Buttons (sheets of 40x16) ---------------------------------------------------------
    put(*sheet(("ButtonTopLeftNormal", "ButtonBottomRightNormal"), r=2.6, rim=3.6, edge="bronze", face="iron", seed=11), "button normal")
    put(*sheet(("ButtonTopLeftHighlight", "ButtonBottomRightHighlight"), r=2.6, rim=3.6, edge="bronze", face="ember", state="hover", seed=12), "button hover")
    put(*sheet(("ButtonTopLeftPushed", "ButtonBottomRightPushed"), r=2.6, rim=3.6, edge="bronze", face="iron", state="pressed", seed=13), "button pressed")
    put(*sheet(("EditBoxTopLeft", "EditBoxBottomRight"), r=2.4, rim=3.2, edge="bronze", face="inset", seed=14), "editbox")
    put(*sheet(("TooltipTopLeft", "TooltipBottomRight"), r=2.4, rim=3.2, edge="gold", face="inset", seed=15), "tooltip")
    # ---- Checkbox and radio ------------------------------------------------------------------
    for name, state, seed in (("Normal", "normal", 21), ("Hover", "hover", 22)):
        r = RECTS["Checkbox" + name]
        put(r, plate(r[2], r[3], r=2.4, rim=3.0, edge="bronze", face="inset", state=state, seed=seed), "checkbox " + name)
        r = RECTS["RadioButton" + name]
        put(r, plate(r[2], r[3], r=r[2] / 2.0 - 0.01, rim=2.6, edge="bronze", face="inset", state=state, seed=seed + 4), "radio " + name)
    r = RECTS["CheckboxMark"]
    t = empty(r)
    t = stroke(t, [(3.4, 7.4), (6.0, 10.2), (11.0, 3.6)], (10, 6, 3), 3.0)
    t = stroke(t, [(3.2, 7.0), (5.8, 9.8), (10.8, 3.3)], PALETTE["gold_bright"], 2.0)
    put(r, t, "checkbox mark")
    r = RECTS["RadioButtonMark"]
    t = empty(r)
    px, py = grid(r[3], r[2])
    d = np.hypot(px - r[2] / 2, py - r[3] / 2)
    t = blend(t, (10, 6, 3), np.clip(4.2 - d, 0, 1))
    t = blend(t, PALETTE["ember_bright"], np.clip(3.4 - d, 0, 1))
    t = blend(t, PALETTE["gold_bright"], np.clip(1.4 - np.hypot(px - r[2] / 2 + 0.9, py - r[3] / 2 + 0.9), 0, 1))
    put(r, t, "radio mark")
    # ---- Close button ------------------------------------------------------------------------
    for name, state, seed in (("Normal", "normal", 31), ("Hover", "hover", 32), ("Pressed", "pressed", 33)):
        r = RECTS["CloseButton" + name]
        t = plate(r[2], r[3], r=2.6, rim=3.2, edge="bronze", face="iron", state=state, seed=seed)
        bright = PALETTE["gold_bright"] if state == "hover" else PALETTE["gold"]
        o = 0.4 if state == "pressed" else 0.0
        for a, b in (((4.6, 4.6), (r[2] - 4.6, r[3] - 4.6)), ((r[2] - 4.6, 4.6), (4.6, r[3] - 4.6))):
            t = stroke(t, [(a[0] + o, a[1] + o), (b[0] + o, b[1] + o)], (10, 6, 3), 2.6)
        for a, b in (((4.4, 4.4), (r[2] - 4.4, r[3] - 4.4)), ((r[2] - 4.4, 4.4), (4.4, r[3] - 4.4))):
            t = stroke(t, [(a[0] + o, a[1] + o), (b[0] + o, b[1] + o)], bright, 1.7)
        put(r, t, "close " + name)
    # ---- Window frame, client brush, title bar -------------------------------------------------
    r = union("WindowTopLeft", "WindowBottomRight")
    w, h = r[2], r[3]
    t = plate(w, h, r=5.0, rim=8.0, edge="bronze", face="inset", seed=41, line=1.4,
              rivets=[(5.6, 5.6, 2.1), (w - 5.6, 5.6, 2.1), (5.6, h - 5.6, 2.1), (w - 5.6, h - 5.6, 2.1)])
    put(r, t, "window frame")
    r = RECTS["ClientBrush"]
    px, py = grid(r[3], r[2])
    body = col("inset")[None, None, :] * (1.06 - 0.16 * py[..., None] / r[3])
    body = body + (soft_noise(r[3], r[2], 43, 9) * 3.0 + fine_noise(r[3], r[2], 44) * 1.8)[..., None]
    put(r, opaque(body), "client brush")
    r = union("TitleBarLeft", "SysAreaRight")
    w, h = r[2], r[3]
    t = plate(w, h, r=6.0, rim=5.0, edge="bronze", face="iron", seed=45, line=1.3,
              rivets=[(6.6, h / 2.0, 1.9), (w - 6.6, h / 2.0, 1.9)])
    left_w, right_w = RECTS["TitleBarLeft"][2], RECTS["SysAreaRight"][2]
    mid_x = RECTS["TitleBarMiddle"][0] - r[0]
    t[:, left_w:w - right_w] = t[:, mid_x:mid_x + 1]
    put(r, t, "title bar")
    # ---- Tabs ------------------------------------------------------------------------------------
    put(*sheet(("TabButtonUpperLeftNormal", "TabButtonLowerRightNormal"), r=5.0, rim=3.6, edge="bronze", face="iron", seed=51), "tab normal")
    put(*sheet(("TabButtonUpperLeftSelected", "TabButtonLowerRightSelected"), r=5.0, rim=3.6, edge="bronze", face="ember", state="selected", seed=52), "tab selected")
    put(*sheet(("TabButtonUpperLeftHover", "TabButtonLowerRightHover"), r=5.0, rim=3.6, edge="gold", face="iron", state="hover", seed=53), "tab hover")
    put(*sheet(("TabContentPaneUpperLeft", "TabContentPaneLowerRight"), r=5.0, rim=3.6, edge="bronze", face="inset", seed=54), "tab pane")
    for name, state, arrow, seed in (("TabButtonScrollLeftNormal", "normal", "left", 55), ("TabButtonScrollRightNormal", "normal", "right", 56),
                                     ("TabButtonScrollLeftHover", "hover", "left", 57), ("TabButtonScrollRightHover", "hover", "right", 58)):
        r = RECTS[name]
        t = plate(r[2], r[3], r=3.0, rim=3.2, edge="bronze", face="iron", state=state, seed=seed)
        put(r, gold_arrow(t, r[2] / 2.0, r[3] / 2.0, 3.4, arrow, state == "hover"), name)
    # ---- Scrollbars ---------------------------------------------------------------------------------
    for name, arrow, state, seed in (("VertScrollUpNormal", "up", "normal", 61), ("VertScrollDownNormal", "down", "normal", 62),
                                     ("VertScrollUpHover", "up", "hover", 63), ("VertScrollDownHover", "down", "hover", 64),
                                     ("MiniHorzScrollLeftNormal", "left", "normal", 65), ("MiniHorzScrollRightNormal", "right", "normal", 66),
                                     ("MiniHorzScrollLeftHover", "left", "hover", 67), ("MiniHorzScrollRightHover", "right", "hover", 68)):
        r = RECTS[name]
        t = plate(r[2], r[3], r=3.0, rim=3.2, edge="bronze", face="iron", state=state, seed=seed)
        put(r, gold_arrow(t, r[2] / 2.0, r[3] / 2.0, 4.4, arrow, state == "hover"), name)
    for state, seed in (("Normal", 71), ("Hover", 72)):
        r = RECTS["MiniVertScrollThumb" + state]
        t = plate(r[2], r[3], r=3.4, rim=3.2, edge="bronze", face="iron", state=state.lower(), seed=seed)
        cy = r[3] / 2.0
        for dy in (-3.0, 0.0, 3.0):
            t = stroke(t, [(6.0, cy + dy), (r[2] - 6.0, cy + dy)], PALETTE["contour"], 1.0)
            t = stroke(t, [(6.0, cy + dy - 0.9), (r[2] - 6.0, cy + dy - 0.9)], PALETTE["bronze"] if state == "Normal" else PALETTE["gold"], 0.8)
        put(r, t, "vscroll thumb " + state)
        r = RECTS["MiniHorzScrollThumb" + state]
        t = plate(r[2], r[3], r=3.4, rim=3.2, edge="bronze", face="iron", state=state.lower(), seed=seed + 2)
        cx = r[2] / 2.0
        for dx in (-3.0, 0.0, 3.0):
            t = stroke(t, [(cx + dx, 6.0), (cx + dx, r[3] - 6.0)], PALETTE["contour"], 1.0)
            t = stroke(t, [(cx + dx - 0.9, 6.0), (cx + dx - 0.9, r[3] - 6.0)], PALETTE["bronze"] if state == "Normal" else PALETTE["gold"], 0.8)
        put(r, t, "hscroll thumb " + state)
    r = RECTS["MiniVertScrollBarSegment"]
    t = empty(r)
    t[..., :3] = np.array([[16, 11, 8], [26, 19, 14], [26, 19, 14], [40, 30, 22]], dtype=np.float32)[None, :r[2], :]
    t[..., 3] = 1.0
    put(r, t, "vscroll segment")
    r = RECTS["MiniHorzScrollBarSegment"]
    t = empty(r)
    t[..., :3] = np.array([[16, 11, 8], [26, 19, 14], [26, 19, 14], [26, 19, 14], [40, 30, 22]], dtype=np.float32)[:r[3], None, :]
    t[..., 3] = 1.0
    put(r, t, "hscroll segment")
    # ---- Sliders ---------------------------------------------------------------------------------------
    put(*sheet(("HorizontalSliderTopLeft", "HorizontalSliderBottomRight"), r=2.0, rim=2.6, edge="bronze", face="inset", seed=81, line=0.9), "hslider track")
    put(*sheet(("VerticalSliderTopLeft", "VerticalSliderBottomRight"), r=2.0, rim=2.6, edge="bronze", face="inset", seed=82, line=0.9), "vslider track")
    put(*sheet(("HorizontalSliderThumbNormalTopLeft", "HorizontalSliderThumbNormalBottomRight"), r=3.6, rim=3.4, edge="bronze", face="iron", seed=83), "hslider thumb")
    put(*sheet(("HorizontalSliderThumbHoverTopLeft", "HorizontalSliderThumbHoverBottomRight"), r=3.6, rim=3.4, edge="gold", face="iron", state="hover", seed=84), "hslider thumb hover")
    put(*sheet(("VerticalSliderThumbNormalLeft", "VerticalSliderThumbNormalRight"), r=3.6, rim=3.4, edge="bronze", face="iron", seed=85), "vslider thumb")
    put(*sheet(("VerticalSliderThumbHoverLeft", "VerticalSliderThumbHoverRight"), r=3.6, rim=3.4, edge="gold", face="iron", state="hover", seed=86), "vslider thumb hover")
    # ---- Progress bars ------------------------------------------------------------------------------------
    put(*sheet(("ProgressBarTopLeft", "ProgressBarBottomRight"), r=2.4, rim=3.2, edge="bronze", face="inset", seed=91, line=0.9), "progress frame")
    for name, lit, seed in (("ProgressBarDimSegment", False, 92), ("ProgressBarLitSegment", True, 93)):
        r = RECTS[name]
        y = (np.arange(r[3], dtype=np.float32) + 0.5)[:, None, None] / r[3]
        if lit:
            base = lerp(col("ember")[None, None, :], col("gold")[None, None, :], np.clip(1.4 - 1.4 * y, 0, 1)) * (0.75 + 0.25 * (1 - y))
        else:
            base = col("inset")[None, None, :] * (1.15 - 0.4 * y)
        base = np.broadcast_to(base, (r[3], r[2], 3)) + (fine_noise(r[3], r[2], seed) * 3)[..., None]
        put(r, opaque(base), name)
    put(*sheet(("AltProgressBarTopLeft", "AltProgressBarBottomRight"), r=5.0, rim=4.0, edge="bronze", face="inset", seed=94, line=1.1), "alt progress frame")
    r = RECTS["AltProgressBarLight"]
    px, py = grid(r[3], r[2])
    heat = np.clip(1.0 - (np.abs(py / r[3] - 0.5) * 2) ** 1.6, 0, 1)
    mixv = np.clip(heat * (0.55 + 0.6 * (0.5 + 0.5 * soft_noise(r[3], r[2], 95, 5))), 0, 1)[..., None]
    base = lerp(col("ember_dark")[None, None, :], col("ember_bright")[None, None, :], mixv)
    base = lerp(base, col("gold_bright")[None, None, :], np.clip(mixv - 0.72, 0, 1) * 2.4)
    put(r, opaque(base), "alt progress light")
    # ---- Frames of lists, menus, static areas ----------------------------------------------------------------
    put(*sheet(("StaticTopLeft", "StaticBottomRight"), r=1.6, rim=2.0, edge="bronze", face="inset", seed=101, line=0.9), "static frame")
    put(*sheet(("MenuTopLeft", "MenuBottomRight"), r=2.0, rim=3.0, edge="bronze", face="inset", seed=102, line=0.9), "menu frame")
    put(*sheet(("PopupMenuFrameTopLeft", "PopupMenuFrameBottomRight"), r=2.0, rim=3.0, edge="bronze", face="inset", seed=103, line=0.9), "popup frame")
    put(*sheet(("MultiListTopLeft", "MultiListBottomRight"), r=2.0, rim=3.0, edge="bronze", face="inset", seed=104, line=0.9), "multilist frame")
    put(*sheet(("MultiLineEditBoxTopLeft", "MultiLineEditBoxBottomRight"), r=2.0, rim=3.0, edge="bronze", face="inset", seed=105, line=0.9), "multiline editbox frame")
    for name, direction in (("PopupMenuArrowLeft", "left"), ("PopupMenuArrowRight", "right")):
        r = RECTS[name]
        put(r, gold_arrow(empty(r), r[2] / 2.0 + 0.6, r[3] / 2.0, 2.6, direction), name)
    for name, state, seed in (("HeaderBarBackdropNormal", "normal", 111), ("HeaderBarBackdropHover", "hover", 112)):
        r = RECTS[name]
        y = (np.arange(r[3], dtype=np.float32) + 0.5)[:, None, None] / r[3]
        base = col("iron")[None, None, :] * (1.18 - 0.5 * y)
        if state == "hover":
            base = base + col("ember")[None, None, :] * 0.32 * y
        base = np.broadcast_to(base, (r[3], r[2], 3)) + (fine_noise(r[3], r[2], seed) * 2.5)[..., None]
        t = opaque(base)
        t[0, :, :3] = col("bronze") if state == "normal" else col("gold")
        t[-1, :, :3] = col("contour")
        put(r, t, name)
    for name, state in (("HeaderBarSplitterNormal", "normal"), ("HeaderBarSplitterHover", "hover")):
        r = RECTS[name]
        t = empty(r)
        t[..., :3] = col("contour")
        t[..., 3] = 1.0
        t[:, 1, :3] = col("bronze") if state == "normal" else col("gold")
        put(r, t, name)
    for name, direction in (("HeaderBarSortUp", "up"), ("HeaderBarSortDown", "down")):
        r = RECTS[name]
        put(r, gold_arrow(empty(r), r[2] / 2.0, r[3] / 2.0, 2.6, direction), name)
    for name, direction, hover in (("SpinnerUpNormal", "up", False), ("SpinnerDownNormal", "down", False),
                                   ("SpinnerUpHover", "up", True), ("SpinnerDownHover", "down", True)):
        r = RECTS[name]
        put(r, gold_arrow(empty(r), r[2] / 2.0, r[3] / 2.0, 3.0, direction, hover), name)
    # ---- Combobox --------------------------------------------------------------------------------------------------
    put(*sheet(("ComboboxEditTopLeft", "ComboboxEditBottom"), r=2.4, rim=3.2, edge="bronze", face="inset", seed=121), "combobox edit")
    for name, first, last, state, seed in (("normal", "ComboboxListButtonNormalTopLeft", "ComboboxListButtonNormalBottomRight", "normal", 122),
                                           ("hover", "ComboboxListButtonHoverTopLeft", "ComboboxListButtonHoverBottomRight", "hover", 123)):
        r = union(first, last)
        t = plate(r[2], r[3], r=2.6, rim=3.2, edge="bronze", face="iron", state=state, seed=seed)
        put(r, gold_arrow(t, r[2] / 2.0 + 0.3, r[3] / 2.0 + 0.4, 3.6, "down", state == "hover"), "combobox button " + name)
    put(*sheet(("ComboboxListTopLeft", "ComboboxListBottomRight"), r=2.0, rim=3.0, edge="bronze", face="inset", seed=124), "combobox list")
    # ---- Bronze pipes: a lit cylinder with a soft hammered grain ---------------------------------------------------
    for name, seed in (("HorizontalPipe", 151), ("VerticalPipe", 152), ("VerticalPipeBig", 153)):
        r = RECTS[name]
        px, py = grid(r[3], r[2])
        across = (py / r[3]) if r[2] > r[3] else (px / r[2])       # 0..1 across the pipe
        angle = (across - 0.5) * 2.0
        nz = np.sqrt(np.clip(1 - angle ** 2, 0, 1))
        diffuse = np.clip(-0.7 * angle + 0.55 * nz, 0, 1)
        halfway = np.clip(-0.3 * angle + 0.9 * nz, 0, 1)
        tone = np.clip(0.18 + 0.95 * diffuse, 0, 1.1)[..., None]
        base = lerp(col("bronze_dark")[None, None, :], col("bronze")[None, None, :], np.clip(tone, 0, 1))
        base = base + col("gold_bright")[None, None, :] * (0.55 * halfway ** 14)[..., None]
        base = base + (soft_noise(r[3], r[2], seed, 3) * 4 + fine_noise(r[3], r[2], seed + 1) * 3)[..., None]
        base = np.where((np.abs(angle) > 0.86)[..., None], base * 0.45, base)
        put(r, opaque(base), "pipe " + name)
    # ---- Forged bar: the top strip and the message rail of the game screen (64 x 48, tiled sideways) --------------------
    r = RECTS["ForgedBar"]
    put(r, forged_bar(r[2], r[3], 161), "forged bar")
    # ---- Game button sheets (60x60) ----------------------------------------------------------------------------------
    r = RECTS["ButtonBackground"]
    px, py = grid(r[3], r[2])
    d = np.hypot(px - r[2] * 0.42, py - r[3] * 0.38) / (r[2] * 0.75)
    base = col("stone")[None, None, :] * (1.05 - 0.70 * np.clip(d, 0, 1) ** 0.9)[..., None]
    base = base + (soft_noise(r[3], r[2], 131, 7) * 3.0 + fine_noise(r[3], r[2], 132) * 2.0)[..., None]
    put(r, opaque(base), "button background")
    r = RECTS["ButtonHoverEffect"]
    px, py = grid(r[3], r[2])
    edge_d = np.minimum(np.minimum(px, r[2] - px), np.minimum(py, r[3] - py))
    t = empty(r)
    t[..., :3] = col("ember_bright")
    t[..., 3] = np.clip(np.exp(-edge_d / 6.5) * 0.9, 0, 1)
    put(r, t, "button hover effect")
    r = RECTS["ButtonPushedEffect"]
    px, py = grid(r[3], r[2])
    d = np.hypot(px - r[2] / 2, py - r[3] / 2) / (r[2] * 0.62)
    t = empty(r)
    t[..., :3] = lerp(col("gold")[None, None, :], col("ember")[None, None, :], np.clip(d, 0, 1)[..., None])
    t[..., 3] = np.clip(0.75 * (1 - np.clip(d, 0, 1)) ** 1.3, 0, 0.75)
    put(r, t, "button pushed effect")
    r = RECTS["ButtonDisabledEffect"]
    t = empty(r)
    t[..., :3] = (6, 4, 3)
    t[..., 3] = 0.58
    put(r, t, "button disabled effect")
    r = RECTS["ButtonFrame"]
    t = plate(r[2], r[3], r=3.2, rim=4.0, edge="bronze", face="iron", seed=141, line=1.2)
    depth = rounded_depth(r[2], r[3], 3.2)
    t[depth >= 4.0, 3] = 0.0
    inner = (depth >= 4.0) & (depth < 5.3)
    t[inner, :3] = col("contour")
    t[inner, 3] = 0.55 * np.clip(1.4 - (depth[inner] - 4.0) / 1.3, 0, 1)
    put(r, t, "button frame")


MENU_BUTTONS = os.path.join(ROOT, "gui", "ODMainMenuButtons.png")


def paint_menu_buttons():
    """The 128x384 sheet of the menu buttons: normal, hover and pressed plates, 128x128 each."""
    out = np.zeros((384, 128, 4), dtype=np.float32)
    rivets = [(11, 11, 3.0), (117, 11, 3.0), (11, 117, 3.0), (117, 117, 3.0)]
    states = (("normal", "iron", 201), ("hover", "ember", 202), ("pressed", "iron", 203))
    for index, (state, face, seed) in enumerate(states):
        out[index * 128:(index + 1) * 128] = plate(128, 128, r=9.0, rim=10.0, edge="bronze", face=face,
                                                      state=state if state != "hover" else "hover", seed=seed,
                                                      line=1.8, rivets=rivets)
    out[..., 3] *= 255.0
    Image.fromarray(to_uint8(out), "RGBA").save(MENU_BUTTONS, optimize=True)


def main():
    preview = sys.argv[sys.argv.index("--preview") + 1] if "--preview" in sys.argv else None
    atlas = Atlas()
    paint(atlas)
    atlas.save()
    paint_menu_buttons()
    print("repainted %d images in %s" % (len(atlas.log), ATLAS))
    if preview:
        os.makedirs(preview, exist_ok=True)
        image = Image.open(ATLAS).convert("RGBA")
        backdrop = Image.new("RGBA", image.size, (72, 60, 96, 255))
        backdrop.alpha_composite(image)
        backdrop.resize((image.width * 3, image.height * 3), Image.NEAREST).save(os.path.join(preview, "skin-atlas.png"))


if __name__ == "__main__":
    main()
