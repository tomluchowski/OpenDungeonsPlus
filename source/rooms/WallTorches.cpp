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

#include "rooms/WallTorches.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace
{
typedef std::pair<int32_t, int32_t> TilePos;
typedef std::vector<TilePos> TileRun;

//! Orders the tiles by x, then by y (the tiles are collected by y, then x)
bool lessColumnFirst(const TilePos& a, const TilePos& b)
{
    if(a.first != b.first)
        return a.first < b.first;

    return a.second < b.second;
}

//! A facing along y makes runs along x
bool isFacingAlongY(int32_t dir)
{
    return WallTorches::getDirY(dir) != 0;
}

//! Splits tiles, ordered line by line, into runs of neighbouring tiles on one line. The line of a tile is
//! its y for a facing along y, else its x. The position along the line is the other coordinate.
void splitRuns(const std::vector<TilePos>& ordered, int32_t dir, std::vector<TileRun>& runs)
{
    bool alongY = isFacingAlongY(dir);
    for(const TilePos& pos : ordered)
    {
        int32_t line = alongY ? pos.second : pos.first;
        int32_t along = alongY ? pos.first : pos.second;
        if(!runs.empty())
        {
            const TilePos& last = runs.back().back();
            int32_t lastLine = alongY ? last.second : last.first;
            int32_t lastAlong = alongY ? last.first : last.second;
            if((lastLine == line) && (lastAlong + 1 == along))
            {
                runs.back().push_back(pos);
                continue;
            }
        }
        runs.push_back(TileRun(1, pos));
    }
}

class WallTorchPlacer
{
public:
    WallTorchPlacer(int32_t sizeX, int32_t sizeY, const std::vector<WallTorchTileInfo>& tiles,
            const WallTorchPlacement& placement, std::vector<WallTorch>& out) :
        mSizeX(sizeX),
        mSizeY(sizeY),
        mTiles(tiles),
        mPlacement(placement),
        mOut(out)
    {}

    void placeRooms();
    void placeCorridors();

private:
    const WallTorchTileInfo* getTile(int32_t x, int32_t y) const;
    //! Tells whether a torch may hang on the wall behind the open tile front when it faces dir
    bool canHang(const TilePos& front, int32_t dir) const;
    bool isFarEnough(int32_t x, int32_t y) const;
    //! Tries the wanted index of the run first, then alternately the next ones to the right and to the left
    void placeNear(const TileRun& run, int32_t dir, uint32_t wanted);

    int32_t mSizeX;
    int32_t mSizeY;
    const std::vector<WallTorchTileInfo>& mTiles;
    const WallTorchPlacement& mPlacement;
    std::vector<WallTorch>& mOut;
};

const WallTorchTileInfo* WallTorchPlacer::getTile(int32_t x, int32_t y) const
{
    if((x < 0) || (y < 0) || (x >= mSizeX) || (y >= mSizeY))
        return nullptr;

    size_t index = static_cast<size_t>(y) * static_cast<size_t>(mSizeX) + static_cast<size_t>(x);
    if(index >= mTiles.size())
        return nullptr;

    return &mTiles[index];
}

bool WallTorchPlacer::canHang(const TilePos& front, int32_t dir) const
{
    const WallTorchTileInfo* frontInfo = getTile(front.first, front.second);
    const WallTorchTileInfo* wallInfo = getTile(front.first - WallTorches::getDirX(dir),
        front.second - WallTorches::getDirY(dir));
    if((frontInfo == nullptr) || (wallInfo == nullptr))
        return false;

    if(wallInfo->mWallSeatId < 0)
        return false;

    if((frontInfo->mOpenTeamId < 0) || (frontInfo->mOpenTeamId != wallInfo->mWallTeamId))
        return false;

    return !frontInfo->mBlocked;
}

bool WallTorchPlacer::isFarEnough(int32_t x, int32_t y) const
{
    for(const WallTorch& torch : mOut)
    {
        int32_t distance = std::abs(torch.mX - x) + std::abs(torch.mY - y);
        if(distance < static_cast<int32_t>(mPlacement.mMinDistance))
            return false;
    }
    return true;
}

void WallTorchPlacer::placeNear(const TileRun& run, int32_t dir, uint32_t wanted)
{
    int32_t length = static_cast<int32_t>(run.size());
    int32_t wish = static_cast<int32_t>(wanted);
    for(int32_t step = 0; step < length; ++step)
    {
        for(int32_t side = 0; side < 2; ++side)
        {
            // Step 0 is the wanted tile itself; then right, left, right, left...
            if((step == 0) && (side == 1))
                break;

            int32_t index = (side == 0) ? wish + step : wish - step;
            if((index < 0) || (index >= length))
                continue;

            const TilePos& front = run[static_cast<size_t>(index)];
            if(!canHang(front, dir))
                continue;

            int32_t wallX = front.first - WallTorches::getDirX(dir);
            int32_t wallY = front.second - WallTorches::getDirY(dir);
            if(!isFarEnough(wallX, wallY))
                continue;

            WallTorch torch;
            torch.mX = wallX;
            torch.mY = wallY;
            torch.mDir = dir;
            torch.mSeatId = getTile(wallX, wallY)->mWallSeatId;
            mOut.push_back(torch);
            return;
        }
    }
}

void WallTorchPlacer::placeRooms()
{
    // Room tiles per room, collected row by row
    std::vector<std::vector<TilePos> > roomTiles;
    for(int32_t y = 0; y < mSizeY; ++y)
    {
        for(int32_t x = 0; x < mSizeX; ++x)
        {
            const WallTorchTileInfo* info = getTile(x, y);
            if((info == nullptr) || (info->mRoomIndex < 0))
                continue;

            size_t room = static_cast<size_t>(info->mRoomIndex);
            if(room >= roomTiles.size())
                roomTiles.resize(room + 1);
            roomTiles[room].push_back(TilePos(x, y));
        }
    }

    for(size_t room = 0; room < roomTiles.size(); ++room)
    {
        for(int32_t dir = 0; dir < WallTorches::NB_DIRECTIONS; ++dir)
        {
            // The side is made of the room tiles whose neighbour on the other side of the facing is not
            // part of the room
            std::vector<TilePos> edge;
            for(const TilePos& pos : roomTiles[room])
            {
                const WallTorchTileInfo* behind = getTile(pos.first - WallTorches::getDirX(dir),
                    pos.second - WallTorches::getDirY(dir));
                if((behind != nullptr) && (behind->mRoomIndex == static_cast<int32_t>(room)))
                    continue;

                edge.push_back(pos);
            }
            if(!isFacingAlongY(dir))
                std::sort(edge.begin(), edge.end(), lessColumnFirst);

            std::vector<TileRun> runs;
            splitRuns(edge, dir, runs);
            for(const TileRun& run : runs)
            {
                uint32_t length = static_cast<uint32_t>(run.size());
                uint32_t nbTorches = length / mPlacement.mRoomSideDivisor;
                for(uint32_t j = 0; j < nbTorches; ++j)
                {
                    // Middle of the j-th of nbTorches equal sections
                    uint32_t wanted = (length * (2 * j + 1)) / (2 * nbTorches);
                    placeNear(run, dir, wanted);
                }
            }
        }
    }
}

void WallTorchPlacer::placeCorridors()
{
    for(int32_t dir = 0; dir < WallTorches::NB_DIRECTIONS; ++dir)
    {
        bool alongY = isFacingAlongY(dir);
        // Open tiles outside of rooms that have a reinforced wall behind them, line by line
        std::vector<TilePos> fronts;
        if(alongY)
        {
            for(int32_t y = 0; y < mSizeY; ++y)
            {
                for(int32_t x = 0; x < mSizeX; ++x)
                {
                    const WallTorchTileInfo* info = getTile(x, y);
                    if((info != nullptr) && (info->mRoomIndex < 0) && canHang(TilePos(x, y), dir))
                        fronts.push_back(TilePos(x, y));
                }
            }
        }
        else
        {
            for(int32_t x = 0; x < mSizeX; ++x)
            {
                for(int32_t y = 0; y < mSizeY; ++y)
                {
                    const WallTorchTileInfo* info = getTile(x, y);
                    if((info != nullptr) && (info->mRoomIndex < 0) && canHang(TilePos(x, y), dir))
                        fronts.push_back(TilePos(x, y));
                }
            }
        }

        std::vector<TileRun> runs;
        splitRuns(fronts, dir, runs);
        int32_t spacing = static_cast<int32_t>(mPlacement.mCorridorSpacing);
        for(const TileRun& run : runs)
        {
            for(size_t index = 0; index < run.size(); ++index)
            {
                int32_t along = alongY ? run[index].first : run[index].second;
                if(along % spacing != spacing / 2)
                    continue;

                placeNear(run, dir, static_cast<uint32_t>(index));
            }
        }
    }
}
} // namespace

int32_t WallTorches::getDirX(int32_t dir)
{
    switch(dir)
    {
        case 2:
            return 1;
        case 3:
            return -1;
        default:
            return 0;
    }
}

int32_t WallTorches::getDirY(int32_t dir)
{
    switch(dir)
    {
        case 0:
            return 1;
        case 1:
            return -1;
        default:
            return 0;
    }
}

uint32_t WallTorches::getKey(int32_t x, int32_t y, int32_t dir, int32_t sizeX)
{
    return static_cast<uint32_t>((y * sizeX + x) * NB_DIRECTIONS + dir);
}

void WallTorches::compute(int32_t sizeX, int32_t sizeY, const std::vector<WallTorchTileInfo>& tiles,
    const WallTorchPlacement& placement, std::vector<WallTorch>& out)
{
    // A placement with a zero value would divide by zero
    WallTorchPlacement safePlacement = placement;
    if(safePlacement.mRoomSideDivisor == 0)
        safePlacement.mRoomSideDivisor = 1;
    if(safePlacement.mCorridorSpacing == 0)
        safePlacement.mCorridorSpacing = 1;

    WallTorchPlacer placer(sizeX, sizeY, tiles, safePlacement, out);
    placer.placeRooms();
    placer.placeCorridors();
}
