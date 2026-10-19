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

#ifndef CREATUREOVERLAYSTATUS_H
#define CREATUREOVERLAYSTATUS_H

#include <OgrePrerequisites.h>

#include <cstdint>
#include <string>
#include <vector>

class Creature;
class MovableTextOverlay;
class Seat;

namespace Ogre
{
    class Entity;
    class Camera;
}

//! \brief Displays creature status and keeps reaction emote lifetimes in game time.
class CreatureOverlayStatus
{
public:
    CreatureOverlayStatus(Creature* creature, Ogre::Entity* ent,
        Ogre::Camera* cam);
    ~CreatureOverlayStatus();

    void displayHealthOverlay(Ogre::Real timeToDisplay);
    //! \brief Shows the icon of the given material for timeToDisplay game seconds; negative means persistent.
    void showEmote(const std::string& materialName, Ogre::Real timeToDisplay);
    //! \brief Hides the emote icon, if one was shown
    //! \brief Hides the reaction icon and clears its game-time lifetime.
    void hideEmote();
    //! \brief Updates interface overlays in real time and reaction icon lifetimes in game time.
    void update(Ogre::Real timeSincelastFrame);
    MovableTextOverlay* getMovableTextOverlay(){ return mMovableTextOverlay; }
private:
    
    void updateHealth();
    void updateStatus(Ogre::Real timeSincelastFrame);

    bool mVisible;
    Creature* mCreature;
    Seat* mSeat;
    MovableTextOverlay* mMovableTextOverlay;
    uint32_t mHealthValue;
    unsigned int mLevel;
    Ogre::Real mTimeDisplayStatus;
    uint32_t mStatus;
    std::vector<uint32_t> mOverlayIds;
    //! Overlay of the emote. It is only created when the first emote is shown
    bool mEmoteCreated;
    uint32_t mEmoteId;
    //! Remaining game seconds; interface hover and health-overlay timers keep their own real-time clock.
    Ogre::Real mEmoteRemaining = 0.0f;
};

#endif // CREATUREOVERLAYSTATUS_H
