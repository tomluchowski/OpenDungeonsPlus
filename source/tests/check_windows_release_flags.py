"""Check the effective MSVC project flags, not the stale CMake cache defaults."""
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('project', type=Path)
args = parser.parse_args()
root = ET.parse(args.project).getroot()
ns = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
checks = 0
for configuration, expected in (
    ('Release', {'Optimization': 'MaxSpeed',
                 'DebugInformationFormat': 'ProgramDatabase',
                 'RuntimeLibrary': 'MultiThreadedDLL',
                 'MinimalRebuild': 'false'}),
    ('Debug', {'Optimization': 'Disabled',
               'DebugInformationFormat': 'EditAndContinue',
               'RuntimeLibrary': 'MultiThreadedDebugDLL',
               'MinimalRebuild': 'false'}),
):
    groups = [group for group in root.findall('m:ItemDefinitionGroup', ns)
              if f"'{configuration}|x64'" in group.get('Condition', '')]
    assert len(groups) == 1, (configuration, 'missing or ambiguous flag group')
    compiler = groups[0].find('m:ClCompile', ns)
    for name, value in expected.items():
        actual = compiler.findtext('m:' + name, namespaces=ns)
        assert actual == value, (configuration, name, actual, value)
        checks += 1
print(f'CHECKS={checks} FAILURES=0')
