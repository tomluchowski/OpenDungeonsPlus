"""Draw the forged navigation textures of the minimap with the real code of Gui.cpp and check them.

The minimap frame, the compass marker, the twelve corner plates (resting, hovered and pressed for
each screen corner) and the four symbols on them are drawn procedurally in Gui.cpp. This check cuts
the drawing functions out of Gui.cpp by signature, builds them with the compiler of the environment,
dumps the textures and checks the geometry that the layout and the map renderer rely on:
- the frame is a circle that touches the edges of its texture: opaque iron on the ring, empty
  outside, translucent haze inside so that the map stays readable;
- the corner plates are opaque on their outer square, empty in the bite of the map circle, mirror
  each other exactly in outline and are lit from the top left (the top edge is brighter than the
  bottom edge on the resting plate);
- the hovered plate glows warmer than the resting plate, the pressed plate is not brighter;
- every symbol is opaque in its centre area, empty in its corners and warm (red above blue) so
  that it reads as metal on the dark well;
- the compass marker is round and empty outside its rim.

With --preview DIRECTORY it also writes enlarged PNGs (needs PIL): every texture, and a mock-up of
the minimap at the size of the 1920x1200 game (176 layout units at scale 1.7).
"""
from pathlib import Path
import argparse
import math
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--preview', help='directory for enlarged preview PNGs')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
source = (repo / 'source/render/Gui.cpp').read_text()


