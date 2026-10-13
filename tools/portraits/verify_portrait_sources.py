"""Verify immutable portrait inputs independently of the evolving delivery manifests."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
catalog = json.loads((ROOT/'tools/portraits/feature-catalog.json').read_text(encoding='utf-8-sig'))
base_hashes = json.loads((ROOT/'tools/portraits/feature-base-hashes.json').read_text())
counts = dict(preview_portraits=0,existing_features=0,neutral_bases=0,helmets=0,outfits=0)
inventory = json.loads((ROOT/'materials/portraits/generation-inventory.json').read_text(encoding='utf-8'))
saved_hashes = {}
for creature in inventory['creatures']:
    for record in creature['helmets']+creature['outfit_options']:
        saved_hashes[record['asset']['file']] = record['asset']['sha256']
    for record in creature['outfit_attempt_images']:
        saved_hashes[record['file']] = record['sha256']
for creature in catalog:
    identifier = creature['id']
    assert hashlib.sha256((ROOT/creature['reference']).read_bytes()).hexdigest()==base_hashes[identifier]
    counts['preview_portraits'] += 1
    raw = ROOT/'materials/portraits/generated-features'/identifier
    verified = json.loads((raw/'verification.json').read_text(encoding='utf-8-sig'))
    expected = {f"{j['slot']}-{j['n']}-{j['name']}.png" for j in creature['jobs']}
    assert {r['file'] for r in verified}==expected
    for record in verified:
        assert hashlib.sha256((raw/record['file']).read_bytes()).hexdigest()==record['sha256']
        counts['existing_features'] += 1
from PIL import Image, ImageChops
baseline = json.loads((ROOT/'materials/portraits/neutral-bases/source-hash-baseline.json').read_text())
fixes = {r['id']:r for r in json.loads((ROOT/'materials/portraits/neutral-bases/canvas-height-fixes.json').read_text())}
for identifier,expected_hash in baseline.items():
    source = ROOT/'materials/portraits/neutral-bases'/(identifier+'.png')
    if identifier not in fixes:
        assert hashlib.sha256(source.read_bytes()).hexdigest()==expected_hash
    else:
        fix = fixes[identifier]
        original = ROOT/fix['original_backup']
        assert hashlib.sha256(original.read_bytes()).hexdigest()==expected_hash==fix['original_sha256']
        assert hashlib.sha256(source.read_bytes()).hexdigest()==fix['fixed_sha256']
        before,after = Image.open(original).convert('RGBA'),Image.open(source).convert('RGBA')
        assert before.size==(887,1773) and after.size==(887,1774)
        assert all(b.getbbox() is None for b in ImageChops.difference(before,after.crop((0,0,887,1773))).split())
        assert all(b.getbbox() is None for b in ImageChops.difference(before.crop((0,1772,887,1773)),after.crop((0,1773,887,1774))).split())
    counts['neutral_bases'] += 1
for record in json.loads((ROOT/'materials/portraits/generated-helmets/catalog.json').read_text()):
    source = ROOT/'materials/portraits/generated-helmets'/record['id']/record['file']
    assert Path(record['original']).suffix.lower()==source.suffix.lower()=='.png'
    assert hashlib.sha256(source.read_bytes()).hexdigest()==saved_hashes[source.relative_to(ROOT).as_posix()]
    counts['helmets'] += 1
for metadata in (ROOT/'materials/portraits/generated-outfits').glob('*/outfit-*.json'):
    record = json.loads(metadata.read_text())
    source = metadata.parent/record['file']
    assert Path(record['original']).suffix.lower()==source.suffix.lower()=='.png'
    assert hashlib.sha256(source.read_bytes()).hexdigest()==saved_hashes[source.relative_to(ROOT).as_posix()]
    if record.get('initial_file'):
        source = metadata.parent/record['initial_file']
        assert hashlib.sha256(source.read_bytes()).hexdigest()==saved_hashes[source.relative_to(ROOT).as_posix()]
    counts['outfits'] += 1
assert counts['preview_portraits']==34 and counts['existing_features']==632 and counts['neutral_bases']==34 and counts['helmets']==16 and counts['outfits']==40
print(json.dumps(dict(**counts,source_preservation='passed'),indent=2))
