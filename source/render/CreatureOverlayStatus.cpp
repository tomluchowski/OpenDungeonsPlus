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
#include "ODApplication.h"

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

//! The child overlays of a creature. The recovery overlay also shows the level caption.
enum class CreatureOverlays
{
    //! Health ring, also decides how long the whole overlay is displayed
    health,
    //! Experience ring
    experience,
    //! Attack recovery clock and level caption
    recovery,
    //! Mood symbol
    status,
    nbCreatureOverlays
};

//! The experience and recovery textures are square atlases with this many columns and rows.
const uint32_t PROGRESS_ATLAS_COLUMNS = 8;
//! Last frame of the atlases. Frame 0 shows no progress.
const uint32_t PROGRESS_ATLAS_LAST_FRAME = PROGRESS_ATLAS_COLUMNS * PROGRESS_ATLAS_COLUMNS - 1;
//! Character heights of the level caption, which gets smaller for two-digit levels.
const Ogre::Real LEVEL_CAPTION_SIZE_ONE_DIGIT = 32;
const Ogre::Real LEVEL_CAPTION_SIZE_TWO_DIGITS = 26;

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
    mMovableTextOverlay->forceTextArea(healthId, 64,64);
    mMovableTextOverlay->centerCaption(healthId);
    mMovableTextOverlay->displayOverlay(healthId, 0);

    uint32_t experienceId = mMovableTextOverlay->createChildOverlay("MedievalSharp", 30,
        Ogre::ColourValue::White, "CreatureExperience", false);
    mOverlayIds[static_cast<uint32_t>(CreatureOverlays::experience)] = experienceId;
    mMovableTextOverlay->forceTextArea(experienceId, 64, 64);
    mMovableTextOverlay->setAtlasFrame(experienceId, 0, PROGRESS_ATLAS_COLUMNS);

    uint32_t recoveryId = mMovableTextOverlay->createChildOverlay("MedievalSharp", 30,
        Ogre::ColourValue(1.0f, 0.91f, 0.66f, 1.0f), "CreatureRecovery", false);
    mOverlayIds[static_cast<uint32_t>(CreatureOverlays::recovery)] = recoveryId;
    mMovableTextOverlay->forceTextArea(recoveryId, 64, 64);
    mMovableTextOverlay->centerCaption(recoveryId);
    mMovableTextOverlay->setCaptionOutline(recoveryId, Ogre::ColourValue(0.04f, 0.04f, 0.04f, 1.0f));
    mMovableTextOverlay->setAtlasFrame(recoveryId, 0, PROGRESS_ATLAS_COLUMNS);
    mMovableTextOverlay->displayOverlay(recoveryId, -1);

    updateHealth();

    uint32_t statusId = mMovableTextOverlay->createChildOverlay("MedievalSharp", 16,
        Ogre::ColourValue::White, "", false);
    mOverlayIds[static_cast<uint32_t>(CreatureOverlays::status)] = statusId;
    mMovableTextOverlay->forceTextArea(statusId, 30,30);
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
        const uint32_t levelId = mOverlayIds[static_cast<uint32_t>(CreatureOverlays::recovery)];
        mMovableTextOverlay->setCaptionSize(levelId,
            mLevel < 10 ? LEVEL_CAPTION_SIZE_ONE_DIGIT : LEVEL_CAPTION_SIZE_TWO_DIGITS);
        if(mStatus == 0)
        {
            mMovableTextOverlay->setCaption(levelId, Helper::toString(mLevel));
        }
    }

}

void CreatureOverlayStatus::updateProgress(Ogre::Real timeSincelastFrame)
{
    const uint32_t experienceId = mOverlayIds[static_cast<uint32_t>(CreatureOverlays::experience)];
    const uint32_t recoveryId = mOverlayIds[static_cast<uint32_t>(CreatureOverlays::recovery)];
    const bool known = mCreature->hasProgressInformation();
    mMovableTextOverlay->displayOverlay(experienceId, known ? -1 : 0);
    mMovableTextOverlay->setAtlasFrame(experienceId,
        static_cast<uint32_t>(mCreature->getExperienceProgress() * PROGRESS_ATLAS_LAST_FRAME),
        PROGRESS_ATLAS_COLUMNS);
    const uint32_t duration = mCreature->getAttackRecoveryDuration();
    const uint32_t remaining = mCreature->getAttackRecoveryTurns();
    const uint32_t serial = mCreature->getAttackRecoverySerial();
    if(remaining != mRecoveryTurns || serial != mRecoverySerial)
    {
        mRecoveryTurns = remaining;
        mRecoverySerial = serial;
        mRecoveryElapsed = 0.0f;
    }
    else
        mRecoveryElapsed += timeSincelastFrame;
    // Smooth only within the reported turn; never announce readiness before the server.
    const double fraction = std::min(0.999, mRecoveryElapsed * ODApplication::turnsPerSecond);
    // Frame 0 means ready, the frames 1 to the last one show the elapsed part of the recovery.
    const double recoveryFrames = PROGRESS_ATLAS_LAST_FRAME - 1;
    const uint32_t frame = known && duration > 0 && remaining > 0 ?
        1 + static_cast<uint32_t>(recoveryFrames * (duration - remaining + fraction) / duration) : 0;
    mMovableTextOverlay->setAtlasFrame(recoveryId, frame, PROGRESS_ATLAS_COLUMNS);
}

void CreatureOverlayStatus::updateStatus(Ogre::Real timeSincelastFrame)
{
    uint32_t moodValue = mCreature->getOverlayMoodValue();
    if(mCreature->getMoodValue() == CreatureMoodLevel::Upset)
        moodValue |= CreatureMoodValues::Upset;

    uint32_t levelId = mOverlayIds[static_cast<uint32_t>(CreatureOverlays::recovery)];
    uint32_t statusId = mOverlayIds[static_cast<uint32_t>(CreatureOverlays::status)];
    if(moodValue == 0)
    {
        mStatus = 0;
        mTimeDisplayStatus = 0.0;
        mMovableTextOverlay->displayOverlay(statusId, 0);
        mMovableTextOverlay->setCaption(levelId, Helper::toString(mLevel));
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
        mMovableTextOverlay->setCaption(levelId, Helper::toString(mLevel));
        return;
    }

    mStatus = newStatus;
    mTimeDisplayStatus = 1.0;
    std::string material = CREATURE_OVERLAY_STATUS_PREFIX + Helper::toString(mStatus);
    mMovableTextOverlay->setMaterialName(statusId, material);
    mMovableTextOverlay->displayOverlay(statusId, -1);
    mMovableTextOverlay->setCaption(levelId, "");
}

void CreatureOverlayStatus::update(Ogre::Real timeSincelastFrame)
{
    updateHealth();

    updateProgress(timeSincelastFrame);

    // A creature with several moods to show takes them in turns, one second each. No time
    // passing means no turn taken: the labels are put back where the camera sees them while
    // the game is paused, and cycling then would run through the moods as fast as the game
    // draws.
    if(timeSincelastFrame > 0.0)
        updateStatus(timeSincelastFrame);

    mMovableTextOverlay->update(timeSincelastFrame);
    // Health, level and need symbols share the same request lifetime, including
    // temporary editor hover; hidden or dead creatures never show the overlay.
    const uint32_t healthId = mOverlayIds[static_cast<uint32_t>(CreatureOverlays::health)];
    mMovableTextOverlay->setVisible(mCreature->getIsOnMap() && mCreature->isAlive() &&
        mMovableTextOverlay->isDisplayed(healthId));
}
