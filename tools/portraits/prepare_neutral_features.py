"""Register preserved isolated sources on neutral bases; never overwrite sources."""
from pathlib import Path
import hashlib, json, random
import argparse
from PIL import Image, ImageDraw, ImageChops, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
CAT = json.loads((ROOT/'tools/portraits/feature-catalog.json').read_text(encoding='utf-8-sig'))
LAYOUT = json.loads((ROOT/'tools/portraits/neutral-feature-layouts.json').read_text())
OUT = ROOT/'materials/portraits/neutral-variants'
# Shrink the alpha channel by "minimum" pixels and blur it by "blur" pixels; the result has the size of the input.
def softened_alpha(alpha,minimum,blur):
    # Transparent padding makes filtering fade rectangular image boundaries too.
    padding = minimum+round(blur*3)
    padded = Image.new('L',(alpha.width+2*padding,alpha.height+2*padding),0)
    padded.paste(alpha,(padding,padding))
    filtered = padded.filter(ImageFilter.MinFilter(minimum)).filter(ImageFilter.GaussianBlur(blur))
    return filtered.crop((padding,padding,padding+alpha.width,padding+alpha.height))

ORDER = ['build','outfit','hair','ears','eyes','nose','mouth','chin','helmet','scar','neck']
parser = argparse.ArgumentParser()
parser.add_argument('--portrait', action='append')
args = parser.parse_args()

