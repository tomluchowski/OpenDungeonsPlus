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

#include "render/CreatureCombatReactions.h"

#include "ODApplication.h"
#include "creatureskill/CreatureSkill.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/Weapon.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "render/CreatureReactions.h"
#include "render/RenderManager.h"
#include "utils/Helper.h"

#include <OgreAnimationState.h>
#include <OgreAxisAlignedBox.h>
#include <OgreEntity.h>
#include <OgreParticleSystemManager.h>
#include <OgreQuaternion.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreVector3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <random>
#include <vector>

namespace
{

//! Seconds between two looks at the weapons and the stance of the fighters
const double TICK_INTERVAL = 0.5;
//! The attack animation has run this long when the blow gets its look (the direction is set by then)
const double ATTACK_LOOK_DELAY = 0.1;
//! Least time between two looks of the blows of one creature
const double STYLE_INTERVAL = 0.7;
//! Least time between two hit reactions of one creature (light and strong ones)
const double HIT_INTERVAL_LIGHT = 0.6;
const double HIT_INTERVAL_STRONG = 1.0;
//! Chance that a blow in front of a creature makes it flinch (the client cannot tell a miss from a hit)
const double FLINCH_CHANCE = 0.8;
//! Chance that a blow on a creature with a shield shows the sparks
const double SHIELD_SPARKS_CHANCE = 0.6;
//! Chance that a fighter with a sword and a shield bashes with the shield
const double SHIELD_BASH_CHANCE = 0.2;
//! The blow hits when this share of the attack animation has run (the damage itself is done by the server)
const double HIT_SHARE_OF_CLIP = 0.45;
const double HIT_DELAY_MIN = 0.12;
const double HIT_DELAY_MAX = 0.6;
//! A shot needs this much more time for each world unit it flies
const double HIT_DELAY_PER_UNIT = 0.07;
//! Reach of a blow in melee and of a shot, and the distance from which a creature with a shot counts as shooting
const double MELEE_RANGE = 2.8;
const double SHOT_RANGE = 9.0;
const double SHOT_MIN_DISTANCE = 2.3;
//! A target has to be this much in front of the attacker (cosine of the angle)
const double FRONT_COSINE = 0.3;
//! A creature that did not fight for this long and has no enemy close by puts its weapon away
const double SHEATHE_AFTER = 7.0;
//! An enemy this close (world units) makes a creature draw its weapon
const double THREAT_RADIUS = 10.0;
//! A creature that fought this lately and stands still takes its stance
const double STANCE_WINDOW = 4.0;
//! From this health stage on (0 is unhurt, 7 is dead) the stance is a defensive one
const uint32_t GUARD_STAGE = 5;
//! A creature that is hurt this much or more and has an enemy this close (world units) shows a hit even if no blow was seen
const double HIT_ENEMY_RADIUS = 12.0;
//! A blow seen this lately explains a worse health stage
const double HIT_MEMORY = 1.5;
//! Seconds of the sheathing animation until the weapon is hidden
const double SHEATHE_HIDE_AFTER = 0.55;
//! The weapon is let go this long after the death animation started
const double DROP_DELAY = 0.3;
//! Falling weapons: fall acceleration (world units per second squared), time on the floor and time to vanish
const double WEAPON_GRAVITY = 14.0;
const double WEAPON_LIE_TIME = 4.5;
const double WEAPON_VANISH_TIME = 0.7;
//! At most this many reactions run when a combat reaction is added (they are cheap, but a big fight has many)
const uint32_t MAX_RUNNING = 12;
const uint32_t MAX_PENDING = 64;
const double PI_VALUE = 3.14159265358979323846;

const std::string FALLEN_NODE_PREFIX = "CreatureCombatWeapon_";

//! Cosmetic dice of their own: the game logic must not see them
std::mt19937& combatRng()
{
    static std::mt19937 rng(std::random_device{}());
    return rng;
}

//! \brief Returns a cosmetic random number in [min, max] from the dice of this file
double combatRandom(double min, double max)
{
    std::uniform_real_distribution<double> distribution(min, max);
    return distribution(combatRng());
}

//! \brief Returns true if text begins with prefix
bool startsWith(const std::string& text, const std::string& prefix)
{
    return text.compare(0, prefix.size(), prefix) == 0;
}

//! \brief Returns the text with all letters in lower case
std::string toLower(const std::string& text)
{
    std::string result = text;
    for(std::string::iterator it = result.begin(); it != result.end(); ++it)
        *it = static_cast<char>(std::tolower(static_cast<unsigned char>(*it)));
    return result;
}

//! \brief Returns true if part occurs anywhere in text
bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

//! What the weapon is used for: Shield, Bow, Axe, Hammer, Spear, Dagger, Staff or Sword
std::string weaponKind(const Weapon* weapon)
{
    std::string mesh = toLower(weapon->getMeshName());
    if(contains(mesh, "shield"))
        return "Shield";

    if(contains(mesh, "bow"))
        return "Bow";

    if(contains(mesh, "axe"))
        return "Axe";

    if(contains(mesh, "hammer") || contains(mesh, "mace"))
        return "Hammer";

    if(contains(mesh, "spear") || contains(mesh, "lance") || contains(mesh, "pike"))
        return "Spear";

    if(contains(mesh, "dagger") || contains(mesh, "knife"))
        return "Dagger";

    if(contains(mesh, "staff") || contains(mesh, "wand"))
        return "Staff";

    return "Sword";
}

//! \brief Returns true if the creature carries a shield in one of its hands
bool hasShield(const Creature* creature)
{
    const Weapon* weapons[2] = {creature->getWeaponR(), creature->getWeaponL()};
    for(uint32_t i = 0; i < 2; ++i)
    {
        if((weapons[i] != nullptr) && (weaponKind(weapons[i]) == "Shield"))
            return true;
    }
    return false;
}

//! The kind of the weapon that is used to strike, empty if the creature has none
std::string mainWeaponKind(const Creature* creature)
{
    const Weapon* weapons[2] = {creature->getWeaponR(), creature->getWeaponL()};
    for(uint32_t i = 0; i < 2; ++i)
    {
        if(weapons[i] == nullptr)
            continue;

        std::string kind = weaponKind(weapons[i]);
        if(kind != "Shield")
            return kind;
    }
    return std::string();
}

//! \brief Returns true if the creature definition has the MissileLaunch skill (it shoots)
bool hasMissileSkill(const Creature* creature)
{
    const CreatureDefinition* definition = creature->getDefinition();
    if(definition == nullptr)
        return false;

    for(const CreatureSkill* skill : definition->getCreatureSkills())
    {
        if(skill->getSkillName() == "MissileLaunch")
            return true;
    }
    return false;
}

//! The game already shows sparks and blood when a weapon hits (a combat effect of its own)
bool hasExternalImpactEffects()
{
    return Ogre::ParticleSystemManager::getSingleton().getTemplate("CombatSparks") != nullptr;
}

//! \brief Returns the name of the scene entity of the weapon in the given hand ("L" or "R") of the creature
std::string weaponEntityName(const Creature* creature, const std::string& hand)
{
    return "Weapon_" + hand + "_" + creature->getName();
}

//! Per creature memory of the fight: when it struck, when it was struck and whether its weapons are put away
struct CombatState
{
    CombatState() :
        mLastAttack(0.0),
        mLastAttacked(0.0),
        mNextStyle(0.0),
        mNextHit(0.0),
        mSheathed(false)
    {}

