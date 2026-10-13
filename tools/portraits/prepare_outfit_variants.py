"""Prepare separate registered/fitted clothing copies; generator PNGs stay untouched."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--portrait',required=True)
args = parser.parse_args()
head_bottom = {'Orc.mesh':850,'Orc.mesh-female':730,'Knight.mesh':620,'Knight.mesh-female':620,'Kobold.mesh':850,'Kobold.mesh-female':820,'Dwarf1.mesh':820,'Dwarf1.mesh-female':820,'Goblin.mesh':855,'Goblin.mesh-female':805,'Dwarf2.mesh':850,'Dwarf2.mesh-female':820,'RunelordDwarf.mesh':775,'RunelordDwarf.mesh-female':775,'Gnome.mesh':815,'Gnome.mesh-female':800,'Adventurer.mesh':630,'Adventurer.mesh-female':630,'Monk.mesh':700,'Monk.mesh-female':690,'Defender.mesh':790,'Defender.mesh-female':780,'Wizard.mesh':930,'Wizard.mesh-female':970,'Elf.mesh-male':940,'Elf.mesh':975,'DarkElf.mesh-male':865,'DarkElf.mesh':820,'Troll.mesh':935,'Troll.mesh-female':860,'Lizardman.mesh':835,'Lizardman.mesh-female':780,'Cultist.mesh':770,'Cultist.mesh-female':815}[args.portrait]
folder = ROOT/'materials/portraits/neutral-variants'/args.portrait
raw = ROOT/'materials/portraits/generated-outfits'/args.portrait
base = Image.open(folder/'base.png').convert('RGBA')
review = folder/'review/outfits'
review.mkdir(parents=True,exist_ok=True)
feature_records = json.loads((folder/'placement.json').read_text())
builds = [r for r in feature_records if r['file'].startswith('build-')]
# True when the pixel is a skin pixel of the chosen portrait; the colour rule depends on the species.
def is_skin(red,green,blue):
    if args.portrait.startswith('Troll.mesh'):
        return red>=green>=blue and red-blue<65 and red>35
    if args.portrait.startswith('DarkElf.mesh'):
        return blue>red*1.1 and blue>green*1.05
    return red>green and blue>green*1.2 if args.portrait.startswith('Kobold.mesh') else red>blue*1.15 and green>blue*.95

skin = Image.new('L',base.size)
skin.putdata([255 if y>=head_bottom+20 and is_skin(red,green,blue) else 0
              for y in range(base.height) for red,green,blue,alpha in
              [base.getpixel((x,y)) for x in range(base.width)]])
# The delivered clothing must cover both preserved build options as well as the neutral body.
for build in builds:
    layer = Image.new('RGBA',base.size)
    layer.alpha_composite(Image.open(folder/build['file']).convert('RGBA'),tuple(build['rect'][:2]))
    mask = Image.new('L',base.size)
    mask.putdata([255 if index//887>=head_bottom+20 and alpha>=128 and is_skin(red,green,blue) else 0
                  for index,(red,green,blue,alpha) in enumerate(layer.getdata())])
    skin = ImageChops.lighter(skin,mask)
small_skin = skin.resize((89,177),Image.Resampling.NEAREST)
skin_pixels = sum(v>0 for v in small_skin.getdata())
records = []


for metadata in sorted(raw.glob('outfit-*.json')):
    job = json.loads(metadata.read_text())
    source = raw/job['file']
    assert source.read_bytes()==Path(job['original']).read_bytes(), 'Generator source changed'
    original = Image.open(source).convert('RGBA')
    source_size = original.size
    repeated_rows = base.height-original.height
    if original.width==base.width and repeated_rows==1:
        padded = Image.new('RGBA',base.size)
        padded.paste(original,(0,0))
        padded.paste(original.crop((0,original.height-1,original.width,original.height)),(0,original.height))
        original = padded
    if original.size!=base.size:
        original = original.resize(base.size,Image.Resampling.LANCZOS)
    assert original.size==base.size and original.getchannel('A').getextrema()==(0,255)
    # Bounded affine registration uses measured uncovered-body pixels, not source edits.
    small = original.resize((89,177),Image.Resampling.LANCZOS)
    best = None
    for horizontal in [1+i*.025 for i in range(21 if args.portrait.startswith(('Kobold.mesh','Goblin.mesh')) else 11)]:
        for vertical in [1+i*.02 for i in range(6)]:
            ox = 44.5*(1-horizontal)
            oy = 72*(1-vertical)
            alpha = small.transform(small.size,Image.Transform.AFFINE,
                (1/horizontal,0,-ox/horizontal,0,1/vertical,-oy/vertical),Image.Resampling.BICUBIC).getchannel('A')
            uncovered = sum(s>0 and a<200 for s,a in zip(small_skin.getdata(),alpha.getdata()))
            score = uncovered+5*(horizontal-1)+5*(vertical-1)
            if best is None or score<best[0]:
                best = score,horizontal,vertical,uncovered
    _,horizontal,vertical,uncovered = best
    ox = 443.5*(1-horizontal)
    oy = 720*(1-vertical)
    registered = original.transform(base.size,Image.Transform.AFFINE,
        (1/horizontal,0,-ox/horizontal,0,1/vertical,-oy/vertical),Image.Resampling.BICUBIC)
    # Reserve a torso margin for the narrowest permitted fitted build; keep the neck fixed.
    margin = Image.new('RGBA',base.size)
    for y in range(base.height):
        ratio = 1+(1/.9-1)*min(1,max(0,(y-850)/250))
        row = registered.crop((0,y,887,y+1))
        offset = 443.5*(1-ratio)
        margin.paste(row.transform((887,1),Image.Transform.AFFINE,
            (1/ratio,0,-offset/ratio,0,1,0),Image.Resampling.BICUBIC),(0,y))
    registered = margin
    if args.portrait.startswith('Knight.mesh'):
        # Register sleeve width against the neutral forearm contour, keeping torso pixels fixed.
        sleeves = Image.new('RGBA',base.size)
        alpha = registered.getchannel('A')
        for y in range(1350,1774) if args.portrait.startswith('Kobold.mesh') else range(1050,1600):
            row = registered.crop((0,y,887,y+1))
            for reverse in [False,True]:
                xs = list(range(220)) if not reverse else list(range(886,666,-1))
                garment = next((i for i,x in enumerate(xs) if alpha.getpixel((x,y))<20),220)
                body = next((i for i,x in enumerate(xs) if not skin.getpixel((x,y))),220)
                if garment<3 or body<=garment:
                    continue
                width = min(220,body+4)
                scale = width/max(1,garment-2)
                left = 0 if not reverse else 887-width
                origin = 0 if not reverse else 887-width/scale
                fitted_row = row.transform((width,1),Image.Transform.AFFINE,
                    (1/scale,0,origin,0,1,0),Image.Resampling.BICUBIC)
                sleeves.paste(fitted_row,(left,y))
        missing = ImageChops.multiply(skin,ImageChops.invert(alpha))
        sleeves.putalpha(ImageChops.multiply(sleeves.getchannel('A'),missing))
        registered.alpha_composite(sleeves)
    if not args.portrait.startswith('Knight.mesh'):
        # Extend existing sleeve texture with continuous fixed-sector transforms.
        # The fit affects only measured uncovered body; raw generator PNGs stay intact.
        alpha = registered.getchannel('A')
        missing = ImageChops.multiply(skin,ImageChops.invert(alpha))
        if args.portrait.startswith(('Kobold.mesh','Goblin.mesh','Defender.mesh','DarkElf.mesh','Lizardman.mesh')):
            missing = missing.filter(ImageFilter.MaxFilter(9)).filter(ImageFilter.GaussianBlur(2))
        extension = Image.new('RGBA',base.size)
        for y in range(head_bottom+20,base.height):
            ramp = min(1,max(0,(y-head_bottom-20)/240))
            factor = 1+(1.5 if args.portrait.startswith(('Kobold.mesh','Defender.mesh','DarkElf.mesh','Lizardman.mesh')) else 0.9)*ramp
            width = round(240*factor)
            row = registered.crop((0,y,240,y+1)).resize((width,1),Image.Resampling.BICUBIC)
            extension.paste(row,(0,y))
            row = registered.crop((647,y,887,y+1)).resize((width,1),Image.Resampling.BICUBIC)
            extension.paste(row,(887-width,y))
        extension.putalpha(ImageChops.multiply(extension.getchannel('A'),missing))
        registered.alpha_composite(extension)
    if args.portrait.startswith(('Defender.mesh','DarkElf.mesh','Lizardman.mesh')):
        missing = ImageChops.multiply(skin,ImageChops.invert(registered.getchannel('A')))
        shifted = Image.new('RGBA',base.size)
        shifted.paste(registered.transform(base.size,Image.Transform.AFFINE,(1,0,80,0,1,0),Image.Resampling.BICUBIC).crop((0,0,443,1774)),(0,0))
        shifted.paste(registered.transform(base.size,Image.Transform.AFFINE,(1,0,-80,0,1,0),Image.Resampling.BICUBIC).crop((443,0,887,1774)),(443,0))
        shifted.putalpha(ImageChops.multiply(shifted.getchannel('A'),missing))
        registered.alpha_composite(shifted)
    if args.portrait.startswith('Lizardman.mesh'):
        missing = ImageChops.multiply(skin,ImageChops.invert(registered.getchannel('A')))
        shifted = Image.new('RGBA',base.size)
        shifted.paste(registered.transform(base.size,Image.Transform.AFFINE,(1,0,160,0,1,0),Image.Resampling.BICUBIC).crop((0,0,443,1774)),(0,0))
        shifted.paste(registered.transform(base.size,Image.Transform.AFFINE,(1,0,-160,0,1,0),Image.Resampling.BICUBIC).crop((443,0,887,1774)),(443,0))
        shifted.putalpha(ImageChops.multiply(shifted.getchannel('A'),missing))
        registered.alpha_composite(shifted)
    # Collars pass behind the existing neutral head, never across its chin.
    head = Image.new('L',base.size)
    head.putdata([255 if y<head_bottom and is_skin(red,green,blue) else 0
                  for y in range(base.height) for red,green,blue,alpha in
                  [base.getpixel((x,y)) for x in range(base.width)]])
    if args.portrait.startswith('Kobold.mesh'):
        # Mask only the measured head silhouette, avoiding a horizontal neck cut.
        head = Image.new('L',base.size)
        polygon = [(180,150),(670,150),(670,680),(595,805),(510,845),(335,795),(245,665),(175,400)] if args.portrait=='Kobold.mesh' else [(180,120),(665,120),(670,650),(600,750),(500,790),(340,775),(235,645),(175,400)]
        ImageDraw.Draw(head).polygon(polygon,fill=255)
        head = head.filter(ImageFilter.GaussianBlur(3))
    if args.portrait.startswith('Troll.mesh'):
        head = Image.new('L',base.size)
        polygon = [(200,470),(390,400),(600,400),(635,500),(650,750),(625,850),(555,900),(350,925),(250,890),(200,760),(180,620)] if args.portrait=='Troll.mesh' else [(180,350),(600,350),(640,500),(655,700),(600,850),(520,920),(300,920),(180,750)]
        ImageDraw.Draw(head).polygon(polygon,fill=255)
        head = head.filter(ImageFilter.GaussianBlur(4))
    if not args.portrait.startswith(('Knight.mesh','Kobold.mesh')):
        # Fade the neck boundary instead of cutting a horizontal strip at the collar.
        transition = Image.new('L',base.size,255)
        draw = ImageDraw.Draw(transition)
        for y in range(max(0,head_bottom-40),base.height):
            draw.line((0,y,886,y),fill=max(0,min(255,round(255*(head_bottom-y)/40))))
        head = ImageChops.multiply(head,transition)
    if not args.portrait.startswith('Knight.mesh'):
        registered.putalpha(ImageChops.multiply(registered.getchannel('A'),ImageChops.invert(head)))
    variants = [(0,registered,None)]
    for number,patch,warp in variants:
        filename = f"outfit-{job['n']}-{job['name']}-build{number}.png"
        patch.save(folder/filename,compress_level=1)
        composite = base.copy()
        if number:
            build = next(r for r in builds if int(r['file'].split('-')[1])==number)
            composite.alpha_composite(Image.open(folder/build['file']).convert('RGBA'),tuple(build['rect'][:2]))
        composite.alpha_composite(patch)
        composite.save(review/filename,compress_level=1)
        composite.resize((50,100),Image.Resampling.LANCZOS).save(review/('small-'+filename))
        records.append(dict(id=args.portrait,n=job['n'],name=job['name'],build=number,file=filename,
            source=source.relative_to(ROOT).as_posix(),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
            source_size=list(source_size),canvas_registration='resampled to canonical canvas' if source_size not in [(887,1774),(887,1773)] else 'canonical canvas',body_span_fit=('continuous outer sleeve sector extensions and 80-pixel inner-gap translations clipped to uncovered body' if args.portrait.startswith(('Defender.mesh','DarkElf.mesh','Lizardman.mesh')) else 'continuous outer sleeve sector extensions clipped to uncovered body') if not args.portrait.startswith('Knight.mesh') else None,bottom_rows_repeated=max(0,repeated_rows),horizontal_scale=horizontal,vertical_scale=vertical,offset=[ox,oy],mesh_warp=warp,
            torso_margin=1/.9,sleeve_warp='row-wise sleeve width registered to neutral forearm bounds, outer 220-pixel sectors, rows 1050-1599' if args.portrait.startswith('Knight.mesh') else None,head_occlusion='mandatory helmet covers collar' if args.portrait.startswith('Knight.mesh') else dict(measured_head_polygon=polygon) if args.portrait.startswith(('Kobold.mesh','Troll.mesh')) else dict(warm_neutral_skin_above_row=head_bottom),
            native_uncovered_body_pixels=ImageChops.multiply(skin,patch.getchannel('A').point(lambda a:255 if a<200 else 0)).histogram()[255],
            coarse_uncovered_body_pixels=uncovered,coarse_body_pixels=skin_pixels,
            visual_acceptance='pending',prompt=job['prompt']+'\n\nCorrection prompt: '+job['correction_prompt'] if job.get('correction_prompt') else job['prompt']))
(folder/'outfit-placement.json').write_text(json.dumps(records,indent=2)+'\n')
print(json.dumps(dict(portrait=args.portrait,outfits=len({r['n'] for r in records}),fitted_copies=sum(r['build']>0 for r in records),visual_acceptance='pending')))
