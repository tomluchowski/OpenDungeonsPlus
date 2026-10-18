"""Validate the fixed campaign artwork contract; requires Pillow.

Usage: python tools/artwork/campaign-map/validate_world.py [asset-directory]
State padding is 16 px; lift padding is 32 px around a 106% province crop.
"""
import json
import math
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

EXPECTED = [
    ('T01', 'Mossgate'), ('T02', 'Brackenford'), ('T03', 'Coldwell'),
    ('T04', 'Tinmoor'), ('T05', 'Ravensledge'), ('T06', 'Ashcombe'),
    ('T07', 'Greywater'), ('T08', 'Hollin Fen'), ('T09A', 'Saltmere'),
    ('T09B', 'Dunmarrow'), ('T10', 'Ironbridge'), ('T11', 'Wolfscar'),
    ('T12', 'Lanternhill'), ('T13', 'Cinderhollow'), ('T14', 'Thornreach'),
    ('T15', 'Bellwick'), ('T16', 'Highcairn'), ('T17', 'Stormhaven'),
    ('T18A', 'Mirewatch'), ('T18B', 'Goldspire'), ('T19', 'Ebonrook'),
    ('T20', "Varn's Crossing"), ('T21', 'Silverdeep'), ('T22', 'Wraithwood'),
    ('T23', 'Kingsfall'), ('T24', 'Hollowmark Citadel')]
SITES = [('B01', 'The Boulder Course', 'T04'),
         ('B02', 'The Crowshot Gallery', 'T08'),
         ('B03', 'The Twisting Halls', 'T12'),
         ('B04', 'The Skittle Cavern', 'T16'),
         ('B05', 'The Swarm Night', 'T20')]
SIZE = (2560, 1600)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integers(value, length):
    return isinstance(value, list) and len(value) == length and all(
        type(n) is int for n in value)


def image(root, relative, size, mode, limited=False):
    path = (root / relative).resolve()
    require(path.is_relative_to(root.resolve()), f'Unsafe path: {relative}')
    require(path.is_file(), f'Missing file: {relative}')
    if limited:
        require(path.stat().st_size < 2_000_000, f'Layer exceeds 2 MB: {relative}')
    header = path.read_bytes()[:26]
    require(len(header) >= 26 and header[:8] == b'\x89PNG\r\n\x1a\n' and header[24] == 8,
            f'Expected 8-bit PNG: {relative}')
    with Image.open(path) as source:
        source.load()
        require(source.size == size, f'Wrong size: {relative}: {source.size} != {size}')
        require(source.mode == mode, f'Wrong mode: {relative}: {source.mode}')
        require(source.info.get('srgb') == 0, f'Missing sRGB declaration: {relative}')
        return source.copy()


