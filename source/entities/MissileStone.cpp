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

#include "entities/MissileStone.h"

#include "network/ODPacket.h"

#include <iostream>

MissileStone::MissileStone(GameMap* gameMap, Seat* seat, const std::string& meshName) :
    MissileObject(gameMap, seat, "PortalStone", meshName, Ogre::Vector3::ZERO, 0.0, nullptr, false, false)
{
    // The stone lies still
    stopMissile();
}

MissileStone::MissileStone(GameMap* gameMap) :
    MissileObject(gameMap)
{
}

MissileStone* MissileStone::getMissileStoneFromStream(GameMap* gameMap, std::istream& is)
{
    MissileStone* obj = new MissileStone(gameMap);
    obj->importFromStream(is);
    return obj;
}

MissileStone* MissileStone::getMissileStoneFromPacket(GameMap* gameMap, ODPacket& is)
{
    MissileStone* obj = new MissileStone(gameMap);
    obj->importFromPacket(is);
    return obj;
}