    //! Time of the last blow of the creature and of the last blow on it (or its alarm)
    double mLastAttack;
    double mLastAttacked;
    double mNextStyle;
    double mNextHit;
    //! The weapons are put away (their models are hidden)
    bool mSheathed;
};

//! A blow that was just seen: after a moment the direction is known and the look of the blow is chosen
struct PendingAttack
{
    std::string mAttacker;
    double mDue;
    std::string mClip;
};

//! A creature that is hit: after a moment (the blow lands) it shows how hard it was
struct PendingHit
{
    std::string mTarget;
    double mDue;
    uint32_t mLevel;
    bool mShield;
};

//! A creature that died: after a moment its weapons are let go
struct PendingDrop
{
    std::string mCreature;
    double mDue;
};

//! A weapon that fell out of the hand of a dead creature
struct FallenWeapon
{
    FallenWeapon() :
        mElapsed(0.0),
        mFallTime(0.2),
        mScale(Ogre::Vector3::UNIT_SCALE),
        mLocalCenter(Ogre::Vector3::ZERO),
        mStartCenter(Ogre::Vector3::ZERO),
        mEndCenter(Ogre::Vector3::ZERO)
    {}

    std::string mEntityName;
    std::string mNodeName;
    double mElapsed;
    double mFallTime;
    Ogre::Vector3 mScale;
    //! The middle of the model in its own coordinates
    Ogre::Vector3 mLocalCenter;
    Ogre::Vector3 mStartCenter;
    Ogre::Vector3 mEndCenter;
    Ogre::Quaternion mStartOrientation;
    Ogre::Quaternion mEndOrientation;
};

std::map<std::string, CombatState> sStates;
std::vector<PendingAttack> sAttacks;
std::vector<PendingHit> sHits;
std::vector<PendingDrop> sDrops;
std::vector<FallenWeapon> sFallen;
double sTickTimer = 0.0;
uint32_t sNextFallenId = 0;

//! \brief Returns the combat state of the creature with that name and creates it (calm, as of now) if it is new
CombatState& getState(const std::string& name, double now)
{
    std::map<std::string, CombatState>::iterator it = sStates.find(name);
    if(it == sStates.end())
    {
        // New creatures start calm: they put the weapon away after the usual time
        CombatState state;
        state.mLastAttack = now;
        state.mLastAttacked = now;
        it = sStates.insert(std::make_pair(name, state)).first;
    }
    return it->second;
}

//! \brief Multiplies the vector component by component with the scale
Ogre::Vector3 scaleVector(const Ogre::Vector3& value, const Ogre::Vector3& scale)
{
    return Ogre::Vector3(value.x * scale.x, value.y * scale.y, value.z * scale.z);
}

} // namespace

