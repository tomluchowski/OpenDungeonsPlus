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

#include "render/CreatureWeaponVisuals.h"

#include "entities/Creature.h"
#include "entities/Weapon.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "network/CosmeticEvent.h"
#include "render/CreatureCombatReactions.h"
#include "render/CreatureReactions.h"
#include "render/RenderManager.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"

#include <OgreBillboardSet.h>
#include <OgreBone.h>
#include <OgreEntity.h>
#include <OgreMeshManager.h>
#include <OgreQuaternion.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreSkeletonInstance.h>
#include <OgreTagPoint.h>
#include <OgreVector3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <vector>

namespace
{

//! Seconds between two looks at who is a shooter with an enemy near
const double TICK_INTERVAL = 0.25;
//! An enemy this close (world units) makes an archer take an arrow to the string
const double THREAT_RADIUS = 10.0;
//! The arrow stays on the string this long after a shot even without an enemy close by
const double ARMED_AFTER_SHOT = 5.0;
//! The arrow is gone this long after the shot, then a new one is taken or loaded
const double RELOAD_GAP = 0.3;
//! Seconds a new arrow or bolt needs to get into place, and a bow string to be drawn fully
const double RELOAD_TIME = 0.45;
const double DRAW_TIME = 1.6;
//! How far the drawn string is pulled back (in the frame of the bow model)
const double DRAW_DISTANCE = 0.10;

//! The arrow mesh: the long axis is y and the tip points to -y. The cross section is thin, so it is widened
//! the same way as the flying arrow (see RenderManager)
const char* ARROW_MESH = "ArrowProjectile";
const double ARROW_LENGTH_SCALE = 0.55;
const double BOLT_LENGTH_SCALE = 0.4;
const double ARROW_WIDTH_FACTOR = 3.0;
//! Bow model: the string is at z = -0.09 and the arrow flies to +z. Crossbow model: the bolt lies on the rail,
//! the prod is toward +z. The tail of the arrow mesh is at y = 0.265. These positions come from the vertex data
//! of the models; they have not been checked on screen.
const double BOW_STRING_Z = -0.09;
const double CROSSBOW_RAIL_Z = -0.05;
const double CROSSBOW_RAIL_X = 0.025;
const double ARROW_TAIL_Y = 0.265;

//! A blow with this share (per mille) of the damage or more is a strong one
const int32_t STRONG_BLOW = 800;
//! Delays after the event, in seconds (the blow lands a moment after the attack animation starts)
const double DODGE_DELAY = 0.25;
const double MISS_DELAY = 0.1;
const double SOFT_MEMORY = 1.0;
//! The last result of an attacker is remembered this long (seconds)
const double LAST_HIT_MEMORY = 10.0;
const uint32_t MAX_PENDING = 32;
const uint32_t MAX_LAST_HITS = 64;
const uint32_t MAX_TRAILS = 6;

//! Weapon trail
const double TRAIL_WINDOW = 0.5;
const double TRAIL_FADE = 0.35;
const double TRAIL_SAMPLE_INTERVAL = 0.03;
const uint32_t TRAIL_POINTS = 16;
const double TRAIL_SIZE = 0.28;
const std::string TRAIL_PREFIX = "CreatureWeaponTrail_";
const std::string ARROW_PREFIX = "CreatureWeaponArrow_";

//! \brief Returns the text with all letters in lower case
std::string toLower(const std::string& text)
{
    std::string result = text;
    for(std::string::iterator it = result.begin(); it != result.end(); ++it)
        *it = static_cast<char>(std::tolower(static_cast<unsigned char>(*it)));
    return result;
}

//! The bow or crossbow of a creature
struct ShooterWeapon
{
    ShooterWeapon() :
        mCrossbow(false),
        mHand("L"),
        mMeshName()
    {}

