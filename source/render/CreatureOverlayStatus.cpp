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

#include "render/CreatureOverlayStatus.h"

#include "creaturemood/CreatureMood.h"
#include "entities/Creature.h"
#include "entities/CreatureMoodValues.h"
#include "game/Seat.h"
#include "render/MovableTextOverlay.h"
#include "render/RenderManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <OgreEntity.h>

const std::string CREATURE_OVERLAY_STATUS_PREFIX = "CreatureOverlayStatus_";

enum class CreatureOverlays
{
    health,
    status,
    nbCreatureOverlays
};

CreatureOverlayStatus::CreatureOverlayStatus(Creature* creature, Ogre::Entity* ent,
        Ogre::Camera* cam) :
    mCreature(creature),
    mSeat(nullptr),
    mMovableTextOverlay(nullptr),
    mHealthValue(0),
    mLevel(0),
    mTimeDisplayStatus(0),
    mStatus(0),
    mOverlayIds(std::vector<uint32_t>(static_cast<uint32_t>(CreatureOverlays::nbCreatureOverlays), 0))
{
    mMovableTextOverlay = new MovableTextOverlay(creature->getName(),
        ent, cam);

    uint32_t healthId = mMovableTextOverlay->createChildOverlay("MedievalSharp", 30,
        Ogre::ColourValue(0.04f, 0.04f, 0.04f, 1.0f), "");
    mOverlayIds[static_cast<uint32_t>(CreatureOverlays::health)] = healthId;
    mMovableTextOverlay->setCaption(healthId, Helper::toString(creature->getLevel()));
    mMovableTextOverlay->forceTextArea(healthId, 48,48);
    mMovableTextOverlay->centerCaption(healthId);
    mMovableTextOverlay->displayOverlay(healthId, 0);

    updateHealth();

    uint32_t statusId = mMovableTextOverlay->createChildOverlay("MedievalSharp", 16,
        Ogre::ColourValue::White, "", false);
    mOverlayIds[static_cast<uint32_t>(CreatureOverlays::status)] = statusId;
    mMovableTextOverlay->forceTextArea(statusId, 22,22);
    // Note: We set the material to the first status overlay material otherwise, materials
    // are not shown when we change then ingame
    mMovableTextOverlay->setMaterialName(statusId, CREATURE_OVERLAY_STATUS_PREFIX + "1");
    mMovableTextOverlay->displayOverlay(statusId, 0);

}

CreatureOverlayStatus::~CreatureOverlayStatus()
{
    delete mMovableTextOverlay;
}

void CreatureOverlayStatus::displayHealthOverlay(Ogre::Real timeToDisplay)
{
    uint32_t healthId = mOverlayIds[static_cast<uint32_t>(CreatureOverlays::health)];
    mMovableTextOverlay->displayOverlay(healthId, timeToDisplay);
}

void CreatureOverlayStatus::updateHealth()
{
    // We adapt the material
    if((mHealthValue != mCreature->getOverlayHealthValue()) ||
        (mSeat != mCreature->getSeat()))
    {
        mHealthValue = mCreature->getOverlayHealthValue();
        mSeat = mCreature->getSeat();
        std::string material = RenderManager::getSingleton().rrBuildSkullFlagMaterial(
            "CreatureOverlay" + Helper::toString(mHealthValue),
            mSeat->getColorValue());
        uint32_t healthId = mOverlayIds[static_cast<uint32_t>(CreatureOverlays::health)];
        mMovableTextOverlay->setMaterialName(healthId, material);
    }

    if(mLevel != mCreature->getLevel())
    {
        mLevel = mCreature->getLevel();
        if(mStatus == 0)
        {
            uint32_t healthId = mOverlayIds[static_cast<uint32_t>(CreatureOverlays::health)];
            mMovableTextOverlay->setCaption(healthId, Helper::toString(mLevel));
        }
    }
}

void CreatureOverlayStatus::updateStatus(Ogre::Real timeSincelastFrame)
{
    uint32_t moodValue = mCreature->getOverlayMoodValue();
    if(mCreature->getMoodValue() == CreatureMoodLevel::Upset)
        moodValue |= CreatureMoodValues::Upset;

    uint32_t healthId = mOverlayIds[static_cast<uint32_t>(CreatureOverlays::health)];
    uint32_t statusId = mOverlayIds[static_cast<uint32_t>(CreatureOverlays::status)];
    if(moodValue == 0)
    {
        mStatus = 0;
        mTimeDisplayStatus = 0.0;
        mMovableTextOverlay->displayOverlay(statusId, 0);
        mMovableTextOverlay->setCaption(healthId, Helper::toString(mLevel));
        return;
    }

    if(mTimeDisplayStatus > timeSincelastFrame &&
       (mStatus == 0 || (mStatus & moodValue) != 0))
    {
        mTimeDisplayStatus -= timeSincelastFrame;
        return;
    }

    uint32_t newStatus = mStatus == 0 ? 1 : mStatus << 1;
    while(newStatus != 0 && newStatus <= moodValue && (newStatus & moodValue) == 0)
        newStatus <<= 1;

    if(newStatus == 0 || newStatus > moodValue)
    {
        mStatus = 0;
        mTimeDisplayStatus = 1.0;
        mMovableTextOverlay->displayOverlay(statusId, 0);
        mMovableTextOverlay->setCaption(healthId, Helper::toString(mLevel));
        return;
    }

    mStatus = newStatus;
    mTimeDisplayStatus = 1.0;
    std::string material = CREATURE_OVERLAY_STATUS_PREFIX + Helper::toString(mStatus);
    mMovableTextOverlay->setMaterialName(statusId, material);
    mMovableTextOverlay->displayOverlay(statusId, -1);
    mMovableTextOverlay->setCaption(healthId, "");
}

void CreatureOverlayStatus::update(Ogre::Real timeSincelastFrame)
{
    // If the creature is not on map, we do not display the overlays. Neither do we over a
    // dead one: it lies there for a few turns before it is taken away, and its level and its
    // mood are of no interest by then.
    mMovableTextOverlay->setVisible(mCreature->getIsOnMap() && mCreature->isAlive());

    updateHealth();

    // A creature with several moods to show takes them in turns, one second each. No time
    // passing means no turn taken: the labels are put back where the camera sees them while
    // the game is paused, and cycling then would run through the moods as fast as the game
    // draws.
    if(timeSincelastFrame > 0.0)
        updateStatus(timeSincelastFrame);

    mMovableTextOverlay->update(timeSincelastFrame);
}
