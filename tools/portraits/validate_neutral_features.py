"""Read-only contract and provenance checks; does not grant visual acceptance."""
from pathlib import Path
import hashlib,json
from PIL import Image,ImageChops

ROOT = Path(__file__).resolve().parents[2]
CAT = json.loads((ROOT/'tools/portraits/feature-catalog.json').read_text(encoding='utf-8-sig'))
OUT = ROOT/'materials/portraits/neutral-variants'
DELIVERY = ROOT/'materials/portraits/variants'

# Path of the neutral base image that the delivered manifest of the catalog id names in its single Base row.
def canonical_base(identifier):
    folder = DELIVERY/identifier
    rows = [line.split('\t') for line in (folder/'manifest.cfg').read_text().splitlines()
            if line and not line.startswith('#')]
    bases = [row for row in rows if row[0]=='Base']
    assert len(bases)==1 and len(bases[0])==2
    path = (folder/bases[0][1]).resolve()
    assert path==(ROOT/'materials/portraits/neutral-bases'/(identifier+'.png')).resolve()
    return path

count = 0
portraits = 0
canonical = 0
provenance_path = OUT/'base-provenance.json'
assert provenance_path.is_file()
if provenance_path.exists():
    for provenance in json.loads(provenance_path.read_text()):
        source = ROOT/provenance['source']
        assert hashlib.sha256(source.read_bytes()).hexdigest()==provenance['source_sha256']
        original = Image.open(source).convert('RGBA')
        delivery = Image.open(canonical_base(provenance['id'])).convert('RGBA')
        assert delivery.size==(887,1774)
        assert all(band.getbbox() is None for band in ImageChops.difference(original,delivery.crop((0,0,*original.size))).split())
        if provenance['bottom_rows_repeated']:
            assert all(band.getbbox() is None for band in ImageChops.difference(delivery.crop((0,1773,887,1774)),original.crop((0,1772,887,1773))).split())
        canonical += 1
