#!/usr/bin/env python3
"""Separation check of the creature pictures (pure source scan, no game and no build needed).

Two rules:

1. The Dungeonbook (profile page, creature card, social window) shows the composed picture of the
   appearance. The preview portraits with costume may be used there only as the fallback, that is
   inside the else branch of the "no composed picture" test in Creature::fillProfilePage.
2. The creature bar (CreaturePanel, hand icons) and the preview portrait code never use the folders
   with the parts and bases (variants, neutral-bases) nor any code of the composed picture.

Usage: python scripts/check_dungeonbook_separation.py
Exit code 0 if both rules hold, 1 otherwise (every violation is printed).
"""

from pathlib import Path
import re
import sys

REPO = Path(__file__).resolve().parents[1]
SOURCE = REPO / 'source'

# Functions that give the preview portraits (creature bar and tinted profile portrait)
PREVIEW_CALLS = ('getCreatureProfilePortraitImage', 'getCreaturePanelPortraitImage', 'getCreatureHandIconImage')
# Code and data of the composed Dungeonbook picture
APPEARANCE_NAMES = ('getCreatureAppearanceImage', 'getCreatureAppearanceRemarks', 'getClientPortraitManifest',
                    'CreatureAppearancePicture', 'AppearanceCompose', 'PortraitManifest', 'DungeonbookQuirks',
                    'DungeonbookAppearanceConfig', 'CreatureAppearance')
FOLDER_NAMES = ('variants', 'neutral-bases')

errors = []


def read(path):
    return path.read_text(encoding='utf-8', errors='replace')


def body(text, signature):
    """Text of the function whose first line starts with signature (up to the closing brace at column 0)."""
    start = text.find(signature)
    if start < 0:
        return None
    brace = text.index('\n{\n', start)
    end = text.index('\n}\n', brace)
    return text[brace:end]


def fail(message):
    errors.append(message)


# ---------------------------------------------------------------- rule 1: the Dungeonbook
creature = read(SOURCE / 'entities/Creature.cpp')
fill = body(creature, 'float Creature::fillProfilePage(CEGUI::Window* page)')
if fill is None:
    fail('Creature::fillProfilePage not found')
else:
    ask = fill.find('getCreatureAppearanceImage(')
    if ask < 0:
        fail('fillProfilePage does not ask for the composed picture (getCreatureAppearanceImage)')
    for name in PREVIEW_CALLS:
        for match in re.finditer(re.escape(name) + r'\(', fill):
            if name != 'getCreatureProfilePortraitImage':
                fail('fillProfilePage uses %s, only the profile portrait fallback is allowed' % name)
                continue
            before = fill[:match.start()]
            # The call has to sit in the else branch that follows the test on the composed picture
            else_pos = before.rfind('else')
            test_pos = before.rfind('if(appearanceImage != nullptr)')
            if ask < 0 or test_pos < 0 or else_pos < test_pos or fill.find('{', else_pos) > match.start():
                fail('getCreatureProfilePortraitImage is used outside the fallback branch of fillProfilePage')
            elif before[else_pos:].count('{') < 1:
                fail('the fallback branch of fillProfilePage has no block')
    if 'getCreatureProfilePortraitImage(' not in fill:
        fail('the fallback to getCreatureProfilePortraitImage is missing in fillProfilePage')
    # Remarks only together with the composed picture
    remarks = fill.find('getCreatureAppearanceRemarks(')
    if remarks >= 0:
        guard = fill.rfind('if(appearanceImage != nullptr)', 0, remarks)
        if guard < 0:
            fail('the remarks are not limited to the composed picture')

# The other Dungeonbook code never reaches for the preview portraits
for name in ('render/SocialWindow.cpp', 'render/SocialWindow.h'):
    text = read(SOURCE / name)
    for call in PREVIEW_CALLS:
        if call in text:
            fail('%s uses the preview portrait function %s' % (name, call))
    if 'materials/textures/portrait-' in text or '"portrait-' in text:
        fail('%s names a preview portrait file' % name)

# The composed picture code decides nothing about the fallback: it never loads a preview portrait itself
# (the header only documents the fallback in its comments, so only the sources are scanned)
for name in ('render/CreatureAppearancePicture.cpp', 'render/AppearanceCompose.cpp', 'render/DungeonbookQuirks.cpp'):
    text = read(SOURCE / name)
    for call in PREVIEW_CALLS:
        if call in text:
            fail('%s uses the preview portrait function %s' % (name, call))
    if re.search(r'"portrait-|portrait-<|CreaturePortrait\.h', text):
        fail('%s uses preview portrait files or the preview portrait code' % name)

# ---------------------------------------------------------------- rule 2: the creature bar
# Files that belong to the creature bar and the preview portraits: no appearance code, no part folders
for name in ('render/CreaturePanel.cpp', 'render/CreaturePanel.h', 'render/CreaturePortrait.cpp',
             'render/CreaturePortrait.h'):
    path = SOURCE / name
    if not path.exists():
        continue
    text = read(path)
    for word in APPEARANCE_NAMES + FOLDER_NAMES:
        if word in text:
            fail('%s (creature bar / preview portraits) mentions %s' % (name, word))

# GameMode builds the creature bar hand icons and only clears the composed pictures when a map is unloaded
game = read(SOURCE / 'modes/GameMode.cpp')
for word in ('getCreatureAppearanceImage', 'getCreatureAppearanceRemarks', 'getClientPortraitManifest',
             'AppearanceCompose', 'PortraitManifest', 'DungeonbookQuirks') + FOLDER_NAMES:
    if word in game:
        fail('modes/GameMode.cpp mentions %s' % word)
icon_start = game.find('getCreatureHandIconImage(')
if icon_start < 0:
    fail('hand icon code not found in GameMode.cpp')

# Nowhere in the game code (tests excluded) is a part or base folder named, except where the pictures are loaded
allowed_folder_users = {
    'entities/Creature.cpp',  # default asset root of the server registry
    'render/CreatureAppearancePicture.cpp',  # default asset root of the client registry
    'render/DungeonbookAppearanceConfig.h',  # documentation of the default
}
for path in sorted(SOURCE.rglob('*')):
    if path.suffix not in ('.cpp', '.h') or 'tests' in path.relative_to(SOURCE).parts:
        continue
    relative = path.relative_to(SOURCE).as_posix()
    text = read(path)
    if re.search(r'neutral-bases|portraits/variants', text) and relative not in allowed_folder_users:
        fail('%s names the part or base folders, only the Dungeonbook picture code may' % relative)

if errors:
    for message in errors:
        print('FAIL: ' + message)
    sys.exit(1)

print('dungeonbook separation ok')
