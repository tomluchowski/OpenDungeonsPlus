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

#include "render/WorkerExtras.h"

#include "entities/Creature.h"
#include "gamemap/GameMap.h"
#include "sound/SoundEffectsManager.h"

#include <OgreMath.h>
#include <OgreQuaternion.h>
#include <OgreSceneNode.h>
#include <OgreVector3.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <vector>

namespace
{

const double PI_VALUE = 3.14159265358979323846;
//! Least time (seconds) between two worker sounds
const double SOUND_INTERVAL = 0.1;
//! How far the carried body moves (node units) and how far it tilts (radians) at full strength
const double STRUGGLE_SWAY = 0.05;
const double STRUGGLE_BOB = 0.07;
const double STRUGGLE_TILT = 0.45;
//! Seconds between two changes of strength and beat, least and most
const double RETARGET_MIN = 0.8;
const double RETARGET_MAX = 2.2;
//! Beat of the struggling (radians per second), least and most
const double BEAT_MIN = 7.0;
const double BEAT_MAX = 17.0;
//! How fast strength and beat follow their new values (per second)
const double FOLLOW_SPEED = 3.0;
//! The squared distance (node units) and the angle (radians) up to which a node counts as left the way we put it
const double UNTOUCHED_DISTANCE_SQUARED = 0.00000001;
const float UNTOUCHED_ANGLE = 0.001f;
//! The chance (0 to 1) that a new strength is a quiet one, and the ranges of a quiet and of a strong strength
const double QUIET_CHANCE = 0.25;
const double QUIET_MIN = 0.1;
const double QUIET_MAX = 0.25;
const double STRONG_MIN = 0.5;
const double STRONG_MAX = 1.0;
//! The sway sideways, the tilt over the long side and the second sway axis follow the beat at these shares
const double SWAY_BEAT_SHARE = 0.6;
const double TILT_BEAT_SHARE = 0.8;
//! The share of the sway and of the tilt that the second axis gets, and its beat and phase against the first axis
const double SECOND_AXIS_SHARE = 0.6;
const double SWAY_SECOND_BEAT = 0.7;
const double SWAY_SECOND_PHASE = 1.0;
const double TILT_SECOND_BEAT = 1.3;
const double TILT_SECOND_PHASE = 0.5;
//! The time that counts as "no sound played yet"
const double NEVER_PLAYED = -1000.0;

//! \brief Returns the random generator of the worker extras (its own, so the game logic never sees its numbers)
std::mt19937& extrasRng()
{
    static std::mt19937 rng(std::random_device{}());
    return rng;
}

//! \brief Returns a random number between min and max from the cosmetic generator
double extrasRandom(double min, double max)
{
    std::uniform_real_distribution<double> distribution(min, max);
    return distribution(extrasRng());
}

//! What the client remembers of one body that is carried and struggles
struct Struggle
{
    Struggle() :
        mApplied(false),
        mRetarget(0.0),
        mStrength(0.0),
        mStrengthTarget(0.0),
        mBeat(BEAT_MIN),
        mBeatTarget(BEAT_MIN),
        mPhaseBob(0.0),
        mPhaseSway(0.0),
        mPhaseTilt(0.0)
    {}

    std::string mCarrierName;
    //! The node was moved by us at least once, the following four fields are valid
    bool mApplied;
    Ogre::Vector3 mBasePosition;
    Ogre::Quaternion mBaseOrientation;
    //! What we left in the node the last time, to see whether someone else changed it since
    Ogre::Vector3 mLastPosition;
    Ogre::Quaternion mLastOrientation;
    double mRetarget;
    double mStrength;
    double mStrengthTarget;
    double mBeat;
    double mBeatTarget;
    double mPhaseBob;
    double mPhaseSway;
    double mPhaseTilt;
};

std::map<std::string, Struggle> sStruggles;
double sLastSound = NEVER_PLAYED;

//! \brief Returns true if the node still is the way the struggling left it
bool isUntouched(const Ogre::SceneNode* node, const Struggle& struggle)
{
    return ((node->getPosition() - struggle.mLastPosition).squaredLength() < UNTOUCHED_DISTANCE_SQUARED) &&
        node->getOrientation().equals(struggle.mLastOrientation, Ogre::Radian(UNTOUCHED_ANGLE));
}

//! \brief Puts the node of the body back the way it was before the struggling, as far as nobody else changed it
//! meanwhile
void restoreBody(GameMap* gameMap, const std::string& bodyName, const Struggle& struggle)
{
    if(!struggle.mApplied)
        return;

    Creature* body = gameMap->getCreature(bodyName);
    if(body == nullptr)
        return;

    // Position and turn are put back each on its own: when the body is put down the game sets its position
    Ogre::SceneNode* node = body->getEntityNode();
    if(node == nullptr)
        return;

    if((node->getPosition() - struggle.mLastPosition).squaredLength() < UNTOUCHED_DISTANCE_SQUARED)
        node->setPosition(struggle.mBasePosition);

    if(node->getOrientation().equals(struggle.mLastOrientation, Ogre::Radian(UNTOUCHED_ANGLE)))
        node->setOrientation(struggle.mBaseOrientation);
}

}

