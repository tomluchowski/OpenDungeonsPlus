/*
 *  Copyright (C) 2011-2017  OpenDungeons Team
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

#ifndef CREATUREACTIVITY_H
#define CREATUREACTIVITY_H

#include "creatureaction/CreatureAction.h"
#include "rooms/RoomType.h"

//! Snapshot of existing AI state for creature-panel views, without changing AI rules.
struct CreatureActivity
{
    bool known = false;
    // nb means no action, and is only meaningful when known is true.
    CreatureActionType action = CreatureActionType::nb;
    CreatureActionType task = CreatureActionType::nb;
    RoomType assignedRoom = RoomType::nullRoomType;
    bool inAssignedRoom = false;

    bool operator==(const CreatureActivity& other) const
    {
        return known == other.known && action == other.action && task == other.task &&
            assignedRoom == other.assignedRoom && inAssignedRoom == other.inAssignedRoom;
    }
};

#endif // CREATUREACTIVITY_H
