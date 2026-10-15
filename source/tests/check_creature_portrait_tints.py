"""Check config/portrait-tints.cfg and the code that applies it to the profile portraits."""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo / 'tools/portraits'))
import preview_tints as tints

palettes, portraits = tints.load_config(repo / 'config/portrait-tints.cfg')

for name, colours in palettes.items():
    assert colours, 'palette %s is empty' % name
    for colour in colours:
        assert 0 <= colour[1] <= 360 and 0 <= colour[2] <= 1 and 0 < colour[3] <= 1, (name, colour)

required = ('Kobold Dwarf1 Dwarf2 RunelordDwarf Gnome Adventurer Monk Knight Wizard Defender Cultist Elf DarkElf '
            'Goblin Orc Troll Lizardman').split()
for mesh in required:
    assert mesh in portraits and portraits[mesh], 'no tint regions for ' + mesh

for mesh, regions in portraits.items():
    assert (repo / ('materials/textures/portrait-%s.mesh.png' % mesh)).exists(), 'no portrait for ' + mesh
    for region_name, region in regions:
        where = '%s/%s' % (mesh, region_name)
        if 'palette' in region:
            assert region['palette'] in palettes, where + ': unknown palette'
        else:
            assert len(region['shift']) == 3, where
        x0, y0, x1, y1 = region['box']
        assert 0 <= x0 < x1 <= 1 and 0 <= y0 < y1 <= 1, where + ': box'
        assert len(region.get('not', [])) % 4 == 0, where + ': not'
        assert len(region['hue']) == 2 and len(region['sat']) == 2 and len(region['val']) == 2, where
        assert 0 <= region['sat'][0] <= region['sat'][1] <= 1 and 0 <= region['val'][0] <= region['val'][1] <= 1, where

creature = (repo / 'source/entities/Creature.cpp').read_text()
assert 'getCreatureProfilePortraitImage(getName(), definition->getMeshName())' in creature
assert 'clearCreatureProfilePortraits();' in (repo / 'source/modes/GameMode.cpp').read_text()
assert 'render/PortraitTint.cpp' in (repo / 'CMakeLists.txt').read_text()
print('portrait tints ok: %d meshes' % len(portraits))