def validate(root):
    data = json.loads((root / 'campaign-world.json').read_text(encoding='utf-8'))
    require(data['version'] == 1 and data['size'] == list(SIZE), 'Wrong version/size')
    provinces = data['provinces']
    require([(p['id'], p['name']) for p in provinces] == EXPECTED,
            'Expected exactly the 26 provinces in campaign order')
    require([(s['id'], s['name'], s['host']) for s in data['sites']] == SITES,
            'Expected exactly the five sites and their hosts')
    for key in ('title_cartouche', 'progress_panel'):
        require(integers(data[key], 4), f'Invalid {key}')
        x, y, w, h = data[key]
        require(w > 0 and h > 0 and x >= 0 and y >= 0 and
                x + w <= SIZE[0] and y + h <= SIZE[1], f'Out-of-bounds {key}')
    base = image(root, 'world_base.png', SIZE, 'RGBA')
    preview = image(root, 'preview_states.png', SIZE, 'RGBA')
    expected_preview = base.copy()
    idmap = image(root, 'world_idmap.png', SIZE, 'RGB')
    colours = [tuple(p['mask_rgb']) for p in provinces]
    require(len(set(colours)) == 26, 'Duplicate mask colours')
    for i, colour in enumerate(colours):
        require(integers(provinces[i]['mask_rgb'], 3) and
                all(0 <= c <= 255 for c in colour) and colour != (0, 0, 0),
                'Invalid mask colour')
        for other in colours[:i]:
            require(sum((a - b) ** 2 for a, b in zip(colour, other)) >= 400,
                    'Mask colours are less than 20 apart in Euclidean RGB distance')
    actual = idmap.getcolors(SIZE[0] * SIZE[1])
    require({colour for count, colour in actual} == set(colours) | {(0, 0, 0)},
            'Missing or unexpected ID-map colours (including antialiasing)')
    masks = {}
    areas = {}
    for province, colour in zip(provinces, colours):
        pid = province['id']
        difference = ImageChops.difference(idmap, Image.new('RGB', SIZE, colour))
        r, g, b = difference.split()
        mask = ImageChops.lighter(ImageChops.lighter(r, g), b).point(
            lambda value: 255 if value == 0 else 0)
        masks[pid] = mask
        areas[pid] = mask.histogram()[255]
        require(integers(province['bbox'], 4), f'Invalid bbox: {pid}')
        x, y, w, h = province['bbox']
        require(w > 0 and h > 0 and x >= 0 and y >= 0 and
                x + w <= SIZE[0] and y + h <= SIZE[1], f'Out-of-bounds bbox: {pid}')
        bounds = mask.getbbox()
        require(bounds is not None and x <= bounds[0] and y <= bounds[1] and
                x + w >= bounds[2] and y + h >= bounds[3], f'Bbox misses mask: {pid}')
        crop = mask.crop(bounds)
        seed = next((index % crop.width, index // crop.width)
                    for index, value in enumerate(crop.tobytes()) if value)
        flooded = crop.copy()
        ImageDraw.floodfill(flooded, seed, 0)
        require(flooded.getbbox() is None, f'Disconnected province: {pid}')
        for key in ('label_anchor', 'banner_anchor'):
            require(integers(province[key], 2), f'Invalid {key}: {pid}')
            ax, ay = province[key]
            require(0 <= ax < SIZE[0] and 0 <= ay < SIZE[1] and
                    mask.getpixel((ax, ay)) == 255, f'Anchor outside province: {pid}/{key}')
        require(province['layer_origin'] == [x - 16, y - 16], f'Wrong state origin: {pid}')
        require(set(province['layers']) == {'locked', 'available', 'conquered', 'lift'},
                f'Wrong layer keys: {pid}')
        expected_alpha = mask.crop((x - 16, y - 16, x + w + 16, y + h + 16))
        for state in ('locked', 'available', 'conquered'):
            relative = f'provinces/{pid}_{state}.png'
            require(province['layers'][state] == relative, f'Wrong layer path: {pid}/{state}')
            layer = image(root, relative, (w + 32, h + 32), 'RGBA', True)
            alpha = layer.getchannel('A')
            if state != 'available':
                require(ImageChops.difference(alpha, expected_alpha).getbbox() is None,
                        f'State mask is misaligned: {pid}/{state}')
            else:
                require(ImageChops.subtract(expected_alpha, alpha).getbbox() is None,
                        f'Available layer misses province: {pid}')
                neutral = base.crop((x - 16, y - 16, x + w + 16, y + h + 16))
                rgb_difference = ImageChops.difference(layer.convert('RGB'), neutral.convert('RGB'))
                require(all(ImageChops.multiply(channel, expected_alpha).getbbox() is None
                            for channel in rgb_difference.split()),
                        f'Available layer changes interior colours: {pid}')
            preview_state = 'conquered' if EXPECTED.index((pid, province['name'])) < 13 else (
                'available' if pid == 'T13' else 'locked')
            if state == preview_state:
                expected_preview.alpha_composite(layer, tuple(province['layer_origin']))
        lw, lh = math.ceil(w * 1.06), math.ceil(h * 1.06)
        expected_origin = [x - (lw - w) // 2 - 32, y - (lh - h) // 2 - 32]
        require(province['lift_origin'] == expected_origin, f'Wrong lift origin: {pid}')
        relative = f'provinces/{pid}_lift.png'
        require(province['layers']['lift'] == relative, f'Wrong lift path: {pid}')
        layer = image(root, relative, (lw + 64, lh + 64), 'RGBA', True)
        require(layer.getchannel('A').getbbox() is not None, f'Empty lift layer: {pid}')
        require(layer.getchannel('A').getpixel((0, 0)) == 0, f'Clipped lift shadow: {pid}')
    require(areas['T24'] == max(areas.values()), 'Citadel must be the largest province')
    for a, b in (('T09A', 'T09B'), ('T18A', 'T18B')):
        shared = False
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            if ImageChops.multiply(masks[a], ImageChops.offset(masks[b], dx, dy)).getbbox():
                shared = True
        require(shared, f'Branch provinces do not share a border: {a}/{b}')
    for site in data['sites']:
        require(integers(site['pos'], 2), f'Invalid site position: {site["id"]}')
        x, y = site['pos']
        require(0 <= x < SIZE[0] and 0 <= y < SIZE[1] and
                masks[site['host']].getpixel((x, y)) == 255,
                f'Site outside its host: {site["id"]}')
        require(set(site['icons']) == {'hidden', 'found', 'done'}, 'Wrong icon keys')
        for state in ('hidden', 'found', 'done'):
            relative = f'sites/{site["id"]}_{state}.png'
            require(site['icons'][state] == relative, 'Wrong icon path')
            icon = image(root, relative, (96, 96), 'RGBA')
            require(icon.getchannel('A').getbbox() is not None, f'Empty icon: {relative}')
            require(icon.getpixel((0, 0))[3] == 0, f'Nontransparent icon corner: {relative}')
            if state == 'found' and site['id'] in ('B01', 'B02', 'B03'):
                expected_preview.alpha_composite(icon, (x - 48, y - 48))
    hover = next(p for p in provinces if p['id'] == 'T13')
    with Image.open(root / hover['layers']['lift']) as lift:
        expected_preview.alpha_composite(lift, tuple(hover['lift_origin']))
    difference = ImageChops.difference(expected_preview, preview)
    require(any(difference.getextrema()[i][1] != 0 for i in range(4)) is False,
            'Preview does not match the required province/site states')
    print('PASS: 26 connected provinces, 104 aligned layers, 5 sites / 15 icons, '
          'base, ID map, preview, sRGB, sizes and Euclidean palette separation.')


if __name__ == '__main__':
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[3] / 'gui/campaign'
    try:
        validate(root)
    except (OSError, ValueError, KeyError, TypeError, StopIteration) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        sys.exit(1)