//! \brief Returns true if the reactions are switched to full and their configuration is loaded
bool CreatureCombatReactions::isActive(const CreatureReactions& reactions)
{
    return (reactions.mMode == CreatureReactions::Mode::full) && reactions.mConfigLoaded;
}

//! \brief Starts the reaction on the creature if it is alive, near the camera and not busy with a death touch or a hand reaction
//! \return true if the reaction was started
bool CreatureCombatReactions::show(CreatureReactions& reactions, Creature* creature, const std::string& eventName)
{
    if((creature == nullptr) || !creature->isAlive() || !creature->getIsOnMap())
        return false;

    if(!reactions.isCreatureNearCamera(creature))
        return false;

    if(reactions.getNbRunning() >= MAX_RUNNING)
        return false;

    // The death touches and the reactions in the hand are not replaced
    CreatureReactions::RunningReaction* running = reactions.findRunning(creature->getName());
    if((running != nullptr) && (running->mDying || running->mInHand))
        return false;

    Ogre::AnimationState* animState = creature->getAnimationState();
    if((animState != nullptr) &&
       ((animState->getAnimationName() == "Die") || (animState->getAnimationName() == "die") ||
        (animState->getAnimationName() == "Rot")))
    {
        return false;
    }

    return reactions.trigger(creature, eventName, true);
}

//! \brief Looks for the closest living enemy in front of the attacker within the range
//! \param target Receives the enemy found
//! \param distance Receives the distance to it
//! \return true if there is one
bool CreatureCombatReactions::findTarget(CreatureReactions& reactions, Creature* attacker, double range,
        Creature*& target, double& distance)
{
    Ogre::SceneNode* node = attacker->getEntityNode();
    if(node == nullptr)
        return false;

    Ogre::Vector3 forward = node->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Y;
    forward.z = 0.0f;
    if(forward.length() < 0.01f)
        return false;

    forward.normalise();

    double best = range;
    bool found = false;
    for(Creature* other : reactions.mGameMap->getCreatures())
    {
        if((other == attacker) || !other->getIsOnMap() || !other->isAlive())
            continue;

        if(other->getSeat()->isAlliedSeat(attacker->getSeat()))
            continue;

        Ogre::Vector3 difference = other->getPosition() - attacker->getPosition();
        difference.z = 0.0f;
        double length = difference.length();
        if(length >= best)
            continue;

        // The creature has to be in front of the attacker
        if(length > 0.5)
        {
            Ogre::Vector3 direction = difference / static_cast<Ogre::Real>(length);
            if(forward.dotProduct(direction) < FRONT_COSINE)
                continue;
        }

        best = length;
        target = other;
        distance = length;
        found = true;
    }

    return found;
}

//! \brief Returns the name of the reaction that gives the blow its look, chosen by the weapon the attacker carries
std::string CreatureCombatReactions::chooseAttackEvent(const CreatureReactions& reactions, const Creature* attacker,
        bool ranged)
{
    (void)reactions;
    std::string kind = mainWeaponKind(attacker);
    bool shield = hasShield(attacker);

    if(ranged)
        return (kind == "Bow") ? "AttackBow" : "AttackCast";

    if(kind == "Sword")
        return (shield && (combatRandom(0.0, 1.0) < SHIELD_BASH_CHANCE)) ? "AttackShieldBash" : "AttackSword";

    if((kind == "Axe") || (kind == "Hammer"))
        return "AttackHeavy";

    if((kind == "Spear") || (kind == "Staff"))
        return "AttackSpear";

    if(kind == "Dagger")
        return "AttackDagger";

    // A bow used as a club, bare hands, claws and teeth
    if(kind.empty() && shield)
        return "AttackShieldBash";

    return "AttackUnarmed";
}

