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

#include "render/WeaponTrail.h"

#include "entities/Creature.h"
#include "entities/Weapon.h"
#include "render/CreatureReactionConfig.h"
#include "render/CreatureReactions.h"
#include "render/CreatureWeaponVisuals.h"
#include "render/RenderManager.h"
#include "utils/Helper.h"

#include <OgreAxisAlignedBox.h>
#include <OgreBillboardChain.h>
#include <OgreColourValue.h>
#include <OgreEntity.h>
#include <OgreNode.h>
#include <OgreQuaternion.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreVector3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

namespace WeaponTrail
{
namespace
{
//! The streak follows the tip from this long before the blow lands until this long after it
const double SWING_LEAD = 0.2;
const double SWING_LAG = 0.08;
//! Points of the path kept at most (the billboard chain has this many elements)
const uint32_t MAX_POINTS = 20;
//! Tip moves shorter than this (world units) between two frames are not added
const double MIN_STEP = 0.01;
const std::string TRAIL_PREFIX = "WeaponTrail_";
//! The existing additive glow material of the reactions (vertex colour, no depth write)
const std::string TRAIL_MATERIAL = "ReactionParticleGlow";

struct TrailPoint
{
    TrailPoint() :
        mPosition(Ogre::Vector3::ZERO),
        mAge(0.0)
    {}

    Ogre::Vector3 mPosition;
    double mAge;
};

struct Trail
{
    Trail() :
        mWait(0.0),
        mSamplingLeft(0.0)
    {}

