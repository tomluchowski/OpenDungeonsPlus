"""Place existing feature images; never generates or changes source images."""
from pathlib import Path
import json, hashlib, random
from PIL import Image, ImageDraw, ImageFilter, ImageChops
ROOT=Path(__file__).resolve().parents[2]; OUT=ROOT/'materials/portraits/variants'; REVIEW=ROOT/'work/existing-feature-review'
CAT=json.loads((ROOT/'tools/portraits/feature-catalog.json').read_text(encoding='utf-8-sig'))
LAYOUT=json.loads((ROOT/'tools/portraits/feature-layouts.json').read_text())
ORDER=['build','hair','ears','eyes','nose','mouth','chin','scar','neck']
for c in CAT:
    identifier=c['id']; folder=OUT/identifier; folder.mkdir(parents=True,exist_ok=True)
    review=REVIEW/identifier; review.mkdir(parents=True,exist_ok=True)
    if (folder/'placement.json').exists() and (review/'sheet-50x100.png').exists() and all(r.get('method')=='visible-alpha-v2' and r['rect']==LAYOUT[identifier][r['file'].split('-')[0]] for r in json.loads((folder/'placement.json').read_text())):
        print(identifier,'already prepared',flush=True); continue
    print('Preparing',identifier,flush=True)
    base=Image.open(ROOT/c['reference']).convert('RGBA'); rects=LAYOUT[identifier]
    lines=['# Placement candidates; visual exceptions: ../corrections.json','Image\t'+Path(c['reference']).name]
    lines += ['\t'.join(map(str,['Slot',slot,*rects[slot]])) for slot in ORDER if slot in rects]
    sourcefolder=ROOT/'materials/portraits/generated-features'/identifier
    prompts=json.loads((sourcefolder/'generation-records.json').read_text(encoding='utf-8-sig'))
    records=[]; patches={}; board=Image.new('RGB',(6*180,4*380),(35,35,35)); draw=ImageDraw.Draw(board)
    for k,j in enumerate(c['jobs']):
        name=f"{j['slot']}-{j['n']}-{j['name']}.png"; source=sourcefolder/name
        im=Image.open(source).convert('RGBA'); l,t,r,b=im.getchannel('A').point(lambda value: 255 if value>=128 else 0).getbbox(); x,y,w,h=rects[j['slot']]
        padding=16 if j['slot'] not in ['build'] else 0
        scale=min((w-2*padding)/(r-l),(h-2*padding)/(b-t))
        ox=(w-(r-l)*scale)/2-l*scale; oy=(h-(b-t)*scale)/2-t*scale
        if j['slot']=='hair': oy=padding-t*scale
        patch=im.transform((w,h),Image.Transform.AFFINE,(1/scale,0,-ox/scale,0,1/scale,-oy/scale),Image.Resampling.BICUBIC)
        # Fade only the outer patch margin; preserve all original feature pixels internally.
        edge=Image.new('L',(w,h),0); ed=ImageDraw.Draw(edge)
        for inset in range(16): ed.rectangle((inset,inset,w-1-inset,h-1-inset),outline=round(255*(inset+1)/16))
        ed.rectangle((16,16,w-17,h-17),fill=255)
        alpha=ImageChops.multiply(patch.getchannel('A'),edge)
        if j['slot'] in ['scar','neck'] or 'patch' in j['name'] or j['name'] in ['pipe','cigar']:
            alpha=ImageChops.multiply(alpha,alpha.filter(ImageFilter.GaussianBlur(1)))
        patch.putalpha(alpha); patch.save(folder/name,compress_level=1); patches[name]=patch
        lines.append('\t'.join(map(str,['Option',j['slot'],j['n'],j['name'],name])))
        comp=base.copy(); comp.alpha_composite(patch,(x,y)); comp.save(review/name,compress_level=1)
        comp.resize((50,100),Image.Resampling.LANCZOS).save(review/('small-'+name))
        board.paste(comp.resize((180,360),Image.Resampling.LANCZOS),(k%6*180,k//6*380))
        draw.text((k%6*180+3,k//6*380+362),name.replace('.png',''),fill='white')
        records.append(dict(method='visible-alpha-v2',file=name,source=str(source.relative_to(ROOT)).replace('\\','/'),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),rect=[x,y,w,h],scale=scale,offset=[ox,oy],visual_acceptance='pending'))
    board.save(review/'options.jpg',quality=94)
    sheet=Image.new('RGB',(4*222,3*444)); small=Image.new('RGB',(4*50,3*100)); rng=random.Random(1001); choices=[]
    for k in range(12):
        comp=base.copy(); selected={}
        for slot in ORDER:
            options=[r for r in records if r['file'].startswith(slot+'-')]
            n=rng.randrange(len(options)+1); selected[slot]=n
            if n: comp.alpha_composite(patches[options[n-1]['file']],tuple(rects[slot][:2]))
        comp.save(review/f'composite-{k+1}.png',compress_level=1); sheet.paste(comp.resize((222,444),Image.Resampling.LANCZOS),(k%4*222,k//4*444)); small.paste(comp.resize((50,100),Image.Resampling.LANCZOS),(k%4*50,k//4*100)); choices.append(selected)
    sheet.save(review/'sheet.png'); small.save(review/'sheet-50x100.png')
    (review/'choices.json').write_text(json.dumps(choices,indent=2)+'\n')
    (folder/'manifest.cfg').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    (folder/'prompts.md').write_text('\n\n'.join('## '+r['file']+'\n\n'+r['prompt'] for r in prompts)+'\n',encoding='utf-8')
    (folder/'placement.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8')
    print(identifier,len(records),flush=True)
print('Prepared',sum(len(c['jobs']) for c in CAT),'existing features across',len(CAT),'portraits',flush=True)

# Keep recorded visual exceptions separate from technical format validation.
if (OUT/'corrections.json').exists():
    exceptions=json.loads((OUT/'corrections.json').read_text(encoding='utf-8'))
    flagged={(r['portrait'],r['file']) for r in exceptions['entries']}
    for c in CAT:
        path=OUT/c['id']/'placement.json'
        records=json.loads(path.read_text())
        for r in records:
            r['visual_acceptance']='needs-correction' if (c['id'],r['file']) in flagged else 'native-review-pending'
        path.write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8')
