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

#ifndef MISSILESTONE_H
#define MISSILESTONE_H

#include "entities/MissileObject.h"

#include <string>
#include <iosfwd>

class GameMap;
class ODPacket;

//! \brief The portal stone of a level: an object that lies on a tile. A level script puts it
//! where the creature that carried it died, or where the quest ends. It is stored with the
//! missiles of a level because it never moves and needs nothing else.
class MissileStone: public MissileObject
{
public:
    //! \brief Creates a stone that lies still
    //! \param gameMap The game map the stone belongs to
    //! \param seat The seat the stone is registered for
    //! \param meshName The mesh that shows the stone
    MissileStone(GameMap* gameMap, Seat* seat, const std::string& meshName);
    //! \brief Creates an empty stone that is filled by reading a stream or a packet
    MissileStone(GameMap* gameMap);

    //! \brief The type that is written to saved games and packets
    virtual MissileObjectType getMissileType() const override
    { return MissileObjectType::stone; }

    //! \brief The stone is never removed after it stopped
    virtual bool staysWhenStopped() const override
    { return true; }

    //! \brief Reads a stone from a level or saved game stream
    static MissileStone* getMissileStoneFromStream(GameMap* gameMap, std::istream& is);
    //! \brief Reads a stone from a network packet
    static MissileStone* getMissileStoneFromPacket(GameMap* gameMap, ODPacket& is);
};

#endif // MISSILESTONE_H