    std::string mCreature;
    std::string mWeaponName;
    std::string mChainName;
    std::string mNodeName;
    //! Seconds until the weapon tip is followed
    double mWait;
    //! Seconds the weapon tip is still followed once the wait is over
    double mSamplingLeft;
    std::vector<TrailPoint> mPoints;
};

std::vector<Trail> sTrails;
uint32_t sNextTrailId = 0;

//! \brief Returns the text with all letters in lower case
std::string toLower(const std::string& text)
{
    std::string result = text;
    for(std::string::iterator it = result.begin(); it != result.end(); ++it)
        *it = static_cast<char>(std::tolower(static_cast<unsigned char>(*it)));

    return result;
}

//! Name of the entity of the weapon in a hand (the same name the weapons are created with)
std::string weaponEntityName(const Creature* creature, const std::string& hand)
{
    return "Weapon_" + hand + "_" + creature->getName();
}

//! The weapon that strikes: the one in the hand of the blow (the left hand only when the blow was shown with the left
//! arm, see RenderManager::isLastBlowLeftHanded, else the right), then the other one, that is shown and is no shield,
//! bow or staff
std::string findStrikingWeapon(const Creature* attacker)
{
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    const bool leftBlow = RenderManager::getSingleton().isLastBlowLeftHanded(attacker);
    const Weapon* weapons[2] = {attacker->getWeaponR(), attacker->getWeaponL()};
    const char* hands[2] = {"R", "L"};
    if(leftBlow)
    {
        std::swap(weapons[0], weapons[1]);
        std::swap(hands[0], hands[1]);
    }
    for(uint32_t i = 0; i < 2; ++i)
    {
        if(weapons[i] == nullptr)
            continue;

        std::string mesh = toLower(weapons[i]->getMeshName());
        if((mesh.find("shield") != std::string::npos) || (mesh.find("bow") != std::string::npos) ||
           (mesh.find("staff") != std::string::npos))
        {
            continue;
        }

        std::string name = weaponEntityName(attacker, hands[i]);
        if(sceneManager->hasEntity(name) && sceneManager->getEntity(name)->isVisible())
            return name;
    }
    return std::string();
}

//! The tip of the blade in the world: the end of the longest side of the bounding box of the weapon model that is
//! farther from the grip (the origin of the model, where it sits on the weapon bone)
bool getTipPosition(Ogre::Entity* weaponEntity, Ogre::Vector3& tip)
{
    Ogre::Node* parent = weaponEntity->getParentNode();
    if(parent == nullptr)
        return false;

    const Ogre::AxisAlignedBox& box = weaponEntity->getBoundingBox();
    if(box.isNull() || box.isInfinite())
        return false;

    Ogre::Vector3 size = box.getSize();
    int axis = 0;
    if(size.y > size[axis])
        axis = 1;
    if(size.z > size[axis])
        axis = 2;

    Ogre::Vector3 local = box.getCenter();
    local[axis] = (std::fabs(box.getMaximum()[axis]) >= std::fabs(box.getMinimum()[axis])) ?
        box.getMaximum()[axis] : box.getMinimum()[axis];

    Ogre::Vector3 scale = parent->_getDerivedScale();
    tip = parent->_getDerivedPosition() + parent->_getDerivedOrientation() *
        Ogre::Vector3(local.x * scale.x, local.y * scale.y, local.z * scale.z);
    return true;
}

//! \brief Removes the billboard chain and the node of a streak from the scene
void destroyTrail(Trail& trail)
{
    RenderManager* renderManager = RenderManager::getSingletonPtr();
    if(renderManager == nullptr)
        return;

    Ogre::SceneManager* sceneManager = renderManager->getSceneManager();
    if(sceneManager == nullptr)
        return;

    if(sceneManager->hasBillboardChain(trail.mChainName))
        sceneManager->destroyBillboardChain(trail.mChainName);

    if(sceneManager->hasSceneNode(trail.mNodeName))
        sceneManager->destroySceneNode(trail.mNodeName);
}

//! The streak in the scene: oldest point first, the colour fades with the age of the point, the end gets thin
void fillChain(Ogre::BillboardChain* chain, const Trail& trail, const CreatureReactionConfig& config)
{
    chain->clearChain(0);
    if(trail.mPoints.size() < 2)
        return;

    double life = config.getWeaponTrailLife();
    double width = config.getWeaponTrailWidth();
    double brightness = config.getWeaponTrailBrightness();
    size_t count = trail.mPoints.size();
    for(size_t i = 0; i < count; ++i)
    {
        const TrailPoint& point = trail.mPoints[i];
        double fade = std::max(0.0, 1.0 - point.mAge / life);
        // Added to the screen: the brightness is in the colour
        double value = fade * brightness;
        Ogre::ColourValue colour(static_cast<Ogre::Real>(config.getWeaponTrailRed() * value),
            static_cast<Ogre::Real>(config.getWeaponTrailGreen() * value),
            static_cast<Ogre::Real>(config.getWeaponTrailBlue() * value), static_cast<Ogre::Real>(value));
        Ogre::Real texCoord = static_cast<Ogre::Real>(i) / static_cast<Ogre::Real>(count - 1);
        chain->addChainElement(0, Ogre::BillboardChain::Element(point.mPosition,
            static_cast<Ogre::Real>(width * (0.3 + 0.7 * fade)), texCoord, colour, Ogre::Quaternion::IDENTITY));
    }
}

//! Adds the tip to the path and cuts what is too long
void sampleTip(Trail& trail, Ogre::Entity* weaponEntity, const CreatureReactionConfig& config)
{
    Ogre::Vector3 tip;
    if(!getTipPosition(weaponEntity, tip))
        return;

    if(!trail.mPoints.empty() && (trail.mPoints.back().mPosition.distance(tip) < MIN_STEP))
        return;

    TrailPoint point;
    point.mPosition = tip;
    trail.mPoints.push_back(point);
    if(trail.mPoints.size() > MAX_POINTS)
        trail.mPoints.erase(trail.mPoints.begin());

    // Only the last part of the path is shown
    double length = 0.0;
    size_t keep = trail.mPoints.size();
    for(size_t i = trail.mPoints.size() - 1; i > 0; --i)
    {
        length += trail.mPoints[i].mPosition.distance(trail.mPoints[i - 1].mPosition);
        if(length > config.getWeaponTrailLength())
        {
            keep = trail.mPoints.size() - i;
            break;
        }
    }
    if(keep < trail.mPoints.size())
        trail.mPoints.erase(trail.mPoints.begin(), trail.mPoints.end() - keep);
}
} // namespace

void noteStrongBlow(CreatureReactions& reactions, Creature* attacker, double delay)
{
    const CreatureReactionConfig& config = reactions.getConfig();
    if((attacker == nullptr) || !config.getWeaponTrail() || !CreatureWeaponVisuals::isActive(reactions))
        return;

    // Only what the server reported: a strong hit of a melee blow, strong enough for the trail
    CreatureWeaponVisuals::HitInfo info;
    if(!CreatureWeaponVisuals::getLastHit(attacker->getName(), info))
        return;

    if(!info.mStrong || info.mMissile ||
       (info.mHealthPermille < static_cast<int32_t>(config.getWeaponTrailMinShare() * 1000.0 + 0.5)))
    {
        return;
    }

    if(!reactions.isCreatureNearCamera(attacker))
        return;

    std::string weaponName = findStrikingWeapon(attacker);
    if(weaponName.empty())
        return;

    double wait = std::max(0.0, delay - SWING_LEAD);
    for(std::vector<Trail>::iterator it = sTrails.begin(); it != sTrails.end(); ++it)
    {
        if(it->mCreature == attacker->getName())
        {
            // The blow before is still running: the streak starts again
            it->mWeaponName = weaponName;
            it->mWait = wait;
            it->mSamplingLeft = SWING_LEAD + SWING_LAG;
            return;
        }
    }

    if(sTrails.size() >= config.getWeaponTrailMax())
        return;

    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    Trail trail;
    trail.mCreature = attacker->getName();
    trail.mWeaponName = weaponName;
    trail.mChainName = TRAIL_PREFIX + Helper::toString(sNextTrailId);
    trail.mNodeName = trail.mChainName + "_node";
    ++sNextTrailId;
    trail.mWait = wait;
    trail.mSamplingLeft = SWING_LEAD + SWING_LAG;

    Ogre::BillboardChain* chain = sceneManager->createBillboardChain(trail.mChainName);
    chain->setMaxChainElements(MAX_POINTS);
    chain->setNumberOfChains(1);
    chain->setMaterialName(TRAIL_MATERIAL);
    chain->setCastShadows(false);
    Ogre::SceneNode* node = sceneManager->getRootSceneNode()->createChildSceneNode(trail.mNodeName);
    node->attachObject(chain);
    sTrails.push_back(trail);
}

void update(CreatureReactions& reactions, double timeSinceLastFrame)
{
    if(sTrails.empty())
        return;

    const CreatureReactionConfig& config = reactions.getConfig();
    if(!config.getWeaponTrail() || !CreatureWeaponVisuals::isActive(reactions))
    {
        stopAll();
        return;
    }

    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    double life = config.getWeaponTrailLife();
    for(std::vector<Trail>::iterator it = sTrails.begin(); it != sTrails.end();)
    {
        Trail& trail = *it;
        if(trail.mWait > 0.0)
        {
            trail.mWait -= timeSinceLastFrame;
            ++it;
            continue;
        }

        trail.mSamplingLeft -= timeSinceLastFrame;
        for(std::vector<TrailPoint>::iterator itPoint = trail.mPoints.begin(); itPoint != trail.mPoints.end(); ++itPoint)
            itPoint->mAge += timeSinceLastFrame;

        while(!trail.mPoints.empty() && (trail.mPoints.front().mAge >= life))
            trail.mPoints.erase(trail.mPoints.begin());

        if((trail.mSamplingLeft > 0.0) && sceneManager->hasEntity(trail.mWeaponName))
        {
            Ogre::Entity* weaponEntity = sceneManager->getEntity(trail.mWeaponName);
            if(weaponEntity->isVisible())
                sampleTip(trail, weaponEntity, config);
        }

        bool finished = (trail.mSamplingLeft <= 0.0) && trail.mPoints.empty();
        if(finished || !sceneManager->hasBillboardChain(trail.mChainName))
        {
            destroyTrail(trail);
            it = sTrails.erase(it);
            continue;
        }

        fillChain(sceneManager->getBillboardChain(trail.mChainName), trail, config);
        ++it;
    }
}

void removeCreature(const std::string& creatureName)
{
    for(std::vector<Trail>::iterator it = sTrails.begin(); it != sTrails.end();)
    {
        if(creatureName.empty() || (it->mCreature == creatureName))
        {
            destroyTrail(*it);
            it = sTrails.erase(it);
        }
        else
            ++it;
    }
}

void stopAll()
{
    removeCreature(std::string());
}
} // namespace WeaponTrail