for creature in CAT:
    identifier = creature['id']
    if identifier not in LAYOUT:
        continue
    if args.portrait and identifier not in args.portrait:
        continue
    base_path = ROOT/'materials/portraits/neutral-bases'/(identifier+'.png')
    base = Image.open(base_path).convert('RGBA')
    folder = OUT/identifier
    evidence = folder/'review'
    evidence.mkdir(parents=True, exist_ok=True)
    height_padding = 1774-base.height
    assert base.width==887 and height_padding in [0,1]
    if height_padding:
        padded = Image.new('RGBA',(887,1774))
        padded.paste(base,(0,0))
        padded.paste(base.crop((0,base.height-1,887,base.height)),(0,base.height))
        base = padded
    base.save(folder/'base.png',compress_level=1)
    (folder/'base-provenance.json').write_text(json.dumps(dict(
        source=base_path.relative_to(ROOT).as_posix(),
        source_sha256=hashlib.sha256(base_path.read_bytes()).hexdigest(),
        size=list(base.size),bottom_rows_repeated=height_padding,
        modification='repeat final row only' if height_padding else 'none'),indent=2)+'\n')
    rects = LAYOUT[identifier]
    jobs = list(creature['jobs'])
    records, patches = [], {}
    lines = ['# Neutral-base registration candidates; combined acceptance pending',
             'Base\tbase.png']
    lines += ['\t'.join(map(str,['Slot',s,*rects[s]])) for s in ORDER if s in rects]
    cells = []
    for job in creature['jobs']:
        filename = f"{job['slot']}-{job['n']}-{job['name']}.png"
        source = ROOT/'materials/portraits/generated-features'/identifier/filename
        image = Image.open(source).convert('RGBA')
        bounds = image.getchannel('A').point(lambda a:255 if a>=128 else 0).getbbox()
        l,t,r,b = bounds
        source_mask = None
        if identifier=='Dwarf1.mesh' and job['slot']=='hair' and job['n']==1:
            # The hair source also contains a complete beard: retain its side braid only.
            source_mask = [(80,180),(230,180),(280,440),(310,670),(345,850),(340,1100),
                           (425,1370),(325,1430),(245,1230),(155,950)]
            mask = Image.new('L',image.size,0)
            ImageDraw.Draw(mask).polygon(source_mask,fill=255)
            image.putalpha(ImageChops.multiply(image.getchannel('A'),mask.filter(ImageFilter.GaussianBlur(3))))
            l,t,r,b = image.getchannel('A').point(lambda a:255 if a>=128 else 0).getbbox()
        if identifier=='Orc.mesh-female' and job['slot']=='chin':
            cutoff = 730 if job['n']==1 else 530
            mask = Image.new('L',image.size,255)
            draw_mask = ImageDraw.Draw(mask)
            for row in range(cutoff-32,image.height):
                draw_mask.line((0,row,image.width-1,row),fill=max(0,min(255,round(255*(cutoff-row)/32))))
            image.putalpha(ImageChops.multiply(image.getchannel('A'),mask))
            b = cutoff
        x,y,w,h = rects[job['slot']]
        assert x>=0 and y>=0 and x+w<=base.width and y+h<=base.height
        padding = 8
        scale = min((w-2*padding)/(r-l),(h-2*padding)/(b-t))
        ox = (w-(r-l)*scale)/2-l*scale
        oy = (h-(b-t)*scale)/2-t*scale
        vertical_scale = scale
        if identifier=='Dwarf1.mesh' and job['slot']=='neck':
            target = (580,1060,180,220) if job['name']=='pin' else (300,860,335,470)
            tx,ty,tw,th = target
            scale = min((tw-16)/(r-l),(th-16)/(b-t))
            vertical_scale = scale if job['name']=='pin' else (th-16)/(b-t)
            ox = tx-x+(tw-(r-l)*scale)/2-l*scale
            oy = ty-y+(th-(b-t)*vertical_scale)/2-t*vertical_scale
        if job['slot']=='build' and not identifier.startswith('Kobold.mesh'):
            scale = w/(r-l)
            vertical_scale = (h+32)/(b-t)
            ox = -l*scale
            oy = -t*vertical_scale
        elif job['slot']=='build' or (job['slot']=='hair' and (identifier.startswith(('Monk.mesh','RunelordDwarf.mesh','Adventurer.mesh','Defender.mesh','Elf.mesh','DarkElf.mesh')) or identifier in ['Goblin.mesh-female','Dwarf2.mesh-female','Troll.mesh-female','Lizardman.mesh-female'])):
            oy = padding-t*scale
        if identifier=='Dwarf2.mesh-female' and job['slot']=='hair' and job['n']==2:
            oy += 40
        patch = image.transform((w,h),Image.Transform.AFFINE,
            (1/scale,0,-ox/scale,0,1/vertical_scale,-oy/vertical_scale),Image.Resampling.BICUBIC)
        if job['slot']=='build':
            original_alpha = patch.getchannel('A')
            # Body anchors span neck to frame bottom; soften cut-off skin contours.
            softened = patch.getchannel('A').filter(ImageFilter.MinFilter(31)).filter(ImageFilter.GaussianBlur(12))
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),softened))
            if identifier.startswith('Kobold.mesh'):
                # Remove old brown clothing; retain and blend the violet body contours.
                clothing = Image.new('L',(w,h))
                clothing.putdata([255 if red>blue*1.2 or green>blue*0.88 else 0
                    for red,green,blue,alpha in patch.getdata()])
                patch.putalpha(ImageChops.multiply(patch.getchannel('A'),ImageChops.invert(clothing)))
            if identifier.startswith('Orc.mesh'):
                clothing = Image.new('L',(w,h),0)
                vertices = [(215,815),(290,910),(450,990),(650,880),(735,1140),(680,1320),(195,1300),(180,1000)] if creature['gender']=='male' else [(230,880),(445,1020),(635,930),(710,1150),(680,1340),(230,1310),(160,1000)]
                ImageDraw.Draw(clothing).polygon([(px-x,py-y) for px,py in vertices],fill=255)
                patch.putalpha(ImageChops.lighter(patch.getchannel('A'),
                    ImageChops.multiply(original_alpha,clothing)))
        if identifier.startswith(('Adventurer.mesh','Defender.mesh','Wizard.mesh','Elf.mesh','Lizardman.mesh')) and job['slot']=='build':
            # Existing body sources include costume outside the neutral silhouette.
            body_mask = Image.new('L',(w,h))
            body_mask.putdata([255 if red>blue*1.15 and green>blue*.95 else 0
                for red,green,blue,alpha in base.crop((x,y,x+w,y+h)).getdata()])
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),body_mask.filter(ImageFilter.GaussianBlur(3))))
        if identifier.startswith('RunelordDwarf.mesh') and job['slot']=='hair':
            # Side-hair sources belong behind the existing bald head silhouette.
            face_mask = Image.new('L',(w,h),255)
            face_contour = [(365,265),(510,260),(600,350),(635,500),(620,650),(580,770),(400,810),(300,720),(270,570),(290,420)]
            ImageDraw.Draw(face_mask).polygon([(px-x,py-y) for px,py in face_contour],fill=0)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),face_mask.filter(ImageFilter.GaussianBlur(8))))
        if identifier=='Adventurer.mesh-female' and job['slot']=='hair':
            face_mask = Image.new('L',(w,h),255)
            contour = [(310,350),(560,350),(630,440),(620,600),(520,660),(370,650),(280,550),(265,425)]
            ImageDraw.Draw(face_mask).polygon([(px-x,py-y) for px,py in contour],fill=0)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),face_mask.filter(ImageFilter.GaussianBlur(8))))
        if identifier=='Troll.mesh-female' and job['slot']=='hair':
            face_mask = Image.new('L',(w,h),255)
            contour = [(220,495),(610,495),(650,620),(600,800),(520,880),(300,870),(180,650)]
            ImageDraw.Draw(face_mask).polygon([(px-x,py-y) for px,py in contour],fill=0)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),face_mask.filter(ImageFilter.GaussianBlur(8))))
        if identifier.startswith('DarkElf.mesh') and job['slot']=='hair':
            face_mask = Image.new('L',(w,h),255)
            contour = [(310,455),(570,455),(640,570),(600,730),(510,830),(380,815),(280,670),(260,545)]
            ImageDraw.Draw(face_mask).polygon([(px-x,py-y) for px,py in contour],fill=0)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),face_mask.filter(ImageFilter.GaussianBlur(8))))
        if identifier.startswith('DarkElf.mesh') and job['slot']=='build':
            body_mask = Image.new('L',(w,h))
            body_mask.putdata([255 if blue>red*1.1 and blue>green*1.05 and blue>40 else 0
                for red,green,blue,alpha in base.crop((x,y,x+w,y+h)).getdata()])
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),body_mask.filter(ImageFilter.GaussianBlur(3))))
        if identifier=='Elf.mesh' and job['slot']=='hair':
            face_mask = Image.new('L',(w,h),255)
            contour = [(310,510),(570,510),(650,640),(600,790),(510,885),(380,875),(280,730),(265,610)]
            ImageDraw.Draw(face_mask).polygon([(px-x,py-y) for px,py in contour],fill=0)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),face_mask.filter(ImageFilter.GaussianBlur(8))))
        if identifier.startswith('Defender.mesh') and job['slot']=='hair':
            head_mask = Image.new('L',(w,h))
            head_mask.putdata([255 if red>blue*1.15 and green>blue*.95 and y+index//w>= (0 if identifier=='Defender.mesh' else 430) and y+index//w<810 else 0
                for index,(red,green,blue,alpha) in enumerate(base.crop((x,y,x+w,y+h)).getdata())])
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),ImageChops.invert(head_mask.filter(ImageFilter.GaussianBlur(5)))))
        if identifier.startswith('Wizard.mesh') and job['slot']=='hair':
            head_mask = Image.new('L',(w,h))
            head_mask.putdata([255 if red>blue*1.15 and green>blue*.95 and y+index//w<900 else 0
                for index,(red,green,blue,alpha) in enumerate(base.crop((x,y,x+w,y+h)).getdata())])
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),ImageChops.invert(head_mask.filter(ImageFilter.GaussianBlur(5)))))
        if identifier=='Gnome.mesh-female' and job['slot']=='hair':
            head_mask = Image.new('L',(w,h),255)
            head_contour = [(310,250),(480,240),(600,320),(650,500),(620,700),(570,790),(380,820),(260,700),(230,500),(250,350)]
            ImageDraw.Draw(head_mask).polygon([(px-x,py-y) for px,py in head_contour],fill=0)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),head_mask.filter(ImageFilter.GaussianBlur(8))))
        if identifier=='Dwarf1.mesh' and job['slot']=='hair':
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),softened_alpha(patch.getchannel('A'),7,3)))
        if identifier.startswith('Cultist.mesh') and job['slot']=='build':
            # Existing build sources include a bright old collar; the selected new robe
            # defines the neckline, so keep the old garment below that transition.
            neckline = Image.new('L',(w,h),255)
            draw = ImageDraw.Draw(neckline)
            for row in range(min(260,h)):
                draw.line((0,row,w-1,row),fill=max(0,min(255,round(255*(row-200)/60))))
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),neckline))
        if identifier.startswith('Kobold.mesh') and job['slot']=='neck' and job['name']=='torque':
            # The open ring's rear tips pass behind this neutral neck silhouette.
            mask = Image.new('L',(w,h),255)
            ImageDraw.Draw(mask).polygon([(360-x,735-y),(530-x,735-y),
                (515-x,830-y),(380-x,830-y)],fill=0)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),mask.filter(ImageFilter.GaussianBlur(3))))
        if identifier.startswith('Orc.mesh') and job['slot']=='neck' and job['name']=='torque':
            mask = Image.new('L',(w,h),255)
            vertices = [(365,810),(580,815),(620,910),(325,910)] if creature['gender']=='male' else [(330,755),(585,755),(575,855),(325,855)]
            ImageDraw.Draw(mask).polygon([(px-x,py-y) for px,py in vertices],fill=0)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),mask.filter(ImageFilter.GaussianBlur(3))))
        if not identifier.startswith(('Orc.mesh','Goblin.mesh')) and job['slot'] in ['eyes','nose','mouth','chin']:
            softened = softened_alpha(patch.getchannel('A'),13,5)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),softened))
        if identifier.startswith('Goblin.mesh') and job['slot'] in ['eyes','nose','mouth','chin']:
            # Blend the skin surrounding facial features into the neutral face.
            radius = 21 if job['slot']=='chin' else 13
            softened = softened_alpha(patch.getchannel('A'),radius,6)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),softened))
        if identifier.startswith('Orc.mesh') and job['slot']=='chin':
            # Fade surrounding skin into the base while retaining the jaw's centre.
            softened = softened_alpha(patch.getchannel('A'),13,5)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),softened))
        if identifier.startswith('Orc.mesh') and job['slot'] in ['eyes','nose','mouth']:
            softened = softened_alpha(patch.getchannel('A'),7,3)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),softened))
            if job['slot']=='mouth':
                edge = Image.new('L',(w,h),255)
                for row in range(max(0,h-35),h):
                    ImageDraw.Draw(edge).line((0,row,w-1,row),fill=round(255*(h-1-row)/35))
                patch.putalpha(ImageChops.multiply(patch.getchannel('A'),edge))
        if identifier in ['Defender.mesh-female','Elf.mesh-male','Elf.mesh','DarkElf.mesh-male','DarkElf.mesh','Lizardman.mesh-female'] and job['slot']=='chin':
            mouth_mask = Image.new('L',base.size,0)
            for mouth_job in [j for j in creature['jobs'] if j['slot']=='mouth']:
                mouth_name = f"mouth-{mouth_job['n']}-{mouth_job['name']}.png"
                layer = Image.new('L',base.size,0)
                layer.paste(patches[mouth_name].getchannel('A'),tuple(rects['mouth'][:2]))
                mouth_mask = ImageChops.lighter(mouth_mask,layer)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),ImageChops.invert(mouth_mask.crop((x,y,x+w,y+h)))))
        if identifier in ['Defender.mesh-female','Elf.mesh-male','Elf.mesh','DarkElf.mesh-male','DarkElf.mesh','Lizardman.mesh-female'] and job['slot']=='chin':
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),softened_alpha(patch.getchannel('A'),21,9)))
        if identifier=='Dwarf1.mesh-female' and job['slot']=='chin':
            softened = softened_alpha(patch.getchannel('A'),13,5)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),softened))
        if identifier=='Dwarf1.mesh-female' and job['slot']=='neck' and job['name']=='torque':
            mask = Image.new('L',(w,h),255)
            vertices = [(320,825),(565,825),(590,920),(300,920)]
            ImageDraw.Draw(mask).polygon([(px-x,py-y) for px,py in vertices],fill=0)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),mask.filter(ImageFilter.GaussianBlur(3))))
        if ((identifier=='Dwarf1.mesh' and job['name']=='tooth') or identifier in ['Dwarf2.mesh','RunelordDwarf.mesh']) and job['slot']=='neck':
            beard = Image.new('L',base.size,0)
            for beard_job in [j for j in creature['jobs'] if j['slot']=='chin']:
                name = f"chin-{beard_job['n']}-{beard_job['name']}.png"
                layer = Image.new('L',base.size,0)
                layer.paste(patches[name].getchannel('A'),tuple(rects['chin'][:2]))
                beard = ImageChops.lighter(beard,layer)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),ImageChops.invert(beard.crop((x,y,x+w,y+h)))))
        if job['slot']=='scar' and 'helmet' not in rects:
            # Bake skin-only placement into the PNG; the frozen contract has no inverse Clip.
            foreground = Image.new('L',base.size,0)
            for preceding_job in creature['jobs']:
                if preceding_job['slot'] not in ['hair','ears']:
                    continue
                preceding_name = f"{preceding_job['slot']}-{preceding_job['n']}-{preceding_job['name']}.png"
                assert preceding_name in patches, 'Hair and ears must be prepared before scars'
                layer = Image.new('L',base.size,0)
                layer.paste(patches[preceding_name].getchannel('A'),tuple(rects[preceding_job['slot']][:2]))
                foreground = ImageChops.lighter(foreground,layer)
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),ImageChops.invert(foreground.crop((x,y,x+w,y+h)))))
        patch.save(folder/filename,compress_level=1)
        patches[filename] = patch
        composite = base.copy()
        composite.alpha_composite(patch,(x,y))
        composite.save(evidence/filename,compress_level=1)
        composite.resize((50,100),Image.Resampling.LANCZOS).save(evidence/('small-'+filename))
        cells.append((filename,composite))
        records.append(dict(file=filename,source=source.relative_to(ROOT).as_posix(),
            source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),source_bounds=list(bounds),
            rect=[x,y,w,h],scale=scale,vertical_scale=vertical_scale,offset=[ox,oy],visual_acceptance='pending',
            source_lower_cutoff=b if identifier=='Orc.mesh-female' and job['slot']=='chin' else None,
            static_foreground_avoidance='union of all hair and ear alpha' if job['slot']=='scar' and 'helmet' not in rects else None,
            source_mask_polygon=source_mask,
            base_sha256=hashlib.sha256(base_path.read_bytes()).hexdigest()))
        lines.append('\t'.join(map(str,['Option',job['slot'],job['n'],job['name'],filename])))
    if 'helmet' in rects:
        helmet_folder = ROOT/'materials/portraits/helmet-variants'/identifier
        for original in json.loads((helmet_folder/'placement.json').read_text()):
            filename = original['file']
            patch = Image.open(helmet_folder/filename).convert('RGBA')
            registered = dict(original)
            if identifier.startswith('Knight.mesh'):
                # Cover the neutral head's left contour without changing the source helmet.
                sx,sy,cx,cy = 1.12,1.06,380,388
                patch = patch.transform(patch.size,Image.Transform.AFFINE,
                    (1/sx,0,cx*(1-1/sx),0,1/sy,cy*(1-1/sy)),Image.Resampling.BICUBIC)
                registered['horizontal_scale'] = original['horizontal_scale']*sx
                registered['scale'] = original['scale']*sy
                registered['offset'] = [(1-sx)*cx+original['offset'][0]*sx,(1-sy)*cy+original['offset'][1]*sy]
                registered['neutral_head_fit'] = dict(horizontal=sx,vertical=sy,anchor=[cx,cy])
            assert patch.size==tuple(rects['helmet'][2:])
            patch.save(folder/filename,compress_level=1)
            patches[filename] = patch
            composite = base.copy()
            composite.alpha_composite(patch,tuple(rects['helmet'][:2]))
            composite.save(evidence/filename,compress_level=1)
            composite.resize((50,100),Image.Resampling.LANCZOS).save(evidence/('small-'+filename))
            cells.append((filename,composite))
            records.append(dict(**{k:v for k,v in registered.items() if k not in ['visual_acceptance','base_sha256']},
                visual_acceptance='pending',base_sha256=hashlib.sha256(base_path.read_bytes()).hexdigest()))
            jobs.append(dict(slot='helmet',n=original['n'],name=original['name'],file=filename))
            lines.append('\t'.join(map(str,['Option','helmet',original['n'],original['name'],filename])))
        # Bake helmet-damage clipping into the shared scar part for every helmet option.
        common = Image.new('L',base.size,255)
        for helmet_job in [j for j in jobs if j['slot']=='helmet']:
            alpha = Image.new('L',base.size,0)
            alpha.paste(patches[helmet_job['file']].getchannel('A'),tuple(rects['helmet'][:2]))
            common = ImageChops.darker(common,alpha)
        for scar_job in [j for j in jobs if j['slot']=='scar']:
            filename = f"scar-{scar_job['n']}-{scar_job['name']}.png"
            patch = patches[filename]
            x,y,w,h = rects['scar']
            patch.putalpha(ImageChops.multiply(patch.getchannel('A'),common.crop((x,y,x+w,y+h))))
            patch.save(folder/filename,compress_level=1)
            composite = base.copy()
            composite.alpha_composite(patch,(x,y))
            composite.save(evidence/filename,compress_level=1)
            composite.resize((50,100),Image.Resampling.LANCZOS).save(evidence/('small-'+filename))
    outfit_records = []
    if (folder/'outfit-placement.json').exists():
        outfit_records = json.loads((folder/'outfit-placement.json').read_text())
        for fitted in outfit_records:
            if fitted['build']:
                continue
            filename = fitted['file']
            patch = Image.open(folder/filename).convert('RGBA')
            assert patch.size==base.size
            patches[filename] = patch
            composite = base.copy()
            composite.alpha_composite(patch)
            composite.save(evidence/filename,compress_level=1)
            composite.resize((50,100),Image.Resampling.LANCZOS).save(evidence/('small-'+filename))
            cells.append((filename,composite))
            records.append(dict(**fitted,rect=rects['outfit'],base_sha256=hashlib.sha256(base_path.read_bytes()).hexdigest()))
            jobs.append(dict(slot='outfit',n=fitted['n'],name=fitted['name'],file=filename))
            lines.append('\t'.join(map(str,['Option','outfit',fitted['n'],fitted['name'],filename])))

    rng = random.Random(1001)
    choices = []
    small_board = Image.new('RGB',(4*50,3*100))
    for number in range(12):
        composite = base.copy()
        selected = {}
        selected_build = number//3 if outfit_records and number<9 else rng.choice([0,1,2]) if outfit_records else None
        for slot in ORDER:
            options = [j for j in jobs if j['slot']==slot]
            if not options:
                continue
            if slot=='build' and selected_build==0:
                continue
            job = options[number%len(options)]
            if slot=='build' and selected_build:
                job = next(j for j in options if j['n']==selected_build)
            if slot=='outfit' and number<9:
                job = options[number%len(options)]
            filename = job.get('file',f"{slot}-{job['n']}-{job['name']}.png")
            selected_patch = patches[filename].copy()
            if slot=='scar' and 'helmet' in selected:
                x,y,w,h = rects[slot]
                helmet_alpha = Image.new('L',base.size,0)
                helmet_alpha.paste(patches[selected['helmet']].getchannel('A'),tuple(rects['helmet'][:2]))
                selected_patch.putalpha(ImageChops.multiply(selected_patch.getchannel('A'),helmet_alpha.crop((x,y,x+w,y+h))))
            composite.alpha_composite(selected_patch,tuple(rects[slot][:2]))
            selected[slot] = filename
        composite.save(evidence/f'combined-{number+1}.png',compress_level=1)
        composite.resize((50,100),Image.Resampling.LANCZOS).save(evidence/f'combined-small-{number+1}.png')
        small_board.paste(composite.resize((50,100),Image.Resampling.LANCZOS),(number%4*50,number//4*100))
        choices.append(selected)
        cells.append((f'combined-{number+1}',composite))
    board = Image.new('RGB',(6*222,((len(cells)+5)//6)*464),(30,30,30))
    draw = ImageDraw.Draw(board)
    for number,(label,composite) in enumerate(cells):
        x,y = number%6*222,number//6*464
        board.paste(composite.resize((222,444),Image.Resampling.LANCZOS),(x,y))
        draw.text((x+2,y+446),label,fill='white')
    board.save(evidence/'overview.jpg',quality=95)
    small_board.save(evidence/'combined-small-sheet.png')
    (folder/'placement.json').write_text(json.dumps(records,indent=2)+'\n')
    (folder/'composition-rules.json').write_text(json.dumps(dict(
        draw_order=ORDER,face_visibility='literal source-over in frozen manifest order',
        scar_visibility='non-helmet scars have a baked inverse union hair/ears mask; helmet damage is baked to the common helmet alpha',
        rear_neck_mask='torque hidden behind species neck polygon for Kobold and Orc only',
        build_mask='softened skin contour; Kobold colour mask and Orc clothing polygon preserve clothing alpha',
        runtime_integration='separate task',combined_visual_acceptance='pending'),indent=2)+'\n')
    (evidence/'choices.json').write_text(json.dumps(choices,indent=2)+'\n')
    (folder/'manifest.cfg').write_text('\n'.join(lines)+'\n')
    prompts = json.loads((ROOT/'materials/portraits/generated-features'/identifier/'generation-records.json').read_text(encoding='utf-8-sig'))
    (folder/'prompts.md').write_text('\n\n'.join('## '+r['file']+'\n\n'+r['prompt'] for r in prompts)+'\n',encoding='utf-8')
    if 'helmet' in rects:
        with (folder/'prompts.md').open('a',encoding='utf-8') as stream:
            stream.write('\n'+(helmet_folder/'prompts.md').read_text(encoding='utf-8'))
    if outfit_records:
        with (folder/'prompts.md').open('a',encoding='utf-8') as stream:
            stream.write('\n\n'.join('## '+r['file']+'\n\n'+r['prompt'] for r in outfit_records if not r['build'])+'\n')
    print(identifier,len(records),'registered candidates; visual acceptance pending',flush=True)
