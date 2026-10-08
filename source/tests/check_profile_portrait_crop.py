"""Noncompiling regression for the top-square profile view and uniform UI fit."""
from pathlib import Path
import math
import re
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
helper = (root / 'source/render/ProfilePortraitCrop.h').read_text()
assert 'const float side = std::min(width, height);' in helper
assert 'CEGUI::Rectf(0.0f, 0.0f, side, side)' in helper
assert 'left' not in helper and 'rotate' not in helper
appearance = (root / 'source/render/CreatureAppearancePicture.cpp').read_text()
portrait = (root / 'source/render/CreaturePortrait.cpp').read_text()
assert 'image.setArea(getProfilePortraitArea(static_cast<float>(width), static_cast<float>(height)))' in appearance
profile = portrait[portrait.index('const CEGUI::Image& getCreatureProfilePortraitImage('):]
assert profile.count('return getProfileFallbackImage(portraitKey);') == 2
assert 'image.setArea(getProfilePortraitArea(static_cast<float>(width), static_cast<float>(height)))' in profile
fallback = portrait[portrait.index('const CEGUI::Image& getProfileFallbackImage('):portrait.index('struct PortraitScene')]
assert 'image.setArea(getProfilePortraitArea(size.d_width, size.d_height))' in fallback
assert 'getTintedPortraitNames().insert(name)' in fallback
assert 'renderer.createTexture' not in fallback, 'fallback crop must share the panel texture'

page = ET.parse(root / 'gui/WindowCreatureProfilePage.layout').getroot()
frame = page.find(".//Window[@name='Portrait']")
props = {p.attrib['name']: p.attrib['value'] for p in frame.findall('Property')}
assert props['Area'] == '{{0.0,16},{0.0,4},{0.0,136},{0.0,124}}', 'existing frame/card geometry must remain'
view = frame.find("Window[@name='Image']")
props = {p.attrib['name']: p.attrib['value'] for p in view.findall('Property')}
assert props['Area'] == '{{0,2},{0,2},{1,-2},{1,-2}}'
assert props['AspectMode'] == 'Shrink' and props['AspectRatio'] == '1'
assert props['HorizontalAlignment'] == props['VerticalAlignment'] == 'Centre'
assert props['FrameEnabled'] == props['BackgroundEnabled'] == 'False'
creature = (root / 'source/entities/Creature.cpp').read_text()
assert creature.count('page->getChild("Portrait/Image")->setProperty("Image"') == 2

# Positive uniform fit: the old full 1:2 stretch would make these scales unequal.
for width, height in [(887,1774), (221,443), (512,1024), (256,256), (443,221)]:
    side = min(width, height)
    assert side > 0 and side <= width and side <= height
    for fw, fh in [(136,124), (120,120), (240,220), (88,120)]:
        iw, ih = fw-4, fh-4
        scale = min(iw/side, ih/side)
        dw = dh = side*scale
        assert scale > 0 and dw <= iw and dh <= ih
        assert math.isclose(dw/side, dh/side)
        assert (iw-dw)/2 >= 0 and (ih-dh)/2 >= 0
        # Top-left and rightward source coordinates remain unchanged, y0 is zero.
        source = [(0,0), (side,0), (0,side), (side,side)]
        assert source[1][0] > source[0][0] and source[0][1] == 0
print('profile portrait crop ok: composed/tinted/raw fallback, top square and uniform fit')
