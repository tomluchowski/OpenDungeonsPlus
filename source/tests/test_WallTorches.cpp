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

// Header-only Boost.Test, see test_DungeonHeartTier.cpp.
// OVERRIDE_BOOST_TEST_INCLUDED_WARNING
#define BOOST_TEST_MODULE WallTorches
#include <boost/test/included/unit_test.hpp>

#include "rooms/WallTorches.h"

#include <cstdlib>
#include <map>
#include <utility>
#include <vector>

namespace
{
const int32_t MAP_SIZE = 24;
const int32_t WALL_SEAT = 1;
const int32_t TEAM = 0;

//! A small map for the placement. Everything is solid and unowned until a room or a corridor is added.
struct TestMap
{
    TestMap() :
        mTiles(static_cast<size_t>(MAP_SIZE) * static_cast<size_t>(MAP_SIZE))
    {}

    WallTorchTileInfo& at(int32_t x, int32_t y)
    {
        return mTiles[static_cast<size_t>(y) * static_cast<size_t>(MAP_SIZE) + static_cast<size_t>(x)];
    }

    void setWall(int32_t x, int32_t y)
    {
        at(x, y).mWallSeatId = WALL_SEAT;
        at(x, y).mWallTeamId = TEAM;
    }

    //! A room of width * height tiles with its lower left tile at (x0, y0), surrounded by reinforced walls
    void addRoom(int32_t index, int32_t x0, int32_t y0, int32_t width, int32_t height)
    {
        for(int32_t x = x0 - 1; x <= x0 + width; ++x)
        {
            for(int32_t y = y0 - 1; y <= y0 + height; ++y)
            {
                bool inside = (x >= x0) && (x < x0 + width) && (y >= y0) && (y < y0 + height);
                if(inside)
                {
                    at(x, y).mRoomIndex = index;
                    at(x, y).mOpenTeamId = TEAM;
                }
                else
                {
                    setWall(x, y);
                }
            }
        }
    }

    void removeWall(int32_t x, int32_t y)
    {
        at(x, y).mWallSeatId = -1;
        at(x, y).mWallTeamId = -1;
    }

    std::vector<WallTorch> compute() const
    {
        std::vector<WallTorch> torches;
        WallTorchPlacement placement;
        WallTorches::compute(MAP_SIZE, MAP_SIZE, mTiles, placement, torches);
        return torches;
    }

    std::vector<WallTorchTileInfo> mTiles;
};

bool hasTorch(const std::vector<WallTorch>& torches, int32_t x, int32_t y, int32_t dir)
{
    for(const WallTorch& torch : torches)
    {
        if((torch.mX == x) && (torch.mY == y) && (torch.mDir == dir))
            return true;
    }
    return false;
}

size_t countDir(const std::vector<WallTorch>& torches, int32_t dir)
{
    size_t count = 0;
    for(const WallTorch& torch : torches)
    {
        if(torch.mDir == dir)
            ++count;
    }
    return count;
}

//! Every torch hangs on a reinforced wall and faces an open tile, and no two torches are closer than 3 tiles
void checkRules(TestMap& map, const std::vector<WallTorch>& torches)
{
    for(size_t i = 0; i < torches.size(); ++i)
    {
        const WallTorch& torch = torches[i];
        BOOST_CHECK(map.at(torch.mX, torch.mY).mWallSeatId >= 0);
        BOOST_CHECK_EQUAL(torch.mSeatId, WALL_SEAT);
        int32_t frontX = torch.mX + WallTorches::getDirX(torch.mDir);
        int32_t frontY = torch.mY + WallTorches::getDirY(torch.mDir);
        BOOST_CHECK(map.at(frontX, frontY).mOpenTeamId >= 0);
        for(size_t j = i + 1; j < torches.size(); ++j)
        {
            int32_t distance = std::abs(torch.mX - torches[j].mX) + std::abs(torch.mY - torches[j].mY);
            BOOST_CHECK(distance >= 3);
        }
    }
}
} // namespace

// The same map always gives the same torches in the same order (the clients rely on stable keys).
BOOST_AUTO_TEST_CASE(test_SameInputSamePositions)
{
    TestMap map;
    map.addRoom(0, 5, 5, 8, 8);
    std::vector<WallTorch> first = map.compute();
    std::vector<WallTorch> second = map.compute();
    BOOST_REQUIRE_EQUAL(first.size(), second.size());
    for(size_t i = 0; i < first.size(); ++i)
    {
        BOOST_CHECK_EQUAL(first[i].mX, second[i].mX);
        BOOST_CHECK_EQUAL(first[i].mY, second[i].mY);
        BOOST_CHECK_EQUAL(first[i].mDir, second[i].mDir);
        BOOST_CHECK_EQUAL(first[i].mSeatId, second[i].mSeatId);
    }
}

