"""Check the Dungeonbook tint files (static check, no game needed).

Mirrors the line rules of PortraitTint::loadFromFile / parseRegion for config/dungeonbook-base-tints.cfg
(regions of the neutral bases, keyed by catalog id) and config/dungeonbook-part-tints.cfg (colours of hair,
beard and eye parts, keyed by slot, slot:option or catalog id:slot:option, with its own palettes). Both must load without a single
parse error. Where the shipped bases are present (materials/portraits/variants/<catalog id>/manifest.cfg)
every catalog id must have a base tint block with at least one region, and no block may name an unknown
id; every part tint key must name a slot and option that exist in some manifest. Without the shipped
bases only the syntax is checked.
"""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]
NUMBER = re.compile(r'^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$')
SIZES = {'shift': 3, 'box': 4, 'ellipse': 4, 'hue': 2, 'sat': 2, 'val': 2}


def floats(text):
    values = []
    for item in text.split(','):
        item = item.strip()
        if not NUMBER.match(item):
            return None
        values.append(float(item))
    return values or None


def parse(path):
    """Returns (palettes, portraits, errors); portraits maps the Mesh key to its list of regions."""
    palettes = {}
    portraits = []
    errors = []
    current = None
    for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        line = line.split('#')[0]
        if not line.strip():
            continue
        columns = [c.strip() for c in line.split('\t')]
        key = columns[0]
        if key == '[Palette]':
            palettes[len(palettes)] = {'name': None, 'colours': []}
            current = 'palette'
        elif key == '[Portrait]':
            portraits.append({'mesh': None, 'regions': []})
            current = 'portrait'
        elif key == 'Name' and current == 'palette' and len(columns) >= 2:
            palettes[len(palettes) - 1]['name'] = columns[1]
        elif key == 'Colour' and current == 'palette' and len(columns) >= 5:
            if any(floats(c) is None for c in columns[2:5]):
                errors.append((number, 'bad Colour numbers'))
            else:
                palettes[len(palettes) - 1]['colours'].append(tuple(floats(c)[0] for c in columns[2:5]))
        elif key == 'Mesh' and current == 'portrait' and len(columns) >= 2:
            portraits[-1]['mesh'] = columns[1]
        elif key == 'Region' and current == 'portrait' and len(columns) >= 3:
            region = {'name': columns[1]}
            ok = True
            for column in columns[2:]:
                if '=' not in column:
                    errors.append((number, 'expected key=value: ' + column))
                    ok = False
                    break
                name, text = column.split('=', 1)
                if name == 'palette':
                    match = [p for p in palettes.values() if p['name'] == text]
                    if not match:
                        errors.append((number, 'unknown palette ' + text))
                        ok = False
                        break
                    if not match[0]['colours']:
                        errors.append((number, 'palette without colours ' + text))
                        ok = False
                        break
                    region['mode'] = True
                    region['palette'] = text
                    continue
                values = floats(text)
                if values is None:
                    errors.append((number, 'bad numbers: ' + column))
                    ok = False
                    break
                if name in SIZES and len(values) == SIZES[name]:
                    region[name] = values
                    if name == 'shift':
                        region['mode'] = True
                elif name == 'not' and len(values) % 4 == 0:
                    region[name] = values
                else:
                    errors.append((number, 'unknown key or wrong value count: ' + column))
                    ok = False
                    break
            if ok and not (all(k in region for k in ('mode', 'hue', 'sat', 'val')) and ('box' in region or 'ellipse' in region)):
                errors.append((number, 'region needs palette= or shift=, box= or ellipse=, hue=, sat=, val='))
                ok = False
            if ok and 'box' in region:
                box = region['box']
                assert 0 <= box[0] < box[2] <= 1 and 0 <= box[1] < box[3] <= 1, (number, 'box outside 0..1')
            if ok and 'ellipse' in region:
                cx, cy, rx, ry = region['ellipse']
                assert 0 < cx < 1 and 0 < cy < 1 and 0 < rx < 0.2 and 0 < ry < 0.2, (number, 'ellipse outside the part')
            if ok:
                portraits[-1]['regions'].append(region)
        else:
            errors.append((number, 'unexpected line ' + key))
    return palettes, portraits, errors


