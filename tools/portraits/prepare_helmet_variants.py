"""Prepare helmet overlays from the 16 owner-authorized isolated assets; sources stay unchanged."""
from pathlib import Path
import hashlib,json
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[2]
RAW=ROOT/'materials/portraits/generated-helmets'
OUT=ROOT/'materials/portraits/helmet-variants'
JOBS=json.loads((RAW/'catalog.json').read_text())
# Pixel-coordinate anchors measured on the saved neutral head silhouettes.
HEADS={'Knight.mesh':[205,155,350,465],'Knight.mesh-female':[205,155,350,465],'Cultist.mesh':[255,302,290,463],'Cultist.mesh-female':[265,380,310,435]}
records=[]
for id in HEADS:
 folder=OUT/id;folder.mkdir(parents=True,exist_ok=True)
 review=folder/'review';review.mkdir(exist_ok=True)
 basepath=ROOT/'materials/portraits/neutral-bases'/(id+'.png')
 base=Image.open(basepath).convert('RGBA') if basepath.exists() else None
 x,y,w,h=HEADS[id]
 rect=[0,0,887,950]
 lines=['# Supplemental helmet slot; neutral base and required helmet selection documented in README.','Image\t../../neutral-bases/'+id+'.png','Slot\thelmet\t'+ '\t'.join(map(str,rect))]
 board=Image.new('RGB',(4*222,474),(30,30,30))
 for j in [r for r in JOBS if r['id']==id]:
  source=RAW/id/j['file']
  im=Image.open(source).convert('RGBA')
  assert source.read_bytes()==Path(j['original']).read_bytes(),str(source)+' differs from generator'
  assert im.getchannel('A').getextrema()==(0,255),str(source)+' lacks complete alpha range'
  bounds=im.getchannel('A').point(lambda v:255 if v>=128 else 0).getbbox()
  l,t,r,b=bounds
  # Crests/horns extend above the shell; only the shell is registered to the neutral head.
  shell_top=t if id.startswith('Knight') else t+round((b-t)*({'arch':.36,'crown':.20,'horned':.18,'ridged':.22}[j['name']]))
  base_scale=h/(b-shell_top)
  vertical_factor=1.04 if id.startswith('Cultist') and j['name']=='arch' else 1.08 if (id.startswith('Cultist') or j['name'] in {'greathelm','houndskull'}) else 1
  if (id,j['name']) in {('Knight.mesh-female','houndskull'),('Cultist.mesh','crown'),('Cultist.mesh','ridged')}: vertical_factor=1.2
  horizontal_factor=1.39 if id=='Cultist.mesh-female' and j['name']=='arch' else 1.25 if id.startswith('Cultist') and j['name']=='arch' else 1.28 if id.startswith('Cultist') else vertical_factor
  scale=base_scale*vertical_factor
  horizontal_scale=base_scale*horizontal_factor
  ox=x+w/2-(l+r)*horizontal_scale/2
  oy=y+h-b*scale
  adjustment={('Knight.mesh-female','houndskull'):(0,45),('Cultist.mesh','crown'):(0,50),('Cultist.mesh','horned'):(0,30),('Cultist.mesh','ridged'):(0,35),('Cultist.mesh-female','horned'):(0,40),('Cultist.mesh-female','arch'):(-15,0)}.get((id,j['name']),(0,0))
  ox+=adjustment[0];oy+=adjustment[1]
  ox=max(2-l*horizontal_scale,min(ox,885-r*horizontal_scale))
  assert ox+l*horizontal_scale>=0 and ox+r*horizontal_scale<=887 and oy+t*scale>=0 and oy+b*scale<=950, str((id,j['name'],ox+l*horizontal_scale,ox+r*horizontal_scale,oy+t*scale,oy+b*scale))
  patch=im.transform((887,950),Image.Transform.AFFINE,(1/horizontal_scale,0,-ox/horizontal_scale,0,1/scale,-oy/scale),Image.Resampling.BICUBIC)
  assert patch.getchannel('A').getbbox()
  patch.save(folder/j['file'])
  lines.append('\t'.join(map(str,['Option','helmet',j['n'],j['name'],j['file']])))
  entry={**j,'source':source.relative_to(ROOT).as_posix(),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'source_bbox':list(bounds),'shell_top':shell_top,'rect':rect,'scale':scale,'horizontal_scale':horizontal_scale,'offset':[ox,oy],'head_anchor':HEADS[id],'base':basepath.relative_to(ROOT).as_posix(),'base_sha256':hashlib.sha256(basepath.read_bytes()).hexdigest() if base else None,'visual_acceptance':'candidate-pending' if base else 'deferred-missing-base'}
  records.append(entry)
  if base:
   comp=base.copy();comp.alpha_composite(patch)
   comp.save(review/j['file'])
   small=comp.resize((50,100),Image.Resampling.LANCZOS);small.save(review/('small-'+j['file']))
   board.paste(comp.resize((222,444),Image.Resampling.LANCZOS),((j['n']-1)*222,0))
  else:
   background=Image.new('RGBA',patch.size,(35,35,35,255));background.alpha_composite(patch)
   background.thumbnail((222,444));board.paste(background.convert('RGB'),((j['n']-1)*222,0))
  ImageDraw.Draw(board).text(((j['n']-1)*222+4,449),j['name'],fill='white')
 board.save(review/'overview.jpg',quality=95)
 (folder/'manifest.cfg').write_text('\n'.join(lines)+'\n',encoding='utf-8')
 selected=[r for r in records if r['id']==id]
 assert sorted(r['n'] for r in selected)==[1,2,3,4]
 (folder/'placement.json').write_text(json.dumps(selected,indent=2)+'\n')
 (folder/'prompts.md').write_text('\n\n'.join('## '+r['file']+'\n\n'+r['prompt']+('\n\nCorrection: '+r['correction_prompt'] if r.get('correction_prompt') else '') for r in selected)+'\n')
assert len(records)==16
(OUT/'validation.json').write_text(json.dumps({'sources':16,'unchanged_generator_copies':16,'alpha_passed':16,'derived_rgba_patches':16,'neutral_composites':12,'deferred_composites':4,'visual_acceptance':'pending','rules':{'selection':'one helmet from 1..4 is mandatory; option 0 would expose a featureless head','draw_order':['build','hair','helmet','scar','neck'],'scar_mask':'clip helmet damage to the selected helmet alpha','missing_base':'Knight male remains deferred; do not substitute the original clothed portrait'}},indent=2)+'\n')
print('Prepared 16 helmet overlays; 12 neutral-base composites, 4 deferred for missing Knight male base.')