//! \brief The body is picked up by the carrier: a creature that is still alive starts to struggle
void WorkerExtras::startStruggle(GameMap* gameMap, Creature* carrier, Creature* body)
{
    if((gameMap == nullptr) || (carrier == nullptr) || (body == nullptr) || !body->isAlive())
        return;

    endStruggle(gameMap, body->getName());

    Struggle struggle;
    struggle.mCarrierName = carrier->getName();
    struggle.mPhaseBob = extrasRandom(0.0, 2.0 * PI_VALUE);
    struggle.mPhaseSway = extrasRandom(0.0, 2.0 * PI_VALUE);
    struggle.mPhaseTilt = extrasRandom(0.0, 2.0 * PI_VALUE);
    struggle.mBeat = extrasRandom(BEAT_MIN, BEAT_MAX);
    struggle.mBeatTarget = struggle.mBeat;
    sStruggles[body->getName()] = struggle;
}

//! \brief The body is put down or lost: its node is put back the way it was and the struggle is forgotten
void WorkerExtras::endStruggle(GameMap* gameMap, const std::string& bodyName)
{
    std::map<std::string, Struggle>::iterator it = sStruggles.find(bodyName);
    if(it == sStruggles.end())
        return;

    if(gameMap != nullptr)
        restoreBody(gameMap, bodyName, it->second);

    sStruggles.erase(it);
}

//! \brief Moves the struggling bodies a little each frame and lets go of those that are put down, dead or moved by
//! someone else; with enabled false all of them are put back and forgotten
void WorkerExtras::update(GameMap* gameMap, double timeSinceLastFrame, bool enabled)
{
    if(sStruggles.empty())
        return;

    if(!enabled || (gameMap == nullptr))
    {
        stopAll(gameMap);
        return;
    }

    std::vector<std::string> finished;
    for(std::map<std::string, Struggle>::iterator it = sStruggles.begin(); it != sStruggles.end(); ++it)
    {
        Struggle& struggle = it->second;
        Creature* body = gameMap->getCreature(it->first);
        Creature* carrier = gameMap->getCreature(struggle.mCarrierName);
        Ogre::SceneNode* node = (body == nullptr) ? nullptr : body->getEntityNode();
        if((body == nullptr) || (node == nullptr) || !body->isAlive() || (carrier == nullptr) ||
           (carrier->getCarriedEntity() != body))
        {
            finished.push_back(it->first);
            continue;
        }

        if(!struggle.mApplied)
        {
            struggle.mBasePosition = node->getPosition();
            struggle.mBaseOrientation = node->getOrientation();
            struggle.mLastPosition = struggle.mBasePosition;
            struggle.mLastOrientation = struggle.mBaseOrientation;
            struggle.mApplied = true;
        }
        else if(!isUntouched(node, struggle))
        {
            // Someone else moved the body: it is theirs now, we let go of it
            struggle.mApplied = false;
            finished.push_back(it->first);
            continue;
        }

        // Strength and beat change by chance now and then: a quiet moment, a heave, a fit of shaking
        struggle.mRetarget -= timeSinceLastFrame;
        if(struggle.mRetarget <= 0.0)
        {
            struggle.mRetarget = extrasRandom(RETARGET_MIN, RETARGET_MAX);
            struggle.mStrengthTarget = (extrasRandom(0.0, 1.0) < QUIET_CHANCE) ?
                extrasRandom(QUIET_MIN, QUIET_MAX) : extrasRandom(STRONG_MIN, STRONG_MAX);
            struggle.mBeatTarget = extrasRandom(BEAT_MIN, BEAT_MAX);
        }

        double follow = std::min(1.0, timeSinceLastFrame * FOLLOW_SPEED);
        struggle.mStrength += (struggle.mStrengthTarget - struggle.mStrength) * follow;
        struggle.mBeat += (struggle.mBeatTarget - struggle.mBeat) * follow;
        struggle.mPhaseBob += struggle.mBeat * timeSinceLastFrame;
        struggle.mPhaseSway += struggle.mBeat * SWAY_BEAT_SHARE * timeSinceLastFrame;
        struggle.mPhaseTilt += struggle.mBeat * TILT_BEAT_SHARE * timeSinceLastFrame;

        double strength = struggle.mStrength;
        Ogre::Vector3 offset(
            static_cast<Ogre::Real>(STRUGGLE_SWAY * strength * std::sin(struggle.mPhaseSway)),
            static_cast<Ogre::Real>(STRUGGLE_SWAY * SECOND_AXIS_SHARE * strength *
                std::sin(struggle.mPhaseSway * SWAY_SECOND_BEAT + SWAY_SECOND_PHASE)),
            static_cast<Ogre::Real>(STRUGGLE_BOB * strength * std::fabs(std::sin(struggle.mPhaseBob))));
        double tiltX = STRUGGLE_TILT * strength * std::sin(struggle.mPhaseTilt);
        double tiltY = STRUGGLE_TILT * SECOND_AXIS_SHARE * strength *
        std::sin(struggle.mPhaseTilt * TILT_SECOND_BEAT + TILT_SECOND_PHASE);
        Ogre::Quaternion tilt = Ogre::Quaternion(Ogre::Radian(static_cast<Ogre::Real>(tiltX)), Ogre::Vector3::UNIT_X) *
            Ogre::Quaternion(Ogre::Radian(static_cast<Ogre::Real>(tiltY)), Ogre::Vector3::UNIT_Y);

        node->setPosition(struggle.mBasePosition + offset);
        node->setOrientation(tilt * struggle.mBaseOrientation);
        struggle.mLastPosition = node->getPosition();
        struggle.mLastOrientation = node->getOrientation();
    }

    for(const std::string& name : finished)
        endStruggle(gameMap, name);
}

