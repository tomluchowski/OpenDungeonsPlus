#!/usr/bin/env python3
"""Pure check (nothing compiled, nothing started): a badly hurt creature walks with the clip WalkHurt when the
skeleton has it (else Walk), the entity state stays Walk (same speed factors), the clip is
switched when the health stage crosses the threshold."""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]

def read(path):
    return (root / path).read_text(encoding='utf-8').replace('\r\n', '\n')

movable_h = read('source/entities/MovableGameEntity.h')
creature_h = read('source/entities/Creature.h')
creature = read('source/entities/Creature.cpp')
render = read('source/render/RenderManager.cpp')

assert 'walk_hurt_anim = "WalkHurt"' in movable_h

# One stage test shared by the walk factor and the clip choice (16a condition and threshold)
assert 'static bool isLowHealthWalkStage(uint32_t healthStage);' in creature_h
assert 'isLowHealthWalkStage(mOverlayHealthValue)' in creature_h
stage = creature[creature.index('bool Creature::isLowHealthWalkStage'):creature.index('double Creature::getLowHealthWalkFactor')]
assert 'getLowHealthWalkThresholdPercent()' in stage and 'NB_OVERLAY_HEALTH_VALUES - 2' in stage
factor = creature[creature.index('double Creature::getLowHealthWalkFactor'):creature.index('double Creature::getClientPoseSpeedFactor')]
assert 'if(!isLowHealthWalking())' in factor and 'return 1.0;' in factor
assert 'getLowHealthWalkSpeedFactor()' in factor

# Clip choice: only for Walk, only if the skeleton has it
hurt = render.index('anim = EntityAnimation::walk_hurt_anim;')
block = render[hurt - 400:hurt]
assert '(anim == EntityAnimation::walk_anim)' in block
assert 'dropCreature->isLowHealthWalking()' in block
assert 'hasAnimation(EntityAnimation::walk_hurt_anim)' in block
# The existing missing-clip loop still runs afterwards (Walk is never replaced because the skeleton is checked)
assert render.index('while (!objectEntity->getSkeleton()->hasAnimation(anim))') > hurt

# Speed: the entity state stays Walk, the clip time uses the Walk factors, nothing is added for WalkHurt
movable = read('source/entities/MovableGameEntity.cpp')
assert 'walk_hurt_anim' not in movable

# The clip is switched when the health stage crosses the threshold while walking
update = creature[creature.index('void Creature::updateFromPacket'):]
assert 'isLowHealthWalkStage(oldHealthValue) != isLowHealthWalking()' in update
assert 'rrSetObjectAnimationState(this, EntityAnimation::walk_anim, true)' in update

print('check_hurt_walk_clip: ok')
