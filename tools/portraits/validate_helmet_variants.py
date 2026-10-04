"""Validate the supplemental helmet asset contract without modifying source or base PNGs."""
from pathlib import Path
import json,hashlib
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
RAW=ROOT/'materials/portraits/generated-helmets'; OUT=ROOT/'materials/portraits/helmet-variants'
jobs=json.loads((RAW/'catalog.json').read_text())
assert len(jobs)==16
assert {(r['id'],r['n']) for r in jobs}=={(id,n) for id in ['Knight.mesh','Knight.mesh-female','Cultist.mesh','Cultist.mesh-female'] for n in range(1,5)}
sources=patches=composites=0
for id in sorted({r['id'] for r in jobs}):
 chosen=[r for r in jobs if r['id']==id];folder=OUT/id
 assert {p.name for p in (RAW/id).glob('*.png')}=={r['file'] for r in chosen}
 assert {p.name for p in folder.glob('*.png')}=={r['file'] for r in chosen}
 records={r['file']:r for r in json.loads((folder/'placement.json').read_text())}
 rows=[line.split('\t') for line in (folder/'manifest.cfg').read_text().splitlines() if line and not line.startswith('#')]
 assert rows[0]==['Image','../../neutral-bases/'+id+'.png']
 assert rows[1]==['Slot','helmet','0','0','887','950']
 assert rows[2:]==[['Option','helmet',str(r['n']),r['name'],r['file']] for r in chosen]
 for j in chosen:
  p=RAW/id/j['file'];r=records[j['file']]
  assert hashlib.sha256(p.read_bytes()).hexdigest()==r['source_sha256']
  assert p.read_bytes()==Path(j['original']).read_bytes()
  with Image.open(p) as im: assert im.format=='PNG' and im.mode=='RGBA' and im.getchannel('A').getextrema()==(0,255)
  with Image.open(folder/j['file']) as im:
   assert im.format=='PNG' and im.mode=='RGBA' and im.size==(887,950)
   box=im.getchannel('A').point(lambda v:255 if v>=128 else 0).getbbox()
   assert box and box[0]>=0 and box[1]>=0 and box[2]<=887 and box[3]<=950
  base=ROOT/r['base']
  if base.exists():
   assert hashlib.sha256(base.read_bytes()).hexdigest()==r['base_sha256']
   assert r['visual_acceptance']=='single-helmet-native-and-small-inspected'
   with Image.open(folder/'review'/j['file']) as im:assert im.size==(887,1774)
   with Image.open(folder/'review'/('small-'+j['file'])) as im:assert im.size==(50,100)
   composites+=1
  else:assert id=='Knight.mesh' and r['visual_acceptance']=='deferred-missing-base'
  sources+=1;patches+=1
assert sources==patches==16 and composites==12
print(json.dumps({'source_hashes_verified':sources,'rgba_alpha_verified':sources,'slot_patches_verified':patches,'manifests_verified':4,'neutral_base_composites_verified':composites,'missing_base_deferred':4,'existing_slots_combined_acceptance':'open'}))
