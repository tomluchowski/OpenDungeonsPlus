#!/usr/bin/env python3
"""Create uniformly scaled weapon copies while retaining materials and animation."""
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = Path(r'C:\Users\mario\GitHub\OpenDungeonsPlus-Private')
WORK = PRIVATE/'anim-work/armament'
CONVERTER = Path(r'C:\Users\mario\od-deps\build\ogre\bin\release\OgreXMLConverter.exe')

# Convert one asset with closed input, bounded execution and a private log.
def convert(source, target, skeleton=False):
    args = [str(CONVERTER), '-q'] + (['-o'] if skeleton else []) + [str(source),str(target)]
    log = WORK/'logs'/(target.name+'.log')
    with log.open('w') as stream:
        subprocess.run(args,stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,cwd=WORK/'logs',timeout=600,check=True)

# Scale positions and track translations only; retain every other XML value.
def scale_xml(source, target, factor, skeleton=False):
    tree = ET.parse(source)
    tags = ('position','translate') if skeleton else ('position',)
    for tag in tags:
        for element in tree.getroot().iter(tag):
            for axis in ('x','y','z'):
                element.set(axis,format(float(element.get(axis))*factor,'.9g'))
    if not skeleton:
        link = tree.getroot().find('skeletonlink')
        if link is not None and source.name == 'Staff.mesh.xml':
            link.set('name','StaffLich.skeleton')
    tree.write(target,encoding='utf-8',xml_declaration=True)

# Create the three meshes and the scaled staff skeleton in their planned paths.
def main():
    for folder in ('xml','logs','orig','prev'):
        (WORK/folder).mkdir(parents=True,exist_ok=True)
    for original,name,factor in (('BasicAxe','BasicAxeOrc',1.55),('BasicHammer','BasicHammerDwarf2',.74),('Staff','StaffLich',1.54)):
        source=WORK/'xml'/(original+'.mesh.xml')
        target=WORK/'xml'/(name+'.mesh.xml')
        convert(ROOT/'models'/(original+'.mesh'),source)
        scale_xml(source,target,factor)
        convert(target,ROOT/'models'/(name+'.mesh'))
    source=WORK/'xml/Staff.skeleton.xml'
    target=WORK/'xml/StaffLich.skeleton.xml'
    convert(ROOT/'models/Staff.skeleton',source)
    scale_xml(source,target,1.54,True)
    convert(target,ROOT/'models/StaffLich.skeleton',True)

if __name__ == '__main__':
    main()
