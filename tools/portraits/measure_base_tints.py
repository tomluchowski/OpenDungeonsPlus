"""Measure existing neutral-base pixels; never use feature placement rectangles."""
import hashlib
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
BASES = ROOT / 'materials/portraits/neutral-bases'
identifiers = sorted(p.parent.name for p in (ROOT / 'materials/portraits/variants').glob('*/manifest.cfg'))
lines = ['# Neutral-base tint rectangles measured from actual image pixels.',
         '# TAB-separated format from origin/feature/dungeonbook-appearance.',
         '# Hair, eyes and beard are absent on all 34 inspected blank neutral bases.',
         '# No absent region is fabricated from a feature slot; shifts remain neutral.', '']
records = []
for identifier in identifiers:
    source = BASES / (identifier + '.png')
    image = Image.open(source).convert('RGB')
    assert image.size == (887, 1774)
    pixels = list(image.getdata())
    # True when the (red, green, blue) pixel of this neutral base is a skin pixel; the colour rule depends on the species.
    def skin(rgb):
        red, green, blue = rgb
        if identifier.startswith('Kobold.mesh'):
            return red > green * 1.12 and blue > green * 1.2 and red > 25
        if identifier.startswith('DarkElf.mesh'):
            return red >= green * .95 and blue > red * 1.05 and red > 35
        if identifier.startswith('Troll.mesh'):
            return min(rgb) > 25 and max(rgb) - min(rgb) < 65 and red >= blue * .9
        return red > blue * 1.15 and green > blue * .95 and red > 30
    mask = Image.new('L', image.size)
    mask.putdata([255 if skin(rgb) else 0 for rgb in pixels])
    bounds = mask.getbbox()
    assert bounds is not None
    x0, y0, x1, y1 = bounds
    box = [x0 / image.width, y0 / image.height, x1 / image.width, y1 / image.height]
    assert all(0 <= value <= 1 for value in box)
    lines.extend(['[Portrait]', 'Mesh\t' + identifier,
                  'Region\tSkin\tshift=0,0,0\tbox=' + ','.join(f'{v:.6f}' for v in box) +
                  '\thue=0,360\tsat=0,1\tval=0,1', ''])
    records.append(dict(portrait=identifier, source=source.relative_to(ROOT).as_posix(),
                        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                        size=list(image.size), skin_pixel_bounds=list(bounds), skin_box=box,
                        skin_pixels=sum(v == 255 for v in mask.getdata()),
                        hair='absent', eyes='absent', beard='absent',
                        method='bounding rectangle of species-colour pixels in the actual neutral base; blank featureless bases visually inspected'))
assert len(records) == 34
(ROOT / 'config/dungeonbook-base-tints.cfg').write_text('\n'.join(lines), encoding='utf-8')
(ROOT / 'materials/portraits/neutral-variants/base-tint-measurements.json').write_text(json.dumps(records, indent=2) + '\n', encoding='utf-8')
print(json.dumps(dict(bases=len(records), measured_skin_regions=len(records), absent_hair_eyes_beard=34)))