//! \brief Queues a hit reaction of the target (level 0 light, 1 and 2 strong) for the moment the blow lands;
//! a blow that is already on its way to the same target only gets stronger
void CreatureCombatReactions::scheduleHit(CreatureReactions& reactions, Creature* target, uint32_t level, bool shield,
        double delay)
{
    for(PendingHit& hit : sHits)
    {
        if(hit.mTarget == target->getName())
        {
            // The blow that is on its way gets stronger
            hit.mLevel = std::max(hit.mLevel, level);
            hit.mShield = hit.mShield && shield;
            return;
        }
    }

    if(sHits.size() >= MAX_PENDING)
        return;

    PendingHit hit;
    hit.mTarget = target->getName();
    hit.mDue = reactions.mTime + delay;
    hit.mLevel = level;
    hit.mShield = shield;
    sHits.push_back(hit);
}

void CreatureCombatReactions::noteAttack(CreatureReactions& reactions, Creature* attacker, const std::string& clip)
{
    if(!isActive(reactions))
        return;

    CombatState& state = getState(attacker->getName(), reactions.mTime);
    state.mLastAttack = reactions.mTime;

    // Whoever strikes has its weapon out
    draw(reactions, attacker, true);

    if(sAttacks.size() >= MAX_PENDING)
        return;

    PendingAttack attack;
    attack.mAttacker = attacker->getName();
    attack.mDue = reactions.mTime + ATTACK_LOOK_DELAY;
    attack.mClip = clip;
    sAttacks.push_back(attack);
}

void CreatureCombatReactions::noteAlarm(CreatureReactions& reactions, Creature* creature)
{
    if(!isActive(reactions))
        return;

    CombatState& state = getState(creature->getName(), reactions.mTime);
    state.mLastAttacked = reactions.mTime;
    draw(reactions, creature, true);
}

void CreatureCombatReactions::noteDeath(CreatureReactions& reactions, Creature* creature)
{
    if(!isActive(reactions) || (sDrops.size() >= MAX_PENDING))
        return;

    if((creature->getWeaponL() == nullptr) && (creature->getWeaponR() == nullptr))
        return;

    PendingDrop drop;
    drop.mCreature = creature->getName();
    drop.mDue = reactions.mTime + DROP_DELAY;
    sDrops.push_back(drop);
}

void CreatureCombatReactions::noteHealth(CreatureReactions& reactions, Creature* creature, uint32_t oldHealth)
{
    if(!isActive(reactions) || !creature->isAlive())
        return;

    uint32_t health = creature->getOverlayHealthValue();
    if(health <= oldHealth)
        return;

    // Only a hurt creature in a fight shows it (not a creature that is just loaded, or hurt by something far away)
    CombatState& state = getState(creature->getName(), reactions.mTime);
    bool recent = (reactions.mTime - state.mLastAttacked) < HIT_MEMORY;
    if(!recent && !isEnemyNear(reactions, creature, HIT_ENEMY_RADIUS))
        return;

    state.mLastAttacked = reactions.mTime;
    uint32_t level = ((health - oldHealth) >= 2) ? 2 : 1;
    scheduleHit(reactions, creature, level, false, 0.0);
}

//! \brief Gives the blows that were seen a moment ago their look and queues the hit on the creature in front
void CreatureCombatReactions::processAttacks(CreatureReactions& reactions)
{
    for(std::vector<PendingAttack>::iterator it = sAttacks.begin(); it != sAttacks.end();)
    {
        if(it->mDue > reactions.mTime)
        {
            ++it;
            continue;
        }

        PendingAttack attack = *it;
        it = sAttacks.erase(it);

        Creature* attacker = reactions.mGameMap->getCreature(attack.mAttacker);
        if((attacker == nullptr) || !attacker->isAlive() || !attacker->getIsOnMap())
            continue;

        bool shooter = hasMissileSkill(attacker);
        Creature* target = nullptr;
        double distance = 0.0;
        bool found = findTarget(reactions, attacker, shooter ? SHOT_RANGE : MELEE_RANGE, target, distance);
        bool ranged = shooter && found && (distance > SHOT_MIN_DISTANCE);

        // How long the blow takes, for the moment it lands
        double clipSeconds = 1.0;
        Ogre::AnimationState* animState = attacker->getAnimationState();
        double speed = ODApplication::turnsPerSecond * attacker->getAnimationSpeedFactor();
        if((animState != nullptr) && (speed > 0.1))
            clipSeconds = static_cast<double>(animState->getLength()) / speed;

        CombatState& state = getState(attacker->getName(), reactions.mTime);

        // The game has several attack animations of its own: only the single one gets a look per blow
        if((attack.mClip != "CombatAttack") && (reactions.mTime >= state.mNextStyle))
        {
            // The weapon that was just drawn is not cut off
            CreatureReactions::RunningReaction* running = reactions.findRunning(attacker->getName());
            if((running == nullptr) || (running->mEventName != "WeaponDraw") || (running->mElapsed > 0.45))
            {
                if(show(reactions, attacker, chooseAttackEvent(reactions, attacker, ranged)))
                    state.mNextStyle = reactions.mTime + STYLE_INTERVAL;
            }
        }

        if(!found)
            continue;

        CombatState& targetState = getState(target->getName(), reactions.mTime);
        targetState.mLastAttacked = reactions.mTime;
        draw(reactions, target, true);

        double delay = ranged ? (0.15 + distance * HIT_DELAY_PER_UNIT) :
            std::max(HIT_DELAY_MIN, std::min(HIT_DELAY_MAX, clipSeconds * HIT_SHARE_OF_CLIP));
        scheduleHit(reactions, target, 0, !ranged && hasShield(target), delay);
    }
}

