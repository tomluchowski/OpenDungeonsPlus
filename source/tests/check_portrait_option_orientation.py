"""Noncompiling checks for frozen portrait sources and option-only geometry."""
from pathlib import Path
import hashlib
import importlib.util
import subprocess
import sys
from PIL import Image, ImageChops
ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('manifest_check', ROOT/'scripts/check_portrait_manifests.py')
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)
indexed = subprocess.check_output(['git','ls-files','-s'],cwd=ROOT,text=True).splitlines()
protected = 0
for line in indexed:
    info,name = line.split('\t',1)
    if 'portrait' not in name.lower() or not name.lower().endswith(('.png','.jpg','.jpeg')):
        continue
    data = (ROOT/name).read_bytes()
    git_hash = hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
    assert git_hash==info.split()[1], 'Changed portrait raster: '+name
    protected += 1
assert protected>=1696
catalogs = sorted((ROOT/'materials/portraits/variants').glob('*/manifest.cfg'))
assert len(catalogs)==34
for path in catalogs:
    check.check(path)
    rows = [r.split('\t') for r in path.read_text().splitlines() if r and not r.startswith('#')]
    options = {r[4] for r in rows if r[0]=='Option'}
    import json
    choices = json.loads((path.parent/'review/choices.json').read_text())
    assert options==set().union(*(set(c.values()) for c in choices))
    assert not any('flip-x' in r for r in rows)
    assert not any(r[0] in ['Rule','Mask','SourcePolygonMask'] for r in rows) or all(
        r[0] not in ['Rule','Mask','SourcePolygonMask'] or (len(r) in [4,7] or (r[0]=='SourcePolygonMask' and len(r)>=10 and len(r)%2==0)) for r in rows)
# Every chosen mouth is protected for both male beard and female chin options.
for catalog,chin_files in [('Dwarf2.mesh',['chin-1-forked.png','chin-2-braided.png']),
                           ('Dwarf2.mesh-female',['chin-1-square.png','chin-2-rounded.png'])]:
    folder = ROOT/'materials/portraits/variants'/catalog
    rows = [r.split('\t') for r in (folder/'manifest.cfg').read_text().splitlines() if r and not r.startswith('#')]
    slots = {r[1]:tuple(map(int,r[2:])) for r in rows if r[0]=='Slot'}
    dx,dy = slots['mouth'][0]-slots['chin'][0],slots['mouth'][1]-slots['chin'][1]
    for chin in chin_files:
        b = Image.open(folder/chin).getchannel('A')
        for mouth in ['mouth-1-teeth.png','mouth-2-grin.png','mouth-3-cigar.png']:
            m = Image.new('L',b.size)
            m.paste(Image.open(folder/mouth).getchannel('A'),(dx,dy))
            masked = ImageChops.multiply(b,ImageChops.invert(m))
            assert all(a!=255 or v==0 for a,v in zip(m.getdata(),masked.getdata()))
            assert all(a!=0 or v==original for a,v,original in zip(m.getdata(),masked.getdata(),b.getdata()))
# Source contour model uses the same pixel centers/ray edges/outside feather as C++.
renderer = (ROOT/'tools/portraits/render_manifest_reviews.py').read_text()
function = renderer[renderer.index('def polygon_mask('):renderer.index('ROOT =')]
model = {'Image':Image}
exec(function,model)
mask = model['polygon_mask']((10,10),[3,3,7,3,7,7,3,7],2)
assert mask.getpixel((4,4))==0 and mask.getpixel((0,0))==255
assert 0<mask.getpixel((2,4))<255
assert mask.getpixel((2,4))==mask.getpixel((7,4))
for catalog in ['Monk.mesh-female','Lizardman.mesh-female']:
    folder = ROOT/'materials/portraits/variants'/catalog
    rows = [r.split('\t') for r in (folder/'manifest.cfg').read_text().splitlines() if r.startswith('SourcePolygonMask\t')]
    assert len(rows)==1 and rows[0][1:3]==['hair','2']
    row = rows[0]
    options = [r.split('\t') for r in (folder/'manifest.cfg').read_text().splitlines() if r.startswith('Option\thair\t2\t')]
    original = Image.open(folder/options[0][4]).getchannel('A')
    mask = model['polygon_mask'](original.size,list(map(int,row[4:])),int(row[3]))
    corrected = ImageChops.multiply(original,mask)
    assert corrected.tobytes()!=original.tobytes()
    assert all(m!=255 or a==b for m,a,b in zip(mask.getdata(),original.getdata(),corrected.getdata()))
# Default nearest sampling must preserve source index exactly; scaling keeps orientation.
for source,destination in [(700,700),(700,887),(510,510),(465,465)]:
    indices = [((2*x+1)*source)//(2*destination) for x in range(destination)]
    assert indices==sorted(indices) and min(indices)==0 and max(indices)==source-1
    if source==destination:
        assert indices==list(range(source))
cpp = (ROOT/'source/render/AppearanceCompose.cpp').read_text()
assert 'part.mWidth ? part.mWidth : part.mImage.mWidth' in cpp
assert 'alpha = alpha * (255 - maskAlpha) / 255' in cpp
assert 'mFlipX' not in cpp
picture = (ROOT/'source/render/CreatureAppearancePicture.cpp').read_text()
assert 'wrong size of ' in picture and 'part.mMaskSlot = option->mMaskSlot' in picture
# Persisted per-file records must cover exactly the current registered options.
import json, hashlib
inventory = json.loads((ROOT/'materials/portraits/generation-inventory.json').read_text())
review = json.loads((ROOT/'materials/portraits/neutral-variants/feature-review.json').read_text())
record_keys = set()
for creature in inventory['creatures']:
    catalog = creature['catalog_id']
    folder = ROOT/'materials/portraits/variants'/catalog
    options = [r.split('\t') for r in (folder/'manifest.cfg').read_text().splitlines() if r.startswith('Option\t')]
    records = creature['geometry_review']['options']
    assert {(r['slot'],r['option'],r['file']) for r in records} == {(r[1],int(r[2]),r[4]) for r in options}
    for r in records:
        assert r['native_combinations'] and r['visible_pixels']>0
        assert hashlib.sha256((folder/r['file']).read_bytes()).hexdigest()==r['source_sha256']
        record_keys.add((catalog,r['slot'],r['option'],r['file']))
assert len(record_keys)==688
assert record_keys=={(r['portrait'],r['slot'],r['option'],r['file']) for r in review['geometry_review']['entries']}
print('34 catalogs, complete option coverage, frozen rasters, default geometry and twelve real mouth masks and measured source contours passed')
