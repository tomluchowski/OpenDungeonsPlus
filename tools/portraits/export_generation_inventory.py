"""Export all saved source-generation records and current derived work into one JSON."""
import datetime
import hashlib
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
PORTRAITS = ROOT/'materials/portraits'


# Read a JSON file (a leading byte order mark is accepted); returns None when the file does not exist.
def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig')) if path.exists() else None


# Describe an image file for the inventory: repo-relative path, absolute path, SHA-256 of the bytes, size and mode.
def asset(path):
    with Image.open(path) as image:
        size,mode = list(image.size),image.mode
    return dict(file=path.relative_to(ROOT).as_posix(),absolute_path=str(path),
                sha256=hashlib.sha256(path.read_bytes()).hexdigest(),size=size,mode=mode)


catalog = read(ROOT/'tools/portraits/feature-catalog.json')
bases = read(PORTRAITS/'neutral-bases/generation-records.json')
helmets = read(PORTRAITS/'generated-helmets/catalog.json')
review = read(PORTRAITS/'neutral-variants/feature-review.json')
visual_review = read(PORTRAITS/'neutral-variants/visual-acceptance.json')
entries = []
counts = dict(neutral_bases=0,existing_feature_sources=0,helmet_sources=0,
              selected_outfit_options=0,outfit_generation_attempt_images=0)
for creature in catalog:
    identifier = creature['id']
    raw = PORTRAITS/'generated-features'/identifier
    outfits = PORTRAITS/'generated-outfits'/identifier
    candidate = PORTRAITS/'neutral-variants'/identifier
    delivery = PORTRAITS/'variants'/identifier
    generated = read(raw/'generation-records.json')
    features = []
    for job in creature['jobs']:
        filename = f"{job['slot']}-{job['n']}-{job['name']}.png"
        records = [r for r in generated if r.get('file')==filename] if isinstance(generated,list) else generated
        features.append(dict(**job,**asset(raw/filename),generation_records=records))
    helmet_records = [dict(**r,asset=asset(PORTRAITS/'generated-helmets'/identifier/r['file']))
                      for r in helmets if r['id']==identifier]
    outfit_records = []
    for path in sorted(outfits.glob('outfit-*.json')):
        record = read(path)
        outfit_records.append(dict(**record,asset=asset(outfits/record['file'])))
    attempts = [asset(path) for path in sorted(outfits.glob('*.png'))]
    for path in sorted((ROOT/'work/outfit-rejected'/identifier).glob('*.png')):
        prompt = path.with_name(path.stem+'-prompt.txt')
        attempts.append(dict(**asset(path),attempt_record=read(path.with_suffix('.json')),
                             prompt=prompt.read_text() if prompt.exists() else None,selected=False))
    entries.append(dict(creature=creature['mesh'],gender=creature['gender'],catalog_id=identifier,
        neutral_base=dict(asset=asset(PORTRAITS/'neutral-bases'/(identifier+'.png')),
                          generation_records=[r for r in bases['completed'] if r['id']==identifier]),
        existing_features=features,helmets=helmet_records,outfit_options=outfit_records,
        outfit_attempt_images=attempts,candidate_placement=read(candidate/'placement.json'),
        outfit_placement=read(candidate/'outfit-placement.json'),composition_rules=read(candidate/'composition-rules.json'),
        composite_choices=read(candidate/'review/choices.json'),
        current_manifest=(delivery/'manifest.cfg').read_text() if (delivery/'manifest.cfg').exists() else None,
        exact_current_issues=[e for e in review['entries'] if e['portrait']==identifier],
        final_visual_review=visual_review.get('portraits',{}).get(identifier) if visual_review else None,
        candidate_folder=str(candidate),delivery_folder=str(delivery)))
    counts['neutral_bases'] += 1
    counts['existing_feature_sources'] += len(features)
    counts['helmet_sources'] += len(helmet_records)
    counts['selected_outfit_options'] += len(outfit_records)
    counts['outfit_generation_attempt_images'] += len(attempts)
output = dict(generated_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),workspace=str(ROOT),
    scope='All saved portrait-task generation records; original sources, selected options, attempts and derived placement data are separate.',
    counts=counts,creatures=entries,neutral_base_generation_record=bases,
    canonical_base_provenance=read(PORTRAITS/'neutral-variants/base-provenance.json'),
    canvas_height_fixes=read(PORTRAITS/'neutral-bases/canvas-height-fixes.json'),
    current_review=review,visual_acceptance=visual_review,
    base_tint_measurements=read(PORTRAITS/'neutral-variants/base-tint-measurements.json'),
    earlier_base_review=read(PORTRAITS/'neutral-bases/feature-review.json'),
    earlier_individual_review=read(PORTRAITS/'variants/individual-review.json'))
target = PORTRAITS/'generation-inventory.json'
target.write_text(json.dumps(output,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
print(json.dumps(dict(file=str(target),counts=counts)))
