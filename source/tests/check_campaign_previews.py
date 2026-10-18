#!/usr/bin/env python3
"""Check campaign preview assets without compiling or starting the game.

Run with --self-test to exercise malformed assets entirely in memory.
"""
import argparse
from io import BytesIO
from pathlib import Path
import re
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


# Resolve a design rectangle after the same offset scaling used by Gui.
def scaled_rectangle(values, parent, scale):
    return tuple(values[index * 2] * parent[index % 2] + values[index * 2 + 1] * scale
                 for index in range(4))


# Validate image, story and goals against actual layout and registered runtime areas.
def briefing_layout_errors(layout, mode, gui):
    errors = []
    windows = {window.get("name"): window for window in layout.iter("Window")}
    required = ("BriefingPanel", "BriefingImage", "BriefingText", "BriefingGoals",
                "BriefingStartButton", "BriefingCloseButton")
    if any(name not in windows for name in required):
        return ["briefing is missing an image, story, goals or button window"]
    properties = {name: {prop.get("name"): prop.get("value") for prop in windows[name].findall("Property")}
                  for name in required}
    if properties["BriefingImage"].get("AspectMode") != "Shrink" or abs(
            float(properties["BriefingImage"].get("AspectRatio", "0")) - 16 / 9) > 0.000001:
        errors.append("briefing preview must preserve its 16:9 aspect ratio")
    if properties["BriefingText"].get("VertScrollbar") != "True":
        errors.append("briefing story must remain scrollable")
    if properties["BriefingGoals"].get("VertScrollbar") != "False":
        errors.append("briefing goals must fit without scrolling")
    helper = mode.split("void MenuModeCampaign::setBriefingPreviewVisible", 1)[-1].split(
        "void MenuModeCampaign::hideBriefing", 1)[0]
    if helper.count("gui.setScaledArea(") != 4 or 'setProperty("Area"' in helper:
        errors.append("briefing runtime areas must use registered GUI scaling")
    if "void Gui::setScaledArea" not in gui or "it->second.area = designArea" not in gui:
        errors.append("briefing scaling must retain registered design areas")
    if any(reference not in mode for reference in ("Goals::loadGoalFromStream", "goal->getDescription(player)",
                                                    "script.importFromStream(level)", "LevelScriptActionType::objective",
                                                     'descriptions += "\\n- " + action.mText')):
        errors.append("briefing must read actual goal descriptions and parsed script objectives")
    number = r"-?\d+(?:\.\d+)?"
    areas = {name: [float(value) for value in re.findall(number, properties[name]["Area"])] for name in required}
    runtime = {}
    for name in ("BRIEFING_PREVIEW_PANEL_AREA", "BRIEFING_TEXT_PANEL_AREA", "BRIEFING_PREVIEW_STORY_AREA",
                 "BRIEFING_PREVIEW_GOALS_AREA", "BRIEFING_STORY_AREA", "BRIEFING_GOALS_AREA", "BRIEFING_TEXT_AREA"):
        match = re.search(r"CEGUI::URect " + name + r"\((.*?)\);", mode, re.S)
        if match is None:
            return errors + ["briefing is missing registered design area " + name]
        runtime[name] = [float(value) for value in re.findall(number, match[1])]
        if len(runtime[name]) != 8:
            return errors + ["briefing design area cannot be read: " + name]
    for width, height in ((1280, 720), (1920, 1080)):
        design_width = float(re.search(r"LAYOUT_DESIGN_WIDTH = ([0-9.]+)f", gui)[1])
        design_height = float(re.search(r"LAYOUT_DESIGN_HEIGHT = ([0-9.]+)f", gui)[1])
        scale = min(width / design_width, height / design_height)
        for preview in (True, False):
            panel = scaled_rectangle(runtime["BRIEFING_PREVIEW_PANEL_AREA" if preview else "BRIEFING_TEXT_PANEL_AREA"],
                                     (width, height), scale)
            size = (panel[2] - panel[0], panel[3] - panel[1])
            story = scaled_rectangle(runtime["BRIEFING_PREVIEW_STORY_AREA" if preview else "BRIEFING_STORY_AREA"], size, scale)
            goals = scaled_rectangle(runtime["BRIEFING_PREVIEW_GOALS_AREA" if preview else "BRIEFING_GOALS_AREA"], size, scale)
            buttons = [scaled_rectangle(areas[name], size, scale) for name in required[-2:]]
            rectangles = [story, goals] + buttons
            invalid = not (0 <= panel[0] < panel[2] <= width and 0 <= panel[1] < panel[3] <= height)
            invalid |= story[2] >= goals[0] or max(story[3], goals[3]) >= min(button[1] for button in buttons)
            if preview:
                image = scaled_rectangle(areas["BriefingImage"], size, scale)
                rectangles.append(image)
                invalid |= image[3] >= min(story[1], goals[1])
                invalid |= abs((image[2] - image[0]) / (image[3] - image[1]) - 16 / 9) > 0.000001
                invalid |= any(areas[xml_name] != runtime[cpp_name] for xml_name, cpp_name in (
                    ("BriefingPanel", "BRIEFING_PREVIEW_PANEL_AREA"),
                    ("BriefingText", "BRIEFING_PREVIEW_STORY_AREA"), ("BriefingGoals", "BRIEFING_PREVIEW_GOALS_AREA")))
            invalid |= any(not (0 <= rect[0] < rect[2] <= size[0] and 0 <= rect[1] < rect[3] <= size[1]) for rect in rectangles)
            if invalid:
                errors.append(f"briefing image/story/goals/buttons overlap or exceed bounds at {width}x{height} (preview={preview})")
    return errors