// A room side of length L gets L / divisor torches, spread over the side and facing into the room.
BOOST_AUTO_TEST_CASE(test_RoomSides)
{
    // 4x4: one torch per side, in the middle of the section (index 2 of 0..3)
    TestMap room4;
    room4.addRoom(0, 5, 5, 4, 4);
    std::vector<WallTorch> torches = room4.compute();
    BOOST_CHECK_EQUAL(torches.size(), 4u);
    BOOST_CHECK(hasTorch(torches, 7, 4, 0));
    BOOST_CHECK(hasTorch(torches, 7, 9, 1));
    BOOST_CHECK(hasTorch(torches, 4, 7, 2));
    BOOST_CHECK(hasTorch(torches, 9, 7, 3));
    checkRules(room4, torches);

    // 4x3: only the sides of length 4 get a torch (3 / 4 rounds down to none)
    TestMap room43;
    room43.addRoom(0, 5, 5, 4, 3);
    torches = room43.compute();
    BOOST_CHECK_EQUAL(torches.size(), 2u);
    BOOST_CHECK_EQUAL(countDir(torches, 0), 1u);
    BOOST_CHECK_EQUAL(countDir(torches, 1), 1u);
    BOOST_CHECK_EQUAL(countDir(torches, 2), 0u);
    BOOST_CHECK_EQUAL(countDir(torches, 3), 0u);
    checkRules(room43, torches);

    // 6x6: still one per side
    TestMap room6;
    room6.addRoom(0, 5, 5, 6, 6);
    torches = room6.compute();
    BOOST_CHECK_EQUAL(torches.size(), 4u);
    BOOST_CHECK(hasTorch(torches, 8, 4, 0));
    BOOST_CHECK(hasTorch(torches, 8, 11, 1));
    BOOST_CHECK(hasTorch(torches, 4, 8, 2));
    BOOST_CHECK(hasTorch(torches, 11, 8, 3));
    checkRules(room6, torches);

    // 8x8: two per side, each in the middle of its half
    TestMap room8;
    room8.addRoom(0, 5, 5, 8, 8);
    torches = room8.compute();
    BOOST_CHECK_EQUAL(torches.size(), 8u);
    BOOST_CHECK(hasTorch(torches, 7, 4, 0));
    BOOST_CHECK(hasTorch(torches, 11, 4, 0));
    BOOST_CHECK(hasTorch(torches, 7, 13, 1));
    BOOST_CHECK(hasTorch(torches, 11, 13, 1));
    BOOST_CHECK(hasTorch(torches, 4, 7, 2));
    BOOST_CHECK(hasTorch(torches, 4, 11, 2));
    BOOST_CHECK(hasTorch(torches, 13, 7, 3));
    BOOST_CHECK(hasTorch(torches, 13, 11, 3));
    checkRules(room8, torches);
}

// A torch that would stand in front of a blocked tile moves to the next fitting wall tile of the side.
BOOST_AUTO_TEST_CASE(test_MovingAside)
{
    // The wanted wall tile (7,4) is an opening: the torch moves to the right first (growing x)
    TestMap right;
    right.addRoom(0, 5, 5, 4, 4);
    right.removeWall(7, 4);
    std::vector<WallTorch> torches = right.compute();
    BOOST_CHECK_EQUAL(countDir(torches, 0), 1u);
    BOOST_CHECK(hasTorch(torches, 8, 4, 0));
    checkRules(right, torches);

    // With the right neighbour blocked as well it moves to the left
    TestMap left;
    left.addRoom(0, 5, 5, 4, 4);
    left.removeWall(7, 4);
    left.removeWall(8, 4);
    torches = left.compute();
    BOOST_CHECK_EQUAL(countDir(torches, 0), 1u);
    BOOST_CHECK(hasTorch(torches, 6, 4, 0));
    checkRules(left, torches);

    // A room object in front of the wanted tile moves the torch the same way
    TestMap object;
    object.addRoom(0, 5, 5, 4, 4);
    object.at(7, 5).mBlocked = true;
    torches = object.compute();
    BOOST_CHECK(hasTorch(torches, 8, 4, 0));
    BOOST_CHECK(!hasTorch(torches, 7, 4, 0));
    checkRules(object, torches);
}