//! \brief Shows the hit reactions whose blow has landed
void CreatureCombatReactions::processHits(CreatureReactions& reactions)
{
    for(std::vector<PendingHit>::iterator it = sHits.begin(); it != sHits.end();)
    {
        if(it->mDue > reactions.mTime)
        {
            ++it;
            continue;
        }

        PendingHit hit = *it;
        it = sHits.erase(it);

        Creature* target = reactions.mGameMap->getCreature(hit.mTarget);
        if((target == nullptr) || !target->isAlive() || !target->getIsOnMap())
            continue;

        CombatState& state = getState(target->getName(), reactions.mTime);
        if(reactions.mTime < state.mNextHit)
            continue;

        bool external = hasExternalImpactEffects();
        std::string eventName;
        if(hit.mLevel >= 2)
        {
            eventName = external ? "HitStagger" : "HitKnockback";
        }
        else if(hit.mLevel == 1)
        {
            eventName = "HitStagger";
        }
        else
        {
            // A blow in front of a creature: not every one shows (the server alone knows if it did damage)
            if(combatRandom(0.0, 1.0) >= FLINCH_CHANCE)
                continue;

            if(hit.mShield && !external && (combatRandom(0.0, 1.0) < SHIELD_SPARKS_CHANCE))
                eventName = "ShieldBlock";
            else
                eventName = "HitFlinch";
        }

        if(show(reactions, target, eventName))
            state.mNextHit = reactions.mTime + ((hit.mLevel > 0) ? HIT_INTERVAL_STRONG : HIT_INTERVAL_LIGHT);
    }
}