//! \brief Puts all bodies back and forgets them
void WorkerExtras::stopAll(GameMap* gameMap)
{
    std::vector<std::string> names;
    for(std::map<std::string, Struggle>::const_iterator it = sStruggles.begin(); it != sStruggles.end(); ++it)
        names.push_back(it->first);

    for(const std::string& name : names)
        endStruggle(gameMap, name);

    sStruggles.clear();
    sLastSound = NEVER_PLAYED;
}

//! \brief Returns the sound family of the reaction event, an empty string if the event has no sound
std::string WorkerExtras::getSoundFamily(const std::string& eventName)
{
    static const std::string PREFIX = "Creatures/Worker/";
    if((eventName == "DigHitDirt") || (eventName == "DigFinishDirt"))
        return PREFIX + "DigDirt";
    if((eventName == "DigHitRock") || (eventName == "DigFinishRock"))
        return PREFIX + "DigRock";
    if((eventName == "DigHitGold") || (eventName == "DigFinishGold"))
        return PREFIX + "DigGold";
    if((eventName == "DigHitGem") || (eventName == "DigFinishGem"))
        return PREFIX + "DigGem";
    if((eventName == "GoldCloud") || (eventName == "TreasuryToss") || (eventName == "WorkerDeathCoins"))
        return PREFIX + "CoinJingle";
    if((eventName == "ClaimStomp") || (eventName == "ClaimStompEnemy"))
        return PREFIX + "Stomp";
    if((eventName == "ReinforceWork") || (eventName == "TrapKnock"))
        return PREFIX + "Knock";
    if(eventName == "GoldCheer")
        return PREFIX + "Cheer";

    return std::string();
}

//! \brief Plays the sound of the reaction event at the place of the creature, not more often than SOUND_INTERVAL
void WorkerExtras::playSound(const std::string& eventName, const Creature* creature, double now)
{
    if((creature == nullptr) || (SoundEffectsManager::getSingletonPtr() == nullptr))
        return;

    std::string family = getSoundFamily(eventName);
    if(family.empty() || ((now - sLastSound) < SOUND_INTERVAL))
        return;

    sLastSound = now;
    Ogre::Vector3 position = creature->getPosition();
    SoundEffectsManager::getSingleton().playSpatialSound(family, static_cast<float>(position.x),
        static_cast<float>(position.y));
}
