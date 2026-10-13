"""Wiring checks for the coins and gems on the treasury gold: single coins and gems on the piles, overflow at the
open edges and scattered coins on bare floors. Client mesh and server names only, within the 'Treasury detail'
option (no compiler needed)."""
from pathlib import Path

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


mesh = read('source/render/TreasuryGoldMesh.cpp')
layer = read('source/rooms/TreasuryGoldLayer.h')
treasury = read('source/rooms/RoomTreasury.cpp')
material = read('materials/scripts/TreasuryGoldPile.material')
config = read('config/treasury.cfg')
settings = read('source/rooms/TreasurySettings.h')

# Coins, gems and spilled coins: a second mesh section, only at the full detail, derived from the pile name alone.
assert 'material TreasuryGoldDetail' in material and 'ambient vertexcolour' in material
assert 'DetailMaterial' in mesh and 'buildPileMesh(sceneManager, name + ".mesh", shape, reduced ? ReducedRings : FullRings, !reduced)' in mesh
for name in ('topCoinCount', 'gemCount', 'edgeOpen', 'spillCoinsPerEdge', 'hasFloorScatter'):
    assert name in layer and name in mesh + treasury, name
assert 'maxTopCoins' in layer and 'maxGems' in layer and 'maxSpillCoins' in layer

# The numbers are settings of config/treasury.cfg
for key in ('MaxTopCoins', 'TopCoinMinLevel', 'MaxGems', 'SpillCoinsFull', 'ScatterCoins'):
    assert ('    ' + key + '\t') in config and ('#   ' + key) in config and key in settings, key

# Bare floor: the server names a level 0 pile for some empty tiles; the client draws nothing for it unless full.
assert 'hasFloorScatter(tile->getX(), tile->getY())' in treasury
assert 'mGoldChanged = true;\n    return new RoomTreasuryTileData' in treasury.replace('\r\n', '\n')
assert 'shape.mLevel == 0 && (currentDetail != Detail::full || farAway)' in mesh

# Reduced and off keep the round surface, without coins and gems
assert 'const bool reduced = (currentDetail != Detail::full) || (farAway && currentDetail == Detail::full);' in mesh
assert 'withDetail && hasDetail(shape)' in mesh
print('ok')
