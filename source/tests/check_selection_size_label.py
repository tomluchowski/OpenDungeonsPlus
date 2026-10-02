"""Static checks of the hand selection size label (width x height of the dragged area).

Run from any directory. It does not start a game.
"""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
mode = (root / "source/modes/GameMode.cpp").read_text(encoding="utf-8")
header = (root / "source/modes/GameMode.h").read_text(encoding="utf-8")
failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


match = re.search(r"void GameMode::refreshSelectionSizeLabel\(\)\n\{.*?\n\}\n", mode, re.S)
check(match is not None, "refreshSelectionSizeLabel is missing")
body = match.group(0) if match else ""

check("mSelectionSizeLabel" in header and "void refreshSelectionSizeLabel();" in header,
      "label member or declaration missing in GameMode.h")
check(re.search(r"refreshHeldCreatureIcons\(\);\s*refreshSelectionSizeLabel\(\);", mode) is not None,
      "label is not refreshed every frame after the held creature icons")
check("destroyWindow(mSelectionSizeLabel)" in mode, "label window is never destroyed")

# Size as the dragged rectangle in tiles, from the drag start to the hand tile
check("std::abs(inputManager.mXPos - inputManager.mLStartDragX) + 1" in body, "width not computed from drag")
check("std::abs(inputManager.mYPos - inputManager.mLStartDragY) + 1" in body, "height not computed from drag")
check('std::to_string(width) + "x" + std::to_string(height)' in body, "text is not widthxheight")

# Shown only during an active drag, never for a single tile
check("inputManager.mLMouseDown" in body, "label must depend on the left button being held")
check("width * height > 1" in body, "single tile must not show a label")
check("numObjectsInHand() == 0" in body, "label must not show while holding creatures")
check("setVisible(false)" in body, "label is never hidden")
check("SelectedAction::castSpell" not in body and "SelectedAction::queryEntity" not in body,
      "spell and query actions must not show the label")

# Existing GUI toolkit window, follows the pointer, does not take mouse input
check('createWindow("OD/StaticText", "SelectionSizeLabel")' in body, "label must be an OD/StaticText window")
check("getMouseCursor().getPosition()" in body, "label does not follow the pointer")
check("setMousePassThroughEnabled(true)" in body, "label must let the mouse through")

if failures:
    print("FAIL")
    for failure in failures:
        print(" - " + failure)
    sys.exit(1)
print("OK")
