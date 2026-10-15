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

#ifndef ROOMAMBIENCEEXTRAS_H
#define ROOMAMBIENCEEXTRAS_H

#include <OgreVector3.h>

#include <cstdint>
#include <limits>
#include <map>
#include <string>

class GameMap;
class RoomAmbience;
class Seat;

/*! \brief Moments of the room ambience that are read from what the client already knows.
 *
 * Called by RoomAmbience after every scan. It only watches (the health stage and keeper of the creatures, the heart
 * health of the local player and the dungeon hearts on the map) and starts one-shot events of
 * config/roomAmbienceDeferred.cfg: TempleHealed (bell), ConvertedFlash, HeartHurt and HeartLowBeat. It also tells
 * RoomAmbience how fast each heart beats, for the effects marked "HeartRate". Nothing is sent, saved or changed in
 * the game.
 */
class RoomAmbienceExtras
{
public:
    //! \brief Creates the watcher; it has seen nothing yet
    RoomAmbienceExtras();

    //! \brief Looks at the map once: starts the events of the creatures and the heart and gives the heart rates
    //! \param ambience The ambience that receives the events and the heart rates.
    //! \param gameMap The map whose creatures and dungeon hearts are looked at.
    //! \param clock The time of the ambience in seconds.
    void scan(RoomAmbience& ambience, GameMap* gameMap, double clock);

    //! \brief Forgets what was seen (the next scan only learns, it starts no event)
    void reset();

private:
    //! \brief What was seen of a creature at the last scan
    struct CreatureSnapshot
    {
        //! \brief Creates a snapshot that is not set yet
        CreatureSnapshot() :
            mHealth(0), mSeat(nullptr), mPrisoner(false), mLastEvent(-std::numeric_limits<double>::max()), mGeneration(0)
        {}

        //! The health stage shown in the overlay of the creature
        uint32_t mHealth;
        //! The keeper the creature serves
        const Seat* mSeat;
        //! True while the creature is a prisoner
        bool mPrisoner;
        //! Clock time of the last event started for the creature (never at the start)
        double mLastEvent;
        //! Number of the scan that last saw the creature
        uint32_t mGeneration;
    };

    //! \brief Starts TempleHealed and ConvertedFlash for the creatures that changed
    void scanCreatures(RoomAmbience& ambience, GameMap* gameMap, double clock);
    //! \brief Gives the heart rate of the own dungeon heart and starts HeartHurt and HeartLowBeat
    void scanHearts(RoomAmbience& ambience, GameMap* gameMap, double clock);
    //! \brief True if the position lies on a tile of a temple
    bool isTempleTile(GameMap* gameMap, const Ogre::Vector3& position) const;

    //! What was seen of each creature at the last scan, by creature name
    std::map<std::string, CreatureSnapshot> mCreatures;
    //! Clock time of the next beat of a hurt heart
    double mNextHeartBeat;
    //! Number of the current scan
    uint32_t mGeneration;
    //! False until the first scan, so that nothing is started for the first look
    bool mInitialized;
};

#endif // ROOMAMBIENCEEXTRAS_H
