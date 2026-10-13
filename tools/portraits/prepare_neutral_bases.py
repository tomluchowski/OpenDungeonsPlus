"""Create canonical delivery copies without altering any neutral source pixels."""
import hashlib
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
catalog = json.loads((ROOT/'tools/portraits/feature-catalog.json').read_text(encoding='utf-8-sig'))
records = []
for creature in catalog:
    identifier = creature['id']
    source = ROOT/'materials/portraits/neutral-bases'/(identifier+'.png')
    image = Image.open(source).convert('RGBA')
    assert image.width==887 and image.height in (1773,1774), identifier
    padding = 1774-image.height
    if padding:
        delivery = Image.new('RGBA',(887,1774))
        delivery.paste(image,(0,0))
        delivery.paste(image.crop((0,1772,887,1773)),(0,1773))
    else:
        delivery = image
    folder = ROOT/'materials/portraits/neutral-variants'/identifier
    folder.mkdir(parents=True,exist_ok=True)
    delivery.save(folder/'base.png',compress_level=1)
    record = dict(source=source.relative_to(ROOT).as_posix(),
        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        size=list(delivery.size),bottom_rows_repeated=padding,
        modification='repeat final row only' if padding else 'none')
    (folder/'base-provenance.json').write_text(json.dumps(record,indent=2)+'\n')
    records.append(dict(id=identifier,**record))
(ROOT/'materials/portraits/neutral-variants/base-provenance.json').write_text(json.dumps(records,indent=2)+'\n')
print(json.dumps(dict(canonical_bases=len(records),repeated_bottom_rows=sum(r['bottom_rows_repeated'] for r in records),sources_changed=0)))