// Two torches are never closer than the minimum distance, also not around a corner.
BOOST_AUTO_TEST_CASE(test_MinimumDistanceAroundCorners)
{
    // The torch of the lower side moved to (8,4). The right side can only use (9,5), which is two steps away
    // around the corner, so it gets no torch. The other sides are not affected.
    TestMap map;
    map.addRoom(0, 5, 5, 4, 4);
    map.removeWall(7, 4);
    map.removeWall(9, 6);
    map.removeWall(9, 7);
    map.removeWall(9, 8);
    std::vector<WallTorch> torches = map.compute();
    BOOST_CHECK(hasTorch(torches, 8, 4, 0));
    BOOST_CHECK_EQUAL(countDir(torches, 3), 0u);
    BOOST_CHECK_EQUAL(torches.size(), 3u);
    checkRules(map, torches);
}

// Outside of rooms a straight run of reinforced wall gets one torch at the corridor spacing.
BOOST_AUTO_TEST_CASE(test_Corridors)
{
    // A straight corridor along x: open tiles at y = 10 with a reinforced wall at y = 9. Every fourth wall tile
    // carries a torch, anchored to the coordinate (so the torches stay where they are when the corridor grows).
    TestMap map;
    for(int32_t x = 2; x <= 17; ++x)
    {
        map.at(x, 10).mOpenTeamId = TEAM;
        map.setWall(x, 9);
    }
    std::vector<WallTorch> torches = map.compute();
    BOOST_REQUIRE_EQUAL(torches.size(), 4u);
    BOOST_CHECK(hasTorch(torches, 2, 9, 0));
    BOOST_CHECK(hasTorch(torches, 6, 9, 0));
    BOOST_CHECK(hasTorch(torches, 10, 9, 0));
    BOOST_CHECK(hasTorch(torches, 14, 9, 0));
    checkRules(map, torches);

    // A door, bridge, water or lava tile (no open claimed tile, mOpenTeamId stays -1) in front of the wall
    // carries no torch
    map.at(6, 10).mOpenTeamId = -1;
    torches = map.compute();
    BOOST_CHECK(!hasTorch(torches, 6, 9, 0));
    checkRules(map, torches);
}

// No torch faces a door, a bridge, water or lava.
BOOST_AUTO_TEST_CASE(test_NoTorchOnDoorBridgeWaterLava)
{
    // Such a tile in front of the wanted wall tile (7,4) is not an open claimed tile: the torch moves aside
    TestMap map;
    map.addRoom(0, 5, 5, 4, 4);
    map.at(7, 5).mOpenTeamId = -1;
    std::vector<WallTorch> torches = map.compute();
    BOOST_CHECK(!hasTorch(torches, 7, 4, 0));
    BOOST_CHECK(hasTorch(torches, 8, 4, 0));
    checkRules(map, torches);

    // A wall of another team than the open tile in front of it carries nothing
    TestMap foreign;
    foreign.addRoom(0, 5, 5, 4, 4);
    for(size_t i = 0; i < foreign.mTiles.size(); ++i)
    {
        if(foreign.mTiles[i].mWallSeatId >= 0)
            foreign.mTiles[i].mWallTeamId = TEAM + 1;
    }
    BOOST_CHECK(foreign.compute().empty());
}

// Torches follow the walls: a destroyed wall loses its torch, a side without reinforcement has none.
BOOST_AUTO_TEST_CASE(test_Lifetime)
{
    TestMap map;
    map.addRoom(0, 5, 5, 4, 4);
    std::vector<WallTorch> torches = map.compute();
    BOOST_CHECK(hasTorch(torches, 7, 4, 0));

    // The wall is destroyed: its torch is gone and the side is computed again
    map.removeWall(7, 4);
    torches = map.compute();
    BOOST_CHECK(!hasTorch(torches, 7, 4, 0));
    BOOST_CHECK(hasTorch(torches, 8, 4, 0));
    BOOST_CHECK_EQUAL(countDir(torches, 0), 1u);
    checkRules(map, torches);

    // The whole side loses its reinforcement: no torch on that side, the others stay
    for(int32_t x = 4; x <= 9; ++x)
        map.removeWall(x, 4);
    torches = map.compute();
    BOOST_CHECK_EQUAL(countDir(torches, 0), 0u);
    BOOST_CHECK_EQUAL(torches.size(), 3u);
}