def function(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


helpers = ['float badgeClamp(', 'void badgeMix(', 'void badgeSet(', 'float badgeNoise(', 'float badgeSmoothstep(',
           'float badgeSmoothMin(', 'float badgeHash(', 'float badgeValueNoise(', 'float badgeFbm(', 'float badgeCircle(',
           'float badgeTaper(', 'float badgeBox(', 'float badgeRound(', 'void badgeNormal(']
navigation = source[source.index('//! \\brief Puts a layer (colour 0-255'):source.index('//! \\brief Creates a texture and an image')]
code = r'''
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
HELPERS
NAVIGATION
static void dump(const std::string& directory, const std::string& name, const std::vector<unsigned char>& pixels)
{
    FILE* file = std::fopen((directory + "/" + name + ".rgba").c_str(), "wb");
    std::fwrite(pixels.data(), 1, pixels.size(), file);
    std::fclose(file);
}
int main(int, char** argv)
{
    const std::string directory = argv[1];
    std::vector<unsigned char> pixels;
    drawMiniMapRimPixels(pixels);
    dump(directory, "rim", pixels);
    drawMiniMapNorthPixels(pixels);
    dump(directory, "north", pixels);
    for(int corner = 0; corner < 4; ++corner)
        for(int state = 0; state < 3; ++state)
        {
            drawCornerPlatePixels(pixels, corner, state);
            dump(directory, "plate" + std::to_string(corner) + std::to_string(state), pixels);
        }
    for(int symbol = 0; symbol < 4; ++symbol)
    {
        drawNavigationSymbolPixels(pixels, symbol);
        dump(directory, "symbol" + std::to_string(symbol), pixels);
    }
    return 0;
}
'''.replace('HELPERS', '\n'.join(function(s) for s in helpers)).replace('NAVIGATION', navigation)

checks = 0
failures = 0


def check(ok, why):
    global checks, failures
    checks += 1
    if not ok:
        failures += 1
        print('FAIL', why)


def load(directory, name, size):
    data = (directory / (name + '.rgba')).read_bytes()
    check(len(data) == size * size * 4, name + ' has the expected size')
    return data


def pixel(data, size, x, y):
    i = (y * size + x) * 4
    return data[i], data[i + 1], data[i + 2], data[i + 3]


def brightness(p):
    return p[0] + p[1] + p[2]


with tempfile.TemporaryDirectory(prefix='odp-navigation-style-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(code)
    subprocess.run(['cl', '/nologo', '/EHsc', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True,
                   stdout=subprocess.DEVNULL)
    subprocess.run([str(work / 'check.exe'), str(work)], check=True)

    rim_size, north_size, plate_size, symbol_size = 384, 64, 128, 64
    rim = load(work, 'rim', rim_size)
    centre = rim_size // 2
    unit = rim_size / 176.0
    # Frame: empty outside the circle, opaque iron between 80.2 and 87 units, haze inside
    check(pixel(rim, rim_size, 2, 2)[3] == 0, 'corner of the frame texture is empty')
    check(pixel(rim, rim_size, centre, 3)[3] > 0, 'frame touches the top edge')
    for degrees in range(0, 360, 15):
        for radius in (81.0, 83.7, 86.5):
            x = int(centre + radius * unit * math.sin(math.radians(degrees)))
            y = int(centre - radius * unit * math.cos(math.radians(degrees)))
            check(pixel(rim, rim_size, x, y)[3] == 255, 'frame is opaque at %d degrees, radius %.1f' % (degrees, radius))
        x = int(centre + 60.0 * unit * math.sin(math.radians(degrees)))
        y = int(centre - 60.0 * unit * math.cos(math.radians(degrees)))
        haze = pixel(rim, rim_size, x, y)
        check(0 < haze[3] < 200, 'the map area carries a translucent haze at %d degrees' % degrees)
    middle = pixel(rim, rim_size, centre, centre)
    check(20 < middle[3] < 90, 'the middle of the map is nearly clear (alpha %d)' % middle[3])
    edge = pixel(rim, rim_size, int(centre + 76 * unit), centre)
    check(edge[3] > middle[3] + 40, 'the haze darkens towards the ring')
    lip = pixel(rim, rim_size, int(centre + 81 * unit), centre)
    check(lip[0] > lip[1] > lip[2], 'the inner lip of the frame is warm bronze')

    # Compass marker: round, opaque rim, empty corners
    north = load(work, 'north', north_size)
    check(pixel(north, north_size, 0, 0)[3] == 0, 'compass marker corner is empty')
    check(pixel(north, north_size, north_size // 2, north_size // 2)[3] == 255, 'compass marker centre is opaque')
    rim_x = int(north_size / 2 + 7.4 / 10.0 * north_size / 2)
    check(pixel(north, north_size, rim_x, north_size // 2)[3] == 255, 'compass marker rim is opaque')
    letter = [pixel(north, north_size, x, y) for y in range(north_size) for x in range(north_size)
              if abs(x - 32) < 14 and abs(y - 32) < 14]
    check(sum(1 for p in letter if p[0] > 150 and p[1] > 110 and p[2] < p[1]) > 20, 'compass marker shows a gold letter')

    # Corner plates
    outlines = []
    for corner in range(4):
        states = [load(work, 'plate%d%d' % (corner, state), plate_size) for state in range(3)]
        outlines.append([states[0][i + 3] for i in range(0, plate_size * plate_size * 4, 4)])
        outer_x = 6 if not corner & 1 else plate_size - 7
        outer_y = 6 if not corner & 2 else plate_size - 7
        check(pixel(states[0], plate_size, outer_x, outer_y)[3] == 255, 'plate %d is opaque at its outer corner' % corner)
        bite_x = plate_size - 8 if not corner & 1 else 8
        bite_y = plate_size - 8 if not corner & 2 else 8
        check(pixel(states[0], plate_size, bite_x, bite_y)[3] == 0, 'plate %d is empty where the map circle cuts it' % corner)
        # The medallion centre is at 13.4 of 44 units from the corner
        mx = int(13.4 / 44 * plate_size) if not corner & 1 else plate_size - 1 - int(13.4 / 44 * plate_size)
        my = int(13.4 / 44 * plate_size) if not corner & 2 else plate_size - 1 - int(13.4 / 44 * plate_size)
        rest, hover, pressed = [pixel(s, plate_size, mx, my) for s in states]
        check(hover[0] > rest[0] + 25 and hover[0] - hover[2] > rest[0] - rest[2] + 20,
              'plate %d glows warmer when hovered' % corner)
        check(brightness(pressed) < brightness(rest) + 1, 'plate %d is not brighter when pressed' % corner)
        check(rest[3] == 255 and rest[0] < 90, 'plate %d has a dark well' % corner)
    # Mirrored plates have mirrored outlines
    mirrored = True
    for y in range(plate_size):
        for x in range(plate_size):
            a = outlines[0][y * plate_size + x] > 127
            b = outlines[1][y * plate_size + plate_size - 1 - x] > 127
            c = outlines[2][(plate_size - 1 - y) * plate_size + x] > 127
            d = outlines[3][(plate_size - 1 - y) * plate_size + plate_size - 1 - x] > 127
            mirrored = mirrored and a == b == c == d
    check(mirrored, 'plate outlines mirror each other')
    plate = load(work, 'plate00', plate_size)
    top = pixel(plate, plate_size, 64, 3)
    bottom_edge = pixel(plate, plate_size, 3, 64)
    check(brightness(top) > 200, 'top edge of the resting plate carries a bronze line')
    check(brightness(top) > brightness(pixel(plate, plate_size, 64, 40)), 'the top edge of the plate is lit')
    check(brightness(bottom_edge) > 100, 'left edge of the resting plate carries a bronze line')

    # Symbols
    for symbol in range(4):
        data = load(work, 'symbol%d' % symbol, symbol_size)
        check(pixel(data, symbol_size, 0, 0)[3] == 0 and pixel(data, symbol_size, symbol_size - 1, 0)[3] == 0,
              'symbol %d is empty in its corners' % symbol)
        solid = [pixel(data, symbol_size, x, y) for y in range(symbol_size) for x in range(symbol_size)
                 if pixel(data, symbol_size, x, y)[3] == 255]
        check(len(solid) > 350, 'symbol %d has an opaque body (%d pixels)' % (symbol, len(solid)))
        warm = sum(1 for p in solid if p[0] >= p[2])
        check(warm > 0.6 * len(solid), 'symbol %d is mostly warm metal' % symbol)
        check(max(brightness(p) for p in solid) > 520, 'symbol %d has a bright glint' % symbol)
        check(min(brightness(p) for p in solid) < 300, 'symbol %d has dark relief' % symbol)
        check(all(pixel(data, symbol_size, x, y)[3] == 0 for x in (0, 63) for y in range(0, 64, 9)),
              'symbol %d stays inside its texture' % symbol)

    if args.preview:
        from PIL import Image, ImageDraw
        out = Path(args.preview)
        out.mkdir(parents=True, exist_ok=True)

        def image(name, size):
            return Image.frombytes('RGBA', (size, size), (work / (name + '.rgba')).read_bytes())

        def backdrop(width, height):
            base = Image.new('RGBA', (width, height), (40, 34, 26, 255))
            pix = base.load()
            for y in range(height):
                for x in range(width):
                    n = ((x * 73856093) ^ (y * 19349663)) % 23
                    pix[x, y] = (46 + n, 38 + n, 28 + n // 2, 255)
            return base

        scale = 1.7
        map_size = round(176 * scale)
        pad = round(30 * scale)
        canvas = backdrop(map_size + 2 * pad, map_size + 2 * pad)
        # Sample map: dark ground with a red dungeon, a yellow heart and the viewport trapezoid
        mapimg = Image.new('RGBA', (map_size, map_size), (0, 0, 0, 255))
        draw = ImageDraw.Draw(mapimg)
        k = map_size / 176.0
        draw.polygon([(44 * k, 62 * k), (118 * k, 52 * k), (132 * k, 84 * k), (120 * k, 108 * k), (70 * k, 118 * k), (38 * k, 96 * k)],
                     fill=(178, 32, 30, 255), outline=(120, 60, 24, 255))
        draw.rectangle([82 * k, 68 * k, 96 * k, 80 * k], fill=(230, 220, 20, 255))
        draw.polygon([(52 * k, 58 * k), (132 * k, 58 * k), (118 * k, 100 * k), (66 * k, 100 * k)], outline=(255, 255, 255, 255))
        mask = Image.new('L', (map_size, map_size), 0)
        ImageDraw.Draw(mask).ellipse([0, 0, map_size - 1, map_size - 1], fill=255)
        mapimg.putalpha(mask)
        canvas.alpha_composite(mapimg, (pad, pad))
        canvas.alpha_composite(image('rim', rim_size).resize((map_size, map_size), Image.BILINEAR), (pad, pad))
        boss = round(20 * k)
        canvas.alpha_composite(image('north', north_size).resize((boss, boss), Image.BILINEAR),
                               (pad + map_size // 2 - boss // 2, pad + round(88 * k - 79 * k) - boss // 2))
        # Query (help), sell (bomb), options (gear), map zoom (magnifier) with the icon areas of the layout
        areas = [(0.09, 0.09, 0.52, 0.52), (0.48, 0.09, 0.91, 0.52), (0.09, 0.48, 0.52, 0.91), (0.48, 0.48, 0.91, 0.91)]
        origin = [(0, 0), (132, 0), (0, 132), (132, 132)]
        for corner, area in enumerate(areas):
            plate_pixels = round(44 * k)
            canvas.alpha_composite(image('plate%d0' % corner, plate_size).resize((plate_pixels, plate_pixels), Image.BILINEAR),
                                   (pad + round(origin[corner][0] * k), pad + round(origin[corner][1] * k)))
            left = pad + round((origin[corner][0] + area[0] * 44) * k)
            top = pad + round((origin[corner][1] + area[1] * 44) * k)
            size = round((area[2] - area[0]) * 44 * k)
            canvas.alpha_composite(image('symbol%d' % corner, symbol_size).resize((size, size), Image.BILINEAR), (left, top))
        canvas.save(out / 'navigation_real.png')
        canvas.resize((canvas.width * 3, canvas.height * 3), Image.LANCZOS).save(out / 'navigation_real_x3.png')
        # Enlarged sheets of the parts
        sheet = backdrop(4 * 400, 3 * 400)
        for corner in range(4):
            for state in range(3):
                sheet.alpha_composite(image('plate%d%d' % (corner, state), plate_size).resize((384, 384), Image.LANCZOS),
                                      (corner * 400 + 8, state * 400 + 8))
        sheet.save(out / 'navigation_plates.png')
        symbols = backdrop(4 * 400, 400)
        for symbol in range(4):
            well = Image.new('RGBA', (384, 384), (34, 20, 18, 255))
            well.alpha_composite(image('symbol%d' % symbol, symbol_size).resize((384, 384), Image.LANCZOS), (0, 0))
            symbols.alpha_composite(well, (symbol * 400 + 8, 8))
        symbols.save(out / 'navigation_symbols.png')
        image('rim', rim_size).save(out / 'navigation_rim.png')
        for name, size in [('north', north_size)] + [('plate%d%d' % (c, t), plate_size) for c in range(4) for t in range(3)] +                 [('symbol%d' % i, symbol_size) for i in range(4)]:
            image(name, size).save(out / ('texture_' + name + '.png'))
        image('rim', rim_size).save(out / 'texture_rim.png')
        boss_sheet = Image.new('RGBA', (512, 512), (34, 30, 24, 255))
        boss_sheet.alpha_composite(image('north', north_size).resize((512, 512), Image.LANCZOS), (0, 0))
        boss_sheet.save(out / 'navigation_north.png')

print('CHECKS=%d FAILURES=%d' % (checks, failures))
raise SystemExit(1 if failures else 0)
