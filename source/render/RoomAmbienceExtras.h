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

class GameMap;
class RoomAmbience;

/*! \brief Moments of the room ambience that are read from what the client already knows.
 *
 * Called by RoomAmbience after every scan. It only watches (here: the heart health of the local player and the
 * dungeon hearts on the map) and tells RoomAmbience how fast each heart beats, for the effects marked "HeartRate".
 * Nothing is sent, saved or changed in the game.
 */
class RoomAmbienceExtras
{
public:
    //! \brief Looks at the map once and gives the heart rates to the ambience
    //! \param ambience The ambience that receives the heart rates.
    //! \param gameMap The map whose dungeon hearts are looked at.
    void scan(RoomAmbience& ambience, GameMap* gameMap);
};

#endif // ROOMAMBIENCEEXTRAS_H
