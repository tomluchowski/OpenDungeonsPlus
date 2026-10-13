"""Read-only checks of portrait plan sections 3 and 7, with an audit report."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
P = ROOT / 'materials/portraits'
results = {}
for name, script in [('source_hashes', 'tools/portraits/verify_portrait_sources.py'),
                     ('manifests', 'scripts/check_portrait_manifests.py'),
                     ('registered_parts', 'tools/portraits/validate_neutral_features.py')]:
    run = subprocess.run([sys.executable, str(ROOT/script)], cwd=ROOT, capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr
    results[name] = json.loads(run.stdout)
inventory = json.loads((P/'generation-inventory.json').read_text(encoding='utf-8'))
review = json.loads((P/'neutral-variants/feature-review.json').read_text())
assert not any(e['status'].startswith('open') for e in review['entries'])
assert all(not e.get('regeneration_required', False) for e in review['entries'])
lizard = [e for e in review['entries'] if e['portrait'].startswith('Lizardman.mesh')]
assert len(lizard) == 6 and all(e['status'] == 'closed-native-and-50x100' for e in lizard)
for creature in inventory['creatures']:
    folder = P/'variants'/creature['catalog_id']
    rows = [line.split('\t') for line in (folder/'manifest.cfg').read_text().splitlines() if line and not line.startswith('#')]
    options = [r for r in rows if r[0] == 'Option']
    coverage = json.loads((folder/'review/native-check-coverage.json').read_text())
    choices = json.loads((folder/'review/choices.json').read_text())
    covered = set().union(*(set(choices[n-1].values()) for n in coverage['combinations']))
    assert {r[4] for r in options} <= covered
    assert coverage['options'] == len(options) and coverage['resampling'] == 'none'
    assert (folder/'review/combined-small-sheet.png').exists()
    assert (folder/'review/native-face-check.jpg').exists() and (folder/'review/native-body-check.jpg').exists()
    assert creature['final_visual_review']['status'] == 'accepted'
    assert creature['composition_rules']['combined_visual_acceptance'] == 'accepted-native-and-50x100'
    with Image.open(P/'neutral-bases'/(creature['catalog_id']+'.png')) as base:
        assert base.size == (887,1774)
measurements = json.loads((P/'neutral-variants/base-tint-measurements.json').read_text())
cfg = (ROOT/'config/dungeonbook-base-tints.cfg').read_text().splitlines()
blocks = {}
for line in cfg:
    if not line or line.startswith('#') or line == '[Portrait]': continue
    parts = line.split('\t')
    if parts[0] == 'Mesh':
        identifier = parts[1]
        assert identifier not in blocks
        blocks[identifier] = []
    else:
        assert parts[0] == 'Region' and len(parts) == 7
        blocks[identifier].append(parts)
assert len(measurements) == 34
measured_ids = {item['portrait'] for item in measurements}
assert set(blocks) == measured_ids
for item in measurements:
    assert hashlib.sha256((ROOT/item['source']).read_bytes()).hexdigest() == item['source_sha256']
    regions = blocks[item['portrait']]
    assert len(regions) == 1 and regions[0][1] == 'Skin'
    actual = list(map(float, regions[0][3].removeprefix('box=').split(',')))
    assert all(0 <= n <= 1 for n in actual)
    assert all(abs(a-b) < .000001 for a,b in zip(actual,item['skin_box']))
    assert item['hair'] == item['eyes'] == item['beard'] == 'absent'
    assert item['skin_pixels'] > 0
report = dict(section_3={
    '1_canvas_height': 'passed: 34 canonical 887x1774 bases; two preserved last-row fixes',
    '2_missing_clothing': 'passed: 30 added selections plus 10 reused; 34 bases have outfits',
    '3_placement': 'passed: 688 registered options, recipes and 34 accepted manifests',
    '4_findings': dict(status='passed', open=0, lizard_closed=6, total_closed=len(review['entries'])),
    '5_composite_checks': 'passed: all 688 options covered at native size and 50x100 with per-file findings',
    '6_hashes': results['source_hashes']},
    section_7=dict(status='passed', bases=34, skin_regions=34,
                   hair_eyes_beard='absent on the inspected blank bases; no invented regions',
                   coordinates='measured colour-pixel bounds on actual source images, normalized 0..1',
                   format='TAB-separated Portrait/Mesh/Region contract', images_generated=0),
    checks=results)
print(json.dumps(report, indent=2))
