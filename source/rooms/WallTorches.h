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

#ifndef WALLTORCHES_H
#define WALLTORCHES_H

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

//! \brief A torch that hangs on a reinforced wall tile. The server computes the torches (see
//! WallTorches::compute) and sends them to the clients, which only show them.
struct WallTorch
{
    //! Tile of the wall that carries the torch
    int32_t mX;
    int32_t mY;
    //! Side the torch faces, see WallTorches::getDirX/getDirY: the open tile is at wall + direction
    int32_t mDir;
    //! Seat that owns the wall
    int32_t mSeatId;
};

//! \brief What the placement needs to know about one tile. The server fills it from the map.
struct WallTorchTileInfo
{
    //! \brief Creates the info of a tile that is neither a wall nor part of a room nor open
    WallTorchTileInfo() :
        mWallSeatId(-1),
        mWallTeamId(-1),
        mRoomIndex(-1),
        mOpenTeamId(-1),
        mBlocked(false)
    {}

    //! Seat id of the owner when the tile is a reinforced wall, else -1
    int32_t mWallSeatId;
    //! Team id of that owner
    int32_t mWallTeamId;
    //! Index of the room that covers the tile (rooms are numbered by their first tile, row by row), else -1
    int32_t mRoomIndex;
    //! Team id of the claimant when the tile is open, claimed and neither a door, bridge, water nor lava
    //! tile (so a torch may face it), else -1
    int32_t mOpenTeamId;
    //! A room object stands on the tile
    bool mBlocked;
};

//! \brief Default room side divisor of the placement: a room side of length L gets L / 4 torches
const uint32_t DEFAULT_TORCH_ROOM_SIDE_DIVISOR = 4;
//! \brief Default corridor spacing of the placement: every 4th wall tile of a straight run gets a torch
const uint32_t DEFAULT_TORCH_CORRIDOR_SPACING = 4;
//! \brief Default smallest distance in steps between two torches
const uint32_t DEFAULT_TORCH_MIN_DISTANCE = 3;

//! \brief Values of the placement (see WallTorchConfig for the config keys)
struct WallTorchPlacement
{
    //! \brief Creates the placement with the default values
    WallTorchPlacement() :
        mRoomSideDivisor(DEFAULT_TORCH_ROOM_SIDE_DIVISOR),
        mCorridorSpacing(DEFAULT_TORCH_CORRIDOR_SPACING),
        mMinDistance(DEFAULT_TORCH_MIN_DISTANCE)
    {}

    //! A room side of length L gets L / mRoomSideDivisor torches
    uint32_t mRoomSideDivisor;
    //! Outside of rooms, every mCorridorSpacing-th wall tile of a straight run gets a torch
    uint32_t mCorridorSpacing;
    //! Smallest distance (steps along the tiles, also around corners) between two torches
    uint32_t mMinDistance;
};

//! \brief The placement rule of the wall torches. It works on plain data (no map, no Ogre), so it can be
//! tested alone. The result only depends on the input, so the same map gives the same torches.
class WallTorches
{
public:
    static const int32_t NB_DIRECTIONS = 4;

    //! \brief X step of a direction. Direction 0: +y, 1: -y, 2: +x, 3: -x
    static int32_t getDirX(int32_t dir);
    //! \brief Y step of the direction (see getDirX)
    static int32_t getDirY(int32_t dir);

    //! \brief Number that identifies a torch (wall tile and side); equal torches have equal keys
    static uint32_t getKey(int32_t x, int32_t y, int32_t dir, int32_t sizeX);

    //! \brief Computes the torches of a map of sizeX * sizeY tiles (tiles is indexed y * sizeX + x).
    //! - Room sides: a straight stretch of room tiles that all face the same side is one side. It gets
    //!   length / mRoomSideDivisor torches, evenly spread, each at the middle of its section. When the
    //!   wanted wall tile cannot carry the torch (opening, no reinforced wall, object in front, too close
    //!   to another torch), the torch moves to the next fitting wall tile of the same side, alternately
    //!   to the right (growing coordinate) and to the left. Without a fitting tile it is dropped.
    //! - Outside of rooms: on a straight run of reinforced wall tiles that face open claimed ground, the
    //!   tiles whose coordinate along the run is mCorridorSpacing / 2 modulo mCorridorSpacing are wanted,
    //!   with the same moving and distance rule.
    //! - Rooms come first (in the order of their index), then the corridors, each by direction and position.
    //! The result is appended to out in the order of placement.
    static void compute(int32_t sizeX, int32_t sizeY, const std::vector<WallTorchTileInfo>& tiles,
        const WallTorchPlacement& placement, std::vector<WallTorch>& out);

    //! \brief True if one of the torches (keyed by getKey, as GameMap::getWallTorches) lies within radius tiles of one
    //! of the given tiles. Torches of every owner count. Only the rows of the box around the tiles are looked at, not
    //! the whole list.
    static bool hasTorchWithin(const std::map<uint32_t, WallTorch>& torches, int32_t sizeX,
        const std::vector<std::pair<int32_t, int32_t> >& tiles, double radius);
};

#endif // WALLTORCHES_H
