#!/usr/bin/env python3
"""Check campaign preview assets without compiling or starting the game.

Run with --self-test to exercise malformed assets entirely in memory.
"""
import argparse
from io import BytesIO
from pathlib import Path
import struct
import sys
import re
import xml.etree.ElementTree as ET
import zlib

from PIL import Image

EXPECTED_LEVELS = 31
PREVIEW_WIDTH = 640
PREVIEW_HEIGHT = 360
MAX_PREVIEW_BYTES = 400 * 1024
PNG_SIGNATURE = bytes((137, 80, 78, 71, 13, 10, 26, 10))
LAYOUT_TOLERANCE = 0.01


# Read level fields while ignoring commented section markers and retaining empty levels.
def read_levels(definition):
    levels = []
    definition = "\n".join(line for line in definition.splitlines() if not line.lstrip().startswith("#"))
    for section in definition.split("[Level]")[1:]:
        fields = {}
        for line in section.splitlines():
            line = line.strip()
            if line.startswith("[/Level]"):
                break
            if "=" in line and not line.startswith("#"):
                key, value = line.split("=", 1)
                fields[key.strip()] = value.strip()
        levels.append(fields)
    return levels


# Reject non-PNG, damaged, oversized or incorrectly sized previews, including undecodable pixels.
def png_errors(data):
    errors = []
    if len(data) > MAX_PREVIEW_BYTES:
        errors.append("exceeds 400 KB")
    if not data.startswith(PNG_SIGNATURE):
        return errors + ["not a PNG"]
    position = len(PNG_SIGNATURE)
    chunk_types = []
    compressed = bytearray()
    while position < len(data):
        if position + 12 > len(data):
            return errors + ["truncated PNG chunk"]
        length = struct.unpack_from(">I", data, position)[0]
        end = position + 12 + length
        if end > len(data):
            return errors + ["truncated PNG chunk"]
        kind = data[position + 4:position + 8]
        payload = data[position + 8:end - 4]
        crc = struct.unpack_from(">I", data, end - 4)[0]
        if zlib.crc32(kind + payload) & 0xffffffff != crc:
            errors.append("invalid PNG checksum")
        if not chunk_types and kind != b"IHDR":
            errors.append("PNG does not start with IHDR")
        if kind == b"IHDR":
            if length != 13:
                errors.append("invalid PNG header")
            elif struct.unpack_from(">II", payload) != (PREVIEW_WIDTH, PREVIEW_HEIGHT):
                errors.append("must be 640 x 360")
        if kind == b"IDAT":
            compressed.extend(payload)
        chunk_types.append(kind)
        position = end
        if kind == b"IEND":
            if length or position != len(data):
                errors.append("invalid PNG ending")
            break
    if chunk_types.count(b"IHDR") != 1 or not chunk_types or chunk_types[-1] != b"IEND":
        errors.append("missing PNG header or ending")
    if not compressed:
        errors.append("missing PNG image data")
    else:
        try:
            with Image.open(BytesIO(data)) as image:
                image.verify()
            with Image.open(BytesIO(data)) as image:
                image.load()
        except (OSError, SyntaxError, ValueError):
            errors.append("invalid PNG image data")
    return errors


# Resolve a layout rectangle using the same offset scaling as the GUI.
def layout_rectangle(window, width, height, scale):
    area = next(prop.get("value") for prop in window.findall("Property") if prop.get("name") == "Area")
    dimensions = re.findall(r"\{([^,{}]+),([^{}]+)\}", area)
    return [float(relative) * parent + float(offset) * scale
            for (relative, offset), parent in zip(dimensions, (width, height, width, height))]


