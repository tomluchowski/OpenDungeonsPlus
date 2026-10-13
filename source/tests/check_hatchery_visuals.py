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
for key in ('HatcheryChickScale', 'HatcheryRoosterScale'):
    assert key in config and key in looks, key

# Every material the code asks for exists and the new assets have a credit.
for name in ('ChickenEgg', 'ChickenEggInside', 'ChickenStraw', 'ChickenRoosterComb', 'ChickenRoosterTail', 'ChickenFeatherDecor', 'ChickenEggShell'):
    assert 'material %s\n' % name in materials, name
for name in ('ChickenStraw', 'ChickenFeatherDecor', 'ChickenEggShell'):
    assert '"%s"' % name in looks, name
assert 'particles/ChickenEggShell.particle' in credits and 'ChickenHatchery.material' in credits

# The egg, chick and rooster are meshes made in Blender (shared hen skeleton with the clips Peep, Run, Crow, Hatch).
models = root / 'models'
for mesh, names in (('ChickenEgg', ('ChickenEgg', 'ChickenStraw')), ('ChickenEggCracked', ('ChickenEggInside',)),
                    ('ChickenChick', ('ChickenChick', 'Chicken.skeleton')),
                    ('ChickenRooster', ('ChickenRooster', 'ChickenRoosterComb', 'ChickenRoosterTail', 'Chicken.skeleton'))):
    data = (models / (mesh + '.mesh')).read_bytes()
    for name in names:
        assert name.encode() in data, (mesh, name)
    assert mesh + '.mesh' in credits, mesh
skeleton = (models / 'Chicken.skeleton').read_bytes()
for clip in (b'Peep', b'Run', b'Crow', b'Hatch', b'Walk', b'Idle'):
    assert clip in skeleton, clip
breeds = (root / 'materials/scripts/ChickenBreeds.material').read_text()
assert 'material ChickenChick' in breeds and 'material ChickenRooster' in breeds
assert '"ChickenChick"' in chicken and '"ChickenRooster"' in chicken
assert 'ChickenPose::skeletonClip' in hook and 'hatchClip' in body(chicken, 'void ChickenEntity::setKindFromServer')
assert 'ChickenEggCracked' in looks

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
# Coop mesh with nests, door clip and roof lookout, and the real hen clips Lay and Flutter (text checks only)
coop_h = (root / 'source/rooms/HatcheryCoopHouse.h').read_text()
for name in ('meshName = "ChickenCoopHouse"', 'oldMeshName = "ChickenCoop"', 'doorClip = "Door"', 'roofPerchHeight',
             'roofPerchOffset', 'isCoopMesh'):
    assert name in coop_h, name
assert 'HatcheryCoopHouse::meshName' in body(room, 'BuildingObject* RoomHatchery::notifyActiveSpotCreated')
assert 'HatcheryCoopHouse::roofPerchHeight' in room and 'HatcheryCoopHouse::roofPerchOffset' in room
assert 'ChickenCoop"' not in room
assert render.count('HatcheryCoopHouse::isCoopMesh') == 2
coop_mesh = (models / 'ChickenCoopHouse.mesh').read_bytes()
assert b'ChickenCoopHouse.skeleton' in coop_mesh and b'ChickenCoop' in coop_mesh and b'ChickenCoopStraw' in coop_mesh
coop_skeleton = (models / 'ChickenCoopHouse.skeleton').read_bytes()
for name in (b'Root', b'Door', b'Lookout', b'Idle'):
    assert name in coop_skeleton, name
assert (models / 'ChickenCoop.mesh').exists()
bounds = (root / 'source/gamemap/RoomObjectBounds.h').read_text()
assert '{"ChickenCoopHouse", -.203275f, -.4f, .796725f, .4f}' in bounds
assert 'ChickenCoopHouse.mesh' in credits and 'ChickenCoopHouse.skeleton' in credits and 'clips Lay, Flutter' in credits
# The door of the coop swings with the animal that comes out, the old mesh shakes as before
assert 'mDoor' in looks and 'HatcheryCoopHouse::doorClip' in body(looks, 'void RenderManager::rrCreateCoopDecor')
assert 'mDoor->setEnabled(true)' in body(looks, 'void RenderManager::rrSetChickenPose')
assert 'mShake = 0.8f' in body(looks, 'void RenderManager::rrSetChickenPose')
assert 'mDoor->addTime' in body(looks, 'void RenderManager::updateChickenLooks')
assert 'mNest' not in body(looks, 'void RenderManager::rrDestroyCoopDecor'), 'the nests belong to the hatchery, not to a coop'
# Hen clips: used when the skeleton has them, procedural motion stays as fallback
for name in (b'Lay', b'Flutter', b'Peep', b'Run', b'Crow', b'Hatch', b'Die', b'Pick', b'Paw', b'Sleep', b'Walk', b'Idle'):
    assert name in skeleton, name
assert 'return "Lay"' in pose and 'return "Flutter"' in pose and 'isOneShotClip' in pose
assert 'ChickenPose::isOneShotClip(clip)' in hook and 'hasAnimation(clip)' in hook
assert 'hasAnimation("Lay")' in looks and 'hasAnimation("Flutter")' in looks
assert 'stretch = Ogre::Vector3(1.14f, 1.1f' in looks and 'lift = 0.14f * rise' in looks
for name in ('hen_lay_flutter.py', 'coop_house.py'):
    assert (root / 'tools/blender-assets' / name).exists(), name
# Lay and Flutter keep their translations in the parent frame (the frame fix tool says why ogre_fix must not touch them)
fix_tool = (root / 'tools/blender-assets/chicken_frame_fix.py').read_text()
assert 'ogre_fix.py must NOT be applied' in fix_tool and 'T_new = q_bind * T_old' in fix_tool
import shutil
import subprocess
import sys
import tempfile
if shutil.which('OgreXMLConverter') is not None:
    sys.path.insert(0, str(root / 'tools/blender-assets'))
    import chicken_frame_fix as frame_fix
    import xml.etree.ElementTree as ET
    with tempfile.TemporaryDirectory() as tmp:
        out_xml = str(Path(tmp) / 'Chicken.xml')
        subprocess.check_call(['OgreXMLConverter', '-q', str(models / 'Chicken.skeleton'), out_xml],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        skeleton_xml = ET.parse(out_xml).getroot()
    assert len(frame_fix.structure(skeleton_xml)[0]) == frame_fix.BONE_COUNT
    assert frame_fix.structure(skeleton_xml)[2] == sorted(frame_fix.CLIP_NAMES)
    lay_hip = frame_fix.track_values(skeleton_xml, 'Lay', 'Hip', 'z')
    flutter_root = frame_fix.track_values(skeleton_xml, 'Flutter', 'Root', 'z')
    assert abs(min(lay_hip) + 0.05) < 0.002, min(lay_hip)
    assert abs(max(flutter_root) - 0.075) < 0.002, max(flutter_root)
    assert max(abs(v) for v in frame_fix.track_values(skeleton_xml, 'Lay', 'Hip', 'y')) < 0.002
else:
    print('OgreXMLConverter not on the PATH: skeleton frame check skipped')
print('hatchery coop and hen clip checks passed')
