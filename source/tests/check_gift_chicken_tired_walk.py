"""Chicken gift for a creature that is not hungry and the slower walk of a tired creature.

Pure source wiring checks (no compiler, no game): server authority, configuration, saving with
old saves still loading, no new network message."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]


def read(path):
    return (root / path).read_text(encoding='utf-8')


def function_body(source, signature):
    start = source.index(signature)
    end = source.index('\n}\n', start)
    return source[start:end]


rooms_cfg = read('config/rooms.cfg')
global_cfg = read('config/global.cfg')
chicken = read('source/entities/ChickenEntity.cpp')
chicken_h = read('source/entities/ChickenEntity.h')
action = read('source/creatureaction/CreatureActionEatChicken.cpp')
action_h = read('source/creatureaction/CreatureActionEatChicken.h')
creature = read('source/entities/Creature.cpp')
creature_h = read('source/entities/Creature.h')
config_h = read('source/utils/ConfigManager.h')
config_cpp = read('source/utils/ConfigManager.cpp')
movable = read('source/entities/MovableGameEntity.cpp')
render = read('source/render/RenderManager.cpp')

# Every value of the gift is in the configuration (documented, set, read with a default)
gift_keys = ['HatcheryGiftOfferTurns', 'HatcheryGiftOfferRadius', 'HatcheryGiftSniffTurns',
             'HatcheryGiftHungerPerChicken', 'HatcheryGiftHpRecoveredPerChicken',
             'HatcheryGiftCooldownMin', 'HatcheryGiftCooldownMax']
for key in gift_keys:
    assert re.search(r'^# ' + key + r'\s', rooms_cfg, re.M), key + ' not documented'
    assert re.search(r'^    ' + key + r'\t\d+', rooms_cfg, re.M), key + ' not set'
    assert re.search(r'getRoomConfigDoubleOrDefault\(\s*"' + key + '"', chicken + action), key + ' not read'

# The offer: only a hen the keeper dropped outside a hatchery, server side, never in the editor
drop = function_body(chicken, 'void ChickenEntity::drop(')
assert 'getIsOnServerMap()' in drop and 'ChickenKind::hen' in drop and 'isInEditorMode()' in drop
assert 'checkCoveringRoomType(RoomType::hatchery)' in drop and 'mGiftTurns =' in drop
assert 'void drop(const Ogre::Vector3& v) override;' in chicken_h
assert 'mGiftTurns = 0;' in function_body(chicken, 'void ChickenEntity::pickup()')
upkeep = function_body(chicken, 'void ChickenEntity::doUpkeep()')
assert 'if(mGiftTurns > 0)\n        offerGift(*tile);' in upkeep
offer = function_body(chicken, 'void ChickenEntity::offerGift(')
assert 'getCreaturesBySeat(tile.getSeat())' in offer      # only creatures of the keeper whose tile it fell on
assert 'canAcceptGift' in offer and 'CreatureActionEatChicken>(*closest, *this, true)' in offer
assert 'mLockedEat' in offer                               # a chicken somebody is after is not offered

# The creature: idle, not hungry, no worker; the hatchery meal rules stay untouched
accept = function_body(action, 'bool CreatureActionEatChicken::canAcceptGift(')
for token in ('isWorker()', 'getActions().empty()', 'isHungry()', 'isKo()', 'isInPrison()'):
    assert token in accept, token
assert 'handleGiftChicken' in action_h and 'bool gift = false' in action_h
assert 'if(mGift)' in action and 'handleGiftChicken, std::ref(mCreature), mChicken' in action
# handleEatChicken stays exactly the meal of the hatchery (a navigation check compiles that very text);
# the new functions come after getListenerName so they are not part of it
listener = 'std::string CreatureActionEatChicken::getListenerName()'
eat = action[action.index('bool CreatureActionEatChicken::handleEatChicken('):action.index('\n' + listener)]
assert 'Gift' not in eat and 'mGift' not in eat
assert action.index('bool CreatureActionEatChicken::canAcceptGift(') > action.index(listener)
assert action.index('bool CreatureActionEatChicken::handleGiftChicken(') > action.index(listener)
gift_meal = function_body(action, 'bool CreatureActionEatChicken::handleGiftChicken(')
assert 'edibleBefore' in gift_meal and 'setHunger(' in gift_meal and 'setJobCooldown(' in gift_meal
assert 'HatcheryHungerPerChicken' not in gift_meal         # not the hatchery value
ctor = action[action.index('CreatureActionEatChicken::CreatureActionEatChicken('):action.index('CreatureActionEatChicken::~')]
assert 'mGift' in ctor and 'HatcheryGiftSniffTurns' in ctor

# Saving: the gift state is appended (after the age) and read as optional, old saves end earlier
export = function_body(chicken, 'void ChickenEntity::exportToStream(')
assert export.index('<< mAge <<') < export.index('mGiftTurns')
importBody = function_body(chicken, 'bool ChickenEntity::importFromStream(')
assert importBody.index('mAge = age;') < importBody.index('is >> giftTurns') < importBody.index('is.clear();')
assert 'Age\\tGiftTurns' in chicken
# The gift is server only: no new packet field, no new message
packet = chicken[chicken.index('void ChickenEntity::exportToPacket('):chicken.index('void ChickenEntity::exportToStream(')]
assert 'mGiftTurns' not in packet
assert 'ServerNotificationType' not in offer + gift_meal + drop

# Slower walk of a tired creature: both sides, same value, from the configuration
for key in ('TiredWakefulness', 'TiredWalkSpeedFactor'):
    assert re.search(r'^    ' + key + r'\t[\d.]+', global_cfg, re.M), key
    assert ('"' + key + '"') in config_cpp
assert 'double getTiredWalkSpeedFactor() const' in config_h
assert 'std::max(0.2, std::min(1.0, mTiredWalkSpeedFactor))' in config_h
assert 'mTiredWakefulness(20.0)' in config_cpp and 'mTiredWalkSpeedFactor(0.8)' in config_cpp
tired = function_body(creature, 'bool Creature::isTired() const')
assert 'mWakefulness <= ConfigManager::getSingleton().getTiredWakefulness()' in tired and '20.0' not in tired
speed = function_body(creature, 'double Creature::getMoveSpeed(Tile* tile) const')
assert 'if(isTired())' in speed and 'getTiredWalkSpeedFactor()' in speed
assert speed.count('* tiredFactor') == 4                     # server and client: building, default, bridge, default
# The walk clip speed is not scaled again in the renderer
assert 'getTiredWalkSpeedFactor' not in render

# Batch 13a: a chicken outside a hatchery walks back to one (dies only without a reachable one)
for key in ('HatcheryReturnPathTiles', 'HatcheryReturnSearchTiles', 'HatcheryReturnRetryTurns', 'HatcheryReturnMaxTurns'):
    assert re.search(r'^# ' + key + r'\s', rooms_cfg, re.M), key + ' not documented'
    assert re.search(r'^    ' + key + r'\t\d+', rooms_cfg, re.M), key + ' not set'
    assert re.search(r'getRoomConfigDoubleOrDefault\(\s*"' + key + '"', chicken), key + ' not read'
assert 'const bool henMayReturn = (mKind == ChickenKind::hen) && (mGiftTurns == 0) && !mLockedEat;' in upkeep
assert 'henMayReturn) && (currentHatchery == nullptr) && !mIsSlapped && runBackToHatchery(tile)' in upkeep
# the walk back comes before the death rule, so only a chicken without a way dies (30 turn rule unchanged)
assert upkeep.index('runBackToHatchery(tile)') < upkeep.index('NB_TURNS_OUTSIDE_HATCHERY_BEFORE_DIE))')
assert 'const int32_t NB_TURNS_OUTSIDE_HATCHERY_BEFORE_DIE = 30;' in chicken
# the gift window and a chicken somebody is after are not interrupted; the offer goes first
assert upkeep.index('runBackToHatchery(tile)') < upkeep.index('offerGift(*tile)')
back = function_body(chicken, 'bool ChickenEntity::runBackToHatchery(')
assert 'setWalkPath(' in back and 'getRoomsByTypeAndSeat(RoomType::hatchery, seat)' in back
assert 'getRoomsByType(RoomType::hatchery)' in back and 'seat = tile->getSeat();' in back
assert 'failReturn();' in back and 'mReturnRetryTurns' in chicken and 'mReturnTurns' in back
assert 'getIsOnServerMap' in chicken and 'setWalkPath' not in drop
# arrival: the hatchery counts the chickens on its tiles, so there is no separate registration (no double count)
assert 'mHomeSeat = currentHatchery->getSeat();' in upkeep
hatchery = read('source/rooms/RoomHatchery.cpp')
assert 'tile->fillWithEntities(entities, SelectionEntityWanted::chicken' in hatchery
# nothing saved or sent for it
assert 'mReturnTurns' not in export + importBody + packet and 'mReturnRetryTurns' not in export + importBody + packet

# Batch 13n: the chicken meal (E05) is grab, hold in front of the mouth, exactly two bites, struggling, gone after the second bite
import math
meal_keys = {}
for key in re.findall(r'^# (ChickenMeal\w+)\s', rooms_cfg, re.M):
    meal_keys[key] = float(re.search(r'^    ' + key + r'\t([\d.]+)\s*$', rooms_cfg, re.M)[1])
assert len(meal_keys) == 22, len(meal_keys)
settings = function_body(render, 'ChickenMealSettings getChickenMealSettings()')
for key, value in meal_keys.items():
    read = re.search(r'getChickenMealValue\("' + key + r'", ([\d.]+)\)', settings)
    assert read is not None, key + ' not read by the renderer'
    assert float(read[1]) == value, key + ' default differs from the configuration'
assert 'getRoomConfigDoubleOrDefault(key, defaultValue)' in function_body(render, 'Ogre::Real getChickenMealValue(')
grab, hold, bite, pause, swallow = [meal_keys['ChickenMeal' + n + 'Seconds'] for n in ('Grab', 'Hold', 'Bite', 'Pause', 'Swallow')]
first_bite = grab + hold
second_bite = first_bite + bite + pause
total = second_bite + bite + swallow
# the whole meal ends before the creature is allowed to do anything else (1.4 turns per second)
assert total <= float(re.search(r'^    HatcheryCooldownChickenMin\t(\d+)', rooms_cfg, re.M)[1]) / 1.4, total
assert 'meal.mTotal = meal.mSecondBite + meal.mBite + meal.mSwallow;' in settings
assert 'meal.mFirstBite = meal.mGrab + meal.mHold;' in settings
assert 'meal.mSecondBite = meal.mFirstBite + meal.mBite + meal.mPause;' in settings


def smooth(value):
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)


def bite_wave(time, start):
    progress = (time - start) / bite
    return 0.0 if progress <= 0.0 or progress >= 1.0 else math.sin(math.pi * progress) ** 2


def phase(time):
    after = second_bite + bite + 0.1 * swallow
    first_chunk = smooth((time - (first_bite + 0.4 * bite)) / (0.2 * bite))
    rest = smooth((time - (second_bite + 0.4 * bite)) / (0.3 * bite))
    return dict(reach=smooth(time / (0.55 * grab)),
                lift=smooth((time - 0.5 * grab) / (first_bite - 0.5 * grab - 0.3 * hold)),
                release=smooth((time - after) / (0.6 * swallow)),
                dip=bite_wave(time, first_bite) + bite_wave(time, second_bite),
                size=1.0 - 0.25 * first_chunk - 0.75 * rest,
                struggle=smooth((time - 0.45 * grab) / (0.25 * grab)) * (1.0 - 0.35 * first_chunk) * (1.0 - rest))


# the C++ phase function is the one simulated here
phase_src = function_body(render, 'ChickenMealPhase getChickenMealPhase(')
for text in ('phase.mReach = chickenMealSmooth(time / (0.55f * meal.mGrab));',
             '(meal.mFirstBite - 0.5f * meal.mGrab - 0.3f * meal.mHold)',
             'phase.mRelease = chickenMealSmooth((time - afterMeal) / (0.6f * meal.mSwallow));',
             'const Ogre::Real afterMeal = secondEnd + 0.1f * meal.mSwallow;',
             'phase.mSize = 1.0f - 0.25f * firstChunk - 0.75f * rest;',
             '(1.0f - 0.35f * firstChunk) * (1.0f - rest);',
             'chickenMealBite(time, meal.mFirstBite, meal.mBite) +', 'chickenMealBite(time, meal.mSecondBite, meal.mBite);'):
    assert text in phase_src, text
steps = int(total * 1000)
dips = [phase(i / 1000.0)['dip'] for i in range(steps + 1)]
assert len([i for i in range(1, steps) if dips[i - 1] <= 0.0 < dips[i]]) == 2      # exactly two bites
assert abs(max(dips[:int((first_bite + bite) * 1000)]) - 1.0) < 0.01 and abs(max(dips[int(second_bite * 1000):]) - 1.0) < 0.01
assert phase(first_bite)['lift'] > 0.999 and phase(first_bite)['struggle'] > 0.9   # held at the mouth and struggling before bite 1
assert phase(0.0)['size'] == 1.0 and 0.74 < phase(second_bite - 0.01)['size'] < 0.76  # three quarters left after the first bite
assert phase(second_bite + bite)['size'] < 0.001 and phase(second_bite + bite)['struggle'] < 0.001   # gone, still, after the second
assert phase(second_bite)['release'] == 0.0 and phase(total)['release'] > 0.999          # hands let go after the meal
struggle_times = [i / 1000.0 for i in range(steps + 1) if phase(i / 1000.0)['struggle'] > 0.5]
assert min(struggle_times) < first_bite - 0.2 and max(struggle_times) < second_bite + bite   # struggles from the grab to the second bite

# the clip, the hands and the chicken all follow the same phase function (no second timeline)
start_src = function_body(render, 'void RenderManager::startCreatureFeedingAnimation(')
assert 'getChickenMealPhase(progress * duration, meal)' in start_src and 'const Ogre::Real duration = meal.mTotal;' in start_src
assert 'Real bites' not in start_src and '2.2f' not in start_src and 'chew' not in start_src       # no seven pecks, no old timeline
assert 'meal.mLookAngle * phase.mPresent + meal.mBiteAngle * phase.mDip' in start_src and '-meal.mJawAngle * phase.mJaw' in start_src
loop_src = render[render.index('for(CreatureFeedingAnimation& feeding : mCreatureFeedingAnimations)\n    {\n        const ChickenMealSettings'):]
loop_src = loop_src[:loop_src.index('for(Creature* creature : finishedFeeding)')]
assert 'getChickenMealPhase(time, meal)' in loop_src and 'phase.mStruggle' in loop_src and 'phase.mSize' in loop_src
assert 'setVisible(phase.mSize > 0.03f)' in loop_src                                       # the chicken is gone after the second bite
assert 'if(feeding.mFeatherBursts < 2 && time >= meal.mFirstBite + 0.45f * meal.mBite +' in loop_src   # feather bursts only moved to the bites
assert loop_src.count('createChickenFeatherEffect(feeding.mNode->convertLocalToWorldPosition(mouth),') == 1
assert 'updateCreatureFeedingReach(feeding, progress, phase.mReach, phase.mLift,' in loop_src
reach_src = function_body(render, 'Ogre::Vector3 RenderManager::updateCreatureFeedingReach(')
assert 'std::function' not in reach_src and 'Degree(35.0f * crouch + biteLean)' in reach_src
assert 'Ogre::Vector3(side == 0 ? spread : -spread, 0, -drop)' in reach_src                 # fists below the chicken, seen from above
prepare_src = function_body(render, 'void RenderManager::prepareCreatureFeedingReach(')
assert 'holdAtRest.distance(shoulders) > armLength * meal.mReachRatio' in prepare_src      # short arms: chicken into the jaws
assert prepare_src.index('holdAtRest.distance(shoulders)') < prepare_src.index('retain(feeding.mSpine)')
mouth_src = function_body(render, 'Ogre::Vector3 RenderManager::getCreatureFeedingMouth(')
assert 'mouth.y = std::min(mouth.y, feeding.mJaw->_getDerivedPosition().y - 0.04f);' in mouth_src
# server side nothing changes: the chicken is taken and the meal paid at once, only the picture is longer
assert 'creature.fireChickenFeeding(chicken->getName(), chicken->getPosition());' in eat
assert 'chicken->eatChicken(&creature)' in eat

print('check_gift_chicken_tired_walk: ok')
