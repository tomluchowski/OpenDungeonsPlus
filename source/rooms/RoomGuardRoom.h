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

#ifndef ROOMGUARDROOM_H
#define ROOMGUARDROOM_H

#include "rooms/Room.h"
#include "rooms/RoomType.h"

#include <map>

class Creature;
class Tile;
enum class TileVisual;

//! \brief A room where fighters stand guard. Each guard keeps to its own tile of the room
//! and only leaves it when it is hungry, sleepy, has to get its fee or has to fight. Enemies
//! seen by a guard are attacked through the normal creature behaviour.
class RoomGuardRoom: public Room
{
public:
    //! \brief Creates an empty guard room on the given map.
    RoomGuardRoom(GameMap* gameMap);

    //! \brief Returns the type of this room (RoomType::guardRoom).
    RoomType getType() const override
    { return mRoomType; }

    //! \brief One guard per room tile
    bool hasOpenCreatureSpot(Creature* c) override;
    //! \brief Frees the post of the creature when it stops using the room.
    void removeCreatureUsingRoom(Creature* c) override;
    //! \brief One duty turn of a guard: it walks to its post and stands there watching.
    //! \return Always false, the guard never continues with other room actions in the same turn.
    bool useRoom(Creature& creature, bool forced) override;

    static const RoomType mRoomType;
    static const TileVisual mRoomVisual;

private:
    //! \brief Returns the tile where the given creature stands guard, picking a free one if needed
    Tile* getPostForCreature(Creature& creature);

    std::map<Creature*, Tile*> mGuardPosts;
};

#endif // ROOMGUARDROOM_H
