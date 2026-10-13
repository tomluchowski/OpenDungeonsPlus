"""Badly hurt creatures walk slower and limp (server and clients at the same speed).

Pure source wiring checks (no compiler, no game): the values are in the configuration, one shared rule on the
health stage both sides know, the move speed uses it and the client picks the clip WalkHurt for it."""
from pathlib import Path
import math
import re

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


global_cfg = read('config/global.cfg')
config_h = read('source/utils/ConfigManager.h')
config_cpp = read('source/utils/ConfigManager.cpp')
creature = read('source/entities/Creature.cpp')
creature_h = read('source/entities/Creature.h')
movable_h = read('source/entities/MovableGameEntity.h')
render = read('source/render/RenderManager.cpp')

# Values: documented and set in the configuration, read with a default and a limit
for key, value in (('LowHealthWalkSpeedFactor', 0.7), ('LowHealthWalkThresholdPercent', 50.0)):
    assert re.search(r'^# .*\n    ' + key + r'\t[\d.]+', global_cfg, re.M), key + ' not documented and set'
    assert float(re.search(r'^    ' + key + r'\t([\d.]+)', global_cfg, re.M)[1]) == value, key
    assert ('"' + key + '"') in config_cpp, key + ' not read'
assert 'mLowHealthWalkSpeedFactor(0.7)' in config_cpp and 'mLowHealthWalkThresholdPercent(50.0)' in config_cpp
assert 'std::max(0.2, std::min(1.0, mLowHealthWalkSpeedFactor))' in config_h
assert 'std::max(1.0, std::min(100.0, mLowHealthWalkThresholdPercent))' in config_h

# One rule on the health stage (known on server and clients, no new message)
stage = function_body(creature, 'bool Creature::isLowHealthWalkStage(uint32_t healthStage)')
assert 'std::ceil((100.0 - thresholdPercent) / 100.0 * nbSteps - 0.000001) + 1.0' in stage
assert 'NB_OVERLAY_HEALTH_VALUES - 2' in stage
assert 'getIsOnServerMap' not in stage
factor = function_body(creature, 'double Creature::getLowHealthWalkFactor() const')
assert 'isLowHealthWalking()' in factor and 'getLowHealthWalkSpeedFactor()' in factor


def first_stage(percent, nb_steps=6):
    return math.ceil((100.0 - percent) / 100.0 * nb_steps - 0.000001) + 1


assert first_stage(50) == 4
assert first_stage(100) == 1 and first_stage(1) == 7 and first_stage(34) == 5 and first_stage(17) == 6

# Server and client move with it: the same product on every returned speed
speed = function_body(creature, 'double Creature::getMoveSpeed(Tile* tile) const')
assert 'double hurtFactor = getLowHealthWalkFactor();' in speed
assert speed.count('* hurtFactor') == 4

# The client shows the limping clip and switches it when the health stage crosses the threshold
assert 'walk_hurt_anim = "WalkHurt"' in movable_h
assert 'anim = EntityAnimation::walk_hurt_anim;' in render
assert 'dropCreature->isLowHealthWalking()' in render
assert 'isLowHealthWalkStage(oldHealthValue) != isLowHealthWalking()' in creature
assert 'bool isLowHealthWalking() const' in creature_h and 'static bool isLowHealthWalkStage(uint32_t healthStage);' in creature_h

print('check_low_health_walk: ok')