//! \brief Lets the weapons of the creatures that died fall: each model leaves the hand and moves to a node of its own
void CreatureCombatReactions::processDrops(CreatureReactions& reactions)
{
    for(std::vector<PendingDrop>::iterator it = sDrops.begin(); it != sDrops.end();)
    {
        if(it->mDue > reactions.mTime)
        {
            ++it;
            continue;
        }

        PendingDrop drop = *it;
        it = sDrops.erase(it);

        Creature* creature = reactions.mGameMap->getCreature(drop.mCreature);
        if((creature == nullptr) || !reactions.isCreatureNearCamera(creature))
            continue;

        Ogre::Entity* body = reactions.getCreatureEntity(creature);
        if((body == nullptr) || !body->isVisible())
            continue;

        Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
        const char* hands[2] = {"L", "R"};
        for(uint32_t i = 0; i < 2; ++i)
        {
            std::string entityName = weaponEntityName(creature, hands[i]);
            if(!sceneManager->hasEntity(entityName))
                continue;

            Ogre::Entity* weaponEntity = sceneManager->getEntity(entityName);
            if(!weaponEntity->isVisible() || (weaponEntity->getParentNode() == nullptr))
                continue;

            // Where the weapon is in the world right now
            Ogre::Node* parent = weaponEntity->getParentNode();
            FallenWeapon fallen;
            fallen.mEntityName = entityName;
            fallen.mNodeName = FALLEN_NODE_PREFIX + Helper::toString(sNextFallenId);
            ++sNextFallenId;
            fallen.mScale = parent->_getDerivedScale();
            fallen.mStartOrientation = parent->_getDerivedOrientation();

            Ogre::AxisAlignedBox box = weaponEntity->getBoundingBox();
            fallen.mLocalCenter = box.getCenter();
            Ogre::Vector3 size = box.getSize();
            fallen.mStartCenter = parent->_getDerivedPosition() +
                fallen.mStartOrientation * scaleVector(fallen.mLocalCenter, fallen.mScale);

            // On the floor the longest side of the model lies flat, turned by chance
            Ogre::Vector3 longAxis = Ogre::Vector3::UNIT_X;
            Ogre::Real longest = size.x;
            if(size.y > longest)
            {
                longest = size.y;
                longAxis = Ogre::Vector3::UNIT_Y;
            }
            if(size.z > longest)
            {
                longest = size.z;
                longAxis = Ogre::Vector3::UNIT_Z;
            }
            Ogre::Real thinnest = std::min(size.x, std::min(size.y, size.z));

            double yaw = combatRandom(0.0, 2.0 * PI_VALUE);
            Ogre::Vector3 flat(static_cast<Ogre::Real>(std::cos(yaw)), static_cast<Ogre::Real>(std::sin(yaw)), 0.0f);
            fallen.mEndOrientation = longAxis.getRotationTo(flat);

            // The middle of the model ends up half its thickness above the floor, a little aside
            double floorHeight = creature->getPosition().z;
            double halfThickness = 0.5 * static_cast<double>(thinnest) *
                static_cast<double>(std::min(fallen.mScale.x, std::min(fallen.mScale.y, fallen.mScale.z)));
            fallen.mEndCenter = Ogre::Vector3(
                fallen.mStartCenter.x + static_cast<Ogre::Real>(combatRandom(-0.3, 0.3)),
                fallen.mStartCenter.y + static_cast<Ogre::Real>(combatRandom(-0.3, 0.3)),
                static_cast<Ogre::Real>(floorHeight + halfThickness));
            double dropHeight = static_cast<double>(fallen.mStartCenter.z - fallen.mEndCenter.z);
            fallen.mFallTime = (dropHeight > 0.05) ? std::sqrt(2.0 * dropHeight / WEAPON_GRAVITY) : 0.15;

            // The model leaves the hand and moves to a node of its own in the same place
            weaponEntity->detachFromParent();
            Ogre::SceneNode* node = sceneManager->getRootSceneNode()->createChildSceneNode(fallen.mNodeName,
                parent->_getDerivedPosition());
            node->setOrientation(fallen.mStartOrientation);
            node->setScale(fallen.mScale);
            node->attachObject(weaponEntity);
            sFallen.push_back(fallen);
        }
    }
}

//! \brief Moves the fallen weapons along their fall, lets them lie and shrinks them away at the end
void CreatureCombatReactions::updateFallen(CreatureReactions& reactions, double timeSinceLastFrame)
{
    (void)reactions;
    if(sFallen.empty())
        return;

    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    for(std::vector<FallenWeapon>::iterator it = sFallen.begin(); it != sFallen.end();)
    {
        FallenWeapon& fallen = *it;
        fallen.mElapsed += timeSinceLastFrame;

        bool entityGone = !sceneManager->hasEntity(fallen.mEntityName);
        bool over = fallen.mElapsed >= (fallen.mFallTime + WEAPON_LIE_TIME + WEAPON_VANISH_TIME);
        if(entityGone || over || !sceneManager->hasSceneNode(fallen.mNodeName))
        {
            if(!entityGone)
            {
                Ogre::Entity* weaponEntity = sceneManager->getEntity(fallen.mEntityName);
                weaponEntity->detachFromParent();
                sceneManager->destroyEntity(weaponEntity);
            }
            if(sceneManager->hasSceneNode(fallen.mNodeName))
                sceneManager->destroySceneNode(fallen.mNodeName);

            it = sFallen.erase(it);
            continue;
        }

        Ogre::SceneNode* node = sceneManager->getSceneNode(fallen.mNodeName);
        double share = std::min(1.0, fallen.mElapsed / fallen.mFallTime);

        Ogre::Vector3 center = fallen.mEndCenter;
        center.x = fallen.mStartCenter.x + (fallen.mEndCenter.x - fallen.mStartCenter.x) * static_cast<Ogre::Real>(share);
        center.y = fallen.mStartCenter.y + (fallen.mEndCenter.y - fallen.mStartCenter.y) * static_cast<Ogre::Real>(share);
        if(fallen.mElapsed < fallen.mFallTime)
        {
            double height = static_cast<double>(fallen.mStartCenter.z) -
                0.5 * WEAPON_GRAVITY * fallen.mElapsed * fallen.mElapsed;
            center.z = static_cast<Ogre::Real>(std::max(height, static_cast<double>(fallen.mEndCenter.z)));
        }

        Ogre::Quaternion orientation = Ogre::Quaternion::Slerp(static_cast<Ogre::Real>(share),
            fallen.mStartOrientation, fallen.mEndOrientation, true);

        // The weapon shrinks away in the end
        Ogre::Vector3 scale = fallen.mScale;
        double vanishStart = fallen.mFallTime + WEAPON_LIE_TIME;
        if(fallen.mElapsed > vanishStart)
        {
            double remaining = 1.0 - (fallen.mElapsed - vanishStart) / WEAPON_VANISH_TIME;
            scale = scale * static_cast<Ogre::Real>(std::max(0.01, remaining));
        }

        node->setOrientation(orientation);
        node->setScale(scale);
        node->setPosition(center - orientation * scaleVector(fallen.mLocalCenter, scale));
        ++it;
    }
}

