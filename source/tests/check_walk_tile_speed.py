#!/usr/bin/env python3
"""Pure check (nothing compiled, nothing started): the client clip speed of the walk state (Walk, WalkHurt, CarryWalk)
follows the speed of the tile the creature stands on (water, lava, bridge, ground) relative to its ground speed, blended
over a short time when the tile changes, applied once, never to the move speed or the network."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]

def read(path):
    return (root / path).read_text(encoding='utf-8').replace('\r\n', '\n')

creature = read('source/entities/Creature.cpp')
creature_h = read('source/entities/Creature.h')
movable = read('source/entities/MovableGameEntity.cpp')
movable_h = read('source/entities/MovableGameEntity.h')

# Ratio: same tile speed the client moves with (bridge = ground, else tile default), ground speed as reference, clamped
ratio = creature[creature.index('double Creature::getTileSpeedRatio'):creature.index('void Creature::updateClientPose')]
assert 'tile->getHasBridge() ? groundSpeed : tile->getCreatureSpeedDefault(this)' in ratio
assert 'tileSpeed / groundSpeed' in ratio and 'std::max(TILE_SPEED_RATIO_MIN, std::min(TILE_SPEED_RATIO_MAX' in ratio
assert 'TILE_SPEED_RATIO_MIN = 0.2;' in creature and 'TILE_SPEED_RATIO_MAX = 4.0;' in creature
assert 'groundSpeed <= 0.0' in ratio and 'tileSpeed <= 0.0' in ratio and 'return 1.0;' in ratio
# no tired / hurt factor inside the ratio (they are applied once in getClientPoseSpeedFactor)
for word in ('Tired', 'LowHealth', 'getMoveSpeed('):
    assert word not in ratio, word

# Blend: starts without blending, resets when the walk clip stops, bounded step per frame
blend = creature[creature.index('void Creature::updateClientPose'):creature.index('double Creature::getClientPoseSpeedFactor')]
assert 'getAnimationStateName() != EntityAnimation::walk_anim' in blend and 'mClientTileSpeedRatio = -1.0;' in blend
assert 'mClientTileSpeedRatio = target;' in blend and 'timeSinceLastFrame' in blend and '* TILE_SPEED_RATIO_MAX_CHANGE_PER_SECOND' in blend
assert 'TILE_SPEED_RATIO_MAX_CHANGE_PER_SECOND = 4.0;' in creature
assert 'mClientTileSpeedRatio    (-1.0)' in creature and creature.count('mClientTileSpeedRatio    (-1.0),') == 2
assert 'double                          mClientTileSpeedRatio;' in creature_h
assert 'virtual void updateClientPose(double timeSinceLastFrame) override;' in creature_h

# Applied once, only in the walk state, together with the clip rate; tired and low health factors stay single
pose = creature[creature.index('double Creature::getClientPoseSpeedFactor'):creature.index('double Creature::getPhysicalDefense')]
assert pose.count('mClientTileSpeedRatio') == 2 and pose.count('getTileSpeedRatio()') == 1
walk_block = pose[pose.index('if((getAnimationStateName() == EntityAnimation::walk_anim)'):]
assert 'mClientTileSpeedRatio' in walk_block
assert pose.count('getLowHealthWalkFactor()') == 1

# Hook called every frame before the clip time advances, client branch only; move speed and network untouched
assert 'virtual void updateClientPose(double timeSinceLastFrame)' in movable_h
update = movable[movable.index('void MovableGameEntity::update'):]
assert update.index('updateClientPose(') < update.index('shownTime *= getClientPoseSpeedFactor()')
assert not update[:update.index('updateClientPose(')].count('getIsOnServerMap()') == 0
move = creature[creature.index('double Creature::getMoveSpeed(Tile* tile)'):creature.index('double Creature::getLowHealthWalkFactor')]
assert 'mClientTileSpeedRatio' not in move and 'getTileSpeedRatio' not in move
for path in ('source/network/ODPacket.cpp', 'source/network/ServerNotification.cpp'):
    p = root / path
    if p.exists():
        assert 'mClientTileSpeedRatio' not in p.read_text(encoding='utf-8')
print('check_walk_tile_speed: ok')
