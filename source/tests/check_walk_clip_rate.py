"""Per creature type speed of the walk clips on the client (WalkClipRate, WalkHurtClipRate).

Pure source wiring checks (no compiler, no game): the keys are parsed, copied, sent, saved and documented, every
walking type has a rate, and the client uses the rate of the clip that plays on top of the tile speed ratio."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


definition = read('source/entities/CreatureDefinition.cpp')
definition_h = read('source/entities/CreatureDefinition.h')
creatures_cfg = read('config/creatures.cfg')
creature = read('source/entities/Creature.cpp')
creature_h = read('source/entities/Creature.h')
movable = read('source/entities/MovableGameEntity.cpp')

# Keys: defaults, copy, network order (sent and read in the same order), parse, save, documentation
for member, default in (('mLowHealthWalkSpeedFactor', '-1.0'), ('mWalkClipRate', '1.0'), ('mWalkHurtClipRate', '-1.0')):
    assert (member + ' (' + default + ')') in definition, member + ' default'
    assert (member + '(def.' + member + ')') in definition, member + ' copy'
    assert ('os << c->' + member + ';') in definition and ('is >> c->' + member + ';') in definition, member + ' packet'
    assert ('double ' + member + ';') in definition_h, member + ' declaration'
sent = re.findall(r'os << c->(mLowHealthWalkSpeedFactor|mWalkClipRate|mWalkHurtClipRate);', definition)
read_back = re.findall(r'is >> c->(mLowHealthWalkSpeedFactor|mWalkClipRate|mWalkHurtClipRate);', definition)
assert sent == read_back == ['mLowHealthWalkSpeedFactor', 'mWalkClipRate', 'mWalkHurtClipRate']
for key in ('LowHealthWalkSpeedFactor', 'WalkClipRate', 'WalkHurtClipRate'):
    assert ('nextParam == "' + key + '"') in definition, key + ' not parsed'
    assert ('"    ' + key + '\\t"') in definition, key + ' not saved'
    assert re.search(r'^# ' + key + r' ', creatures_cfg, re.M), key + ' not documented'
assert 'std::max(0.2, std::min(8.0, mWalkClipRate))' in definition_h
assert 'getWalkClipRate();' in definition_h   # WalkHurtClipRate falls back to the walk rate

# Rates in the types: within the limits
rates = [float(v) for v in re.findall(r'^    WalkClipRate\t([\d.]+)', creatures_cfg, re.M)]
assert len(rates) >= 20 and all(0.2 <= r <= 8.0 for r in rates)

# Client: the rate of the clip that plays, times the (blended) tile speed ratio, only for the walk clip
pose = function_body(creature, 'double Creature::getClientPoseSpeedFactor() const')
assert 'getWalkHurtClipRate()' in pose and 'getWalkClipRate()' in pose and 'walk_hurt_anim' in pose
assert 'mClientTileSpeedRatio' in pose and 'getTileSpeedRatio()' in pose
assert 'virtual double getClientPoseSpeedFactor() const override;' in creature_h
assert 'updateClientPose(static_cast<double>(timeSinceLastFrame));' in movable
assert movable.count('getClientPoseSpeedFactor()') == 1
ratio = function_body(creature, 'void Creature::updateClientPose(double timeSinceLastFrame)')
assert 'timeSinceLastFrame) * 4.0' in ratio

print('check_walk_clip_rate: ok')
