#!/usr/bin/env python3
# Static checks of the hatchery visuals and the rooster behaviour wiring (the decisions of the rooster are
# covered by the 00-HatcheryCycle unit test, the looks only show in the game).
from pathlib import Path

root = Path(__file__).resolve().parents[2]
chicken_h = (root / 'source/entities/ChickenEntity.h').read_text()
chicken = (root / 'source/entities/ChickenEntity.cpp').read_text()
room = (root / 'source/rooms/RoomHatchery.cpp').read_text()
render = (root / 'source/render/RenderManager.cpp').read_text()
looks = (root / 'source/render/RenderManagerChickens.cpp').read_text()
pose = (root / 'source/entities/ChickenPose.h').read_text()
config = (root / 'config/rooms.cfg').read_text()
cmake = (root / 'CMakeLists.txt').read_text()
credits = (root / 'CREDITS').read_text()
materials = (root / 'materials/scripts/ChickenHatchery.material').read_text()


def body(text, start):
    i = text.index(start)
    return text[i:text.index('\n}\n', i)]


# The egg has a mesh of its own and the mesh follows the kind (also for the clients and old saves).
assert 'ChickenEgg' in chicken and 'getMeshNameForKind' in chicken_h
assert 'setMeshName(getMeshNameForKind(kind))' in body(chicken, 'void ChickenEntity::setKind(')
assert 'destroyMesh()' in body(chicken, 'void ChickenEntity::setKindFromServer')
assert 'rrChickenHatched' in body(chicken, 'void ChickenEntity::setKindFromServer')

# Poses reach the client as animation names and are turned into skeleton animations plus motion.
for name in ('strut', 'chase', 'flee', 'mount', 'cackle', 'perch', 'crow', 'guard', 'lay', 'wobble', 'emerge', 'fight', 'protest', 'scratch', 'flutter'):
    assert 'static const std::string %s =' % name in pose, name
hook = body(render, 'void RenderManager::rrSetObjectAnimationState')
assert 'ChickenPose::isPose(animation)' in hook and 'rrSetChickenPose' in hook
assert hook.index('rrSetChickenPose') < hook.index('hasSkeleton')

# The server drives the behaviour from the hatchery upkeep.
upkeep = body(room, 'void RoomHatchery::doUpkeep')
for call in ('updateChickLine', 'updateRooster', 'updateNestTrips', 'ChickenPose::wobble'):
    assert call in upkeep, call
assert 'ChickenPose::emerge' in body(room, 'bool RoomHatchery::spawnFromCoop')
assert 'ChickenPose::lay' in body(room, 'void RoomHatchery::updateNestTrips')
assert 'new ChickenEntity' not in upkeep

# Chicks follow, the rooster that is dropped outside runs back to a hatchery of his keeper.
chickUpkeep = body(chicken, 'void ChickenEntity::doUpkeep')
assert 'mFollowing' in chickUpkeep and 'runBackToHatchery' in chickUpkeep
assert 'RoomType::hatchery' in body(chicken, 'bool ChickenEntity::runBackToHatchery')
assert 'mOnRoof = false' in body(chicken, 'void ChickenEntity::pickup')

# The rooster is still never food and does not count for the capacity.
assert 'isFree() && (mKind == ChickenKind::hen)' in chicken_h

# Client: looks, procedural meshes and the coop decoration.
for name in ('rrCreateChickenLook', 'rrDestroyChickenLook', 'rrCreateCoopDecor', 'updateChickenLooks'):
    assert name in looks, name
assert 'updateChickenLooks' in render and 'rrCreateCoopDecor' in render
assert 'RenderManagerChickens.cpp' in cmake and 'HatcheryRooster.cpp' in cmake

# Values come from the config.
for key in ('HatcheryRoosterCrowMin', 'HatcheryRoosterCrowMax',
            'HatcheryRoosterChasePercent',
            'HatcheryRoosterGuardRadius', 'HatcheryCoopRoofHeight', 'HatcheryChickGap'):
    assert key in config and key in room, key
