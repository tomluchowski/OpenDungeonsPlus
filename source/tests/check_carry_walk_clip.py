"""A creature that carries something plays the CarryWalk clip while it walks (clients only).

Pure source wiring checks (no compiler, no game): the carry and release messages set a client flag, the clip is
chosen where the walk clip is applied (before WalkHurt) and the carrier switches clips when it picks up or drops."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


movable_h = read('source/entities/MovableGameEntity.h')
creature_h = read('source/entities/Creature.h')
creature = read('source/entities/Creature.cpp')
client = read('source/network/ODClient.cpp')
render = read('source/render/RenderManager.cpp')

assert 'carry_walk_anim = "CarryWalk"' in movable_h
assert 'bool getClientCarrying() const' in creature_h and 'mClientCarrying;' in creature_h
assert creature.count('mClientCarrying          (false)') == 2
assert 'carrier->setClientCarrying(true);' in client and 'carrier->setClientCarrying(false);' in client
assert client.index('carrier->setClientCarrying(true);') < client.index('rrCarryEntity(carrier, carried)')
assert client.index('carrier->setClientCarrying(false);') < client.index('rrReleaseCarriedEntity(carrier, carried)')
choose = render.index('anim = EntityAnimation::carry_walk_anim;')
assert choose < render.index('anim = EntityAnimation::walk_hurt_anim;'), 'carrying must win over limping'
assert 'dropCreature->getClientCarrying()' in render
assert render.count('rrSetObjectAnimationState(carrier, EntityAnimation::walk_anim, true);') == 2

print('check_carry_walk_clip: ok')