//! \brief Removes all fallen weapons from the scene at once
void CreatureCombatReactions::removeFallen(CreatureReactions& reactions)
{
    (void)reactions;
    RenderManager* renderManager = RenderManager::getSingletonPtr();
    Ogre::SceneManager* sceneManager = (renderManager != nullptr) ? renderManager->getSceneManager() : nullptr;
    if(sceneManager != nullptr)
    {
        for(FallenWeapon& fallen : sFallen)
        {
            if(sceneManager->hasEntity(fallen.mEntityName))
            {
                Ogre::Entity* weaponEntity = sceneManager->getEntity(fallen.mEntityName);
                weaponEntity->detachFromParent();
                sceneManager->destroyEntity(weaponEntity);
            }
            if(sceneManager->hasSceneNode(fallen.mNodeName))
                sceneManager->destroySceneNode(fallen.mNodeName);
        }
    }
    sFallen.clear();
}

//! \brief Returns true if a weapon model of the creature exists in the scene
bool CreatureCombatReactions::hasWeaponModel(CreatureReactions& reactions, const Creature* creature)
{
    (void)reactions;
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    return sceneManager->hasEntity(weaponEntityName(creature, "L")) ||
        sceneManager->hasEntity(weaponEntityName(creature, "R"));
}

//! \brief Shows or hides the weapon models of the creature
void CreatureCombatReactions::applyVisibility(CreatureReactions& reactions, const Creature* creature, bool visible)
{
    (void)reactions;
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    const char* hands[2] = {"L", "R"};
    for(uint32_t i = 0; i < 2; ++i)
    {
        std::string entityName = weaponEntityName(creature, hands[i]);
        if(sceneManager->hasEntity(entityName))
            sceneManager->getEntity(entityName)->setVisible(visible);
    }
}

//! \brief Remembers that the creature has put its weapons away (or drawn them) and applies it to the models
void CreatureCombatReactions::setSheathed(CreatureReactions& reactions, Creature* creature, bool sheathed)
{
    CombatState& state = getState(creature->getName(), reactions.mTime);
    state.mSheathed = sheathed;
    applyVisibility(reactions, creature, !sheathed);
}

//! \brief Takes the weapons of the creature out of the sheath; with announce a visible creature also plays the drawing flourish
void CreatureCombatReactions::draw(CreatureReactions& reactions, Creature* creature, bool announce)
{
    CombatState& state = getState(creature->getName(), reactions.mTime);
    if(!state.mSheathed)
        return;

    setSheathed(reactions, creature, false);
    if(!announce || !reactions.isCreatureNearCamera(creature))
        return;

    // Only a creature that does not show anything else draws with a flourish
    if(reactions.findRunning(creature->getName()) == nullptr)
        show(reactions, creature, "WeaponDraw");
}

//! \brief Returns true if a living creature of a seat that is not allied is within the radius
bool CreatureCombatReactions::isEnemyNear(CreatureReactions& reactions, const Creature* creature, double radius)
{
    for(Creature* other : reactions.mGameMap->getCreatures())
    {
        if((other == creature) || !other->getIsOnMap() || !other->isAlive())
            continue;

        if(other->getSeat()->isAlliedSeat(creature->getSeat()))
            continue;

        Ogre::Vector3 difference = other->getPosition() - creature->getPosition();
        difference.z = 0.0f;
        if(difference.length() <= radius)
            return true;
    }
    return false;
}

//! \brief Follows the sheathing and drawing reactions so the models are hidden or shown also when the event was started by hand
void CreatureCombatReactions::watchEvents(CreatureReactions& reactions)
{
    // The weapon is put away or drawn by the event too when it was started by hand (console)
    for(CreatureReactions::RunningReaction& running : reactions.mRunning)
    {
        bool sheathe = (running.mEventName == "WeaponSheathe") && (running.mElapsed >= SHEATHE_HIDE_AFTER);
        bool drawn = (running.mEventName == "WeaponDraw");
        if(!sheathe && !drawn)
            continue;

        Creature* creature = reactions.mGameMap->getCreature(running.mCreatureName);
        if(creature == nullptr)
            continue;

        CombatState& state = getState(creature->getName(), reactions.mTime);
        if(sheathe && !state.mSheathed)
            setSheathed(reactions, creature, true);
        else if(drawn && state.mSheathed)
            setSheathed(reactions, creature, false);
    }
}

