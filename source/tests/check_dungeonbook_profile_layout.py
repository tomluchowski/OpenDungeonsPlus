"""Check that the profile remarks (QuirksText) never move the status and the latest post (static check).

The rows of the profile page flow from top to bottom (Gui::layoutCreatureProfilePage). The remarks of the
picture come after status and latest post, so those two keep the position they have without remarks. The
creature card has a fixed height: its page must have room for the status, the latest post and three more
text lines (heading and two remarks), estimated for the 1024x768 layout with the 8 point font (about 14
pixels per line, 4 pixels padding, 4 pixels gap).
"""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]
gui = (repo / 'source/render/Gui.cpp').read_text()
body = gui[gui.index('float Gui::layoutCreatureProfilePage'):]
body = body[:body.index('\n}\n')]

before = re.search(r'textRowsBefore\[\] = \{([^}]*)\}', body)[1]
after = re.search(r'textRowsAfter\[\] = \{([^}]*)\}', body)[1]
assert 'QuirksText' not in before, 'the remarks must not come before status and latest post'
rows = [r.strip(' "') for r in after.split(',')]
assert rows == ['StatusText', 'LatestText', 'QuirksText'], rows
assert 'i < 4' in body and 'i < 3' in body, 'row counts do not match the row lists'

layout = (repo / 'gui/WindowCreatureProfile.layout').read_text()
m = re.search(r'name="Area" value="\{\{0\.5,(-?\d+)\},\{0\.5,(-?\d+)\},\{0\.5,(\d+)\},\{0\.5,(\d+)\}\}"', layout)
card_height = int(m[4]) - int(m[2])
page = re.search(r'name="ProfilePage" >\s*<Property name="Area" value="\{\{0\.0,0\},\{0\.0,(\d+)\},\{1\.0,0\},\{1\.0,(-\d+)\}\}"', layout)
page_height = card_height - int(page[1]) + int(page[2])

LINE, PAD, GAP = 14, 4, 4


def row(lines):
    return lines * LINE + PAD + GAP


# typical card: bio 2 lines, likes and dislikes 1 line each, friends row with two names, foe row,
# status and latest post 2 lines each
top = 174
before_status = top + row(2) + row(1) + row(1) + (2 * LINE + GAP) + (LINE + PAD + GAP) + 0
status_bottom = before_status + row(2)
latest_bottom = status_bottom + row(2)
with_quirks = latest_bottom + row(3) - GAP
assert latest_bottom - GAP <= page_height, ('status and latest post do not fit the card', latest_bottom, page_height)
assert with_quirks <= page_height, ('the remarks do not fit the card', with_quirks, page_height)
print('profile layout ok: page %d px, latest post ends at %d, remarks end at %d' % (page_height, latest_bottom - GAP, with_quirks))
