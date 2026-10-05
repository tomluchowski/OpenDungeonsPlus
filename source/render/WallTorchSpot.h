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

#ifndef WALLTORCHSPOT_H
#define WALLTORCHSPOT_H

#include <cstdint>

//! \brief Side of a wall tile on which a wall torch hangs: the direction from the wall tile to the open tile it faces.
enum class WallTorchSide
{
    //! The open tile is at y + 1
    north,
    //! The open tile is at y - 1
    south,
    //! The open tile is at x + 1
    east,
    //! The open tile is at x - 1
    west
};

/*! \brief One wall torch as the server decided it: the reinforced wall tile and the side it hangs on.
 *
 * The client derives nothing: the list of these spots, sent by the server, is shown as it is
 * (see RoomAmbience::setWallTorchSpots).
 */
struct WallTorchSpot
{
    WallTorchSpot() :
        mX(0), mY(0), mSide(WallTorchSide::north)
    {}

    WallTorchSpot(int32_t x, int32_t y, WallTorchSide side) :
        mX(x), mY(y), mSide(side)
    {}

    int32_t mX;
    int32_t mY;
    WallTorchSide mSide;
};

#endif // WALLTORCHSPOT_H
