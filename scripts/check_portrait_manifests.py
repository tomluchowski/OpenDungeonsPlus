"""Validate the frozen neutral portrait contract without third-party packages."""
import argparse
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ORDER = ['build','outfit','hair','ears','eyes','nose','mouth','chin','helmet','scar','neck']


# Read the header of a PNG file; returns ((width, height), colour type) and fails when the file is no PNG.
def png_size(path):
    with path.open('rb') as stream:
        header = stream.read(26)
    assert header[:8] == b'\x89PNG\r\n\x1a\n', f'Not PNG: {path}'
    assert header[12:16] == b'IHDR', f'Missing IHDR: {path}'
    return struct.unpack('>II', header[16:24]), header[25]


# Check one manifest.cfg against the contract (one Base, full-canvas outfit, draw order, source sizes, helmets);
# returns the number of options and of fitted options.
def check(path):
    rows = [line.split('\t') for line in path.read_text(encoding='utf-8-sig').splitlines()
            if line and not line.startswith('#')]
    bases = [r for r in rows if r[0]=='Base']
    assert len(bases)==1 and len(bases[0])==2, 'Exactly one Base is required'
    assert not any(r[0]=='Image' for r in rows), 'Preview Image is forbidden'
    base = (path.parent/bases[0][1]).resolve()
    assert base.is_relative_to(ROOT/'materials/portraits'), 'Base must be a neutral portrait asset'
    assert png_size(base)[0]==(887,1774), 'Base canvas must be 887x1774'
    slots, options, fitted, mandatory, clips = {}, {}, {}, [], []
    for row in rows:
        kind = row[0]
        if kind=='Base':
            continue
        if kind=='Slot':
            assert len(row)==6 and row[1] in ORDER and row[1] not in slots, f'Invalid slot: {row}'
            x,y,w,h = map(int,row[2:])
            assert x>=0 and y>=0 and w>0 and h>0 and x+w<=887 and y+h<=1774, f'Out of bounds: {row}'
            slots[row[1]] = (x,y,w,h)
        elif kind=='Option':
            assert len(row)==5 and int(row[2])>=1, f'Invalid option: {row}'
            key = (row[1],int(row[2]))
            assert key not in options, f'Duplicate option: {key}'
            options[key] = row[4]
        elif kind=='SourcePolygonMask':
            assert len(row)>=10 and len(row)%2==0 and all(v.isdigit() for v in row[2:]), f'Invalid source contour: {row}'
        elif kind=='Mask':
            assert len(row)==4 and row[2].isdigit(), f'Invalid mask: {row}'
        elif kind=='Rule':
            assert len(row)==7 and all(v.isdigit() for v in row[2:]), f'Invalid geometry: {row}'
        else:
            raise AssertionError(f'Unknown row: {row}')
    assert list(slots)==[s for s in ORDER if s in slots], 'Incorrect draw order'
    assert slots.get('outfit')==(0,0,887,1774), 'Full-canvas outfit is required'
    assert any(slot=='outfit' for slot,n in options), 'At least one outfit is required'
    assert set(slots)=={slot for slot,n in options}, 'Every slot must contain a part'
    for (slot,number),filename in options.items():
        assert slot in slots, f'Option has no slot: {slot}'
        asset = (path.parent/filename).resolve()
        assert asset.is_relative_to(path.parent.resolve()), 'Option escapes its portrait folder'
        size,colour = png_size(asset)
        assert size==slots[slot][2:] and colour==6, f'Option must be slot-sized RGBA: {filename}'
    seen = set()
    for row in rows:
        if row[0]=='SourcePolygonMask':
            key = ('SourcePolygonMask',row[1],int(row[2]))
            assert key not in seen and (row[1],int(row[2])) in options
            seen.add(key)
            width,height = slots[row[1]][2:]
            feather = int(row[3])
            coords = list(map(int,row[4:]))
            assert feather<=max(width,height)
            assert all(v<=width if i%2==0 else v<=height for i,v in enumerate(coords))
            continue
        if row[0]=='Mask':
            key = ('Mask',row[1],int(row[2]))
            assert key not in seen and (row[1],int(row[2])) in options
            assert row[3] in slots and ORDER.index(row[3])<ORDER.index(row[1])
            seen.add(key)
            continue
        if row[0]!='Rule':
            continue
        kind,slot,number = row[:3]
        key = (kind,slot,int(number))
        assert key not in seen and (slot,int(number)) in options, f'Duplicate/missing option geometry: {row}'
        seen.add(key)
        x,y,w,h = map(int,row[3:])
        width,height = (887,1774)
        assert w>0 and h>0 and x+w<=width and y+h<=height, f'Out of bounds geometry: {row}'
    if path.parent.name.startswith(('Knight.mesh','Cultist.mesh')):
        assert sorted(n for s,n in options if s=='helmet')==[1,2,3,4], 'Existing four helmets are required'
    return len(options),len(fitted)


# Check the manifests of all delivered portraits, or only of the portraits given with --portrait.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--portrait',action='append')
    args = parser.parse_args()
    catalog = json.loads((ROOT/'tools/portraits/feature-catalog.json').read_text(encoding='utf-8-sig'))
    expected = {c['id']:{(j['slot'],str(j['n']),j['name']) for j in c['jobs']} for c in catalog}
    identifiers = args.portrait or [c['id'] for c in catalog]
    errors,options,fitted = [],0,0
    for identifier in identifiers:
        path = ROOT/'materials/portraits/variants'/identifier/'manifest.cfg'
        try:
            count,copies = check(path)
            rows = [line.split('\t') for line in path.read_text().splitlines() if line.startswith('Option\t')]
            assert identifier in expected and expected[identifier]<={tuple(row[1:4]) for row in rows}, 'Missing or renumbered original feature options'
            options += count
            fitted += copies
        except (AssertionError,OSError,ValueError,IndexError,struct.error) as error:
            errors.append({'portrait':identifier,'error':str(error)})
    print(json.dumps({'portraits':len(identifiers),'options':options,'fitted':fitted,
                     'contract':'failed' if errors else 'passed','errors':errors},indent=2))
    return bool(errors)


if __name__=='__main__':
    raise SystemExit(main())