    bool mCrossbow;
    std::string mHand;
    std::string mMeshName;
};

//! \brief Looks for a bow or crossbow in the hands of the creature
//! \param result Receives the kind and the hand of the weapon
//! \return true if there is one
bool findShooterWeapon(const Creature* creature, ShooterWeapon& result)
{
    const Weapon* weapons[2] = {creature->getWeaponL(), creature->getWeaponR()};
    const char* hands[2] = {"L", "R"};
    for(uint32_t i = 0; i < 2; ++i)
    {
        if(weapons[i] == nullptr)
            continue;

        std::string mesh = toLower(weapons[i]->getMeshName());
        if(mesh.find("bow") == std::string::npos)
            continue;

        result.mCrossbow = (mesh.find("crossbow") != std::string::npos);
        result.mHand = hands[i];
        result.mMeshName = weapons[i]->getMeshName();
        return true;
    }
    return false;
}

//! \brief Returns the name of the scene entity of the weapon in the given hand ("L" or "R") of the creature
std::string weaponEntityName(const Creature* creature, const std::string& hand)
{
    return "Weapon_" + hand + "_" + creature->getName();
}

//! The arrow of an archer: its entity, the weapon it belongs to and the time of its last launch
struct Shooter
{
    Shooter() :
        mCrossbow(false),
        mMountOffset(Ogre::Vector3::ZERO),
        mMountRotation(Ogre::Quaternion::IDENTITY),
        mReleasedAt(-1000.0)
    {}

    std::string mArrowName;
    std::string mWeaponName;
    bool mCrossbow;
    //! Offset and rotation of the weapon model on its bone (the arrow sits in the frame of the model)
    Ogre::Vector3 mMountOffset;
    Ogre::Quaternion mMountRotation;
    //! Time of the last launch
    double mReleasedAt;
};

//! A reaction that is started a moment later (the blow lands after the attack animation began)
struct PendingReaction
{
    std::string mCreature;
    std::string mEvent;
    double mDue;
};

//! One sampled place of the weapon and the time since it was sampled
struct TrailPoint
{
    Ogre::Vector3 mPosition;
    double mAge;
};

//! The bright trail behind the weapon of one creature: its billboards and the sampled places
struct Trail
{
    Trail() :
        mElapsed(0.0),
        mSinceSample(1000.0)
    {}