//! \brief Looks at all creatures: puts the weapons of calm ones away, draws them when an enemy comes near and starts the stance between blows
void CreatureCombatReactions::tick(CreatureReactions& reactions)
{
    double now = reactions.mTime;

    // Creatures that are gone are forgotten
    for(std::map<std::string, CombatState>::iterator it = sStates.begin(); it != sStates.end();)
    {
        if(reactions.mGameMap->getCreature(it->first) == nullptr)
            sStates.erase(it++);
        else
            ++it;
    }

    for(Creature* creature : reactions.mGameMap->getCreatures())
    {
        if(!creature->getIsOnMap() || !creature->isAlive())
            continue;

        bool carriesWeapon = (creature->getWeaponL() != nullptr) || (creature->getWeaponR() != nullptr);
        std::map<std::string, CombatState>::iterator itState = sStates.find(creature->getName());
        if(!carriesWeapon && (itState == sStates.end()))
            continue;

        CombatState& state = getState(creature->getName(), now);
        double sinceFight = std::min(now - state.mLastAttack, now - state.mLastAttacked);
        Ogre::AnimationState* animState = creature->getAnimationState();
        bool idle = (animState != nullptr) && (animState->getAnimationName() == "Idle") && !creature->isMoving();

        // Drawing and sheathing: a creature with a weapon that is not fighting and has no enemy close by puts it away
        if(carriesWeapon && hasWeaponModel(reactions, creature))
        {
            bool threatened = (sinceFight < SHEATHE_AFTER) || isEnemyNear(reactions, creature, THREAT_RADIUS);
            if(state.mSheathed)
            {
                if(threatened)
                    draw(reactions, creature, true);
                else
                    applyVisibility(reactions, creature, false);
            }
            else if(!threatened && !creature->isMoving() && !reactions.isInHand(creature))
            {
                // The sheathing animation hides the weapon in the middle. Without one (nobody sees it, the
                // animation cannot be shown) the weapon is put away at once.
                CreatureReactions::RunningReaction* running = reactions.findRunning(creature->getName());
                if((running != nullptr) && (running->mEventName == "WeaponSheathe"))
                {
                    // On its way
                }
                else if(!reactions.isCreatureNearCamera(creature))
                {
                    setSheathed(reactions, creature, true);
                }
                else if(running == nullptr)
                {
                    if(!reactions.trigger(creature, "WeaponSheathe", false))
                        setSheathed(reactions, creature, true);
                }
                // else another reaction goes on: the weapon is put away at a later look
            }
        }
        else if(state.mSheathed)
        {
            state.mSheathed = false;
        }

        // Between two blows: a stance, a defensive one when badly hurt
        if(idle && ((now - state.mLastAttack) < STANCE_WINDOW))
        {
            bool guard = creature->getOverlayHealthValue() >= GUARD_STAGE;
            reactions.trigger(creature, guard ? "CombatGuard" : "CombatStance", false);
        }
    }
}

void CreatureCombatReactions::update(CreatureReactions& reactions, double timeSinceLastFrame)
{
    if(!isActive(reactions))
    {
        // The weapons are shown again and the fallen ones removed when the reactions are reduced or off
        if(!sStates.empty() || !sFallen.empty() || !sAttacks.empty() || !sHits.empty() || !sDrops.empty())
            stopAll(reactions);

        return;
    }

    processAttacks(reactions);
    processHits(reactions);
    processDrops(reactions);
    updateFallen(reactions, timeSinceLastFrame);
    watchEvents(reactions);

    sTickTimer += timeSinceLastFrame;
    if(sTickTimer >= TICK_INTERVAL)
    {
        sTickTimer = 0.0;
        tick(reactions);
    }
}

void CreatureCombatReactions::stopAll(CreatureReactions& reactions)
{
    RenderManager* renderManager = RenderManager::getSingletonPtr();
    if(renderManager != nullptr)
    {
        for(std::map<std::string, CombatState>::iterator it = sStates.begin(); it != sStates.end(); ++it)
        {
            if(!it->second.mSheathed)
                continue;

            Creature* creature = reactions.mGameMap->getCreature(it->first);
            if(creature != nullptr)
                applyVisibility(reactions, creature, true);
        }
    }

    removeFallen(reactions);
    sStates.clear();
    sAttacks.clear();
    sHits.clear();
    sDrops.clear();
    sTickTimer = 0.0;
}