# Measure every campaign goal with the shipped font at the supported minimum and full-HD sizes.
def compact_goal_fit_errors(root):
    from PIL import ImageFont
    import math
    errors = []
    layout = ET.fromstring((root / "gui/MenuCampaign.layout").read_text(encoding="utf-8"))
    hud = ET.fromstring((root / "gui/WindowObjectives.layout").read_text(encoding="utf-8"))
    properties = {}
    for tree, name in ((layout, "BriefingGoals"), (hud, "ObjectivesText")):
        window = tree.find(".//Window[@name='%s']" % name)
        properties[name] = {p.get("name"): p.get("value") for p in window.findall("Property")}
        if properties[name].get("VertScrollbar") != "False":
            errors.append(name + " must not require scrolling")
    mode = (root / "source/modes/MenuModeCampaign.cpp").read_text(encoding="utf-8")
    rectangle = re.search(r"CEGUI::URect BRIEFING_PREVIEW_GOALS_AREA\((.*?)\);", mode, re.S)
    area = [float(n) for n in re.findall(r"-?\d+(?:\.\d+)?", rectangle[1])]
    descriptions = {}
    for kind in ("KillAllEnemies", "ProtectDungeonTemple"):
        text = (root / ("source/goals/Goal" + kind + ".cpp")).read_text(encoding="utf-8")
        descriptions[kind] = re.search(r"getDescription\([^)]*\)\s*\{\s*return \"([^\"]+)\";", text)[1]
    for entry in read_levels((root / "levels/campaign/Campaign.cfg").read_text(encoding="utf-8")):
        text = (root / "levels" / entry["File"]).read_text(encoding="utf-8")
        classic = [descriptions[row.split()[0]] for row in text.split("[Goals]", 1)[1].split("[/Goals]", 1)[0].splitlines() if row.strip() and not row.lstrip().startswith("#")]
        objectives = [row.split(None, 4)[4] for row in text.splitlines() if row.startswith("Action\tobjectiveid\t")]
        hints = [row.split(None, 3)[3] for row in text.splitlines() if row.startswith("Action\thint\t")]
        if any(len(label) > 60 for label in objectives + hints):
            errors.append(entry["File"] + ": objective is longer than 60 characters")
        for width, height in ((1280, 720), (1920, 1080)):
            scale = min(width / 1024, height / 768)
            font_scale = min(width / 800, height / 600)
            goals_rect = scaled_rectangle(area, (672 * scale, 704 * scale), scale)
            for name, lines, available in (
                ("BriefingGoals", ["Goals:"] + ["- " + t for t in classic + objectives] + ["Hint: " + t for t in hints],
                 (goals_rect[2] - goals_rect[0] - 12 * scale, goals_rect[3] - goals_rect[1] - 12 * scale)),
                ("ObjectivesText", ["Victory!"] + ["Failed: " + t for t in classic + objectives] + ["Hint: " + t for t in hints],
                 (548 * scale, 370 * scale))):
                font_name = properties[name]["Font"]
                definition = ET.parse(root / ("gui/fonts/" + font_name + ".font")).getroot()
                font = ImageFont.truetype(str(root / "gui/fonts" / definition.get("filename")), math.ceil(float(definition.get("size")) * 96 / 72 * font_scale))
                rows = 0
                for line in lines:
                    current = ""
                    for word in line.split():
                        candidate = (current + " " + word).strip()
                        if font.getlength(candidate) > available[0] and current:
                            rows += 1
                            current = word
                        else:
                            current = candidate
                    rows += 1
                if rows * sum(font.getmetrics()) > available[1]:
                    errors.append("%s: %s overflows at %dx%d (%d rows)" % (entry["File"], name, width, height, rows))
    return errors


# Validate every configured preview and the briefing image window.
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
    failures.extend(compact_goal_fit_errors(root))
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
    layout_text = (root / "gui/MenuCampaign.layout").read_text(encoding="utf-8")
    mode = (root / "source/modes/MenuModeCampaign.cpp").read_text(encoding="utf-8")
    gui = (root / "source/render/Gui.cpp").read_text(encoding="utf-8")
    assert not briefing_layout_errors(ET.fromstring(layout_text), mode, gui)
    overlap = layout_text.replace("{{0.4,8},{0,392},{1,-16},{1,-72}}", "{{0,16},{0,392},{1,-16},{1,-72}}")
    assert briefing_layout_errors(ET.fromstring(overlap), mode, gui), "overlapping goals accepted"
    distortion = layout_text.replace('name="AspectRatio" value="1.77777778"', 'name="AspectRatio" value="1"')
    assert briefing_layout_errors(ET.fromstring(distortion), mode, gui), "distorted image accepted"
    missing_objectives = mode.replace("LevelScriptActionType::objective", "LevelScriptActionType::message")
    assert briefing_layout_errors(ET.fromstring(layout_text), missing_objectives, gui), "missing script objectives accepted"
    unscaled = mode.replace("gui.setScaledArea(", "window->setArea(")
    assert briefing_layout_errors(ET.fromstring(layout_text), unscaled, gui), "unscaled runtime area accepted"
    runtime_overlap = mode.replace("CEGUI::UDim(0.4f, -8)", "CEGUI::UDim(1, -16)")
    assert briefing_layout_errors(ET.fromstring(layout_text), runtime_overlap, gui), "overlapping runtime story accepted"
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