    std::string mCreature;
    std::string mWeaponName;
    std::string mSetName;
    std::string mNodeName;
    double mElapsed;
    double mSinceSample;
    std::vector<TrailPoint> mPoints;
};

std::map<std::string, Shooter> sShooters;
std::map<std::string, double> sSoftened;
//! Last result of a blow or shot per attacker, as the server reported it (event hitResult)
std::map<std::string, CreatureWeaponVisuals::HitInfo> sLastHits;
//! True once the server has sent the event hitResult in this game
bool sHitEvents = false;
std::vector<PendingReaction> sPending;
std::vector<Trail> sTrails;
double sTickTimer = 0.0;
uint32_t sNextTrailId = 0;

//! \brief Queues a reaction of the creature that is shown after the delay
void queueReaction(CreatureReactions& reactions, const std::string& creatureName, const std::string& eventName,
        double delay)
{
    if(sPending.size() >= MAX_PENDING)
        return;

    PendingReaction pending;
    pending.mCreature = creatureName;
    pending.mEvent = eventName;
    pending.mDue = CreatureWeaponVisuals::getTime(reactions) + delay;
    sPending.push_back(pending);
}

//! \brief Returns true if a living creature of a seat that is not allied is within the radius
bool isEnemyNear(CreatureReactions& reactions, const Creature* creature, double radius)
{
    for(Creature* other : CreatureWeaponVisuals::getGameMap(reactions)->getCreatures())
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

//! Where the model of the weapon sits on the skeleton (see RenderManager::getWeaponMount)
bool weaponMount(const Ogre::Skeleton* skeleton, const ShooterWeapon& weapon, RenderManager::WeaponMount& mount)
{
    return RenderManager::getWeaponMount(skeleton, weapon.mHand, weapon.mMeshName, mount);
}

//! \brief Removes the arrow entity with that name from the scene
void destroyArrow(const std::string& arrowName)
{
    RenderManager* renderManager = RenderManager::getSingletonPtr();
    if(renderManager == nullptr)
        return;

    Ogre::SceneManager* sceneManager = renderManager->getSceneManager();
    if(!sceneManager->hasEntity(arrowName))
        return;

    Ogre::Entity* arrow = sceneManager->getEntity(arrowName);
    arrow->detachFromParent();
    sceneManager->destroyEntity(arrow);
}

//! Creates the arrow on the bone of the weapon. Returns false if it cannot be shown.
bool createArrow(CreatureReactions& reactions, Creature* creature, const ShooterWeapon& weapon, Shooter& shooter)
{
    Ogre::Entity* body = CreatureWeaponVisuals::getBody(reactions, creature);
    if((body == nullptr) || (body->getSkeleton() == nullptr))
        return false;

    RenderManager::WeaponMount mount;
    if(!weaponMount(body->getSkeleton(), weapon, mount))
        return false;

    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    std::string meshFile = std::string(ARROW_MESH) + ".mesh";
    if(!Ogre::MeshManager::getSingleton().resourceExists(meshFile, "Graphics"))
        Ogre::MeshManager::getSingleton().load(meshFile, "Graphics");

    Ogre::MeshPtr meshPtr = Ogre::MeshManager::getSingleton().getByName(meshFile, "Graphics");
    if(!meshPtr)
        return false;

    unsigned short src, dest;
    try
    {
        if(!meshPtr->suggestTangentVectorBuildParams(Ogre::VES_TANGENT, src, dest))
            meshPtr->buildTangentVectors(Ogre::VES_TANGENT, src, dest);
    }
    catch(const Ogre::ItemIdentityException&)
    {
        // No texture coordinates: the arrow is drawn without tangents
    }

    shooter.mArrowName = ARROW_PREFIX + creature->getName();
    if(sceneManager->hasEntity(shooter.mArrowName))
        destroyArrow(shooter.mArrowName);

    Ogre::Entity* arrow = sceneManager->createEntity(shooter.mArrowName, meshPtr);
    arrow->setCastShadows(false);

    // The arrow flies to +z of the weapon model: its tip (-y) is turned to +z
    Ogre::Quaternion arrowRotation(Ogre::Degree(-90.0), Ogre::Vector3::UNIT_X);
    body->attachObjectToBone(mount.mBoneName, arrow, mount.mRotation * arrowRotation);
    shooter.mWeaponName = weaponEntityName(creature, weapon.mHand);
    shooter.mCrossbow = weapon.mCrossbow;
    shooter.mMountOffset = mount.mOffset;
    shooter.mMountRotation = mount.mRotation;
    return true;
}

//! Puts the arrow where it belongs for the time since the shot
void placeArrow(CreatureReactions& reactions, Shooter& shooter)
{
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    if(!sceneManager->hasEntity(shooter.mArrowName))
        return;

    Ogre::Entity* arrow = sceneManager->getEntity(shooter.mArrowName);
    Ogre::TagPoint* tagPoint = dynamic_cast<Ogre::TagPoint*>(arrow->getParentNode());
    if(tagPoint == nullptr)
    {
        arrow->setVisible(false);
        return;
    }

    // The arrow is only there while the weapon is out
    bool weaponOut = sceneManager->hasEntity(shooter.mWeaponName) &&
        sceneManager->getEntity(shooter.mWeaponName)->isVisible();
    double sinceShot = CreatureWeaponVisuals::getTime(reactions) - shooter.mReleasedAt;
    if(!weaponOut || (sinceShot < RELOAD_GAP))
    {
        arrow->setVisible(false);
        return;
    }

    // How far the new arrow has come (0 to 1) after it was taken from the quiver or the bolt was loaded
    double arrived = std::min(1.0, (sinceShot - RELOAD_GAP) / RELOAD_TIME);
    double lengthScale = shooter.mCrossbow ? BOLT_LENGTH_SCALE : ARROW_LENGTH_SCALE;
    double tailOffset = ARROW_TAIL_Y * lengthScale;

    // Position in the frame of the weapon model: the arrow flies to +z
    Ogre::Vector3 position;
    if(shooter.mCrossbow)
    {
        // The bolt is pushed onto the rail from the side
        position = Ogre::Vector3(static_cast<Ogre::Real>(CROSSBOW_RAIL_X),
            static_cast<Ogre::Real>((1.0 - arrived) * 0.12),
            static_cast<Ogre::Real>(CROSSBOW_RAIL_Z + tailOffset));
    }
    else
    {
        // The string is drawn slowly while the archer aims
        double drawn = std::max(0.0, std::min(1.0, (sinceShot - RELOAD_GAP - RELOAD_TIME) / DRAW_TIME));
        position = Ogre::Vector3(0.0f, 0.0f,
            static_cast<Ogre::Real>(BOW_STRING_Z + tailOffset - drawn * DRAW_DISTANCE));
    }

    tagPoint->setPosition(shooter.mMountOffset + shooter.mMountRotation * position);
    Ogre::Real length = static_cast<Ogre::Real>(lengthScale * arrived);
    Ogre::Real width = static_cast<Ogre::Real>(ARROW_WIDTH_FACTOR) * length;
    tagPoint->setScale(width, length, width);
    arrow->setVisible(arrived > 0.02);
}

//! \brief Removes the arrows of all archers
void removeAllArrows()
{
    for(std::map<std::string, Shooter>::iterator it = sShooters.begin(); it != sShooters.end(); ++it)
        destroyArrow(it->second.mArrowName);

    sShooters.clear();
}

//! \brief Removes the billboards and the node of a trail from the scene
void destroyTrail(Trail& trail)
{
    RenderManager* renderManager = RenderManager::getSingletonPtr();
    if(renderManager == nullptr)
        return;

    Ogre::SceneManager* sceneManager = renderManager->getSceneManager();
    if(sceneManager->hasBillboardSet(trail.mSetName))
        sceneManager->destroyBillboardSet(trail.mSetName);

    if(sceneManager->hasSceneNode(trail.mNodeName))
        sceneManager->destroySceneNode(trail.mNodeName);
}

//! \brief Starts the bright trail behind the weapon that the attacker strikes with, or lets a running trail go on
void startTrail(CreatureReactions& reactions, Creature* attacker)
{
    if(!reactions.isCreatureNearCamera(attacker))
        return;

    // The weapon that strikes: the first one that is shown and is no shield
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    const Weapon* weapons[2] = {attacker->getWeaponR(), attacker->getWeaponL()};
    const char* hands[2] = {"R", "L"};
    std::string weaponName;
    for(uint32_t i = 0; i < 2; ++i)
    {
        if(weapons[i] == nullptr)
            continue;

        if(toLower(weapons[i]->getMeshName()).find("shield") != std::string::npos)
            continue;

        std::string name = weaponEntityName(attacker, hands[i]);
        if(sceneManager->hasEntity(name) && sceneManager->getEntity(name)->isVisible())
        {
            weaponName = name;
            break;
        }
    }
    if(weaponName.empty())
        return;

    for(Trail& other : sTrails)
    {
        if(other.mCreature == attacker->getName())
        {
            // The blow before is still running: its trail goes on
            other.mElapsed = 0.0;
            return;
        }
    }

    if(sTrails.size() >= MAX_TRAILS)
        return;

    Trail trail;
    trail.mCreature = attacker->getName();
    trail.mWeaponName = weaponName;
    trail.mSetName = TRAIL_PREFIX + Helper::toString(sNextTrailId);
    trail.mNodeName = trail.mSetName + "_node";
    ++sNextTrailId;

    Ogre::BillboardSet* set = sceneManager->createBillboardSet(trail.mSetName, TRAIL_POINTS);
    set->setMaterialName("CreatureProp_Trail");
    set->setDefaultDimensions(static_cast<Ogre::Real>(TRAIL_SIZE), static_cast<Ogre::Real>(TRAIL_SIZE));
    set->setCastShadows(false);
    for(uint32_t i = 0; i < TRAIL_POINTS; ++i)
        set->createBillboard(Ogre::Vector3::ZERO, Ogre::ColourValue(1.0f, 1.0f, 1.0f, 0.0f));

    Ogre::SceneNode* node = sceneManager->getRootSceneNode()->createChildSceneNode(trail.mNodeName);
    node->attachObject(set);
    sTrails.push_back(trail);
}

//! \brief Samples the places of the weapons, fades the trails and removes the finished ones
void updateTrails(double timeSinceLastFrame)
{
    if(sTrails.empty())
        return;

    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    for(std::vector<Trail>::iterator it = sTrails.begin(); it != sTrails.end();)
    {
        Trail& trail = *it;
        trail.mElapsed += timeSinceLastFrame;
        trail.mSinceSample += timeSinceLastFrame;
        for(std::vector<TrailPoint>::iterator itPoint = trail.mPoints.begin(); itPoint != trail.mPoints.end(); ++itPoint)
            itPoint->mAge += timeSinceLastFrame;

        // Sample where the weapon is: the middle of the model
        if((trail.mElapsed < TRAIL_WINDOW) && (trail.mSinceSample >= TRAIL_SAMPLE_INTERVAL) &&
           sceneManager->hasEntity(trail.mWeaponName))
        {
            Ogre::Entity* weaponEntity = sceneManager->getEntity(trail.mWeaponName);
            Ogre::Node* parent = weaponEntity->getParentNode();
            if((parent != nullptr) && weaponEntity->isVisible())
            {
                Ogre::Vector3 scale = parent->_getDerivedScale();
                Ogre::Vector3 center = weaponEntity->getBoundingBox().getCenter();
                TrailPoint point;
                point.mPosition = parent->_getDerivedPosition() + parent->_getDerivedOrientation() *
                    Ogre::Vector3(center.x * scale.x, center.y * scale.y, center.z * scale.z);
                point.mAge = 0.0;
                trail.mPoints.push_back(point);
                trail.mSinceSample = 0.0;
                if(trail.mPoints.size() > TRAIL_POINTS)
                    trail.mPoints.erase(trail.mPoints.begin());
            }
        }

        bool finished = (trail.mElapsed >= TRAIL_WINDOW) &&
            (trail.mPoints.empty() || (trail.mPoints.front().mAge >= TRAIL_FADE));
        if(finished || !sceneManager->hasBillboardSet(trail.mSetName))
        {
            destroyTrail(trail);
            it = sTrails.erase(it);
            continue;
        }

        Ogre::BillboardSet* set = sceneManager->getBillboardSet(trail.mSetName);
        for(uint32_t i = 0; i < TRAIL_POINTS; ++i)
        {
            Ogre::Billboard* billboard = set->getBillboard(static_cast<unsigned short>(i));
            if(i < trail.mPoints.size())
            {
                const TrailPoint& point = trail.mPoints[i];
                double alpha = std::max(0.0, 1.0 - point.mAge / TRAIL_FADE) * 0.8;
                billboard->setPosition(point.mPosition);
                billboard->setColour(Ogre::ColourValue(1.0f, 1.0f, 1.0f, static_cast<Ogre::Real>(alpha)));
            }
            else
            {
                billboard->setColour(Ogre::ColourValue(1.0f, 1.0f, 1.0f, 0.0f));
            }
        }
        ++it;
    }
}

//! \brief Removes all trails at once
void removeAllTrails()
{
    for(std::vector<Trail>::iterator it = sTrails.begin(); it != sTrails.end(); ++it)
        destroyTrail(*it);

    sTrails.clear();
}

//! \brief A creature launched a missile: remembers the time so the next arrow comes in after the shot
void noteShot(CreatureReactions& reactions, const CosmeticEvent& event)
{
    // A magic missile has no mesh and no arrow
    if(event.mText.empty())
        return;

    Creature* shooter = CreatureWeaponVisuals::getGameMap(reactions)->getCreature(event.mSubject);
    if(shooter == nullptr)
        return;

    // The arrow leaves now: the missile entity that the server announced is the same arrow. An archer that
    // was not seen aiming (off screen) is remembered, its arrow is taken at the next look.
    sShooters[shooter->getName()].mReleasedAt = CreatureWeaponVisuals::getTime(reactions);
}

//! \brief A melee blow was reported: a weak result shows a dodge, a glance and a miss, a strong one starts the trail
void noteBlow(CreatureReactions& reactions, const CosmeticEvent& event)
{
    Creature* target = CreatureWeaponVisuals::getGameMap(reactions)->getCreature(event.mObject);
    Creature* attacker = CreatureWeaponVisuals::getGameMap(reactions)->getCreature(event.mSubject);

    if(event.mValue >= 1)
    {
        // The blow did (almost) nothing: the target is not shown flinching
        sSoftened[event.mObject] = CreatureWeaponVisuals::getTime(reactions);
        // A server that sends hitResult drives these reactions from it
        if(sHitEvents)
            return;

        if(target != nullptr)
            queueReaction(reactions, target->getName(), (event.mValue >= 2) ? "BlowDodged" : "BlowGlanced", DODGE_DELAY);

        if((attacker != nullptr) && (event.mValue >= 2))
            queueReaction(reactions, attacker->getName(), "BlowMissed", MISS_DELAY);

        return;
    }

    if((attacker != nullptr) && (event.mValue2 >= STRONG_BLOW))
        startTrail(reactions, attacker);
}

//! \brief The server reported what a blow or shot did: a hit makes the target flinch or stagger, a glancing blow
//! or a total block shows a glance or a dodge, and the attacker of a blow that got nothing overreaches
void noteHitResult(CreatureReactions& reactions, const CosmeticEvent& event)
{
    Creature* target = CreatureWeaponVisuals::getGameMap(reactions)->getCreature(event.mObject);
    Creature* attacker = CreatureWeaponVisuals::getGameMap(reactions)->getCreature(event.mSubject);
    double now = CreatureWeaponVisuals::getTime(reactions);

    // Readable for the looks that follow a strong hit
    CreatureWeaponVisuals::HitInfo info;
    info.mResult = event.mValue;
    info.mHealthPermille = event.mValue2;
    info.mStrong = (event.mValue == static_cast<int32_t>(CosmeticHitResult::hit)) &&
        (event.mValue2 >= static_cast<int32_t>(ConfigManager::getSingleton().getHitStrongShare() * 1000.0 + 0.5));
    info.mMissile = (event.mText == "missile");
    info.mTarget = event.mObject;
    info.mTime = now;
    if(sLastHits.size() < MAX_LAST_HITS || sLastHits.find(event.mSubject) != sLastHits.end())
        sLastHits[event.mSubject] = info;

    switch(event.mValue)
    {
        case static_cast<int32_t>(CosmeticHitResult::hit):
            // The target flinches, a strong hit makes it stagger
            if(target != nullptr)
                CreatureCombatReactions::noteHitEvent(reactions, attacker, target, info.mStrong, info.mMissile);
            break;
        case static_cast<int32_t>(CosmeticHitResult::glanced):
            sSoftened[event.mObject] = now;
            if(target != nullptr)
                queueReaction(reactions, target->getName(), "BlowGlanced", DODGE_DELAY);
            break;
        case static_cast<int32_t>(CosmeticHitResult::blocked):
        case static_cast<int32_t>(CosmeticHitResult::missed):
            // Nothing got through: the target gets out of the way, the attacker of a blow overreaches
            sSoftened[event.mObject] = now;
            if(target != nullptr)
                queueReaction(reactions, target->getName(), "BlowDodged", info.mMissile ? 0.0 : DODGE_DELAY);
            if((attacker != nullptr) && !info.mMissile)
                queueReaction(reactions, attacker->getName(), "BlowMissed", MISS_DELAY);
            break;
        case static_cast<int32_t>(CosmeticHitResult::dodged):
            // The server decided that the defender dodged: it gets out of the way, the attacker overreaches
            sSoftened[event.mObject] = now;
            if(target != nullptr)
                queueReaction(reactions, target->getName(), "BlowDodged", DODGE_DELAY);
            if(attacker != nullptr)
                queueReaction(reactions, attacker->getName(), "BlowMissed", MISS_DELAY);
            break;
        case static_cast<int32_t>(CosmeticHitResult::parried):
            // The server decided that the defender parried: it stops the blow with its weapon, the attacker is thrown back
            sSoftened[event.mObject] = now;
            if(target != nullptr)
                queueReaction(reactions, target->getName(), "BlowParried", DODGE_DELAY);
            if(attacker != nullptr)
                queueReaction(reactions, attacker->getName(), "BlowDeflected", MISS_DELAY);
            break;
        default:
            // A result this client does not know (a newer server): nothing is shown
            break;
    }
}

//! \brief Shows the delayed reactions that are due
void processPending(CreatureReactions& reactions)
{
    for(std::vector<PendingReaction>::iterator it = sPending.begin(); it != sPending.end();)
    {
        if(it->mDue > CreatureWeaponVisuals::getTime(reactions))
        {
            ++it;
            continue;
        }

        PendingReaction pending = *it;
        it = sPending.erase(it);
        CreatureWeaponVisuals::show(reactions, CreatureWeaponVisuals::getGameMap(reactions)->getCreature(pending.mCreature),
            pending.mEvent);
    }
}

//! \brief Looks at all archers: takes an arrow to the string while an enemy is near and puts it away when not
void tick(CreatureReactions& reactions)
{
    // Forget the old results
    for(std::map<std::string, CreatureWeaponVisuals::HitInfo>::iterator it = sLastHits.begin(); it != sLastHits.end();)
    {
        if((CreatureWeaponVisuals::getTime(reactions) - it->second.mTime) > LAST_HIT_MEMORY)
            sLastHits.erase(it++);
        else
            ++it;
    }
    for(std::map<std::string, double>::iterator it = sSoftened.begin(); it != sSoftened.end();)
    {
        if((CreatureWeaponVisuals::getTime(reactions) - it->second) > SOFT_MEMORY)
            sSoftened.erase(it++);
        else
            ++it;
    }

    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    for(Creature* creature : CreatureWeaponVisuals::getGameMap(reactions)->getCreatures())
    {
        if(!creature->getIsOnMap() || !creature->isAlive())
            continue;

        ShooterWeapon weapon;
        if(!findShooterWeapon(creature, weapon))
            continue;

        std::map<std::string, Shooter>::iterator it = sShooters.find(creature->getName());
        bool known = (it != sShooters.end());
        std::string weaponName = weaponEntityName(creature, weapon.mHand);
        bool weaponOut = sceneManager->hasEntity(weaponName) && sceneManager->getEntity(weaponName)->isVisible();
        double lastShot = known ? it->second.mReleasedAt : -1000.0;
        bool armed = ((CreatureWeaponVisuals::getTime(reactions) - lastShot) < ARMED_AFTER_SHOT) || isEnemyNear(reactions, creature, THREAT_RADIUS);

        if(!reactions.isCreatureNearCamera(creature) || !weaponOut || !armed)
        {
            // Nothing to aim at (or nobody looks): the arrow goes back into the quiver
            if(known && !it->second.mArrowName.empty())
            {
                destroyArrow(it->second.mArrowName);
                it->second.mArrowName.clear();
            }
            continue;
        }

        if(!known)
            it = sShooters.insert(std::make_pair(creature->getName(), Shooter())).first;

        if(it->second.mArrowName.empty() || !sceneManager->hasEntity(it->second.mArrowName))
        {
            // A new arrow is taken: it comes in as after a shot
            double shotTime = it->second.mReleasedAt;
            if(createArrow(reactions, creature, weapon, it->second))
                it->second.mReleasedAt = std::max(shotTime, CreatureWeaponVisuals::getTime(reactions) - RELOAD_GAP);
            else
                it->second.mArrowName.clear();
        }
    }

    // Archers that are gone lose the arrow
    for(std::map<std::string, Shooter>::iterator it = sShooters.begin(); it != sShooters.end();)
    {
        if(CreatureWeaponVisuals::getGameMap(reactions)->getCreature(it->first) == nullptr)
        {
            destroyArrow(it->second.mArrowName);
            sShooters.erase(it++);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace

bool CreatureWeaponVisuals::noteCosmeticEvent(CreatureReactions& reactions, const CosmeticEvent& event)
{
    bool result = event.is(CosmeticEventType::hitResult);
    bool blow = event.is(CosmeticEventType::meleeResult);
    bool shot = event.is(CosmeticEventType::missileLaunch);
    if(!result && !blow && !shot)
        return false;

    // From now on the hits, dodges and misses come from the server and are not guessed
    if(result)
        sHitEvents = true;

    if(!CreatureWeaponVisuals::isActive(reactions))
        return true;

    if(result)
        noteHitResult(reactions, event);
    else if(blow)
        noteBlow(reactions, event);
    else
        noteShot(reactions, event);

    return true;
}

void CreatureWeaponVisuals::update(CreatureReactions& reactions, double timeSinceLastFrame)
{
    if(!CreatureWeaponVisuals::isActive(reactions))
    {
        // The arrows and trails are removed when the reactions are reduced or off
        if(!sShooters.empty() || !sTrails.empty() || !sPending.empty())
            stopAll(reactions);

        return;
    }

    processPending(reactions);
    updateTrails(timeSinceLastFrame);

    sTickTimer += timeSinceLastFrame;
    if(sTickTimer >= TICK_INTERVAL)
    {
        sTickTimer = 0.0;
        tick(reactions);
    }

    for(std::map<std::string, Shooter>::iterator it = sShooters.begin(); it != sShooters.end(); ++it)
    {
        if(!it->second.mArrowName.empty())
            placeArrow(reactions, it->second);
    }
}

void CreatureWeaponVisuals::stopAll(CreatureReactions& reactions)
{
    (void)reactions;
    removeAllArrows();
    removeAllTrails();
    sSoftened.clear();
    sLastHits.clear();
    sHitEvents = false;
    sPending.clear();
    sTickTimer = 0.0;
}

bool CreatureWeaponVisuals::getLastHit(const std::string& attackerName, HitInfo& info)
{
    std::map<std::string, HitInfo>::const_iterator it = sLastHits.find(attackerName);
    if(it == sLastHits.end())
        return false;

    info = it->second;
    return true;
}

bool CreatureWeaponVisuals::hasHitEvents()
{
    return sHitEvents;
}

bool CreatureWeaponVisuals::wasBlowSoftened(const std::string& targetName, double now)
{
    std::map<std::string, double>::const_iterator it = sSoftened.find(targetName);
    return (it != sSoftened.end()) && ((now - it->second) < SOFT_MEMORY);
}

bool CreatureWeaponVisuals::isActive(const CreatureReactions& reactions)
{
    return (reactions.mMode == CreatureReactions::Mode::full) && reactions.mConfigLoaded;
}

double CreatureWeaponVisuals::getTime(const CreatureReactions& reactions)
{
    return reactions.mTime;
}

GameMap* CreatureWeaponVisuals::getGameMap(const CreatureReactions& reactions)
{
    return reactions.mGameMap;
}

Ogre::Entity* CreatureWeaponVisuals::getBody(const CreatureReactions& reactions, const Creature* creature)
{
    return reactions.getCreatureEntity(creature);
}

bool CreatureWeaponVisuals::show(CreatureReactions& reactions, Creature* creature, const std::string& eventName)
{
    if((creature == nullptr) || !creature->isAlive() || !creature->getIsOnMap())
        return false;

    if(!reactions.isCreatureNearCamera(creature))
        return false;

    // The death touches and the reactions in the hand are not replaced
    CreatureReactions::RunningReaction* running = reactions.findRunning(creature->getName());
    if((running != nullptr) && (running->mDying || running->mInHand))
        return false;

    return reactions.trigger(creature, eventName, true);
}