# Catch the mixed scaled/unscaled runtime areas and overlapping briefing content.
def briefing_layout_errors(layout, mode, gui):
    errors = []
    names = ("BriefingPanel", "BriefingImage", "BriefingText", "BriefingStartButton", "BriefingCloseButton")
    windows = {window.get("name"): window for window in layout.iter("Window")}
    if any(name not in windows for name in names):
        return ["missing briefing layout window"]
    properties = {prop.get("name"): prop.get("value") for prop in windows["BriefingImage"].findall("Property")}
    if properties.get("AspectMode") != "Shrink" or abs(float(properties.get("AspectRatio", "0"))
                                                      - PREVIEW_WIDTH / PREVIEW_HEIGHT) > LAYOUT_TOLERANCE:
        errors.append("briefing preview must preserve its 16:9 aspect ratio")
    helper = mode.split("void MenuModeCampaign::setBriefingPreviewVisible", 1)[-1].split(
        "void MenuModeCampaign::hideBriefing", 1)[0]
    if helper.count("gui.setScaledArea(") != 4 or 'setProperty("Area"' in helper:
        errors.append("briefing areas must retain registered GUI scaling in both display modes")
    if "void Gui::setScaledArea(" not in gui or "it->second.area = designArea;" not in gui:
        errors.append("GUI must retain updated design areas when the resolution changes")
    design_width = float(re.search(r"LAYOUT_DESIGN_WIDTH\s*=\s*([\d.]+)f", gui).group(1))
    design_height = float(re.search(r"LAYOUT_DESIGN_HEIGHT\s*=\s*([\d.]+)f", gui).group(1))
    for width, height in ((1280, 720), (1920, 1080)):
        scale = min(width / design_width, height / design_height)
        panel = layout_rectangle(windows["BriefingPanel"], width, height, scale)
        panel_width, panel_height = panel[2] - panel[0], panel[3] - panel[1]
        if panel[0] < 0 or panel[1] < 0 or panel[2] > width or panel[3] > height:
            errors.append(str(width) + "x" + str(height) + ": briefing panel outside viewport")
        rectangles = {name: layout_rectangle(windows[name], panel_width, panel_height, scale)
                      for name in names[1:]}
        image = rectangles["BriefingImage"]
        text = rectangles["BriefingText"]
        buttons = (rectangles["BriefingStartButton"], rectangles["BriefingCloseButton"])
        if image[3] > text[1] or text[3] > min(button[1] for button in buttons):
            errors.append(str(width) + "x" + str(height) + ": briefing content overlaps")
        if abs((image[2] - image[0]) / (image[3] - image[1])
               - PREVIEW_WIDTH / PREVIEW_HEIGHT) > LAYOUT_TOLERANCE:
            errors.append(str(width) + "x" + str(height) + ": preview area is not 16:9")
        for rectangle in rectangles.values():
            if (rectangle[0] < 0 or rectangle[1] < 0 or rectangle[2] > panel_width
                    or rectangle[3] > panel_height):
                errors.append(str(width) + "x" + str(height) + ": briefing child outside panel")
    return errors


# Validate configured assets and the aspect-safe, non-overlapping briefing layout.
def check_repository(root):
    failures = []
    levels = read_levels((root / "levels/campaign/Campaign.cfg").read_text(encoding="utf-8"))
    if len(levels) != EXPECTED_LEVELS:
        failures.append("campaign must contain 31 levels")
    art = (root / "gui/campaign").resolve()
    for index, fields in enumerate(levels, 1):
        province = fields.get("Province", "level " + str(index))
        preview = fields.get("Preview", "")
        if not preview:
            failures.append(province + ": missing Preview")
            continue
        image = (art / preview).resolve()
        if not image.is_relative_to(art):
            failures.append(province + ": Preview must stay inside gui/campaign")
        elif not image.is_file():
            failures.append(province + ": missing image " + preview)
        else:
            failures.extend(province + ": " + error for error in png_errors(image.read_bytes()))
        if not (root / "materials/campaign-previews" / (province + ".md")).is_file():
            failures.append(province + ": missing image specification")
    layout = ET.fromstring((root / "gui/MenuCampaign.layout").read_text(encoding="utf-8"))
    if not any(window.get("name") == "BriefingImage" and window.get("type") == "OD/StaticImage"
               for window in layout.iter("Window")):
        failures.append("layout is missing the OD/StaticImage BriefingImage window")
    failures.extend(briefing_layout_errors(layout,
                    (root / "source/modes/MenuModeCampaign.cpp").read_text(encoding="utf-8"),
                    (root / "source/render/Gui.cpp").read_text(encoding="utf-8")))
    return failures


