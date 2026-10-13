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

} // namespace

CreatureReactions::CreatureReactions(GameMap* gameMap, const std::string& configPath) :
    mGameMap(gameMap),
    mConfigFileName(configPath + "creatureReactions.cfg"),
    mConfigLoaded(false),
    mMode(Mode::full),
    mTime(0.0),
    mTimeLastPrune(0.0),
    mNextParticleId(0)
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
        return ReactionPriority::death;

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

    return true;
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

    if(!creature->getIsOnMap() || !creature->isAlive())
        return false;

    const ReactionEvent* event = mConfig.getEvent(eventName);
    if(event == nullptr)
    {
        logMissingOnce("reaction event", eventName);
        return false;
    }

    if(!forced)
    {
        if(!isCreatureNearCamera(creature))
            return false;

        std::map<std::string, double>::const_iterator itCooldown =
            mCooldownEnd.find(creature->getName() + "|" + eventName);
        if((itCooldown != mCooldownEnd.end()) && (itCooldown->second > mTime))
            return false;

        if((mRunning.size() >= mConfig.getMaxSimultaneous()) && (findRunning(creature->getName()) == nullptr))
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

    if(mMode == Mode::full)
    {
        // Tier C / B: a clip, only while the creature stands still (it must not slide while posing)
        if(!creature->isMoving() && !event.mWhileWorking && startClip(reaction, creature, variant))
            shown = true;

        for(const ReactionEffect& effect : variant.mEffects)
        {
            if(addParticles(reaction, creature, effect.mName))
            {
                reaction.mDuration = std::max(reaction.mDuration, effect.mTime);
                shown = true;
            }
        }

        Ogre::SceneNode* node = creature->getEntityNode();
        // Turning and squashing look wrong on a creature that walks
        bool standingMotion = (variant.mMotion.mType == ReactionMotion::Type::spin) ||
            (variant.mMotion.mType == ReactionMotion::Type::turn) ||
            (variant.mMotion.mType == ReactionMotion::Type::squash);
        if((variant.mMotion.mType != ReactionMotion::Type::none) && (variant.mMotion.mDuration > 0.0) &&
           (node != nullptr) && !(standingMotion && creature->isMoving()))
        {
            reaction.mMotion = variant.mMotion;
            reaction.mMotionLastPosition = node->getPosition();
            reaction.mMotionLastScale = node->getScale();
            reaction.mDuration = std::max(reaction.mDuration, variant.mMotion.mDuration);
            shown = true;
        }
    }

    if(!shown)
        return false;

    double cooldown = (variant.mCooldown >= 0.0) ? variant.mCooldown : event.mCooldown;
    mCooldownEnd[creature->getName() + "|" + event.mName] = mTime + cooldown;

    mRunning.push_back(reaction);
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
            case ReactionMotion::Type::turn:
            {
                if(!reaction.mMotionTurnComputed)
                {
                    reaction.mMotionTurnComputed = true;
                    reaction.mMotionTurnAngle = 0.0;
                    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
                    Ogre::Camera* camera = (frameListener != nullptr) ?
                        frameListener->getCameraManager()->getActiveCamera() : nullptr;
                    if(camera != nullptr)
                    {
                        Ogre::Vector3 toCamera = camera->getDerivedPosition() - node->getPosition();
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
        node->rotate(Ogre::Quaternion(Ogre::Radian(static_cast<Ogre::Real>(deltaAngle)), Ogre::Vector3::UNIT_Z),
            Ogre::Node::TS_PARENT);

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
    const ReactionEvent* runningEvent = reaction.mWhileWorking ? mConfig.getEvent(reaction.mEventName) : nullptr;
    if(!(reaction.mPriority < getCreaturePriority(creature, runningEvent)))
        return false;

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
    return true;
}

void CreatureReactions::endReaction(RunningReaction& reaction, Creature* creature)
{
    stopClip(reaction, creature);
    clearMotion(reaction, creature);
    removeParticles(reaction);

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
        bool stillRunning = (creature != nullptr) && creature->getIsOnMap() && creature->isAlive() &&
            updateReaction(*it, creature, timeSinceLastFrame);
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

    for(std::map<std::string, Delivery>::iterator it = mDeliveries.begin(); it != mDeliveries.end();)
    {
        if((mTime - it->second.mSince) > DELIVERY_WINDOW)
            mDeliveries.erase(it++);
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

    // Something that goes on for a while is shown now and then, until the creature does something else
    std::string ongoingEvent = getOngoingEvent(creature, clip);
    finishOngoing(creature, ongoingEvent);
    if(!ongoingEvent.empty())
        startOngoing(creature, ongoingEvent);

    if((clip == "Die") || (clip == "die"))
    {
        celebrateVictory(creature, false);
    }
    else if(clip == "Flee")
    {
        celebrateVictory(creature, true);
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
            mLastAttack[creature->getName()] = mTime;
        else
            noteRoomWork(creature);
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

    if(!isCreatureNearCamera(creature))
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

void CreatureReactions::noteCreatureUpdate(Creature* creature, uint32_t oldLevel, uint32_t oldMood, Seat* oldSeat,
        Seat* oldSeatPrison)
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

void CreatureReactions::stopAll()
{
    mPending.clear();
    mOngoing.clear();
    for(RunningReaction& reaction : mRunning)
        endReaction(reaction, mGameMap->getCreature(reaction.mCreatureName));

    mRunning.clear();
}