for key in ('HatcheryChickScale', 'HatcheryRoosterScale', 'HatcheryChickTintR', 'HatcheryRoosterTintR'):
    assert key in config and key in looks, key

# Every material the code asks for exists and the new assets have a credit.
for name in ('ChickenEgg', 'ChickenStraw', 'ChickenRoosterComb', 'ChickenRoosterTail', 'ChickenFeatherDecor', 'ChickenEggShell'):
    assert 'material %s\n' % name in materials, name
    assert '"%s"' % name in looks, name
assert 'particles/ChickenEggShell.particle' in credits and 'ChickenHatchery.material' in credits
assert 'ChickenEgg.mesh' in credits and 'ChickenRoosterTail.mesh' in credits

print('hatchery visuals checks passed')

# The rooster does not call the hens or chicks: no call mood, no call settings; the crow stays (pose)
rooster_h = (root / 'source/rooms/HatcheryRooster.h').read_text()
rooster_cpp = (root / 'source/rooms/HatcheryRooster.cpp').read_text()
for text in (rooster_h, rooster_cpp, room):
    assert 'RoosterMood::call' not in text and 'mCallPercent' not in text and 'mCallTurns' not in text and 'mCallScratchChance' not in text
assert 'HatcheryRoosterCall' not in config and 'HatcheryRoosterCall' not in room
assert 'RoosterMood::crow' in rooster_cpp and 'ChickenPose::crow' in room
assert 'ChickenKind::hen' in body(chicken, 'void ChickenEntity::doUpkeep')

# The rooster fight is shown with feather clouds between the two animals, the interval is a setting
assert 'HatcheryFightFeatherSeconds' in looks and 'rrChickenFight' in looks and 'ChickenPose::fight' in looks
# The rooster guarding the flock lunges with his head, the rooster in the hand protests (pose and look)
assert 'std::sin(t * 7.0f)' in body(looks, 'void RenderManager::updateChickenLooks')
assert 'ChickenPose::protest' in body(chicken, 'void ChickenEntity::pickup') and 'protest)' in pose
assert 'ChickenPose::protest' in looks
# A hatchery with only the rooster looks abandoned: loose feathers, no nest. The rooster is not counted.
decor = body(looks, 'const bool check = mCoopDecorTimer')
assert 'ChickenKind::rooster' in decor and 'getEntitiesInTile' in decor
assert 'decor.mFeathers->setVisible(animals == 0)' in decor
assert 'countEntitiesOnTile(GameEntityType::chickenEntity)' not in decor
print('hatchery rooster look checks passed')

# Flocking of the hens: scratching, fluttering and scattering from a hungry creature
assert 'updateFlock' in body(room, 'void RoomHatchery::doUpkeep')
flock = body(room, 'void RoomHatchery::updateFlock')
for call in ('ChickenPose::flutter', 'ChickenPose::scratch', 'scatterTo', 'collectHungry'):
    assert call in flock, call
assert 'CreatureActionType::eatChicken' in body(room, 'void RoomHatchery::collectHungry')
for key in ('HatcheryScatterRadius', 'HatcheryScatterTurns', 'HatcheryFlutterPercent', 'HatcheryScratchPercent'):
    assert key in config and key in room, key
assert 'ChickenPose::scratch' in looks and 'ChickenPose::flutter' in looks
# Eggs and chicks dropped outside a hatchery are lost
assert 'HatcheryYoungLostTurns' in chickUpkeep and 'HatcheryYoungLostTurns' in config

# there is no night: no sleeping pose, no chick tucked in under a hen, the chicks always walk in a line behind the hen
chick_line = body(room, 'void RoomHatchery::updateChickLine')
assert 'night' not in chick_line.lower() and 'Each chick follows the one in front' in chick_line
assert 'ChickenPose::roost' not in looks and 'mSnuggleGap' not in room
print('hatchery chicks under hen checks passed')