# Build a PNG chunk only for transient, in-memory checker regression tests.
def test_chunk(kind, payload):
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff)


# Check asset failures, level parsing and the original briefing scaling regression.
def self_test():
    header = struct.pack(">IIBBBBB", PREVIEW_WIDTH, PREVIEW_HEIGHT, 8, 2, 0, 0, 0)
    pixels = (bytes(1 + PREVIEW_WIDTH * 3)) * PREVIEW_HEIGHT
    png = PNG_SIGNATURE + test_chunk(b"IHDR", header) + test_chunk(b"IDAT", zlib.compress(pixels)) + test_chunk(b"IEND", b"")
    assert not png_errors(png), "valid PNG rejected"
    assert "not a PNG" in png_errors(b"not an image")
    assert "truncated PNG chunk" in png_errors(png[:-1])
    wrong_header = struct.pack(">IIBBBBB", PREVIEW_WIDTH - 1, PREVIEW_HEIGHT, 8, 2, 0, 0, 0)
    wrong_size = PNG_SIGNATURE + test_chunk(b"IHDR", wrong_header) + png[len(PNG_SIGNATURE) + 25:]
    assert "must be 640 x 360" in png_errors(wrong_size)
    assert "exceeds 400 KB" in png_errors(png + bytes(MAX_PREVIEW_BYTES))
    damaged = bytearray(png)
    damaged[len(PNG_SIGNATURE) + 8] ^= 1
    assert "invalid PNG checksum" in png_errors(damaged)
    bad_pixels = PNG_SIGNATURE + test_chunk(b"IHDR", header) + test_chunk(b"IDAT", b"broken") + test_chunk(b"IEND", b"")
    assert "invalid PNG image data" in png_errors(bad_pixels)
    short_pixels = PNG_SIGNATURE + test_chunk(b"IHDR", header) + test_chunk(b"IDAT", zlib.compress(b"x")) + test_chunk(b"IEND", b"")
    assert "invalid PNG image data" in png_errors(short_pixels)
    assert read_levels("[Level]\nProvince=T01\nPreview=previews/T01.png\n[/Level]\n") == [{"Province": "T01", "Preview": "previews/T01.png"}]
    assert read_levels("# [Level] starts each campaign entry.\n[Level]\nProvince=T01\nPreview=previews/T01.png\n[/Level]\n") == [{"Province": "T01", "Preview": "previews/T01.png"}]
    assert read_levels("[Level]\n[/Level]\n") == [{}], "empty real level hidden"
    root = Path(__file__).resolve().parents[2]
    layout = ET.fromstring((root / "gui/MenuCampaign.layout").read_text(encoding="utf-8"))
    mode = (root / "source/modes/MenuModeCampaign.cpp").read_text(encoding="utf-8")
    gui = (root / "source/render/Gui.cpp").read_text(encoding="utf-8")
    assert not briefing_layout_errors(layout, mode, gui), "current briefing geometry rejected"
    assert briefing_layout_errors(layout, mode.replace("gui.setScaledArea(", "window.setProperty("), gui), \
        "unscaled runtime areas accepted"
    for window in layout.iter("Window"):
        if window.get("name") == "BriefingImage":
            for prop in window.findall("Property"):
                if prop.get("name") == "Area":
                    prop.set("value", "{{0,16},{0,16},{1,-16},{0,600}}")
    assert briefing_layout_errors(layout, mode, gui), "overlapping, distorted preview accepted"
    print("campaign preview checker self-tests passed")


# Select in-memory tests or validate the current checkout with a failing exit on omissions.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    try:
        failures = check_repository(Path(__file__).resolve().parents[2])
    except (OSError, ET.ParseError) as error:
        print(str(error))
        return 1
    if failures:
        print("\n".join(failures))
        return 1
    print("campaign preview checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
