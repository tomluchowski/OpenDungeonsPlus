"""Render the frozen manifest literally, without undocumented conditional rules."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageChops

def polygon_mask(size, coordinates, feather):
    """Same source pixel-center contour and outside feather as the runtime composer."""
    import math
    points = list(zip(coordinates[::2],coordinates[1::2]))
    mask = Image.new('L',size,255)
    left = max(0,int(min(x for x,y in points)-feather-1))
    right = min(size[0],int(max(x for x,y in points)+feather+1))
    top = max(0,int(min(y for x,y in points)-feather-1))
    bottom = min(size[1],int(max(y for x,y in points)+feather+1))
    for sy in range(top,bottom):
        for sx in range(left,right):
            x,y = sx+0.5,sy+0.5
            inside = False
            distance_squared = 1.0e30
            for (ax,ay),(bx,by) in zip(points[-1:]+points[:-1],points):
                if ((ay>y)!=(by>y)) and x<(bx-ax)*(y-ay)/(by-ay)+ax:
                    inside = not inside
                dx,dy = bx-ax,by-ay
                length_squared = dx*dx+dy*dy
                t = max(0,min(1,((x-ax)*dx+(y-ay)*dy)/length_squared)) if length_squared else 0
                distance_squared = min(distance_squared,(x-ax-t*dx)**2+(y-ay-t*dy)**2)
            alpha = 0 if inside else 255
            if not inside and feather and distance_squared<feather*feather:
                alpha = int(255*math.sqrt(distance_squared)/feather+0.5)
            mask.putpixel((sx,sy),alpha)
    return mask

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--portrait',action='append',required=True)
parser.add_argument('--out',type=Path,help='Separate derived-review destination; source review files remain unchanged')
args = parser.parse_args()
for identifier in args.portrait:
    folder = ROOT/'materials/portraits/variants'/identifier
    rows = [line.split('\t') for line in (folder/'manifest.cfg').read_text().splitlines()
            if line and not line.startswith('#')]
    base = Image.open(folder/next(r[1] for r in rows if r[0]=='Base')).convert('RGBA')
    slots = {r[1]:tuple(map(int,r[2:])) for r in rows if r[0]=='Slot'}
    allowed = {r[4] for r in rows if r[0] in ['Option','Fitted']}
    clips = [(r[1],r[2]) for r in rows if r[0]=='Clip']
    rules = {(r[1],int(r[2])):tuple(map(int,r[3:])) for r in rows if r[0]=='Rule'}
    polygons = {(r[1],int(r[2])):(int(r[3]),list(map(int,r[4:]))) for r in rows if r[0]=='SourcePolygonMask'}
    polygon_cache = {}
    masks = {(r[1],int(r[2])):r[3] for r in rows if r[0]=='Mask'}
    numbers = {(r[1],r[4]):int(r[2]) for r in rows if r[0]=='Option'}
    represented = set()
    visible_pixels = {}
    choices = json.loads((folder/'review/choices.json').read_text())
    runtime_combinations = len(choices)
    diagnostic_combinations = []
    if 'helmet' in slots:
        # Offline part inspection only; this creates no runtime option or appearance.
        for selected in choices[:3]:
            inspection = dict(selected)
            inspection.pop('helmet',None)
            inspection.pop('scar',None)  # helmet damage is reviewed on its actual helmet
            choices.append(inspection)
            diagnostic_combinations.append(len(choices))
    review = args.out/identifier if args.out else folder/'review'
    review.mkdir(parents=True,exist_ok=True)
    small_sheet = Image.new('RGB',(200,((len(choices)+3)//4)*100))
    full_sheet = Image.new('RGB',(4*887,((len(choices)+3)//4)*1774))
    for number,selected in enumerate(choices,1):
        assert set(selected.values())<=allowed
        composite = base.copy()
        layers = []
        for slot,rect in slots.items():
            if slot not in selected:
                continue
            patch = Image.open(folder/selected[slot]).convert('RGBA')
            x,y,w,h = rect
            key = (slot,numbers[(slot,selected[slot])])
            represented.add(selected[slot])
            if key in polygons:
                if key not in polygon_cache:
                    feather,coordinates = polygons[key]
                    polygon_cache[key] = polygon_mask(patch.size,coordinates,feather)
                patch.putalpha(ImageChops.multiply(patch.getchannel('A'),polygon_cache[key]))
            if key in rules:
                x,y,w,h = rules[key]
                patch = patch.resize((w,h),Image.Resampling.NEAREST)
            for source,target in clips:
                if source!=slot:
                    continue
                target_alpha = Image.new('L',base.size)
                if target in selected:
                    target_alpha.paste(Image.open(folder/selected[target]).getchannel('A'),slots[target][:2])
                patch.putalpha(ImageChops.multiply(patch.getchannel('A'),target_alpha.crop((x,y,x+w,y+h))))
            if key in masks and masks[key] in selected:
                mask_slot = masks[key]
                mask_key = (mask_slot,numbers[(mask_slot,selected[mask_slot])])
                mx,my,mw,mh = rules.get(mask_key,slots[mask_slot])
                mask_image = Image.open(folder/selected[mask_slot]).convert('RGBA')
                mask_alpha = mask_image.getchannel('A')
                if mask_key in polygons:
                    if mask_key not in polygon_cache:
                        feather,coordinates = polygons[mask_key]
                        polygon_cache[mask_key] = polygon_mask(mask_alpha.size,coordinates,feather)
                    mask_alpha = ImageChops.multiply(mask_alpha,polygon_cache[mask_key])
                mask_canvas = Image.new('L',base.size)
                mask_canvas.paste(mask_alpha.resize((mw,mh),Image.Resampling.NEAREST),(mx,my))
                inverse = ImageChops.invert(mask_canvas.crop((x,y,x+w,y+h)))
                patch.putalpha(ImageChops.multiply(patch.getchannel('A'),inverse))
            layer_alpha = Image.new('L',base.size)
            layer_alpha.paste(patch.getchannel('A'),(x,y))
            layers.append((selected[slot],layer_alpha))
            composite.alpha_composite(patch,(x,y))
        transmission = Image.new('L',base.size,255)
        for filename,alpha in reversed(layers):
            visible = ImageChops.multiply(alpha,transmission)
            count = sum(visible.histogram()[1:])
            visible_pixels[filename] = max(visible_pixels.get(filename,0),count)
            transmission = ImageChops.multiply(transmission,ImageChops.invert(alpha))
        composite.save(review/f'combined-{number}.png',compress_level=1)
        small = composite.resize((50,100),Image.Resampling.LANCZOS)
        small.save(review/f'combined-small-{number}.png')
        small_sheet.paste(small,((number-1)%4*50,(number-1)//4*100))
        full_sheet.paste(composite,((number-1)%4*887,(number-1)//4*1774))
    assert represented==allowed, (identifier,allowed-represented)
    (review/'coverage.json').write_text(json.dumps(dict(options=sorted(allowed),represented=sorted(represented),native=[887,1774],small=[50,100],combinations=len(choices),runtime_combinations=runtime_combinations,offline_without_helmet=diagnostic_combinations,visible_pixels=visible_pixels),indent=2)+'\n')
    small_sheet.save(review/'combined-small-sheet.png')
    full_sheet.save(review/'combined-native-sheet.jpg',quality=95)
    (review/'choices.json').write_text(json.dumps(choices,indent=2)+'\n')
    print(identifier,len(choices),'literal-manifest composites; visual acceptance pending')
