/*
 *  Copyright (C) 2011-2016  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "render/CreatureReactions.h"

#include "ODApplication.h"
#include "camera/CameraManager.h"
#include "creaturemood/CreatureMood.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/CreatureMoodValues.h"
#include "entities/GameEntity.h"
#include "entities/GameEntityType.h"
#include "entities/MovableGameEntity.h"
#include "entities/Tile.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "render/CreatureOverlayStatus.h"
#include "render/ODFrameListener.h"
#include "render/RenderManager.h"
#include "rooms/Room.h"
#include "rooms/RoomManager.h"
#include "rooms/RoomType.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <OgreAnimationState.h>
#include <OgreBillboard.h>
#include <OgreBillboardSet.h>
#include <OgreCamera.h>
#include <OgreEntity.h>
#include <OgreMaterialManager.h>
#include <OgreParticleSystem.h>
#include <OgreParticleSystemManager.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreSkeletonInstance.h>

#include <algorithm>
#include <cmath>
#include <random>

template<> CreatureReactions* Ogre::Singleton<CreatureReactions>::msSingleton = nullptr;

namespace
{

//! Prefix of the material of an emote icon (the emote name follows)
static const std::string EMOTE_MATERIAL_PREFIX = "CreatureEmote_";
//! Prefix of the scene name of a particle system a reaction creates (a counter follows)
static const std::string PARTICLE_NAME_PREFIX = "CreatureReaction_";
//! Seconds between two runs that delete the cooldown entries that are over
static const double PRUNE_INTERVAL = 30.0;
//! Pi, used for the hop, shake, squash and spin motions
static const double PI_VALUE = 3.14159265358979323846;
//! Seconds after an attack animation during which the creature counts as one of the fighters
static const double ATTACK_MEMORY = 3.0;
//! Creatures that attacked this close to the loser (world units) are the winners
static const double WINNER_RADIUS = 7.0;
//! Creatures this close to the loser can join in the cheering of the group
static const double GROUP_RADIUS = 9.0;
//! The same creature going down or fleeing is not celebrated again within this time
static const double CELEBRATION_PAUSE = 10.0;
//! Seconds a gift of the keeper is remembered
static const double HAND_DROP_MEMORY = 120.0;
//! Seconds a reaction waits at most for the creature to finish what it is doing
static const double PENDING_WAIT_MAX = 3.0;
static const double PENDING_WAIT_STEP = 0.25;
//! Largest squared distance at which a node still counts as being where the motion left it
static const double MOTION_UNCHANGED_SQUARED_TOLERANCE = 0.00000001;
//! Share of the squash by which the model grows sideways while it gets lower
static const double SQUASH_SIDE_GROWTH = 0.5;
//! Shortest distance (world units) at which the direction to the camera or the front is used
static const double TURN_MIN_LENGTH = 0.01;
//! Share of the turn motion spent turning towards the camera, and again turning back
static const double TURN_PHASE = 0.25;
//! Smallest change of the turn angle (radians) that is applied to the node
static const double ROTATION_EPSILON = 0.00001;
//! Seconds a done moment waits at most for the creature to finish its get-up or meal
static const double DONE_WAIT_MAX = 8.0;
//! Seconds after the end of the meal animation in the hatchery before the creature shows how it liked it
static const double HATCHERY_MEAL_DONE_DELAY = 0.5;
//! Value of the wait time of a queued reaction that stands for the usual waiting time
static const double USUAL_WAIT_TIME = -1.0;
//! Seconds a creature has to sleep or pray until the end of it is shown
static const double SLEEP_DONE_MIN = 6.0;
static const double PRAYER_DONE_MIN = 8.0;
static const double CLAIM_DONE_MIN = 2.5;
//! Seconds a creature has to run away until it sulks over the lost fight
static const double FLEE_DONE_MIN = 2.0;
//! Seconds between a prisoner breaking under torture and the first sign of its new loyalty
static const double CONVERTED_DELAY = 2.8;
//! A creature that delivers gold this many times within the window is out of breath
static const double DELIVERY_WINDOW = 90.0;
static const uint32_t DELIVERY_TIRED_COUNT = 3;
//! The treasury work follows the done moment of a delivery after this time
static const double TREASURY_WORK_DELAY = 2.6;
//! Seconds after its last work a creature counts as the one that finished the result of the room
static const double ROOM_WORK_MEMORY = 20.0;
//! The other creatures in the room react to the result after this time
static const double ROOM_RESULT_DELAY = 0.7;
//! Seconds between two tries to show the work reaction of a creature that does something for a while
static const double ONGOING_MIN = 5.0;
static const double ONGOING_MAX = 9.0;
//! Shortest wait before the first try when a creature starts to do something for a while
static const double ONGOING_FIRST_MIN = 2.0;
//! Prefix of the material of a prop sprite (the prop name follows)
static const std::string PROP_MATERIAL_PREFIX = "CreatureProp_";
//! Prefix of the scene name of a prop sprite a reaction creates (a counter follows)
static const std::string PROP_NAME_PREFIX = "CreatureReactionProp_";
//! The reactions of the moods and the habits leave this many places of the simultaneous ones to the events
static const uint32_t MOOD_RESERVED_SLOTS = 2;
//! Health stage (0 is unhurt, 7 is dead) from which a creature counts as hurt
static const uint32_t HURT_STAGE = 4;
//! A hurt creature is scared by an enemy this close (world units)
static const double SCARE_RADIUS = 9.0;
//! Another creature this close can be looked at or catch a yawn
static const double NEIGHBOUR_RADIUS = 6.0;
//! Share of the reaction time spent sinking down, and again getting up, for the sit motion
static const double SIT_RAMP = 0.12;
//! Share of the sit motion by which the model grows sideways while it gets lower
static const double SIT_SIDE_GROWTH = 0.4;
//! Share of the startle motion spent nodding off, the rest is the jump
static const double STARTLE_NOD_SHARE = 0.7;
//! Share of the nod by which the model grows sideways while it gets lower
static const double STARTLE_SIDE_GROWTH = 0.5;
//! Height (world units) of the jump at the end of the startle motion
static const double STARTLE_JUMP_HEIGHT = 0.15;
//! Upper bound of the random number that picks one of the four sides next to a creature
static const double WALL_SIDE_RANDOM_MAX = 3.999;
//! Number of tiles next to a tile that share an edge with it
static const uint32_t NB_WALL_SIDES = 4;
//! Distance (world units) from the middle of a tile to the face of the wall next to it
static const float WALL_FACE_OFFSET = 0.5f;
//! The creatures that catch a yawn show it after this much time
static const double YAWN_CATCH_DELAY = 1.1;
//! Share of the reaction time spent tipping over, and again getting up, for the lie motion
static const double LIE_RAMP = 0.15;
//! Most creatures that turn their head to the same event
static const uint32_t MAX_LOOKERS = 3;
//! Seconds before the creatures look at the next fight, and at the next fall of gold
static const double FIGHT_LOOK_INTERVAL = 4.0;
static const double GOLD_LOOK_INTERVAL = 1.0;
//! Chance (0 to 1) that a small habit comes before a change of the idle pose
static const double HABIT_FIRST_CHANCE = 0.5;
//! Shortest and tallest creature height (world units) the size of the prop sprites follows
static const double PROP_HEIGHT_MIN = 0.3;
static const double PROP_HEIGHT_MAX = 6.0;
//! Distance (world units) in front of a creature the props use as their target when there is no wall to turn to
static const float PROP_FRONT_DISTANCE = 0.9f;
//! Shortest length of a direction vector that can still be normalised
static const float DIRECTION_MIN_LENGTH = 0.01f;
//! Number of juggling balls that have a colour of their own
static const uint32_t NB_BALL_COLOURS = 3;

//! Seconds the target of a slap request is remembered until the server confirms the slap
const double SLAP_MEMORY = 3.0;
//! Without a remembered target, a creature this close (world units) to the hand got the slap
const double SLAP_FALLBACK_RADIUS = 2.0;
//! A creature slapped within this time ducks when the hand comes over it
const double SLAP_DUCK_MEMORY = 90.0;
//! Health stages (see Creature) a creature has to get better by in one update to count as healed
static const uint32_t HEAL_MIN_STEPS = 2;
//! Standing creatures this close to an ally that went down mourn it (world units)
static const double ALLY_DEATH_RADIUS = 8.0;
//! Seconds the mourning creatures wait (staggered) after the ally went down
static const double ALLY_DEATH_DELAY = 1.2;
//! Seconds a fleeing creature waits before it shows the panic
static const double FLEE_PANIC_DELAY = 0.4;
//! The sight of an enemy lets the standing creatures of the keeper close by react at most this often
static const double ENEMY_SPOTTED_INTERVAL = 20.0;
//! Creatures that walk towards each other and are this close (world units) bump into each other
static const double BUMP_RADIUS = 1.3;
//! A worker waves at a fighter that passes within this share more than the meeting radius
static const double WAVE_RADIUS_FACTOR = 1.3;
//! Standing creatures laugh at a slapped creature at most this often (seconds)
static const double SLAP_LAUGH_INTERVAL = 3.0;
//! Shortest and longest wait (seconds) of the answer of a chat partner
static const double CHAT_REPLY_DELAY_MIN = 0.9;
static const double CHAT_REPLY_DELAY_SPREAD = 0.6;
//! Seconds the answer to a sparring push, a wave and a grumble waits
static const double SPAR_REPLY_DELAY = 0.5;
static const double WAVE_REPLY_DELAY = 0.7;
static const double GRUMBLE_REPLY_DELAY = 0.4;
//! Kept off the upper end of the range so that the random choice never indexes past the last option
static const double CHOICE_RANGE_MARGIN = 0.001;
//! Creatures closer than this (world units) have no usable direction to each other
static const float FACING_MIN_LENGTH = 0.01f;
//! Dot product above which a creature looks at the other one (and below its negative the other looks back)
static const float FACING_MIN_DOT = 0.3f;
//! Meeting kinds the dice choose from
static const uint32_t MEETING_CHAT = 0;
static const uint32_t MEETING_SPAR = 1;
static const uint32_t MEETING_WAVE = 2;
static const uint32_t MEETING_GRUMBLE = 3;
//! Creatures that are on the map when the game starts did not arrive: nothing is shown in this time
const double ARRIVAL_QUIET_TIME = 3.0;
//! Seconds the reactions of a picked up, a dropped and an arrived creature wait for the creature to be ready
const double PICKED_UP_DELAY = 0.4;
const double DROPPED_DELAY = 0.35;
const double ARRIVAL_DELAY = 0.9;

//! Cosmetic dice of their own: the reactions must not draw from the generator the game logic uses
std::mt19937& cosmeticRng()
{
    static std::mt19937 rng(std::random_device{}());
    return rng;
}

//! \brief Returns an evenly distributed random number from the cosmetic generator between min and max.
double cosmeticRandom(double min, double max)
{
    std::uniform_real_distribution<double> distribution(min, max);
    return distribution(cosmeticRng());
}

//! \brief True if the text begins with the prefix.
bool startsWith(const std::string& text, const std::string& prefix)
{
    return text.compare(0, prefix.size(), prefix) == 0;
}

//! \brief True if the list holds the value.
bool contains(const std::vector<std::string>& list, const std::string& value)
{
    return std::find(list.begin(), list.end(), value) != list.end();
}

//! 0 at the start and the end of the reaction, 1 in between. The ramps take the share ramp of the time.
double plateau(double progress, double ramp)
{
    double envelope = 1.0;
    if(progress < ramp)
        envelope = progress / ramp;
    else if(progress > (1.0 - ramp))
        envelope = (1.0 - progress) / ramp;

    envelope = std::max(0.0, std::min(1.0, envelope));
    return envelope * envelope * (3.0 - 2.0 * envelope);
}

//! Where the sprites of a prop are: in shares of the height of the creature, seen from the creature
struct PropFrame
{
    Ogre::Vector3 mRight;
    Ogre::Vector3 mForward;
    double mHeight;

    //! \brief The offset of a point from the creature: sideways, ahead and up, each in shares of the height
    Ogre::Vector3 at(double sideways, double ahead, double up) const
    {
        return mRight * static_cast<Ogre::Real>(sideways * mHeight) +
            mForward * static_cast<Ogre::Real>(ahead * mHeight) +
            Ogre::Vector3(0.0f, 0.0f, static_cast<Ogre::Real>(up * mHeight));
    }
};

} // namespace

CreatureReactions::CreatureReactions(GameMap* gameMap, const std::string& configPath) :
    mGameMap(gameMap),
    mConfigFileName(configPath + "creatureReactions.cfg"),
    mConfigLoaded(false),
    mMode(Mode::full),
    mTime(0.0),
    mTimeLastPrune(0.0),
    mNextParticleId(0),
    mNextPropId(0),
    mMoodTimer(0.0),
    mMoodIndex(0),
    mNextLookTarget(Ogre::Vector3::ZERO),
    mHasNextLookTarget(false),
    mSlapTime(0.0),
    mNextInteraction(0.0)
{
    mConfigLoaded = mConfig.load(mConfigFileName);
    if(!mConfigLoaded)
        OD_LOG_WRN("Creature reactions are not available");
}

//! \brief Destroys the reactions. The members clean up themselves, so the body is empty.
CreatureReactions::~CreatureReactions()
{
}

CreatureReactions::Mode CreatureReactions::modeFromString(const std::string& text)
{
    if(text == "off")
        return Mode::off;
    if(text == "reduced")
        return Mode::reduced;

    return Mode::full;
}

std::string CreatureReactions::modeToString(Mode mode)
{
    switch(mode)
    {
        case Mode::off:
            return "off";
        case Mode::reduced:
            return "reduced";
        default:
            return "full";
    }
}

void CreatureReactions::setMode(Mode mode)
{
    mMode = mode;
    if(mMode != Mode::full)
        stopAll();
}

bool CreatureReactions::reloadConfig()
{
    stopAll();
    mConfig = CreatureReactionConfig();
    mLoggedMissing.clear();
    mConfigLoaded = mConfig.load(mConfigFileName);
    return mConfigLoaded;
}

bool CreatureReactions::creatureNeedsSleep(const Creature* creature)
{
    const CreatureDefinition* definition = creature->getDefinition();
    if(definition == nullptr)
        return false;

    // A creature that never loses wakefulness never gets tired and never goes to bed
    return definition->getWakefulnessLostPerTurn() > 0.0;
}

void CreatureReactions::logMissingOnce(const std::string& kind, const std::string& name)
{
    std::string key = kind + " " + name;
    if(!mLoggedMissing.insert(key).second)
        return;

    OD_LOG_WRN("Creature reactions: missing " + kind + " '" + name + "', using the next tier");
}

Ogre::Entity* CreatureReactions::getCreatureEntity(const Creature* creature) const
{
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    std::string entityName = creature->getOgreNamePrefix() + creature->getName();
    if(!sceneManager->hasEntity(entityName))
        return nullptr;

    return sceneManager->getEntity(entityName);
}

bool CreatureReactions::isCreatureNearCamera(Creature* creature) const
{
    Ogre::Entity* entity = getCreatureEntity(creature);
    if(entity == nullptr)
        return false;

    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
    if(frameListener == nullptr)
        return false;

    Ogre::Camera* camera = frameListener->getCameraManager()->getActiveCamera();
    if(camera == nullptr)
        return false;

    if(!camera->isVisible(entity->getWorldBoundingBox(true)))
        return false;

    Ogre::Real distance = (camera->getDerivedPosition() - creature->getPosition()).length();
    return distance <= static_cast<Ogre::Real>(mConfig.getMaxCameraDistance());
}

ReactionPriority CreatureReactions::getCreaturePriority(const Creature* creature, const ReactionEvent* event) const
{
    Ogre::AnimationState* animState = creature->getAnimationState();
    if(animState == nullptr)
        return ReactionPriority::none;

    const std::string& clip = animState->getAnimationName();
    if((clip == "Die") || (clip == "die") || (clip == "Rot"))
    {
        // The death animation is what an event of the dying creature decorates
        if((event != nullptr) && event->mDying)
            return ReactionPriority::none;

        return ReactionPriority::death;
    }

    // The event decorates the work or sleep animation: that is what the creature is expected to do
    if((event != nullptr) && event->mWhileWorking &&
       (startsWith(clip, "Sleep") || (clip == "Dig") || (clip == "Claim") || (clip == "Flee")))
    {
        return ReactionPriority::none;
    }

    // The work in some rooms is shown with the attack animation: that is not a fight
    if(startsWith(clip, "Attack") && isWorkingInRoom(creature))
        return ReactionPriority::work;

    if(startsWith(clip, "Attack") || (clip == "CombatAttack") || (clip == "RangedAttack") ||
       (clip == "Flee") || startsWith(clip, "Cast") || (clip == "Parry") || startsWith(clip, "Hurt") ||
       (clip == "Damage") || startsWith(clip, "Throw"))
    {
        return ReactionPriority::combat;
    }

    if(startsWith(clip, "Sleep") || (clip == "Drop") || (clip == "GetUp") || (clip == "EatChicken"))
        return ReactionPriority::held;

    if((clip == "Dig") || (clip == "Claim"))
        return ReactionPriority::work;

    return ReactionPriority::none;
}

bool CreatureReactions::isWorkingInRoom(const Creature* creature) const
{
    Tile* tile = creature->getPositionTile();
    if(tile == nullptr)
        return false;

    Room* room = tile->getCoveringRoom();
    if(room == nullptr)
        return false;

    RoomType type = room->getType();
    return (type == RoomType::library) || (type == RoomType::workshop) || (type == RoomType::trainingHall) ||
        (type == RoomType::casino) || (type == RoomType::hatchery);
}

bool CreatureReactions::isInHand(const Creature* creature) const
{
    Player* localPlayer = mGameMap->getLocalPlayer();
    if(localPlayer == nullptr)
        return false;

    const std::vector<GameEntity*>& hand = localPlayer->getObjectsInHand();
    return std::find(hand.begin(), hand.end(), creature) != hand.end();
}

bool CreatureReactions::isVariantAllowed(const Creature* creature, const ReactionVariant& variant) const
{
    const CreatureDefinition* definition = creature->getDefinition();
    if(definition == nullptr)
        return false;

    const std::string& creatureName = definition->getClassName();
    if(!variant.mCreatures.empty() && !contains(variant.mCreatures, creatureName))
        return false;

    if(!variant.mJobs.empty() &&
       !contains(variant.mJobs, CreatureDefinition::creatureJobToString(definition->getCreatureJob())))
    {
        return false;
    }

    if(!variant.mGroups.empty())
    {
        bool inGroup = false;
        for(const std::string& group : mConfig.getGroupsOf(creatureName))
        {
            if(contains(variant.mGroups, group))
            {
                inGroup = true;
                break;
            }
        }
        if(!inGroup)
            return false;
    }

    if(variant.mRequiresSleepNeed && !creatureNeedsSleep(creature))
        return false;

    // Turning to a wall, a neighbour or a room only makes sense when there is one
    if((variant.mRequiresWall || variant.mRequiresNeighbour || !variant.mLookAtRoom.empty()))
    {
        Ogre::Vector3 point = Ogre::Vector3::ZERO;
        if(!findLookTarget(creature, variant, point))
            return false;
    }

    return true;
}

bool CreatureReactions::findWall(const Creature* creature, Ogre::Vector3& point) const
{
    Tile* tile = creature->getPositionTile();
    if(tile == nullptr)
        return false;

    static const int offsets[NB_WALL_SIDES][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    // Start at another side each time, so that the creature does not always pick the same wall
    uint32_t start = static_cast<uint32_t>(cosmeticRandom(0.0, WALL_SIDE_RANDOM_MAX));
    for(uint32_t i = 0; i < NB_WALL_SIDES; ++i)
    {
        uint32_t index = (start + i) % NB_WALL_SIDES;
        Tile* neighbour = mGameMap->getTile(tile->getX() + offsets[index][0], tile->getY() + offsets[index][1]);
        if((neighbour == nullptr) || (neighbour->getFullness() <= 0.0))
            continue;

        // The face of the wall is half a tile away from the middle of the tile the creature stands on
        point = Ogre::Vector3(static_cast<Ogre::Real>(tile->getX()) + WALL_FACE_OFFSET * static_cast<Ogre::Real>(offsets[index][0]),
            static_cast<Ogre::Real>(tile->getY()) + WALL_FACE_OFFSET * static_cast<Ogre::Real>(offsets[index][1]), 0.0f);
        return true;
    }

    return false;
}

bool CreatureReactions::findNeighbour(const Creature* creature, Ogre::Vector3& point) const
{
    double best = NEIGHBOUR_RADIUS;
    bool found = false;
    for(Creature* other : mGameMap->getCreatures())
    {
        if((other == creature) || !other->getIsOnMap() || !other->isAlive())
            continue;

        Ogre::Vector3 difference = other->getPosition() - creature->getPosition();
        difference.z = 0.0f;
        double distance = difference.length();
        if(distance >= best)
            continue;

        best = distance;
        point = other->getPosition();
        found = true;
    }

    return found;
}

bool CreatureReactions::findRoomTile(const Creature* creature, const std::string& roomName, Ogre::Vector3& point) const
{
    RoomType type = RoomManager::getRoomTypeFromRoomName(roomName);
    if(type == RoomType::nbRooms)
        return false;

    double best = 0.0;
    bool found = false;
    for(Room* room : mGameMap->getRoomsByTypeAndSeat(type, creature->getSeat()))
    {
        for(uint32_t i = 0; i < room->numCoveredTiles(); ++i)
        {
            Tile* tile = room->getCoveredTile(static_cast<int>(i));
            if(tile == nullptr)
                continue;

            Ogre::Vector3 tilePosition(static_cast<Ogre::Real>(tile->getX()), static_cast<Ogre::Real>(tile->getY()), 0.0f);
            Ogre::Vector3 difference = tilePosition - creature->getPosition();
            difference.z = 0.0f;
            double distance = difference.length();
            if(found && (distance >= best))
                continue;

            best = distance;
            point = tilePosition;
            found = true;
        }
    }

    return found;
}

bool CreatureReactions::findLookTarget(const Creature* creature, const ReactionVariant& variant,
        Ogre::Vector3& point) const
{
    if(variant.mRequiresWall)
        return findWall(creature, point);

    if(variant.mRequiresNeighbour)
        return findNeighbour(creature, point);

    if(!variant.mLookAtRoom.empty())
        return findRoomTile(creature, variant.mLookAtRoom, point);

    return false;
}

bool CreatureReactions::isStandingMotion(ReactionMotion::Type type)
{
    return (type == ReactionMotion::Type::spin) || (type == ReactionMotion::Type::turn) ||
        (type == ReactionMotion::Type::squash) || (type == ReactionMotion::Type::look) ||
        (type == ReactionMotion::Type::lookat) || (type == ReactionMotion::Type::sit) ||
        (type == ReactionMotion::Type::lie) ||
        (type == ReactionMotion::Type::startle);
}

bool CreatureReactions::isProudEvent(const std::string& eventName)
{
    return (eventName == "Victory") || (eventName == "VictoryFled") || (eventName == "GroupVictory") ||
        (eventName == "LevelUp") || (eventName == "TrainingDone");
}

const ReactionVariant* CreatureReactions::chooseVariant(const Creature* creature, const ReactionEvent& event,
        const std::string& variantName) const
{
    if(!variantName.empty())
    {
        for(const ReactionVariant& variant : event.mVariants)
        {
            if(variant.mName == variantName)
                return &variant;
        }
        return nullptr;
    }

    std::vector<const ReactionVariant*> allowed;
    double totalWeight = 0.0;
    for(const ReactionVariant& variant : event.mVariants)
    {
        if((variant.mWeight <= 0.0) || !isVariantAllowed(creature, variant))
            continue;

        allowed.push_back(&variant);
        totalWeight += variant.mWeight;
    }

    if(allowed.empty())
        return nullptr;

    double roll = cosmeticRandom(0.0, totalWeight);
    for(const ReactionVariant* variant : allowed)
    {
        if(roll < variant->mWeight)
            return variant;

        roll -= variant->mWeight;
    }

    return allowed.back();
}

CreatureReactions::RunningReaction* CreatureReactions::findRunning(const std::string& creatureName)
{
    for(RunningReaction& reaction : mRunning)
    {
        if(reaction.mCreatureName == creatureName)
            return &reaction;
    }
    return nullptr;
}

void CreatureReactions::eraseRunning(const std::string& creatureName)
{
    for(std::vector<RunningReaction>::iterator it = mRunning.begin(); it != mRunning.end(); ++it)
    {
        if(it->mCreatureName == creatureName)
        {
            mRunning.erase(it);
            return;
        }
    }
}

bool CreatureReactions::trigger(Creature* creature, const std::string& eventName, bool forced,
        const std::string& variantName)
{
    if((mMode == Mode::off) || !mConfigLoaded || (creature == nullptr))
        return false;

    const ReactionEvent* event = mConfig.getEvent(eventName);
    if(event == nullptr)
    {
        logMissingOnce("reaction event", eventName);
        return false;
    }

    // Only the events of the dying creature are shown on a creature that is no longer alive
    if(!creature->isAlive() && !event->mDying)
        return false;

    // A creature in the hand is not on the map: only the events made for the hand are shown on it
    bool inHand = isInHand(creature);
    if(event->mInHand)
    {
        if(!inHand && !(forced && creature->getIsOnMap()))
            return false;
    }
    else if(!creature->getIsOnMap() && !event->mDying)
    {
        return false;
    }

    if(!forced)
    {
        // The hand is always on the screen
        if(!inHand && !isCreatureNearCamera(creature))
            return false;

        std::map<std::string, double>::const_iterator itCooldown =
            mCooldownEnd.find(creature->getName() + "|" + eventName);
        if((itCooldown != mCooldownEnd.end()) && (itCooldown->second > mTime))
            return false;

        uint32_t reserved = (event->mPriority >= ReactionPriority::mood) ? MOOD_RESERVED_SLOTS : 0;
        if(((mRunning.size() + reserved) >= mConfig.getMaxSimultaneous()) && (findRunning(creature->getName()) == nullptr))
            return false;

        if(!(event->mPriority < getCreaturePriority(creature, event)))
            return false;

        if(cosmeticRandom(0.0, 1.0) >= event->mProbability)
            return false;
    }

    const ReactionVariant* variant = chooseVariant(creature, *event, variantName);
    if(variant == nullptr)
        return false;

    if(!forced && (variant->mProbability >= 0.0) && (cosmeticRandom(0.0, 1.0) >= variant->mProbability))
        return false;

    return startReaction(creature, *event, *variant, forced);
}

void CreatureReactions::triggerGroup(const std::string& eventName, const std::vector<Creature*>& creatures,
        bool forced, double initialDelay)
{
    if((mMode == Mode::off) || !mConfigLoaded)
        return;

    const ReactionEvent* event = mConfig.getEvent(eventName);
    if(event == nullptr)
    {
        logMissingOnce("reaction event", eventName);
        return;
    }

    // Only creatures that can be seen take part: the others would not show anything anyway
    std::vector<Creature*> candidates;
    for(Creature* creature : creatures)
    {
        if(creature == nullptr)
            continue;

        if(!forced && !isCreatureNearCamera(creature))
            continue;

        candidates.push_back(creature);
    }

    std::shuffle(candidates.begin(), candidates.end(), cosmeticRng());

    // One starts, the others join in after a short, different delay
    double delay = initialDelay;
    uint32_t nbReacting = 0;
    for(Creature* creature : candidates)
    {
        if(nbReacting >= event->mGroupMax)
            break;

        PendingReaction pending;
        pending.mCreatureName = creature->getName();
        pending.mEventName = eventName;
        pending.mDelay = delay;
        pending.mWaited = 0.0;
        pending.mWaitMax = PENDING_WAIT_MAX;
        pending.mForced = forced;
        mPending.push_back(pending);

        delay += cosmeticRandom(mConfig.getGroupStaggerMin(), mConfig.getGroupStaggerMax());
        ++nbReacting;
    }
}

bool CreatureReactions::startReaction(Creature* creature, const ReactionEvent& event,
        const ReactionVariant& variant, bool forced)
{
    RunningReaction* running = findRunning(creature->getName());
    if(running != nullptr)
    {
        // A running reaction is only replaced by a more important one
        if(!forced && !(event.mPriority < running->mPriority))
            return false;

        endReaction(*running, creature);
        eraseRunning(creature->getName());
    }

    RunningReaction reaction;
    reaction.mCreatureName = creature->getName();
    reaction.mEventName = event.mName;
    reaction.mPriority = event.mPriority;
    reaction.mWhileWorking = event.mWhileWorking;
    reaction.mInHand = isInHand(creature);
    reaction.mDying = event.mDying;

    // The place the creature turns its head to: the place of an event, or the wall, neighbour or room of the variant
    if(mHasNextLookTarget)
    {
        reaction.mLookTarget = mNextLookTarget;
        reaction.mHasLookTarget = true;
    }
    else
    {
        reaction.mHasLookTarget = findLookTarget(creature, variant, reaction.mLookTarget);
    }

    bool shown = false;

    // Tier A: emote above the head. It is the only thing shown in the reduced mode.
    CreatureOverlayStatus* overlay = creature->getOverlayStatus();
    if(!variant.mEmote.empty())
    {
        std::string material = EMOTE_MATERIAL_PREFIX + variant.mEmote;
        if(!Ogre::MaterialManager::getSingleton().resourceExists(material, "Graphics"))
        {
            logMissingOnce("emote", variant.mEmote);
        }
        else if(overlay != nullptr)
        {
            overlay->showEmote(material, static_cast<Ogre::Real>(variant.mEmoteTime));
            reaction.mEmoteShown = true;
            reaction.mDuration = std::max(reaction.mDuration, variant.mEmoteTime);
            shown = true;
        }
    }

    // A second icon some time later (nodding off, then startled)
    if(!variant.mLateEmote.empty())
    {
        reaction.mLateEmote = variant.mLateEmote;
        reaction.mLateEmoteDelay = variant.mLateEmoteDelay;
        reaction.mLateEmoteTime = variant.mLateEmoteTime;
        reaction.mDuration = std::max(reaction.mDuration, variant.mLateEmoteDelay + variant.mLateEmoteTime);
        shown = true;
    }

    if(mMode == Mode::full)
    {
        // Tier C / B: a clip, only while the creature stands still (it must not slide while posing)
        if(!creature->isMoving() && !event.mWhileWorking && !event.mDying && startClip(reaction, creature, variant))
            shown = true;

        for(const ReactionEffect& effect : variant.mEffects)
        {
            if(addParticles(reaction, creature, effect.mName))
            {
                reaction.mDuration = std::max(reaction.mDuration, effect.mTime);
                shown = true;
            }
        }

        if(!variant.mLateEffect.empty())
        {
            reaction.mLateEffect = variant.mLateEffect;
            reaction.mLateEffectDelay = variant.mLateEffectDelay;
            reaction.mLateEffectTime = variant.mLateEffectTime;
            reaction.mDuration = std::max(reaction.mDuration, variant.mLateEffectDelay + variant.mLateEffectTime);
            shown = true;
        }

        Ogre::SceneNode* node = creature->getEntityNode();
        // Turning and squashing look wrong on a creature that walks
        bool standingMotion = isStandingMotion(variant.mMotion.mType);
        if((variant.mMotion.mType != ReactionMotion::Type::none) && (variant.mMotion.mDuration > 0.0) &&
           (node != nullptr) && !(standingMotion && creature->isMoving()))
        {
            reaction.mMotion = variant.mMotion;
            reaction.mMotionLastPosition = node->getPosition();
            reaction.mMotionLastScale = node->getScale();
            reaction.mDuration = std::max(reaction.mDuration, variant.mMotion.mDuration);
            shown = true;

            // The newer motions end with the creature standing still
            reaction.mEndsWhenMoving = (variant.mMotion.mType == ReactionMotion::Type::look) ||
                (variant.mMotion.mType == ReactionMotion::Type::lookat) ||
                (variant.mMotion.mType == ReactionMotion::Type::sit) ||
                (variant.mMotion.mType == ReactionMotion::Type::lie) ||
                (variant.mMotion.mType == ReactionMotion::Type::startle);
        }

        // A sprite prop, only while the creature stands still
        if((variant.mProp.mPath != ReactionProp::Path::none) && !creature->isMoving() &&
           createProps(reaction, creature, variant))
        {
            reaction.mDuration = std::max(reaction.mDuration, variant.mProp.mSeconds);
            reaction.mEndsWhenMoving = true;
            shown = true;
        }
    }

    if(!shown)
        return false;

    double cooldown = (variant.mCooldown >= 0.0) ? variant.mCooldown : event.mCooldown;
    mCooldownEnd[creature->getName() + "|" + event.mName] = mTime + cooldown;

    mRunning.push_back(reaction);

    // A creature that won or reached a new level is proud for a while
    if(isProudEvent(event.mName))
        mProudUntil[creature->getName()] = mTime + mConfig.getProudSeconds();

    if(!variant.mSpreads.empty())
        spreadTo(creature, variant.mSpreads);

    return true;
}

bool CreatureReactions::startClip(RunningReaction& reaction, Creature* creature, const ReactionVariant& variant)
{
    Ogre::Entity* entity = getCreatureEntity(creature);
    if((entity == nullptr) || !entity->hasSkeleton())
        return false;

    Ogre::AnimationState* base = creature->getAnimationState();
    if(base == nullptr)
        return false;

    Ogre::SkeletonInstance* skeleton = entity->getSkeleton();
    std::string clip;
    double speed = 1.0;
    double start = 0.0;
    double end = 1.0;
    if(!variant.mClip.empty() && skeleton->hasAnimation(variant.mClip))
    {
        // Tier C
        clip = variant.mClip;
        speed = variant.mClipSpeed;
    }
    else
    {
        if(!variant.mClip.empty())
            logMissingOnce("clip", variant.mClip);

        if(variant.mFallbackClip.empty())
            return false;

        if(!skeleton->hasAnimation(variant.mFallbackClip))
        {
            logMissingOnce("clip", variant.mFallbackClip);
            return false;
        }

        // Tier B
        clip = variant.mFallbackClip;
        speed = variant.mFallbackSpeed;
        start = std::max(0.0, std::min(1.0, variant.mFallbackStart));
        end = std::max(start, std::min(1.0, variant.mFallbackEnd));
    }

    // The creature plays this clip already, there is nothing to put on top
    if((clip == base->getAnimationName()) || (speed <= 0.0) || !entity->hasAnimationState(clip))
        return false;

    Ogre::AnimationState* animState = entity->getAnimationState(clip);
    animState->setLoop(false);
    animState->setTimePosition(static_cast<Ogre::Real>(start * animState->getLength()));
    animState->setWeight(1.0f);
    animState->setEnabled(true);

    // The clip of the creature goes on running (it is advanced by the entity as before) but is
    // not shown, so that both clips are not blended into a mess
    base->setWeight(0.0f);

    reaction.mClip = clip;
    reaction.mClipSpeed = speed;
    reaction.mClipEnd = end;
    reaction.mBaseClip = base->getAnimationName();

    double clipSeconds = ((end - start) * static_cast<double>(animState->getLength())) /
        (ODApplication::turnsPerSecond * speed);
    reaction.mDuration = std::max(reaction.mDuration, clipSeconds);
    return true;
}

void CreatureReactions::stopClip(RunningReaction& reaction, Creature* creature)
{
    if(reaction.mClip.empty())
        return;

    Ogre::Entity* entity = (creature != nullptr) ? getCreatureEntity(creature) : nullptr;
    if(entity != nullptr)
    {
        if(entity->hasAnimationState(reaction.mClip))
        {
            Ogre::AnimationState* animState = entity->getAnimationState(reaction.mClip);
            animState->setEnabled(false);
            animState->setWeight(1.0f);
        }

        Ogre::AnimationState* base = creature->getAnimationState();
        if(base != nullptr)
            base->setWeight(1.0f);
    }

    reaction.mClip.clear();
}

bool CreatureReactions::addParticles(RunningReaction& reaction, Creature* creature, const std::string& effect)
{
    if(Ogre::ParticleSystemManager::getSingleton().getTemplate(effect) == nullptr)
    {
        logMissingOnce("particle effect", effect);
        return false;
    }

    if(creature->getEntityNode() == nullptr)
        return false;

    std::string particleName = PARTICLE_NAME_PREFIX + Helper::toString(mNextParticleId);
    ++mNextParticleId;
    Ogre::ParticleSystem* particleSystem = RenderManager::getSingleton().rrEntityAddParticleEffect(creature,
        particleName, effect);
    if(particleSystem == nullptr)
        return false;

    reaction.mParticleSystems.push_back(particleName);
    return true;
}

void CreatureReactions::removeParticles(RunningReaction& reaction)
{
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    for(const std::string& particleName : reaction.mParticleSystems)
    {
        if(!sceneManager->hasParticleSystem(particleName))
            continue;

        Ogre::ParticleSystem* particleSystem = sceneManager->getParticleSystem(particleName);
        // The node is already gone if the creature was removed
        if(particleSystem->getParentSceneNode() != nullptr)
            particleSystem->getParentSceneNode()->detachObject(particleSystem);

        sceneManager->destroyParticleSystem(particleSystem);
    }
    reaction.mParticleSystems.clear();
}

void CreatureReactions::applyMotion(RunningReaction& reaction, Creature* creature)
{
    const ReactionMotion& motion = reaction.mMotion;
    if(motion.mType == ReactionMotion::Type::none)
        return;

    Ogre::SceneNode* node = creature->getEntityNode();
    if(node == nullptr)
        return;

    bool finished = reaction.mElapsed >= motion.mDuration;
    double progress = finished ? 1.0 : (reaction.mElapsed / motion.mDuration);

    // Whoever moves or scales the creature sets the position or scale, so what we added is lost
    // then. We only take our own share away if the node still is the way we left it.
    Ogre::Vector3 position = node->getPosition();
    if((position - reaction.mMotionLastPosition).squaredLength() < MOTION_UNCHANGED_SQUARED_TOLERANCE)
        position -= reaction.mMotionPosition;

    Ogre::Vector3 scale = node->getScale();
    if((scale - reaction.mMotionLastScale).squaredLength() < MOTION_UNCHANGED_SQUARED_TOLERANCE)
        scale = scale / reaction.mMotionScale;

    Ogre::Vector3 addedPosition = Ogre::Vector3::ZERO;
    Ogre::Vector3 addedScale = Ogre::Vector3::UNIT_SCALE;
    double angle = 0.0;
    if(!finished)
    {
        // The humps of the motion: 0 at the start and the end of each one, 1 in the middle
        double wave = std::fabs(std::sin(PI_VALUE * motion.mCount * progress));
        switch(motion.mType)
        {
            case ReactionMotion::Type::hop:
                addedPosition.z = static_cast<Ogre::Real>(motion.mAmount * wave);
                break;
            case ReactionMotion::Type::shake:
            {
                // Sideways to where the creature looks, calming down towards the end
                double swing = std::sin(2.0 * PI_VALUE * motion.mCount * progress) * (1.0 - progress);
                addedPosition = (node->getOrientation() * Ogre::Vector3::UNIT_X) *
                    static_cast<Ogre::Real>(motion.mAmount * swing);
                addedPosition.z = 0.0f;
                break;
            }
            case ReactionMotion::Type::squash:
            {
                double share = motion.mAmount * wave;
                addedScale.x = static_cast<Ogre::Real>(1.0 + SQUASH_SIDE_GROWTH * share);
                addedScale.y = static_cast<Ogre::Real>(1.0 + SQUASH_SIDE_GROWTH * share);
                addedScale.z = static_cast<Ogre::Real>(1.0 - share);
                break;
            }
            case ReactionMotion::Type::spin:
            {
                // Slow start and end
                double eased = progress * progress * (3.0 - 2.0 * progress);
                angle = 2.0 * PI_VALUE * motion.mCount * eased;
                break;
            }
            case ReactionMotion::Type::look:
            {
                // Looks left and right a few times, calming down at both ends
                double swing = std::sin(2.0 * PI_VALUE * motion.mCount * progress) * std::sin(PI_VALUE * progress);
                angle = motion.mAmount * PI_VALUE / 180.0 * swing;
                break;
            }
            case ReactionMotion::Type::sit:
            {
                double share = motion.mAmount * plateau(progress, SIT_RAMP);
                addedScale.x = static_cast<Ogre::Real>(1.0 + SIT_SIDE_GROWTH * share);
                addedScale.y = static_cast<Ogre::Real>(1.0 + SIT_SIDE_GROWTH * share);
                addedScale.z = static_cast<Ogre::Real>(1.0 - share);
                break;
            }
            case ReactionMotion::Type::lie:
            {
                if(!reaction.mMotionAxisComputed)
                {
                    reaction.mMotionAxisComputed = true;
                    Ogre::Vector3 forward = node->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Y;
                    forward.z = 0.0f;
                    if(forward.length() > DIRECTION_MIN_LENGTH)
                        reaction.mMotionAxis = forward.normalisedCopy();
                }

                angle = motion.mAmount * PI_VALUE / 180.0 * plateau(progress, LIE_RAMP);
                break;
            }
            case ReactionMotion::Type::startle:
            {
                // Nods off a few times, then jumps up
                if(progress < STARTLE_NOD_SHARE)
                {
                    double share = motion.mAmount * std::fabs(std::sin(PI_VALUE * motion.mCount * progress / STARTLE_NOD_SHARE));
                    addedScale.x = static_cast<Ogre::Real>(1.0 + STARTLE_SIDE_GROWTH * share);
                    addedScale.y = static_cast<Ogre::Real>(1.0 + STARTLE_SIDE_GROWTH * share);
                    addedScale.z = static_cast<Ogre::Real>(1.0 - share);
                }
                else
                {
                    double jump = (progress - STARTLE_NOD_SHARE) / (1.0 - STARTLE_NOD_SHARE);
                    addedPosition.z = static_cast<Ogre::Real>(STARTLE_JUMP_HEIGHT * std::sin(PI_VALUE * jump));
                }
                break;
            }
            case ReactionMotion::Type::turn:
            case ReactionMotion::Type::lookat:
            {
                if(!reaction.mMotionTurnComputed)
                {
                    reaction.mMotionTurnComputed = true;
                    reaction.mMotionTurnAngle = 0.0;

                    // The camera for 'turn', the point of the reaction for 'lookat'
                    bool hasTarget = false;
                    Ogre::Vector3 toTarget = Ogre::Vector3::ZERO;
                    if(motion.mType == ReactionMotion::Type::lookat)
                    {
                        if(reaction.mHasLookTarget)
                        {
                            toTarget = reaction.mLookTarget - node->getPosition();
                            hasTarget = true;
                        }
                    }
                    else
                    {
                        ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
                        Ogre::Camera* camera = (frameListener != nullptr) ?
                            frameListener->getCameraManager()->getActiveCamera() : nullptr;
                        if(camera != nullptr)
                        {
                            toTarget = camera->getDerivedPosition() - node->getPosition();
                            hasTarget = true;
                        }
                    }

                    if(hasTarget)
                    {
                        Ogre::Vector3 toCamera = toTarget;
                        Ogre::Vector3 forward = node->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Y;
                        toCamera.z = 0.0f;
                        forward.z = 0.0f;
                        if((toCamera.length() > TURN_MIN_LENGTH) && (forward.length() > TURN_MIN_LENGTH))
                        {
                            toCamera.normalise();
                            forward.normalise();
                            reaction.mMotionTurnAngle = std::atan2(forward.crossProduct(toCamera).z,
                                forward.dotProduct(toCamera));
                        }
                    }
                }

                // Turns in the first quarter, stays, and turns back in the last quarter
                double envelope = 1.0;
                if(progress < TURN_PHASE)
                    envelope = progress / TURN_PHASE;
                else if(progress > (1.0 - TURN_PHASE))
                    envelope = (1.0 - progress) / TURN_PHASE;
                envelope = envelope * envelope * (3.0 - 2.0 * envelope);
                angle = reaction.mMotionTurnAngle * envelope;
                break;
            }
            default:
                break;
        }
    }

    node->setPosition(position + addedPosition);
    node->setScale(scale * addedScale);
    reaction.mMotionPosition = addedPosition;
    reaction.mMotionScale = addedScale;
    reaction.mMotionLastPosition = node->getPosition();
    reaction.mMotionLastScale = node->getScale();

    double deltaAngle = angle - reaction.mMotionAngle;
    if(std::fabs(deltaAngle) > ROTATION_EPSILON)
    {
        // The creature tips over onto its side around the axis it looks along, everything else turns around the vertical
        Ogre::Vector3 axis = (motion.mType == ReactionMotion::Type::lie) ? reaction.mMotionAxis : Ogre::Vector3::UNIT_Z;
        node->rotate(Ogre::Quaternion(Ogre::Radian(static_cast<Ogre::Real>(deltaAngle)), axis),
            Ogre::Node::TS_PARENT);
    }

    reaction.mMotionAngle = angle;
}

void CreatureReactions::clearMotion(RunningReaction& reaction, Creature* creature)
{
    if((reaction.mMotion.mType == ReactionMotion::Type::none) || (creature == nullptr))
        return;

    // Past the end of the motion, so that everything it added is taken away
    reaction.mElapsed = reaction.mMotion.mDuration;
    applyMotion(reaction, creature);

    reaction.mMotion = ReactionMotion();
}

bool CreatureReactions::updateReaction(RunningReaction& reaction, Creature* creature,
        Ogre::Real timeSinceLastFrame)
{
    reaction.mElapsed += timeSinceLastFrame;
    if(reaction.mElapsed >= reaction.mDuration)
        return false;

    // Something more important than the reaction now happens to the creature
    const ReactionEvent* runningEvent = (reaction.mWhileWorking || reaction.mDying) ?
        mConfig.getEvent(reaction.mEventName) : nullptr;
    if(!(reaction.mPriority < getCreaturePriority(creature, runningEvent)))
        return false;

    // Poses and props need a creature that stands: when it sets off the reaction is over
    if(reaction.mEndsWhenMoving && creature->isMoving())
        return false;

    updateLate(reaction, creature);

    if(!reaction.mClip.empty())
    {
        Ogre::Entity* entity = getCreatureEntity(creature);
        Ogre::AnimationState* base = creature->getAnimationState();
        bool stop = (entity == nullptr) || (base == nullptr) || creature->isMoving() ||
            (base->getAnimationName() != reaction.mBaseClip) || !entity->hasAnimationState(reaction.mClip);
        if(!stop)
        {
            Ogre::AnimationState* animState = entity->getAnimationState(reaction.mClip);
            // The clip is stopped when the entity switched to another one
            stop = !animState->getEnabled();
            if(!stop)
            {
                double step = ODApplication::turnsPerSecond * static_cast<double>(timeSinceLastFrame) *
                    creature->getAnimationSpeedFactor() * reaction.mClipSpeed;
                animState->addTime(static_cast<Ogre::Real>(step));
                stop = animState->hasEnded() ||
                    (animState->getTimePosition() >= static_cast<Ogre::Real>(reaction.mClipEnd) * animState->getLength());
            }
        }
        if(stop)
            stopClip(reaction, creature);
    }

    // Turning and squashing stop when the creature sets off
    if(creature->isMoving() && ((reaction.mMotion.mType == ReactionMotion::Type::spin) ||
       (reaction.mMotion.mType == ReactionMotion::Type::turn) ||
       (reaction.mMotion.mType == ReactionMotion::Type::squash)))
    {
        clearMotion(reaction, creature);
    }

    applyMotion(reaction, creature);
    updateProps(reaction, creature);
    return true;
}

void CreatureReactions::updateLate(RunningReaction& reaction, Creature* creature)
{
    if(!reaction.mLateEmote.empty() && (reaction.mElapsed >= reaction.mLateEmoteDelay))
    {
        std::string material = EMOTE_MATERIAL_PREFIX + reaction.mLateEmote;
        CreatureOverlayStatus* overlay = creature->getOverlayStatus();
        if(!Ogre::MaterialManager::getSingleton().resourceExists(material, "Graphics"))
        {
            logMissingOnce("emote", reaction.mLateEmote);
        }
        else if(overlay != nullptr)
        {
            overlay->showEmote(material, static_cast<Ogre::Real>(reaction.mLateEmoteTime));
            reaction.mEmoteShown = true;
        }
        reaction.mLateEmote.clear();
    }

    if(!reaction.mLateEffect.empty() && (reaction.mElapsed >= reaction.mLateEffectDelay))
    {
        std::string effect = reaction.mLateEffect;
        reaction.mLateEffect.clear();
        addParticles(reaction, creature, effect);
    }
}

void CreatureReactions::endReaction(RunningReaction& reaction, Creature* creature)
{
    stopClip(reaction, creature);
    clearMotion(reaction, creature);
    removeParticles(reaction);
    removeProps(reaction);

    if(reaction.mEmoteShown && (creature != nullptr) && (creature->getOverlayStatus() != nullptr))
        creature->getOverlayStatus()->hideEmote();

    reaction.mEmoteShown = false;
}

void CreatureReactions::update(Ogre::Real timeSinceLastFrame)
{
    if(timeSinceLastFrame <= 0.0)
        return;

    mTime += timeSinceLastFrame;

    updateOngoing();
    updateMoods(timeSinceLastFrame);

    for(std::vector<PendingReaction>::iterator it = mPending.begin(); it != mPending.end();)
    {
        it->mDelay -= timeSinceLastFrame;
        if(it->mDelay > 0.0)
        {
            ++it;
            continue;
        }

        PendingReaction pending = *it;
        Creature* creature = mGameMap->getCreature(pending.mCreatureName);
        const ReactionEvent* event = mConfig.getEvent(pending.mEventName);
        if((creature != nullptr) && (event != nullptr) && !pending.mForced &&
           !(event->mPriority < getCreaturePriority(creature, event)) && (pending.mWaited < pending.mWaitMax))
        {
            // Still busy (for example finishing the last blow): look again in a moment
            it->mDelay = PENDING_WAIT_STEP;
            it->mWaited += PENDING_WAIT_STEP;
            ++it;
            continue;
        }

        it = mPending.erase(it);
        if(creature != nullptr)
            trigger(creature, pending.mEventName, pending.mForced);
    }

    for(std::vector<RunningReaction>::iterator it = mRunning.begin(); it != mRunning.end();)
    {
        Creature* creature = mGameMap->getCreature(it->mCreatureName);
        bool stillRunning = false;
        if(creature != nullptr)
        {
            if(it->mDying)
            {
                // The creature is no longer alive, its reaction goes on as long as it can be seen
                stillRunning = (creature->getEntityNode() != nullptr) && updateReaction(*it, creature, timeSinceLastFrame);
            }
            else
            {
                stillRunning = (it->mInHand ? isInHand(creature) : creature->getIsOnMap()) && creature->isAlive() &&
                    updateReaction(*it, creature, timeSinceLastFrame);
            }
        }
        if(stillRunning)
        {
            ++it;
            continue;
        }

        endReaction(*it, creature);
        it = mRunning.erase(it);
    }

    if((mTime - mTimeLastPrune) > PRUNE_INTERVAL)
        pruneCooldowns();
}

void CreatureReactions::pruneCooldowns()
{
    mTimeLastPrune = mTime;
    for(std::map<std::string, double>::iterator it = mCooldownEnd.begin(); it != mCooldownEnd.end();)
    {
        if(it->second <= mTime)
            mCooldownEnd.erase(it++);
        else
            ++it;
    }

    for(std::map<std::string, double>::iterator it = mLastAttack.begin(); it != mLastAttack.end();)
    {
        if((mTime - it->second) > ATTACK_MEMORY)
            mLastAttack.erase(it++);
        else
            ++it;
    }

    for(std::map<std::string, double>::iterator it = mLastCelebration.begin(); it != mLastCelebration.end();)
    {
        if((mTime - it->second) > CELEBRATION_PAUSE)
            mLastCelebration.erase(it++);
        else
            ++it;
    }

    for(std::map<std::string, RoomWork>::iterator it = mLastRoomWork.begin(); it != mLastRoomWork.end();)
    {
        if((mTime - it->second.mTime) > ROOM_WORK_MEMORY)
            mLastRoomWork.erase(it++);
        else
            ++it;
    }

    for(std::map<std::string, double>::iterator it = mProudUntil.begin(); it != mProudUntil.end();)
    {
        if(it->second <= mTime)
            mProudUntil.erase(it++);
        else
            ++it;
    }

    for(std::map<std::string, double>::iterator it = mNextLook.begin(); it != mNextLook.end();)
    {
        if(it->second <= mTime)
            mNextLook.erase(it++);
        else
            ++it;
    }

    // A creature that is gone is no longer seen standing idle
    for(std::map<std::string, double>::iterator it = mIdleSince.begin(); it != mIdleSince.end();)
    {
        if(mGameMap->getCreature(it->first) == nullptr)
            mIdleSince.erase(it++);
        else
            ++it;
    }

    for(std::map<std::string, Delivery>::iterator it = mDeliveries.begin(); it != mDeliveries.end();)
    {
        if((mTime - it->second.mSince) > DELIVERY_WINDOW)
            mDeliveries.erase(it++);
        else
            ++it;
    }

    for(std::map<std::string, double>::iterator it = mSlappedAt.begin(); it != mSlappedAt.end();)
    {
        if((mTime - it->second) > SLAP_DUCK_MEMORY)
            mSlappedAt.erase(it++);
        else
            ++it;
    }

    for(std::map<std::string, HandDrop>::iterator it = mHandDrops.begin(); it != mHandDrops.end();)
    {
        if((mTime - it->second.mTime) > HAND_DROP_MEMORY)
            mHandDrops.erase(it++);
        else
            ++it;
    }
}

void CreatureReactions::noteAnimation(MovableGameEntity* entity, const std::string& clip)
{
    if((mMode == Mode::off) || !mConfigLoaded || (entity->getObjectType() != GameEntityType::creature))
        return;

    Creature* creature = static_cast<Creature*>(entity);

    // A creature that does anything but stand is no longer idle
    if(!mIdleSince.empty() && (clip != "Idle"))
        mIdleSince.erase(creature->getName());

    // Something that goes on for a while is shown now and then, until the creature does something else
    std::string ongoingEvent = getOngoingEvent(creature, clip);
    finishOngoing(creature, ongoingEvent);
    if(!ongoingEvent.empty())
        startOngoing(creature, ongoingEvent);

    if((clip == "Die") || (clip == "die"))
    {
        celebrateVictory(creature, false);
        noteAllyDied(creature);

        // Small touches on the death animation, nothing that changes how long it takes
        trigger(creature, "Death");
    }
    else if(clip == "Flee")
    {
        celebrateVictory(creature, true);

        // A prisoner that struggles is not fleeing
        if(!creature->isInContainment())
            queueReaction(creature, "FleePanic", -1.0, FLEE_PANIC_DELAY);
    }
    else if(clip == "Dig")
    {
        noteDigging(creature);
    }
    else if((clip == "EatChicken") && (getRoomName(creature) == "Hatchery"))
    {
        // The meal in the hatchery is over when the animation is: then the creature shows how it liked it
        queueReaction(creature, "HatcheryMealDone", DONE_WAIT_MAX, HATCHERY_MEAL_DONE_DELAY);
    }
    else if(startsWith(clip, "Attack") || (clip == "CombatAttack") || (clip == "RangedAttack") ||
            startsWith(clip, "Cast"))
    {
        // The work in some rooms is shown with the attack animation too, that is no fight
        if(!isWorkingInRoom(creature))
        {
            mLastAttack[creature->getName()] = mTime;

            // The creatures that stand around look at the fight
            noteNearbyEvent("AmbientLookFight", creature->getPosition(), creature, FIGHT_LOOK_INTERVAL);
        }
        else
        {
            noteRoomWork(creature);
        }
    }
}

//! Seconds before the winners start to cheer when the loser ran away
static const double VICTORY_FLED_DELAY = 0.6;
//! Seconds before the winners start to cheer when the loser went down
static const double VICTORY_DELAY = 0.8;
//! Seconds before the other creatures nearby join in the cheering
static const double GROUP_VICTORY_DELAY = 1.8;
//! Seconds before the spectators of an arena bout cheer after it is over
static const double ARENA_BOUT_OVER_DELAY = 1.6;

void CreatureReactions::noteRoomWork(Creature* creature)
{
    Tile* tile = creature->getPositionTile();
    Room* room = (tile != nullptr) ? tile->getCoveringRoom() : nullptr;
    if(room == nullptr)
        return;

    std::string eventName;
    if(room->getType() == RoomType::library)
        eventName = "LibraryWork";
    else if(room->getType() == RoomType::workshop)
        eventName = "WorkshopWork";
    else if(room->getType() == RoomType::trainingHall)
        eventName = "TrainingWork";
    else if(room->getType() == RoomType::hatchery)
        eventName = "HatcheryWork";
    else
        return;

    RoomWork work;
    work.mRoomType = room->getType();
    work.mRoomName = room->getName();
    work.mTime = mTime;
    mLastRoomWork[creature->getName()] = work;

    queueReaction(creature, eventName);
}

void CreatureReactions::queueReaction(Creature* creature, const std::string& eventName, double waitMax,
        double delay)
{
    const ReactionEvent* event = mConfig.getEvent(eventName);
    if(event == nullptr)
    {
        logMissingOnce("reaction event", eventName);
        return;
    }

    std::map<std::string, double>::const_iterator itCooldown =
        mCooldownEnd.find(creature->getName() + "|" + eventName);
    if((itCooldown != mCooldownEnd.end()) && (itCooldown->second > mTime))
        return;

    if(findRunning(creature->getName()) != nullptr)
        return;

    // The creature is asked once at a time, the dice are thrown when it is its turn
    for(const PendingReaction& pending : mPending)
    {
        if(pending.mCreatureName == creature->getName())
            return;
    }

    if(!isInHand(creature) && !isCreatureNearCamera(creature))
        return;

    PendingReaction pending;
    pending.mCreatureName = creature->getName();
    pending.mEventName = eventName;
    pending.mDelay = (delay >= 0.0) ? delay : PENDING_WAIT_STEP;
    pending.mWaited = 0.0;
    pending.mWaitMax = (waitMax > 0.0) ? waitMax : PENDING_WAIT_MAX;
    pending.mForced = false;
    mPending.push_back(pending);
}

void CreatureReactions::noteEntityAdded(GameEntity* entity)
{
    if((mMode == Mode::off) || !mConfigLoaded)
        return;

    if(entity->getObjectType() == GameEntityType::creature)
    {
        noteCreatureAdded(static_cast<Creature*>(entity));
        return;
    }

    RoomType roomType = RoomType::nbRooms;
    std::string resultEvent;
    std::string othersEvent;
    switch(entity->getObjectType())
    {
        case GameEntityType::skillEntity:
            roomType = RoomType::library;
            resultEvent = "ResearchDone";
            othersEvent = "ResearchLookUp";
            break;
        case GameEntityType::craftedTrap:
            roomType = RoomType::workshop;
            resultEvent = "ItemCrafted";
            othersEvent = "ItemCraftedApplause";
            break;
        default:
            return;
    }

    Tile* tile = mGameMap->getTile(Helper::round(entity->getPosition().x), Helper::round(entity->getPosition().y));
    Room* room = (tile != nullptr) ? tile->getCoveringRoom() : nullptr;
    if((room == nullptr) || (room->getType() != roomType))
        return;

    // The creature that worked last in this room finished the result. Without such a creature
    // (the entity was there before, for example in a loaded game) nothing is shown.
    Creature* finisher = nullptr;
    double newest = -1.0;
    for(std::map<std::string, RoomWork>::const_iterator it = mLastRoomWork.begin(); it != mLastRoomWork.end(); ++it)
    {
        if((it->second.mRoomName != room->getName()) || ((mTime - it->second.mTime) > ROOM_WORK_MEMORY) ||
           (it->second.mTime <= newest))
        {
            continue;
        }

        Creature* creature = mGameMap->getCreature(it->first);
        if((creature == nullptr) || !creature->getIsOnMap() || !creature->isAlive())
            continue;

        finisher = creature;
        newest = it->second.mTime;
    }

    if(finisher == nullptr)
        return;

    trigger(finisher, resultEvent);

    // The others in the room notice it one after the other
    std::vector<Creature*> others;
    for(Creature* creature : mGameMap->getCreatures())
    {
        if((creature == finisher) || !creature->getIsOnMap() || !creature->isAlive())
            continue;

        Tile* creatureTile = creature->getPositionTile();
        if((creatureTile == nullptr) || (creatureTile->getCoveringRoom() != room))
            continue;

        others.push_back(creature);
    }

    if(!others.empty())
        triggerGroup(othersEvent, others, false, ROOM_RESULT_DELAY);
}

std::string CreatureReactions::getRoomName(const Creature* creature) const
{
    Tile* tile = creature->getPositionTile();
    Room* room = (tile != nullptr) ? tile->getCoveringRoom() : nullptr;
    if(room == nullptr)
        return std::string();

    return RoomManager::getRoomNameFromRoomType(room->getType());
}

std::string CreatureReactions::getOngoingEvent(const Creature* creature, const std::string& clip) const
{
    // Digging and claiming are one animation for as long as the creature does it
    if(clip == "Dig")
        return "DigWork";

    if(clip == "Claim")
        return "ClaimWork";

    std::string roomName = getRoomName(creature);

    // Creatures that wait in the arena while others fight watch the bouts
    if((clip == "Idle") && (roomName == "Arena"))
        return "ArenaWork";

    // Sleeping in a bed and praying (the prayer is the idle animation in the temple)
    if(startsWith(clip, "Sleep") && (roomName == "Dormitory"))
        return "DormitoryWork";

    if((clip == "Idle") && (roomName == "Temple"))
        return "TempleWork";

    // Prisoners wait in their cell, and struggle or glare while they are tortured
    if(creature->isInContainment())
    {
        if((clip == "Idle") && (roomName == "Prison"))
            return "PrisonWork";

        if(((clip == "Idle") || (clip == "Flee")) && (roomName == "Torture"))
            return "TortureWork";
    }

    // A creature that runs for its life keeps looking panicked
    if((clip == "Flee") && !creature->isInContainment())
        return "FleePanic";

    return std::string();
}

void CreatureReactions::startOngoing(Creature* creature, const std::string& eventName)
{
    std::map<std::string, OngoingWork>::iterator it = mOngoing.find(creature->getName());
    if((it != mOngoing.end()) && (it->second.mEventName == eventName))
        return;

    OngoingWork work;
    work.mEventName = eventName;
    work.mSince = mTime;
    work.mNext = mTime + cosmeticRandom(ONGOING_FIRST_MIN, ONGOING_MAX);
    mOngoing[creature->getName()] = work;
}

void CreatureReactions::finishOngoing(Creature* creature, const std::string& newEvent)
{
    std::map<std::string, OngoingWork>::iterator it = mOngoing.find(creature->getName());
    if((it == mOngoing.end()) || (it->second.mEventName == newEvent))
        return;

    // The end of a long sleep or prayer is shown
    std::string doneEvent;
    double duration = mTime - it->second.mSince;
    if((it->second.mEventName == "DormitoryWork") && (duration >= SLEEP_DONE_MIN))
        doneEvent = "WakeRested";
    else if((it->second.mEventName == "TempleWork") && (duration >= PRAYER_DONE_MIN))
        doneEvent = "TempleDone";
    else if((it->second.mEventName == "ClaimWork") && (duration >= CLAIM_DONE_MIN))
        doneEvent = "ClaimDone";
    else if((it->second.mEventName == "FleePanic") && (duration >= FLEE_DONE_MIN))
        doneEvent = "LostFight";

    mOngoing.erase(it);
    if(!doneEvent.empty())
        queueReaction(creature, doneEvent, DONE_WAIT_MAX);
}

void CreatureReactions::updateOngoing()
{
    for(std::map<std::string, OngoingWork>::iterator it = mOngoing.begin(); it != mOngoing.end();)
    {
        if(mTime < it->second.mNext)
        {
            ++it;
            continue;
        }

        it->second.mNext = mTime + cosmeticRandom(ONGOING_MIN, ONGOING_MAX);
        Creature* creature = mGameMap->getCreature(it->first);
        if((creature == nullptr) || !creature->getIsOnMap() || !creature->isAlive())
        {
            mOngoing.erase(it++);
            continue;
        }

        queueReaction(creature, it->second.mEventName);
        ++it;
    }
}

void CreatureReactions::celebrateBout(Creature* loser)
{
    Tile* tile = loser->getPositionTile();
    Room* room = (tile != nullptr) ? tile->getCoveringRoom() : nullptr;
    if((room == nullptr) || (RoomManager::getRoomNameFromRoomType(room->getType()) != "Arena"))
        return;

    // The winners fought the loser a moment ago, the others in the arena watched
    std::vector<Creature*> winners;
    std::vector<Creature*> spectators;
    for(Creature* creature : mGameMap->getCreatures())
    {
        if((creature == loser) || !creature->getIsOnMap() || !creature->isAlive())
            continue;

        Tile* creatureTile = creature->getPositionTile();
        if((creatureTile == nullptr) || (creatureTile->getCoveringRoom() != room))
            continue;

        std::map<std::string, double>::const_iterator itAttack = mLastAttack.find(creature->getName());
        bool fought = (itAttack != mLastAttack.end()) && ((mTime - itAttack->second) <= ATTACK_MEMORY) &&
            ((creature->getPosition() - loser->getPosition()).length() <= WINNER_RADIUS);
        if(fought)
            winners.push_back(creature);
        else
            spectators.push_back(creature);
    }

    if(!winners.empty())
        triggerGroup("Victory", winners, false, VICTORY_DELAY);

    if(!spectators.empty())
        triggerGroup("ArenaBoutOver", spectators, false, ARENA_BOUT_OVER_DELAY);
}

void CreatureReactions::celebrateVictory(Creature* loser, bool fled)
{
    // The same creature going down or running away several times leads to one celebration
    std::map<std::string, double>::const_iterator itLast = mLastCelebration.find(loser->getName());
    if((itLast != mLastCelebration.end()) && ((mTime - itLast->second) < CELEBRATION_PAUSE))
        return;

    // The winners are the creatures of the other side that fought close to the loser a moment ago
    Ogre::Vector3 position = loser->getPosition();
    std::map<int, std::vector<Creature*> > winnersBySeat;
    for(Creature* creature : mGameMap->getCreatures())
    {
        if((creature == loser) || !creature->getIsOnMap() || !creature->isAlive())
            continue;

        if(creature->getSeat()->isAlliedSeat(loser->getSeat()))
            continue;

        std::map<std::string, double>::const_iterator itAttack = mLastAttack.find(creature->getName());
        if((itAttack == mLastAttack.end()) || ((mTime - itAttack->second) > ATTACK_MEMORY))
            continue;

        if((creature->getPosition() - position).length() > WINNER_RADIUS)
            continue;

        winnersBySeat[creature->getSeat()->getId()].push_back(creature);
    }

    if(winnersBySeat.empty())
        return;

    mLastCelebration[loser->getName()] = mTime;

    for(std::map<int, std::vector<Creature*> >::iterator it = winnersBySeat.begin(); it != winnersBySeat.end(); ++it)
    {
        const std::vector<Creature*>& winners = it->second;
        if(fled)
        {
            triggerGroup("VictoryFled", winners, false, VICTORY_FLED_DELAY);
            continue;
        }

        triggerGroup("Victory", winners, false, VICTORY_DELAY);

        // The battle is over if no fighter of the loser's side is left close by. Then the others
        // of the winning side nearby join in the cheering.
        Seat* winnerSeat = winners[0]->getSeat();
        bool battleOver = true;
        std::vector<Creature*> neighbours;
        for(Creature* creature : mGameMap->getCreatures())
        {
            if((creature == loser) || !creature->getIsOnMap() || !creature->isAlive())
                continue;

            if((creature->getPosition() - position).length() > GROUP_RADIUS)
                continue;

            if(creature->getSeat()->isAlliedSeat(winnerSeat))
            {
                neighbours.push_back(creature);
            }
            else if(!creature->getDefinition()->isWorker())
            {
                battleOver = false;
                break;
            }
        }

        if(battleOver && (neighbours.size() >= 2))
            triggerGroup("GroupVictory", neighbours, false, GROUP_VICTORY_DELAY);
    }
}

void CreatureReactions::noteCreatureUpdate(Creature* creature, uint32_t oldLevel, uint32_t oldMood,
        uint32_t oldHealth, Seat* oldSeat, Seat* oldSeatPrison)
{
    if((mMode == Mode::off) || !mConfigLoaded || !creature->getIsOnMap())
        return;

    if(creature->getLevel() > oldLevel)
    {
        // A trainee that reached a new level shows its success with a punch into the air
        trigger(creature, (getRoomName(creature) == "TrainingHall") ? "TrainingDone" : "LevelUp");
    }

    // A prisoner that now serves another keeper was converted (after a torture it shows that it broke)
    if((oldSeatPrison != nullptr) && (creature->getSeatPrison() == nullptr) && (creature->getSeat() != oldSeat))
    {
        std::map<std::string, OngoingWork>::const_iterator itOngoing = mOngoing.find(creature->getName());
        if((itOngoing != mOngoing.end()) && (itOngoing->second.mEventName == "TortureWork"))
        {
            queueReaction(creature, "PrisonConverted", DONE_WAIT_MAX, CONVERTED_DELAY);
            trigger(creature, "TortureBroken");
        }
        else
        {
            trigger(creature, "PrisonConverted");
        }
    }
    else if((oldSeatPrison != nullptr) && (creature->getSeatPrison() == nullptr))
    {
        // The prisoner is free again and still serves its own keeper
        trigger(creature, "PrisonFreed");
    }

    // The creature decided to leave the dungeon
    if(((oldMood & CreatureMoodValues::LeaveDungeon) == 0) &&
       ((creature->getOverlayMoodValue() & CreatureMoodValues::LeaveDungeon) != 0))
    {
        trigger(creature, "LeaveAngry");
    }

    // The health got clearly better (the stage is 0 for unhurt). In the temple one stage is enough.
    uint32_t health = creature->getOverlayHealthValue();
    if(health < oldHealth)
    {
        uint32_t steps = oldHealth - health;
        if((steps >= HEAL_MIN_STEPS) || ((steps >= 1) && (oldHealth >= HURT_STAGE) && (getRoomName(creature) == "Temple")))
            trigger(creature, "Healed");
    }

    // A prisoner that was just put into a cell waits there. The animation that tells so may have come first.
    if((creature->getSeatPrison() != nullptr) && (oldSeatPrison == nullptr))
    {
        std::string ongoingEvent = getOngoingEvent(creature, "Idle");
        if(!ongoingEvent.empty())
            startOngoing(creature, ongoingEvent);
    }

    // A creature knocked out in the arena ends the bout
    if(((oldMood & CreatureMoodValues::KoTemp) == 0) &&
       ((creature->getOverlayMoodValue() & CreatureMoodValues::KoTemp) != 0))
    {
        celebrateBout(creature);
    }

    // The fee is collected while the mood shows it. When it is over the creature is paid if it
    // stands in a treasury (that is where the gold is taken), else there was nothing to take.
    if(((oldMood & CreatureMoodValues::GetFee) != 0) && ((creature->getOverlayMoodValue() & CreatureMoodValues::GetFee) == 0))
    {
        Tile* tile = creature->getPositionTile();
        Room* room = (tile != nullptr) ? tile->getCoveringRoom() : nullptr;
        bool paid = (room != nullptr) && (room->getType() == RoomType::treasury);
        trigger(creature, paid ? "PaydayPaid" : "PaydayUnpaid");
    }
}

void CreatureReactions::noteDigging(Creature* creature)
{
    Tile* tile = creature->getPositionTile();
    if(tile == nullptr)
        return;

    // The client knows what the tiles around the digger are made of
    for(int dx = -1; dx <= 1; ++dx)
    {
        for(int dy = -1; dy <= 1; ++dy)
        {
            Tile* neighbour = mGameMap->getTile(tile->getX() + dx, tile->getY() + dy);
            if((neighbour == nullptr) || (neighbour->getFullness() <= 0.0))
                continue;

            if((neighbour->getType() == TileType::gold) || (neighbour->getType() == TileType::gem))
            {
                trigger(creature, "DigGold");
                return;
            }
        }
    }
}

void CreatureReactions::noteCarry(Creature* carrier, GameEntity* carried)
{
    if((mMode == Mode::off) || !mConfigLoaded || (carried->getObjectType() != GameEntityType::treasuryObject))
        return;

    trigger(carrier, "CarryGold");
}

void CreatureReactions::noteRelease(Creature* carrier, GameEntity* carried)
{
    if((mMode == Mode::off) || !mConfigLoaded || (carried->getObjectType() != GameEntityType::treasuryObject))
        return;

    if(getRoomName(carrier) != "Treasury")
        return;

    // Delivering again and again is tiring: after some deliveries the creature is out of breath
    Delivery& delivery = mDeliveries[carrier->getName()];
    if((delivery.mCount == 0) || ((mTime - delivery.mSince) > DELIVERY_WINDOW))
    {
        delivery.mCount = 0;
        delivery.mSince = mTime;
    }

    ++delivery.mCount;
    if(delivery.mCount >= DELIVERY_TIRED_COUNT)
    {
        delivery.mCount = 0;
        trigger(carrier, "TreasuryFull");
        return;
    }

    // The work with the gold follows the done moment (it is queued first, the running reaction would refuse it)
    queueReaction(carrier, "TreasuryWork", USUAL_WAIT_TIME, TREASURY_WORK_DELAY);
    trigger(carrier, "GoldDelivered");
}

void CreatureReactions::noteHandDrop(GameEntity* entity, Tile* tile)
{
    if((mMode == Mode::off) || !mConfigLoaded || (tile == nullptr))
        return;

    GameEntityType type = entity->getObjectType();
    if((type != GameEntityType::treasuryObject) && (type != GameEntityType::chickenEntity))
        return;

    HandDrop drop;
    drop.mType = type;
    drop.mTileX = tile->getX();
    drop.mTileY = tile->getY();
    drop.mTime = mTime;
    mHandDrops[entity->getName()] = drop;

    // The creatures that stand around look at the gold that falls
    if(type == GameEntityType::treasuryObject)
    {
        noteNearbyEvent("AmbientLookGold", Ogre::Vector3(static_cast<Ogre::Real>(tile->getX()),
            static_cast<Ogre::Real>(tile->getY()), 0.0f), nullptr, GOLD_LOOK_INTERVAL);
    }
}

void CreatureReactions::noteEntityRemoved(GameEntity* entity)
{
    if(mHandDrops.empty())
        return;

    std::map<std::string, HandDrop>::iterator it = mHandDrops.find(entity->getName());
    if((it == mHandDrops.end()) || (it->second.mType != entity->getObjectType()))
        return;

    HandDrop drop = it->second;
    mHandDrops.erase(it);
    if((mMode == Mode::off) || (drop.mType != GameEntityType::treasuryObject) ||
       ((mTime - drop.mTime) > HAND_DROP_MEMORY))
    {
        return;
    }

    // The gold the keeper dropped is gone: a fighter of the keeper standing there took it
    Player* localPlayer = mGameMap->getLocalPlayer();
    if(localPlayer == nullptr)
        return;

    for(Creature* creature : mGameMap->getCreatures())
    {
        if((creature->getSeat() != localPlayer->getSeat()) || creature->getDefinition()->isWorker())
            continue;

        Tile* tile = creature->getPositionTile();
        if((tile == nullptr) || (tile->getX() != drop.mTileX) || (tile->getY() != drop.mTileY))
            continue;

        trigger(creature, "GoldGift");
    }
}

void CreatureReactions::noteCreatureAdded(Creature* creature)
{
    Player* localPlayer = mGameMap->getLocalPlayer();
    if((localPlayer == nullptr) || (mTime < ARRIVAL_QUIET_TIME) || creature->isInContainment())
        return;

    // A creature of the keeper that appears on a portal has just arrived in the dungeon
    if(creature->getSeat() == localPlayer->getSeat())
    {
        Tile* tile = creature->getPositionTile();
        Room* room = (tile != nullptr) ? tile->getCoveringRoom() : nullptr;
        if((room != nullptr) && ((room->getType() == RoomType::portal) || (room->getType() == RoomType::portalWave)))
            queueReaction(creature, "PortalArrival", DONE_WAIT_MAX, ARRIVAL_DELAY);

        return;
    }

    // An enemy that comes into sight: the creatures of the keeper that stand close by notice it
    if(!creature->getSeat()->isAlliedSeat(localPlayer->getSeat()))
    {
        noteNearbyEvent("EnemySpotted", creature->getPosition(), creature, ENEMY_SPOTTED_INTERVAL,
            localPlayer->getSeat());
    }
}

void CreatureReactions::noteParticleEffect(GameEntity* entity, const std::string& script)
{
    if((mMode == Mode::off) || !mConfigLoaded || (entity->getObjectType() != GameEntityType::creature))
        return;

    std::string eventName;
    if(script == "SpellCreatureHeal")
        eventName = "Healed";
    else if(script == "SpellCreatureHaste")
        eventName = "SpellHaste";
    else if(script == "SpellCreatureStrength")
        eventName = "SpellStrength";
    else if(script == "SpellCreatureDefense")
        eventName = "SpellDefense";
    else
        return;

    trigger(static_cast<Creature*>(entity), eventName);
}

void CreatureReactions::noteAllyDied(Creature* dead)
{
    std::vector<Creature*> mourners;
    for(Creature* creature : mGameMap->getCreatures())
    {
        if((creature == dead) || !creature->getIsOnMap() || !creature->isAlive() || creature->isMoving() ||
           creature->isInContainment())
        {
            continue;
        }

        if(!creature->getSeat()->isAlliedSeat(dead->getSeat()))
            continue;

        Ogre::Vector3 difference = creature->getPosition() - dead->getPosition();
        difference.z = 0.0f;
        if(difference.length() > ALLY_DEATH_RADIUS)
            continue;

        // Creatures that fight or do something else on their own are left alone
        if(getCreaturePriority(creature) != ReactionPriority::none)
            continue;

        mourners.push_back(creature);
    }

    if(!mourners.empty())
        triggerGroup("AllyDied", mourners, false, ALLY_DEATH_DELAY);
}

void CreatureReactions::noteSlapRequest(GameEntity* entity)
{
    // The server confirms a slap without telling what was hit, so the target of the request is remembered
    mSlapTarget.clear();
    if(entity->getObjectType() != GameEntityType::creature)
        return;

    mSlapTarget = entity->getName();
    mSlapTime = mTime;
}

void CreatureReactions::noteSlapped(const Ogre::Vector3& handPosition)
{
    if((mMode == Mode::off) || !mConfigLoaded)
        return;

    Creature* target = nullptr;
    if(!mSlapTarget.empty() && ((mTime - mSlapTime) <= SLAP_MEMORY))
        target = mGameMap->getCreature(mSlapTarget);

    mSlapTarget.clear();

    // Without a request (for example a slap the client did not ask for) it is the creature closest to the hand
    if(target == nullptr)
    {
        double best = SLAP_FALLBACK_RADIUS;
        for(Creature* creature : mGameMap->getCreatures())
        {
            if(!creature->getIsOnMap() || !creature->isAlive())
                continue;

            Ogre::Vector3 difference = creature->getPosition() - handPosition;
            difference.z = 0.0f;
            double distance = difference.length();
            if(distance >= best)
                continue;

            best = distance;
            target = creature;
        }
    }

    if(target == nullptr)
        return;

    mSlappedAt[target->getName()] = mTime;
    trigger(target, "Slapped");

    // Some of the creatures that stand around laugh at it
    noteNearbyEvent("LaughAtSlapped", target->getPosition(), target, SLAP_LAUGH_INTERVAL);
}

void CreatureReactions::noteHandPicked(Creature* creature)
{
    if((mMode == Mode::off) || !mConfigLoaded)
        return;

    // The creature is in the hand after a moment, the reaction waits for the hand to be ready
    queueReaction(creature, "PickedUp", DONE_WAIT_MAX, PICKED_UP_DELAY);
}

void CreatureReactions::noteHandDropped(Creature* creature)
{
    if((mMode == Mode::off) || !mConfigLoaded)
        return;

    queueReaction(creature, "Dropped", DONE_WAIT_MAX, DROPPED_DELAY);
}

void CreatureReactions::noteHandHover(Creature* creature)
{
    if((mMode == Mode::off) || !mConfigLoaded || (creature == nullptr))
        return;

    // Only the creatures of the keeper look up to the hand
    Player* localPlayer = mGameMap->getLocalPlayer();
    if((localPlayer == nullptr) || (creature->getSeat() != localPlayer->getSeat()))
        return;

    // A creature that was slapped lately knows what the hand can do
    std::map<std::string, double>::const_iterator itSlapped = mSlappedAt.find(creature->getName());
    bool ducks = (itSlapped != mSlappedAt.end()) && ((mTime - itSlapped->second) <= SLAP_DUCK_MEMORY);
    trigger(creature, ducks ? "HandHoverDuck" : "HandHover");
}

void CreatureReactions::endForCreature(Creature* creature)
{
    for(std::vector<PendingReaction>::iterator it = mPending.begin(); it != mPending.end();)
    {
        if(it->mCreatureName == creature->getName())
            it = mPending.erase(it);
        else
            ++it;
    }

    RunningReaction* running = findRunning(creature->getName());
    if(running != nullptr)
    {
        endReaction(*running, creature);
        eraseRunning(creature->getName());
    }
}

bool CreatureReactions::triggerLook(Creature* creature, const std::string& eventName, const Ogre::Vector3& target)
{
    mNextLookTarget = target;
    mHasNextLookTarget = true;
    bool started = trigger(creature, eventName);
    mHasNextLookTarget = false;
    return started;
}

void CreatureReactions::noteNearbyEvent(const std::string& eventName, const Ogre::Vector3& position,
        const Creature* exclude, double minInterval, Seat* onlyAlliedTo)
{
    if((mMode == Mode::off) || !mConfigLoaded)
        return;

    // Looking is not needed for every blow of a fight: one look in a while is enough
    std::map<std::string, double>::iterator itNext = mNextLook.find(eventName);
    if((itNext != mNextLook.end()) && (itNext->second > mTime))
        return;

    mNextLook[eventName] = mTime + minInterval;

    // Creatures standing still close by, a few of them, in no special order
    std::vector<Creature*> candidates;
    for(Creature* creature : mGameMap->getCreatures())
    {
        if((creature == exclude) || !creature->getIsOnMap() || !creature->isAlive() || creature->isMoving())
            continue;

        if((onlyAlliedTo != nullptr) && !creature->getSeat()->isAlliedSeat(onlyAlliedTo))
            continue;

        Ogre::Vector3 difference = creature->getPosition() - position;
        difference.z = 0.0f;
        if(difference.length() > mConfig.getLookRadius())
            continue;

        candidates.push_back(creature);
    }

    std::shuffle(candidates.begin(), candidates.end(), cosmeticRng());

    uint32_t nbLooking = 0;
    for(Creature* creature : candidates)
    {
        if(nbLooking >= MAX_LOOKERS)
            break;

        if(triggerLook(creature, eventName, position))
            ++nbLooking;
    }
}

void CreatureReactions::spreadTo(Creature* creature, const std::string& eventName)
{
    // The closest creature that stands still, gets tired, and is not busy catches it
    Creature* closest = nullptr;
    double best = NEIGHBOUR_RADIUS;
    for(Creature* other : mGameMap->getCreatures())
    {
        if((other == creature) || !other->getIsOnMap() || !other->isAlive() || other->isMoving() ||
           !creatureNeedsSleep(other))
        {
            continue;
        }

        Ogre::Vector3 difference = other->getPosition() - creature->getPosition();
        difference.z = 0.0f;
        double distance = difference.length();
        if(distance >= best)
            continue;

        best = distance;
        closest = other;
    }

    if(closest != nullptr)
        queueReaction(closest, eventName, -1.0, YAWN_CATCH_DELAY + cosmeticRandom(0.0, 0.6));
}

bool CreatureReactions::isHurtAndThreatened(const Creature* creature) const
{
    if(creature->getOverlayHealthValue() < HURT_STAGE)
        return false;

    for(Creature* other : mGameMap->getCreatures())
    {
        if((other == creature) || !other->getIsOnMap() || !other->isAlive() || other->getDefinition()->isWorker())
            continue;

        if(other->getSeat()->isAlliedSeat(creature->getSeat()))
            continue;

        if((other->getPosition() - creature->getPosition()).length() <= SCARE_RADIUS)
            return true;
    }

    return false;
}

void CreatureReactions::updateMoods(Ogre::Real timeSinceLastFrame)
{
    if((mMode == Mode::off) || !mConfigLoaded || (mConfig.getMoodPerTick() == 0))
        return;

    mMoodTimer -= timeSinceLastFrame;
    if(mMoodTimer > 0.0)
        return;

    mMoodTimer = mConfig.getMoodInterval();

    // A few creatures at a time, one after the other: every creature is looked at now and then, not every frame
    const std::vector<Creature*>& creatures = mGameMap->getCreatures();
    if(creatures.empty())
        return;

    size_t nbLooks = std::min(static_cast<size_t>(mConfig.getMoodPerTick()), creatures.size());
    for(size_t i = 0; i < nbLooks; ++i)
    {
        ++mMoodIndex;
        if(mMoodIndex >= creatures.size())
            mMoodIndex = 0;

        examineMood(creatures[mMoodIndex]);
    }
}

void CreatureReactions::examineMood(Creature* creature)
{
    if(!creature->getIsOnMap() || !creature->isAlive() || creature->isInContainment())
        return;

    Ogre::AnimationState* animState = creature->getAnimationState();
    if(animState == nullptr)
        return;

    // Feelings are shown while the creature stands, less often while it walks, never while it does something
    bool moving = creature->isMoving();
    bool idle = !moving && (animState->getAnimationName() == "Idle");
    if(!moving && !idle)
        return;

    if(moving && (cosmeticRandom(0.0, 1.0) >= mConfig.getMoodWalkingChance()))
        return;

    if(findRunning(creature->getName()) != nullptr)
        return;

    if(!isCreatureNearCamera(creature))
    {
        mIdleSince.erase(creature->getName());
        return;
    }

    // Waiting in a room where idling means something else (praying, watching a bout, being held) is not boredom
    if(idle && !getOngoingEvent(creature, "Idle").empty())
        idle = false;

    // How long the creature has been seen standing idle
    double idleFor = 0.0;
    if(idle)
    {
        std::map<std::string, double>::const_iterator itIdle = mIdleSince.find(creature->getName());
        if(itIdle == mIdleSince.end())
            mIdleSince[creature->getName()] = mTime;
        else
            idleFor = mTime - itIdle->second;
    }
    else
    {
        mIdleSince.erase(creature->getName());
    }

    // The mood is only known for the creatures of the local keeper
    Player* localPlayer = mGameMap->getLocalPlayer();
    bool own = (localPlayer != nullptr) && (creature->getSeat() == localPlayer->getSeat());
    uint32_t bits = own ? creature->getOverlayMoodValue() : CreatureMoodValues::Nothing;
    if((bits & CreatureMoodValues::KoDeathOrTemp) != 0)
        return;

    CreatureMoodLevel level = creature->getMoodValue();

    // The most pressing feelings first, then the pleasant ones, then what a creature does when it has nothing to do
    std::vector<std::string> events;
    if((bits & CreatureMoodValues::LeaveDungeon) != 0)
        events.push_back("MoodLeaving");
    if((bits & (CreatureMoodValues::Angry | CreatureMoodValues::Furious)) != 0)
        events.push_back("MoodAngry");
    if(own && (level == CreatureMoodLevel::Upset))
        events.push_back("MoodUpset");
    if(idle && isHurtAndThreatened(creature))
        events.push_back("MoodScared");
    if(creature->getOverlayHealthValue() >= HURT_STAGE)
        events.push_back(moving ? "HurtWalk" : "HurtIdle");
    if((bits & CreatureMoodValues::Hungry) != 0)
        events.push_back("MoodHungry");
    if((bits & CreatureMoodValues::Tired) != 0)
        events.push_back("MoodTired");
    if((bits & CreatureMoodValues::GetFee) != 0)
        events.push_back("MoodGreedy");

    std::map<std::string, double>::const_iterator itProud = mProudUntil.find(creature->getName());
    if((itProud != mProudUntil.end()) && (mTime < itProud->second))
        events.push_back("MoodProud");

    if(idle && (idleFor >= mConfig.getBoredAfter()))
        events.push_back("MoodBored");
    else if(idle && (idleFor >= mConfig.getImpatientAfter()))
        events.push_back("MoodImpatient");

    if(own && (level == CreatureMoodLevel::Happy))
        events.push_back("MoodHappy");
    else if(own && (level == CreatureMoodLevel::Neutral) &&
            ((bits & (CreatureMoodValues::Hungry | CreatureMoodValues::Tired | CreatureMoodValues::GetFee)) == 0))
    {
        events.push_back("MoodContent");
    }

    // A long rest: sits down, and lies down if it goes on, in the open
    if(idle && (idleFor >= mConfig.getLieAfter()) && (getRoomName(creature) != "Dormitory"))
        events.push_back("AmbientLieDown");
    else if(idle && (idleFor >= mConfig.getSitAfter()))
        events.push_back("AmbientSitDown");

    // Small habits of the kind of creature and small changes of the idle pose, in a changing order
    if(idle && (idleFor >= mConfig.getAmbientAfter()))
    {
        if(cosmeticRandom(0.0, 1.0) < HABIT_FIRST_CHANCE)
        {
            events.push_back("AmbientHabit");
            events.push_back("AmbientIdle");
        }
        else
        {
            events.push_back("AmbientIdle");
            events.push_back("AmbientHabit");
        }
    }

    for(const std::string& eventName : events)
    {
        if(trigger(creature, eventName))
            return;
    }

    examineInteraction(creature, idle, moving);
}

bool CreatureReactions::isInGroup(const Creature* creature, const std::string& group) const
{
    const CreatureDefinition* definition = creature->getDefinition();
    if(definition == nullptr)
        return false;

    return contains(mConfig.getGroupsOf(definition->getClassName()), group);
}

bool CreatureReactions::isFreeAndIdle(Creature* creature)
{
    if(!creature->getIsOnMap() || !creature->isAlive() || creature->isMoving() || creature->isInContainment())
        return false;

    Ogre::AnimationState* animState = creature->getAnimationState();
    if((animState == nullptr) || (animState->getAnimationName() != "Idle"))
        return false;

    // Waiting in a room where idling means something else (praying, watching a bout) is not free time
    if(!getOngoingEvent(creature, "Idle").empty())
        return false;

    return findRunning(creature->getName()) == nullptr;
}

bool CreatureReactions::areFacingEachOther(const Creature* first, const Creature* second)
{
    Ogre::SceneNode* firstNode = first->getEntityNode();
    Ogre::SceneNode* secondNode = second->getEntityNode();
    if((firstNode == nullptr) || (secondNode == nullptr))
        return false;

    Ogre::Vector3 toSecond = second->getPosition() - first->getPosition();
    toSecond.z = 0.0f;
    if(toSecond.length() < FACING_MIN_LENGTH)
        return false;

    toSecond.normalise();
    Ogre::Vector3 firstForward = firstNode->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Y;
    Ogre::Vector3 secondForward = secondNode->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Y;
    firstForward.z = 0.0f;
    secondForward.z = 0.0f;
    if((firstForward.length() < FACING_MIN_LENGTH) || (secondForward.length() < FACING_MIN_LENGTH))
        return false;

    firstForward.normalise();
    secondForward.normalise();
    return (firstForward.dotProduct(toSecond) > FACING_MIN_DOT) && (secondForward.dotProduct(toSecond) < -FACING_MIN_DOT);
}

bool CreatureReactions::startMeeting(Creature* first, const std::string& firstEvent, Creature* second,
        const std::string& secondEvent, double secondDelay)
{
    if(!trigger(first, firstEvent))
        return false;

    mNextInteraction = mTime + mConfig.getInteractionPause();
    queueReaction(second, secondEvent, -1.0, secondDelay);
    return true;
}

void CreatureReactions::examineInteraction(Creature* creature, bool idle, bool moving)
{
    // A short meeting is a dice throw now and then, and not more often than a pause over all the creatures
    if((mTime < mNextInteraction) || (cosmeticRandom(0.0, 1.0) >= mConfig.getInteractionChance()))
        return;

    bool worker = creature->getDefinition()->isWorker();
    bool fighterLike = !worker && (isInGroup(creature, "Fighters") || isInGroup(creature, "LargeBeasts"));
    double radius = mConfig.getInteractionRadius();
    double radiusWave = radius * WAVE_RADIUS_FACTOR;

    Creature* chatPartner = nullptr;
    Creature* sparPartner = nullptr;
    Creature* passingFighter = nullptr;
    Creature* bumpPartner = nullptr;
    double bestChat = radius;
    double bestSpar = radius;
    double bestPassing = radiusWave;
    double bestBump = BUMP_RADIUS;
    for(Creature* other : mGameMap->getCreatures())
    {
        if((other == creature) || !other->getIsOnMap() || !other->isAlive() || other->isInContainment())
            continue;

        if(!other->getSeat()->isAlliedSeat(creature->getSeat()))
            continue;

        Ogre::Vector3 difference = other->getPosition() - creature->getPosition();
        difference.z = 0.0f;
        double distance = difference.length();
        if(distance > radiusWave)
            continue;

        if(other->isMoving())
        {
            if(worker && !other->getDefinition()->isWorker() && (distance < bestPassing) &&
               (findRunning(other->getName()) == nullptr))
            {
                bestPassing = distance;
                passingFighter = other;
            }

            if(moving && (distance < bestBump) && areFacingEachOther(creature, other) &&
               (findRunning(other->getName()) == nullptr))
            {
                bestBump = distance;
                bumpPartner = other;
            }
            continue;
        }

        if(!idle || (distance >= radius) || !isFreeAndIdle(other))
            continue;

        if(distance < bestChat)
        {
            bestChat = distance;
            chatPartner = other;
        }

        if(fighterLike && (distance < bestSpar) && !other->getDefinition()->isWorker() &&
           (isInGroup(other, "Fighters") || isInGroup(other, "LargeBeasts")))
        {
            bestSpar = distance;
            sparPartner = other;
        }
    }

    // One of the meetings that is possible now, chosen by chance
    std::vector<uint32_t> options;
    if(chatPartner != nullptr)
        options.push_back(MEETING_CHAT);
    if(sparPartner != nullptr)
        options.push_back(MEETING_SPAR);
    if(passingFighter != nullptr)
        options.push_back(MEETING_WAVE);
    if(bumpPartner != nullptr)
        options.push_back(MEETING_GRUMBLE);

    if(options.empty())
        return;

    uint32_t choice = options[static_cast<size_t>(cosmeticRandom(0.0,
        static_cast<double>(options.size()) - CHOICE_RANGE_MARGIN))];
    switch(choice)
    {
        case MEETING_CHAT:
            startMeeting(creature, "Chat", chatPartner, "ChatReply",
                CHAT_REPLY_DELAY_MIN + cosmeticRandom(0.0, CHAT_REPLY_DELAY_SPREAD));
            break;
        case MEETING_SPAR:
            startMeeting(creature, "SparPush", sparPartner, "SparStumble", SPAR_REPLY_DELAY);
            break;
        case MEETING_WAVE:
            startMeeting(creature, "WaveAtFighter", passingFighter, "NodBack", WAVE_REPLY_DELAY);
            break;
        default:
            startMeeting(creature, "Grumble", bumpPartner, "GrumbleBack", GRUMBLE_REPLY_DELAY);
            break;
    }
}

bool CreatureReactions::createProps(RunningReaction& reaction, Creature* creature, const ReactionVariant& variant)
{
    const ReactionProp& prop = variant.mProp;
    std::string material = PROP_MATERIAL_PREFIX + prop.mSprite;
    if(!Ogre::MaterialManager::getSingleton().resourceExists(material, "Graphics"))
    {
        logMissingOnce("prop sprite", prop.mSprite);
        return false;
    }

    Ogre::Entity* entity = getCreatureEntity(creature);
    Ogre::SceneNode* creatureNode = creature->getEntityNode();
    if((entity == nullptr) || (creatureNode == nullptr) || (creatureNode->getParentSceneNode() == nullptr))
        return false;

    // The size of the sprites follows the size of the creature
    double height = static_cast<double>(entity->getWorldBoundingBox(true).getSize().z);
    height = std::max(PROP_HEIGHT_MIN, std::min(PROP_HEIGHT_MAX, height));

    // Without a wall to turn to, the props use a point in front of the creature
    if(!reaction.mHasLookTarget)
    {
        Ogre::Vector3 forward = creatureNode->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Y;
        forward.z = 0.0f;
        if(forward.length() > DIRECTION_MIN_LENGTH)
            forward.normalise();
        reaction.mLookTarget = creature->getPosition() + forward * PROP_FRONT_DISTANCE;
        reaction.mHasLookTarget = true;
    }

    uint32_t nbSprites = std::max<uint32_t>(1, prop.mCount);
    if((prop.mPath == ReactionProp::Path::yoyo) || (prop.mPath == ReactionProp::Path::flip) ||
       (prop.mPath == ReactionProp::Path::toss) || (prop.mPath == ReactionProp::Path::critter) ||
       (prop.mPath == ReactionProp::Path::balance) || (prop.mPath == ReactionProp::Path::shadow) ||
       (prop.mPath == ReactionProp::Path::kick))
    {
        nbSprites = 1;
    }

    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    std::string id = Helper::toString(mNextPropId);
    ++mNextPropId;
    std::string setName = PROP_NAME_PREFIX + id;
    std::string nodeName = setName + "_node";

    Ogre::BillboardSet* set = sceneManager->createBillboardSet(setName, nbSprites);
    set->setMaterialName(material);
    Ogre::Real size = static_cast<Ogre::Real>(prop.mSize * height);
    set->setDefaultDimensions(size, size);
    set->setCastShadows(false);
    if(prop.mPath == ReactionProp::Path::doodle)
    {
        // The marks lie flat on the ground
        set->setBillboardType(Ogre::BBT_PERPENDICULAR_COMMON);
        set->setCommonDirection(Ogre::Vector3::UNIT_Z);
        set->setCommonUpVector(Ogre::Vector3::UNIT_Y);
    }

    // The balls of the juggler have colours of their own
    const Ogre::ColourValue ballColours[NB_BALL_COLOURS] = {Ogre::ColourValue(1.0f, 0.45f, 0.4f), Ogre::ColourValue(0.5f, 0.9f, 0.5f),
        Ogre::ColourValue(0.5f, 0.65f, 1.0f)};
    for(uint32_t i = 0; i < nbSprites; ++i)
    {
        Ogre::ColourValue colour = Ogre::ColourValue::White;
        if(prop.mPath == ReactionProp::Path::juggle)
            colour = ballColours[i % NB_BALL_COLOURS];

        set->createBillboard(Ogre::Vector3::ZERO, colour);
    }

    Ogre::SceneNode* node = creatureNode->getParentSceneNode()->createChildSceneNode(nodeName);
    node->attachObject(set);
    node->setPosition(creature->getPosition());

    reaction.mProp = prop;
    reaction.mPropSetName = setName;
    reaction.mPropNodeName = nodeName;
    reaction.mPropHeight = height;

    updateProps(reaction, creature);
    return true;
}

void CreatureReactions::updateProps(RunningReaction& reaction, Creature* creature)
{
    if(reaction.mPropSetName.empty())
        return;

    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    Ogre::SceneNode* creatureNode = creature->getEntityNode();
    if((creatureNode == nullptr) || !sceneManager->hasBillboardSet(reaction.mPropSetName) ||
       !sceneManager->hasSceneNode(reaction.mPropNodeName))
    {
        return;
    }

    Ogre::BillboardSet* set = sceneManager->getBillboardSet(reaction.mPropSetName);
    Ogre::SceneNode* node = sceneManager->getSceneNode(reaction.mPropNodeName);
    node->setPosition(creature->getPosition());

    PropFrame frame;
    frame.mHeight = reaction.mPropHeight;
    frame.mForward = creatureNode->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Y;
    frame.mForward.z = 0.0f;
    if(frame.mForward.length() > DIRECTION_MIN_LENGTH)
        frame.mForward.normalise();
    frame.mRight = frame.mForward.crossProduct(Ogre::Vector3::UNIT_Z);

    const ReactionProp& prop = reaction.mProp;
    double time = reaction.mElapsed;
    double seconds = std::max(0.1, prop.mSeconds);
    double progress = std::min(1.0, time / seconds);
    Ogre::Real size = static_cast<Ogre::Real>(prop.mSize * reaction.mPropHeight);
    const Ogre::Real hidden = 0.0001f;

    // Where the wall or the point the prop turns to is, seen from the creature
    Ogre::Vector3 target = reaction.mLookTarget - node->getPosition();
    target.z = 0.0f;

    int nbBillboards = set->getNumBillboards();
    for(int i = 0; i < nbBillboards; ++i)
    {
        Ogre::Billboard* billboard = set->getBillboard(static_cast<unsigned short>(i));
        Ogre::Vector3 offset = Ogre::Vector3::ZERO;
        Ogre::Real width = size;
        Ogre::Real length = size;
        Ogre::ColourValue colour = billboard->getColour();
        double index = static_cast<double>(i);
        double count = static_cast<double>(nbBillboards);

        switch(prop.mPath)
        {
            case ReactionProp::Path::juggle:
            {
                // Balls go round from hand to hand; in the end the first one is dropped
                double angle = 2.0 * PI_VALUE * (0.75 * time + index / count);
                double sideways = 0.2 * std::cos(angle);
                double up = 0.5 + 0.3 * std::fabs(std::sin(angle));
                if((i == 0) && (progress > 0.78))
                {
                    double dropAngle = 2.0 * PI_VALUE * 0.75 * 0.78 * seconds;
                    double fall = (progress - 0.78) * seconds;
                    sideways = 0.2 * std::cos(dropAngle) + 0.15 * fall;
                    up = std::max(0.03, 0.5 + 0.3 * std::fabs(std::sin(dropAngle)) - 2.5 * fall * fall);
                }
                offset = frame.at(sideways, 0.3, up);
                break;
            }
            case ReactionProp::Path::yoyo:
            {
                double up = 0.72 - 0.4 * std::fabs(std::sin(PI_VALUE * 1.4 * time));
                offset = frame.at(0.22, 0.28, up);
                break;
            }
            case ReactionProp::Path::flip:
            {
                // The coin goes up and comes down turning, so it is seen from the side now and then
                double phase = std::fmod(time, 0.9) / 0.9;
                double up = 0.55 + 1.6 * phase * (1.0 - phase);
                offset = frame.at(0.1, 0.28, up);
                length = size * static_cast<Ogre::Real>(std::max(0.15, std::fabs(std::cos(2.0 * PI_VALUE * 3.0 * phase))));
                break;
            }
            case ReactionProp::Path::stack:
            {
                // One pebble after the other is put on the pile; at the end the pile falls apart
                double appears = seconds * 0.12 * (index + 1.0);
                double collapse = seconds * 0.72;
                double side = ((i % 2) == 0) ? 1.0 : -1.0;
                double sideways = 0.02 * side;
                double ahead = 0.3;
                double up = 0.03 + 0.075 * index;
                if(time >= collapse)
                {
                    double fall = time - collapse;
                    sideways += side * (0.4 + 0.2 * index) * std::min(fall, 0.8) * 0.35;
                    ahead += 0.1 * index * std::min(fall, 0.8);
                    up = std::max(0.025, up - 2.5 * fall * fall);
                }
                offset = frame.at(sideways, ahead, up);
                if(time < appears)
                {
                    width = hidden;
                    length = hidden;
                }
                break;
            }
            case ReactionProp::Path::toss:
            {
                // From the hand to the wall and back
                double phase = std::fmod(time, 1.0);
                double way = (phase < 0.5) ? (phase * 2.0) : ((1.0 - phase) * 2.0);
                way = way * way * (3.0 - 2.0 * way);
                Ogre::Vector3 hand = frame.at(0.15, 0.25, 0.62);
                Ogre::Vector3 wall = target + Ogre::Vector3(0.0f, 0.0f, static_cast<Ogre::Real>(0.62 * reaction.mPropHeight));
                offset = hand + (wall - hand) * static_cast<Ogre::Real>(way);
                break;
            }
            case ReactionProp::Path::critter:
            {
                // Runs around the feet, and in the end away
                double radius = 0.28;
                if(progress > 0.7)
                    radius += (progress - 0.7) * seconds * 0.5;
                double angle = 2.0 * PI_VALUE * 0.5 * time;
                offset = frame.at(radius * std::cos(angle), radius * std::sin(angle), 0.03);
                break;
            }
            case ReactionProp::Path::balance:
            {
                // The tool wobbles on the fingertip, in the end it falls
                double wobble = 0.35 * std::sin(2.0 * PI_VALUE * 1.3 * time) + 0.2 * std::sin(2.0 * PI_VALUE * 2.9 * time);
                double up = 0.84;
                if(progress > 0.85)
                {
                    double fall = (progress - 0.85) * seconds;
                    up = std::max(0.04, up - 3.0 * fall * fall);
                    wobble += fall * 3.0;
                }
                offset = frame.at(0.2 + 0.03 * wobble, 0.28, up);
                billboard->setRotation(Ogre::Radian(static_cast<Ogre::Real>(wobble)));
                break;
            }
            case ReactionProp::Path::doodle:
            {
                // The marks of a squiggle appear one after the other and fade out in the end
                double along = (count > 1.0) ? (index / (count - 1.0)) : 0.0;
                double appears = seconds * (0.1 + 0.5 * along);
                offset = frame.at(0.15 + 0.22 * along + 0.05 * std::sin(along * 9.0),
                    0.22 + 0.12 * std::sin(along * 6.0), 0.01);
                double alpha = (time < appears) ? 0.0 : 1.0;
                if(progress > 0.75)
                    alpha *= std::max(0.0, 1.0 - (progress - 0.75) / 0.25);
                colour.a = static_cast<Ogre::Real>(alpha);
                break;
            }
            case ReactionProp::Path::shadow:
            {
                // The shadow on the wall flaps and fades in and out
                Ogre::Vector3 towards = target;
                if(towards.length() > DIRECTION_MIN_LENGTH)
                    towards = towards - towards.normalisedCopy() * 0.08f;
                offset = towards + Ogre::Vector3(0.0f, 0.0f, static_cast<Ogre::Real>(0.62 * reaction.mPropHeight));
                width = size * static_cast<Ogre::Real>(1.0 + 0.15 * std::sin(2.0 * PI_VALUE * 2.0 * time));
                colour.a = static_cast<Ogre::Real>(plateau(progress, 0.15));
                break;
            }
            case ReactionProp::Path::kick:
            {
                // Each kick sends the pebble forward over the floor
                double cycle = seconds / std::max<uint32_t>(1, prop.mCount);
                double phase = std::fmod(time, cycle) / cycle;
                double distance = 0.28 + 0.9 * (1.0 - (1.0 - phase) * (1.0 - phase));
                offset = frame.at(0.03, distance, 0.03);
                if(phase > 0.9)
                {
                    width = hidden;
                    length = hidden;
                }
                break;
            }
            default:
                break;
        }

        billboard->setPosition(offset);
        billboard->setDimensions(width, length);
        billboard->setColour(colour);
    }
}

void CreatureReactions::removeProps(RunningReaction& reaction)
{
    if(reaction.mPropSetName.empty())
        return;

    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    if(sceneManager->hasSceneNode(reaction.mPropNodeName))
    {
        Ogre::SceneNode* node = sceneManager->getSceneNode(reaction.mPropNodeName);
        node->detachAllObjects();
        sceneManager->destroySceneNode(node);
    }

    if(sceneManager->hasBillboardSet(reaction.mPropSetName))
        sceneManager->destroyBillboardSet(reaction.mPropSetName);

    reaction.mPropSetName.clear();
    reaction.mPropNodeName.clear();
}

void CreatureReactions::stopAll()
{
    mPending.clear();
    mOngoing.clear();
    for(RunningReaction& reaction : mRunning)
        endReaction(reaction, mGameMap->getCreature(reaction.mCreatureName));

    mRunning.clear();
}