def catalog_manifests():
    root = repo / 'materials' / 'portraits' / 'variants'
    result = {}
    if root.is_dir():
        for folder in sorted(root.iterdir()):
            manifest = folder / 'manifest.cfg'
            if manifest.is_file():
                slots = set()
                options = set()
                for line in manifest.read_text(encoding='utf-8').splitlines():
                    columns = line.split('#')[0].rstrip('\r').split('\t')
                    if columns[0] == 'Slot' and len(columns) >= 2:
                        slots.add(columns[1])
                    elif columns[0] == 'Option' and len(columns) >= 4:
                        options.add((columns[1], columns[3]))
                result[folder.name] = (slots, options)
    return result


manifests = catalog_manifests()

_, bases, errors = parse(repo / 'config' / 'dungeonbook-base-tints.cfg')
assert not errors, errors
meshes = [b['mesh'] for b in bases]
assert None not in meshes
assert len(meshes) == len(set(meshes)), 'a catalog id has two base tint blocks'

part_palettes, parts, errors = parse(repo / 'config' / 'dungeonbook-part-tints.cfg')
assert not errors, errors
keys = [p['mesh'] for p in parts]
assert None not in keys
assert len(keys) == len(set(keys)), 'a part key has two blocks'
assert all(p['regions'] for p in parts), 'a part tint block without a region'

# The skin of the bases varies per creature through the amplitude of the base-skin entry: the real bases may
# come with a skin amplitude of 0, which means no variation
skin = [b for b in parts if b['mesh'] == 'base-skin']
assert len(skin) == 1, 'the part tints need exactly one base-skin entry'
shifts = [r['shift'] for r in skin[0]['regions'] if r['name'] == 'Skin' and 'shift' in r]
assert shifts and all(v > 0 for v in shifts[0]), ('the base skin amplitude must be above 0', shifts)

# Palettes that exist in both files are the ones of the old portraits (same colours, same order), and the
# catalog ids whose old portrait uses fire, glow or hair_brown use them in the new parts as well (the region
# names Eyes and Hair are the old ones, so the colour a creature gets is the same as before)
old_palettes, old_portraits, old_errors = parse(repo / 'config' / 'portrait-tints.cfg')
assert not old_errors, old_errors
old_by_name = {p['name']: p['colours'] for p in old_palettes.values()}
for palette in part_palettes.values():
    if palette['name'] in old_by_name:
        assert palette['colours'] == old_by_name[palette['name']], 'palette %s differs from portrait-tints.cfg' % palette['name']
for name in ('fire', 'glow', 'hair_brown', 'eyes', 'hair'):
    assert name in [p['name'] for p in part_palettes.values()], 'palette %s is missing in the part tints' % name

old_palette_of = {}
for portrait in old_portraits:
    for region in portrait['regions']:
        if 'palette' in region and region['name'] in ('Eyes', 'Hair'):
            old_palette_of.setdefault((portrait['mesh'], region['name']), set()).add(region['palette'])
for portrait in parts:
    names = portrait['mesh'].split(':')
    if len(names) != 3:
        continue
    catalog, slot, _ = names
    for region in portrait['regions']:
        wanted = old_palette_of.get((catalog, region['name']))
        if wanted and wanted & {'fire', 'glow', 'hair_brown'}:
            assert region.get('palette') in wanted, '%s uses %s, the old portrait uses %s' % (portrait['mesh'], region.get('palette'), wanted)
# every base whose old portrait uses one of the three palettes has such blocks in the part tints
for (catalog, region_name), wanted in old_palette_of.items():
    if wanted & {'fire', 'glow', 'hair_brown'}:
        assert any(p['mesh'].startswith(catalog + ':') for p in parts), 'no part block for %s although the old portrait uses %s' % (catalog, wanted)

if manifests:
    missing = [i for i in manifests if i not in meshes]
    assert not missing, 'catalog ids without a base tint block: %s' % missing
    unknown = [m for m in meshes if m not in manifests]
    assert not unknown, 'base tint blocks without a manifest: %s' % unknown
    empty = [b['mesh'] for b in bases if not b['regions']]
    assert not empty, 'base tint blocks without a region: %s' % empty
    for key in keys:
        if key == 'base-skin':
            continue
        names = key.split(':')
        catalog = names.pop(0) if len(names) == 3 else None
        slot, option = names[0], (names[1] if len(names) > 1 else '')
        pool = [manifests[catalog]] if catalog else list(manifests.values())
        assert pool, 'part key of unknown catalog id ' + key
        assert any(slot in slots for slots, _ in pool), 'part key of unknown slot ' + key
        if option:
            assert any((slot, option) in opts for _, opts in pool), 'part key of unknown option ' + key
print('dungeonbook tints ok: %d base blocks, %d part keys, %d manifests' % (len(bases), len(parts), len(manifests)))
