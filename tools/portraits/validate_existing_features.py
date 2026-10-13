"""Validate the existing portrait feature delivery without generating images."""
from pathlib import Path
import json, hashlib
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'materials/portraits/variants'
CAT=json.loads((ROOT/'tools/portraits/feature-catalog.json').read_text())
LAYOUT=json.loads((ROOT/'tools/portraits/feature-layouts.json').read_text())
BASE_HASHES=json.loads((ROOT/'tools/portraits/feature-base-hashes.json').read_text())
ORDER=['build','hair','ears','eyes','nose','mouth','chin','scar','neck']
report=json.loads((OUT/'corrections.json').read_text())
flagged={(r['portrait'],r['file']) for r in report['entries']}
review_path=OUT/'individual-review.json'
individual=json.loads(review_path.read_text())['entries'] if review_path.exists() else []
reviewed={(r['portrait'],r['file']) for r in individual}
assert len(reviewed)==len(individual), 'Duplicate individual review records'
assert flagged=={(r['portrait'],r['file']) for r in individual if r['classification']!='no-obvious-local-defect'}
for r in individual:
 assert r['observation'] and r['cause'] and r['required_action'], 'Incomplete individual finding'
 assert (ROOT/r['evidence']).is_file() and (ROOT/r['composite']).is_file(), 'Missing review evidence'
assert len(flagged)==len(report['entries'])==report['correction_count']
assert len(CAT)==34
count=0; source_count=0; portraits=[]
for c in CAT:
 identifier=c['id']; folder=OUT/identifier; raw=ROOT/'materials/portraits/generated-features'/identifier
 assert hashlib.sha256((ROOT/c['reference']).read_bytes()).hexdigest()==BASE_HASHES[identifier], identifier+' base changed'
 with Image.open(ROOT/c['reference']) as base: assert base.size==(887,1774)
 original={r['file']:r for r in json.loads((raw/'verification.json').read_text(encoding='utf-8-sig'))}
 lines=[line.split('\t') for line in (folder/'manifest.cfg').read_text().splitlines() if line and not line.startswith('#')]
 assert lines[0]==['Image',Path(c['reference']).name]
 slots=[line for line in lines if line[0]=='Slot']; options=[line for line in lines if line[0]=='Option']
 expected_slots=[s for s in ORDER if s in LAYOUT[identifier]]
 assert [line[1] for line in slots]==expected_slots
 rectangles={line[1]:list(map(int,line[2:])) for line in slots}
 assert rectangles==LAYOUT[identifier]
 for x,y,w,h in rectangles.values(): assert x>=0 and y>=0 and w>32 and h>32 and x+w<=887 and y+h<=1774
 expected=[(j['slot'],str(j['n']),j['name'],f"{j['slot']}-{j['n']}-{j['name']}.png") for j in c['jobs']]
 assert [tuple(line[1:]) for line in options]==expected
 assert {p.name for p in folder.glob('*.png')}=={row[3] for row in expected}
 records=json.loads((folder/'placement.json').read_text()); assert len(records)==len(expected)
 placement={r['file']:r for r in records}
 prompts=json.loads((raw/'generation-records.json').read_text(encoding='utf-8-sig'))
 expected_prompts='\n\n'.join('## '+r['file']+'\n\n'+r['prompt'] for r in prompts)+'\n'
 assert (folder/'prompts.md').read_text(encoding='utf-8')==expected_prompts
 for slot,n,name,file in expected:
  source=raw/file; source_hash=hashlib.sha256(source.read_bytes()).hexdigest()
  assert source_hash==original[file]['sha256']==placement[file]['source_sha256'], str(source)+' changed'
  assert placement[file]['source']==str(source.relative_to(ROOT)).replace('\\','/')
  assert placement[file]['rect']==rectangles[slot]
  assert placement[file]['visual_acceptance']==('needs-correction' if (identifier,file) in flagged else 'reviewed-local-only' if (identifier,file) in reviewed else 'native-review-pending')
  with Image.open(folder/file) as patch:
   assert patch.format=='PNG' and patch.mode=='RGBA' and patch.size==tuple(rectangles[slot][2:]), str(folder/file)
   assert patch.getchannel('A').getextrema()[0]==0 and patch.getchannel('A').getbbox(), str(folder/file)+' invalid alpha'
  assert (ROOT/'work/existing-feature-review'/identifier/file).exists()
  with Image.open(ROOT/'work/existing-feature-review'/identifier/('small-'+file)) as small: assert small.size==(50,100)
  count+=1; source_count+=1
 assert (ROOT/'work/existing-feature-review'/identifier/'sheet.png').exists()
 with Image.open(ROOT/'work/existing-feature-review'/identifier/'sheet-50x100.png') as sheet: assert sheet.size==(200,300)
 portraits.append(dict(id=identifier,patches=len(expected),corrections=sum((identifier,file) in flagged for slot,n,name,file in expected)))
assert count==source_count==632
if report['review_complete']:
 assert reviewed=={(c['id'],f"{j['slot']}-{j['n']}-{j['name']}.png") for c in CAT for j in c['jobs']}, 'Incomplete individual audit'
assert flagged <= {(c['id'],f"{j['slot']}-{j['n']}-{j['name']}.png") for c in CAT for j in c['jobs']}
result=dict(portraits=34,patches=count,source_images_unchanged=source_count,base_portraits_unchanged=34,technical_validation='passed',correction_options=len(flagged),individually_reviewed_options=len(reviewed),required_regenerations=None,native_visual_acceptance='open',details=portraits)
(OUT/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='details'},indent=2))
