"""Create native-resolution review panels covering every delivered option."""
import argparse,json
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser()
parser.add_argument('--portrait',action='append',required=True)
args=parser.parse_args()
for identifier in args.portrait:
 folder=ROOT/'materials/portraits/variants'/identifier
 review=folder/'review'
 choices=json.loads((review/'choices.json').read_text())
 options={r.split('\t')[4] for r in (folder/'manifest.cfg').read_text().splitlines() if r.startswith('Option\t')}
 selected=[1,2,3,4,7,8]
 assert options<=set().union(*(set(choices[n-1].values()) for n in selected))
 slots={r.split('\t')[1]:list(map(int,r.split('\t')[2:])) for r in (folder/'manifest.cfg').read_text().splitlines() if r.startswith('Slot\t')}
 top=max(0,slots.get('eyes',[0,400])[1]-280)
 if 'hair' in slots: top=min(top,slots['hair'][1])
 bottom=min(1774,max(slots.get('chin',[0,700,0,150])[1]+slots.get('chin',[0,700,0,150])[3]+45,slots.get('mouth',[0,700,0,150])[1]+220))
 if 'helmet' in slots: top=0;bottom=1050
 face=Image.new('RGB',(3*700,2*(bottom-top)))
 body_top=max(0,slots['build'][1]-80)
 body=Image.new('RGB',(3*887,2*(1774-body_top)))
 for index,n in enumerate(selected):
  source=Image.open(review/f'combined-{n}.png').convert('RGB')
  face.paste(source.crop((80,top,780,bottom)),((index%3)*700,(index//3)*(bottom-top)))
 for index,n in enumerate(selected):
  source=Image.open(review/f'combined-{n}.png').convert('RGB')
  body.paste(source.crop((0,body_top,887,1774)),((index%3)*887,(index//3)*(1774-body_top)))
 face.save(review/'native-face-check.jpg',quality=97)
 body.save(review/'native-body-check.jpg',quality=97)
 (review/'native-check-coverage.json').write_text(json.dumps(dict(combinations=selected,all_options_covered=True,options=len(options),face_crop=[80,top,780,bottom],body_combinations=selected,body_crop=[0,body_top,887,1774],resampling='none'),indent=2)+'\n')
 print(identifier,len(options),'options represented at native pixel scale')
