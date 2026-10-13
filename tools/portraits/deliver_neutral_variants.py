"""Publish prepared neutral assets to the existing local delivery folders, never sources."""
import argparse
import json
import runpy
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
check = runpy.run_path(str(ROOT/'scripts/check_portrait_manifests.py'))['check']
parser = argparse.ArgumentParser()
parser.add_argument('--portrait',action='append',required=True)
args = parser.parse_args()
for identifier in args.portrait:
    source = ROOT/'materials/portraits/neutral-variants'/identifier
    manifest = source/'manifest.cfg'
    options,fitted = check(manifest)
    target = ROOT/'materials/portraits/variants'/identifier
    target.mkdir(parents=True,exist_ok=True)
    rows = [line.split('\t') for line in manifest.read_text().splitlines()
            if line and not line.startswith('#')]
    files = [row[4] for row in rows if row[0] in ['Option','Fitted']]
    for filename in files:
        shutil.copyfile(source/filename,target/filename)
    provenance = json.loads((source/'base-provenance.json').read_text())
    base = ROOT/provenance['source']
    if provenance['bottom_rows_repeated']:
        base = base.with_name(base.stem+'.delivery.png')
        if base.exists():
            assert base.read_bytes()==(source/'base.png').read_bytes(), 'Preserve a differing existing delivery base'
        else:
            shutil.copyfile(source/'base.png',base)
    obsolete_copy = target/'neutral-base.png'
    if obsolete_copy.exists() and obsolete_copy.read_bytes()==(source/'base.png').read_bytes():
        obsolete_copy.unlink()
    for filename in ['base-provenance.json','placement.json','composition-rules.json','outfit-placement.json','prompts.md']:
        shutil.copyfile(source/filename,target/filename)
    final = manifest.read_text().replace('Base\tbase.png','Base\t../../neutral-bases/'+base.name)
    (target/'manifest.cfg').write_text(final)
    check(target/'manifest.cfg')
    print(json.dumps(dict(portrait=identifier,options=options,fitted=fitted,contract='passed',visual_acceptance='pending')))
