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

class CreatureOverlayStatus
{
public:
    CreatureOverlayStatus(Creature* creature, Ogre::Entity* ent,
        Ogre::Camera* cam);
    ~CreatureOverlayStatus();

    void displayHealthOverlay(Ogre::Real timeToDisplay);
    //! \brief Shows the icon of the given material above the creature for timeToDisplay seconds
    void showEmote(const std::string& materialName, Ogre::Real timeToDisplay);
    //! \brief Hides the emote icon, if one was shown
    void hideEmote();
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
};

#endif // CREATUREOVERLAYSTATUS_H