for creature in CAT:
    folder = DELIVERY/creature['id']
    assert (folder/'manifest.cfg').is_file()
    portraits += 1
    base = Image.open(canonical_base(creature['id'])).convert('RGBA')
    assert base.size==(887,1774)
    provenance = json.loads((folder/'base-provenance.json').read_text())
    source = ROOT/provenance['source']
    assert hashlib.sha256(source.read_bytes()).hexdigest()==provenance['source_sha256']
    original = Image.open(source).convert('RGBA')
    assert all(band.getbbox() is None for band in ImageChops.difference(original,base.crop((0,0,*original.size))).split())
    if provenance['bottom_rows_repeated']:
        assert all(band.getbbox() is None for band in ImageChops.difference(base.crop((0,1773,887,1774)),original.crop((0,1772,887,1773))).split())
    records = json.loads((folder/'placement.json').read_text())
    expected = {f"{j['slot']}-{j['n']}-{j['name']}.png" for j in creature['jobs']}
    helmet_folder = ROOT/'materials/portraits/helmet-variants'/creature['id']
    if any(r['file'].startswith('helmet-') for r in records):
        expected.update(r['file'] for r in json.loads((helmet_folder/'placement.json').read_text()))
    fitted_files = set()
    if (folder/'outfit-placement.json').exists():
        outfits = json.loads((folder/'outfit-placement.json').read_text())
        expected.update(r['file'] for r in outfits if not r['build'])
        fitted_files.update(r['file'] for r in outfits if r['build'])
        for record in outfits:
            assert hashlib.sha256((ROOT/record['source']).read_bytes()).hexdigest()==record['source_sha256']
            patch = Image.open(folder/record['file'])
            assert patch.mode=='RGBA' and patch.size==(887,1774)
    assert {r['file'] for r in records}==expected
    manifest = [line.split('\t') for line in (folder/'manifest.cfg').read_text().splitlines() if line and not line.startswith('#')]
    assert manifest[0]==['Base','../../neutral-bases/'+creature['id']+'.png']
    slots = {line[1]:list(map(int,line[2:])) for line in manifest if line[0]=='Slot'}
    order = ['build','outfit','hair','ears','eyes','nose','mouth','chin','helmet','scar','neck']
    assert list(slots)==[slot for slot in order if slot in slots]
    assert all(row[0] in ['Base','Slot','Option'] for row in manifest)
    options = {line[4] for line in manifest if line[0]=='Option'}
    assert options==expected
    for record in records:
        source = ROOT/record['source']
        assert hashlib.sha256(source.read_bytes()).hexdigest()==record['source_sha256']
        assert hashlib.sha256((ROOT/provenance['source']).read_bytes()).hexdigest()==record['base_sha256']
        x,y,w,h = record['rect']
        assert slots[record['file'].split('-')[0]]==[x,y,w,h]
        assert 0<=x and 0<=y and x+w<=887 and y+h<=1774
        patch = Image.open(folder/record['file'])
        assert patch.mode=='RGBA' and patch.size==(w,h)
        single = base.copy()
        single.alpha_composite(patch,(x,y))
        assert single.size==(887,1774)
        assert single.resize((50,100),Image.Resampling.LANCZOS).size==(50,100)
        count += 1
    choices = json.loads((folder/'review/choices.json').read_text())
    assert len(choices)==12
    assert expected<=set().union(*(set(choice.values()) for choice in choices)), 'Every part must appear in a composite'
    small_sheet = Image.new('RGB',(200,300))
    for number,choice in enumerate(choices,1):
        assert set(choice.values())<=expected|fitted_files
        assert set(choice)<=set(slots)
        composite = base.copy()
        for slot,(x,y,w,h) in slots.items():
            if slot not in choice:
                continue
            filename = choice[slot]
            assert any(row[0]=='Option' and row[1]==slot and row[4]==filename for row in manifest)
            patch = Image.open(folder/filename)
            assert patch.mode=='RGBA' and patch.size==(w,h)
            composite.alpha_composite(patch,(x,y))
        assert composite.size==(887,1774)
        small = composite.resize((50,100),Image.Resampling.LANCZOS)
        assert small.size==(50,100)
        small_sheet.paste(small,((number-1)%4*50,(number-1)//4*100))
    recorded_sheet = Image.open(folder/'review/combined-small-sheet.png').convert('RGB')
    assert recorded_sheet.size==small_sheet.size
    assert ImageChops.difference(recorded_sheet,small_sheet).getbbox() is None, f'Composite mismatch: {creature["id"]}'
    coverage = json.loads((folder/'review/native-check-coverage.json').read_text())
    assert coverage['resampling']=='none' and coverage['all_options_covered']
    assert expected<=set().union(*(set(choices[n-1].values()) for n in coverage['combinations']))
    body_options = {name for name in expected if name.startswith(('build-','outfit-'))}
    assert body_options<=set().union(*(set(choices[n-1].values()) for n in coverage['body_combinations']))
    for kind,crop_key,selection in [('face','face_crop','combinations'),('body','body_crop','body_combinations')]:
        x0,y0,x1,y1 = coverage[crop_key]
        assert 0<=x0<x1<=887 and 0<=y0<y1<=1774
        selected_count = len(coverage[selection])
        assert selected_count==6 if kind=='face' else selected_count in (3,6)
        assert Image.open(folder/'review'/f'native-{kind}-check.jpg').size==(3*(x1-x0),(selected_count//3)*(y1-y0))
acceptance_path = OUT/'visual-acceptance.json'
accepted = 0
assert acceptance_path.is_file()
if acceptance_path.exists():
    for identifier,record in json.loads(acceptance_path.read_text())['portraits'].items():
        delivery = ROOT/'materials/portraits/variants'/identifier
        for filename,expected_hash in record['sha256'].items():
            assert hashlib.sha256((delivery/filename).read_bytes()).hexdigest()==expected_hash, f'Stale visual acceptance: {identifier}/{filename}'
        base_source = ROOT/'materials/portraits/neutral-bases'/(identifier+'.png')
        if record.get('base_sha256'):
            assert hashlib.sha256(base_source.read_bytes()).hexdigest()==record['base_sha256']
        coverage = json.loads((delivery/record['coverage']).read_text())
        assert coverage['all_options_covered'] and coverage['options']==record['options']
        accepted += 1
assert portraits==canonical==accepted==len(CAT)==34
assert count==688
print(json.dumps(dict(portraits=portraits,patches=count,source_hashes_verified=count,canonical_bases_verified=canonical,
    visual_acceptance_records_verified=accepted,neutral_source_pixels_preserved=True,technical_validation='passed',visual_acceptance='separate'),indent=2))
