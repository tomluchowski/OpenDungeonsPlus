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

#ifndef ROOMTEMPLE_H
#define ROOMTEMPLE_H

#include "rooms/Room.h"
#include "rooms/RoomType.h"

#include <map>

class Creature;
class Tile;
enum class TileVisual;

//! \brief A room where creatures pray. Each praying creature adds mana to its keeper and
//! feels better afterwards.
class RoomTemple: public Room
{
public:
    RoomTemple(GameMap* gameMap);

    RoomType getType() const override
    { return mRoomType; }

    //! \brief One praying creature per room tile
    bool hasOpenCreatureSpot(Creature* c) override;
    void removeCreatureUsingRoom(Creature* c) override;
    bool useRoom(Creature& creature, bool forced) override;

    //! \brief Praying relieves bad mood, so even unhappy creatures keep praying
    bool shouldNotUseIfBadMood(Creature& creature, bool forced) override
    { return false; }

    static const RoomType mRoomType;
    static const TileVisual mRoomVisual;

private:
    //! \brief Returns the tile where the given creature prays, picking a free one if needed
    Tile* getPrayerSpotForCreature(Creature& creature);

    std::map<Creature*, Tile*> mPrayerSpots;
};

#endif // ROOMTEMPLE_H
