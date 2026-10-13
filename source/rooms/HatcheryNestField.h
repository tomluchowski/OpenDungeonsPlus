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

#ifndef HATCHERYNESTFIELD_H
#define HATCHERYNESTFIELD_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

//! \brief Places of the straw nests that lie scattered over a hatchery. The places are computed from the tiles of the
//! hatchery, the tiles of its coops and its entrances alone, with integer hashes of the tile coordinates and no random numbers, so
//! the server and every client get the same places without anything being sent. The header has no dependency on the
//! game map, so it can be tested alone (a python port of the rules is in the check scripts).
//!
//! Rules (see Settings for the numbers): a nest lies on a tile of the hatchery, at least mEdge from every tile that is
//! not part of the hatchery (walls, other rooms), at least mCoopClearance from the footprint of every coop, outside
//! of the apron in front of the ramp of every coop (the way the animals walk to it), at least mPathClearance from the
//! walking strips (half width mPathHalfWidth) that lead from the middle of every entrance tile to the apron of every
//! coop, and at least mSpacing from every other nest. An entrance is a tile of the hatchery that lies next to a
//! walkable tile that is not part of it (door, corridor, other room); the caller finds them (see
//! RoomHatchery::collectEntrances) from the fullness of the tiles, which the server and every client know. There is about one nest per mTilesPerNest tiles (at most mMaxNests), but at least one per coop.
namespace HatcheryNestField
{
    typedef std::pair<int, int> TileCoord;

    //! Height of the middle of an egg that lies in a nest (the nest mesh is lower than 0.06). Above 0.01, so that the
    //! clients know that the egg lies in a nest.
    static const double eggHeight = 0.03;

    //! Footprint of a coop around the center of its tile (the coops are not turned): the same numbers as the entry of
    //! the coop mesh in the table of the room object bounds. The ramp is on the side of the maximum x.
    static const double coopMinX = -0.203275;
    static const double coopMaxX = 0.796725;
    static const double coopMinY = -0.4;
    static const double coopMaxY = 0.4;

    struct Settings
    {
        Settings() :
            mTilesPerNest(3),
            mMaxNests(16),
            mEdge(0.25),
            mCoopClearance(0.45),
            mLaneLength(1.0),
            mLaneHalfWidth(0.5),
            mLaneClearance(0.2),
            mPathHalfWidth(0.35),
            mPathClearance(0.2),
            mSpacing(0.6)
        {}

        //! About one nest per this many tiles of the hatchery.
        uint32_t mTilesPerNest;
        //! Most nests that the size of the hatchery asks for (the one nest per coop comes on top when it is more).
        uint32_t mMaxNests;
        //! Distance in tiles from the middle of a nest to every tile that is not part of the hatchery.
        double mEdge;
        //! Distance in tiles from the middle of a nest to the footprint of a coop.
        double mCoopClearance;
        //! The apron in front of the ramp of a coop (kept free of nests): length in tiles along x from the footprint...
        double mLaneLength;
        //! ... and half of its width around the center line of the coop.
        double mLaneHalfWidth;
        //! Distance in tiles from the middle of a nest to the apron.
        double mLaneClearance;
        //! Half of the width in tiles of the walking strip from the middle of an entrance tile to the middle of the apron
        //! of every coop...
        double mPathHalfWidth;
        //! ... and the distance in tiles from the middle of a nest to that strip.
        double mPathClearance;
        //! Smallest distance in tiles between the middles of two nests.
        double mSpacing;
    };

    //! A nest: the middle in the world (tiles) and a turn in degrees (for the look only).
    struct Place
    {
        Place() :
            mX(0.0),
            mY(0.0),
            mAngle(0.0)
        {}

        Place(double x, double y, double angle) :
            mX(x),
            mY(y),
            mAngle(angle)
        {}

        double mX;
        double mY;
        double mAngle;
    };

    //! Hash of a tile and an attempt number (32 bit integer arithmetic only).
    inline uint32_t hashTile(int x, int y, uint32_t attempt)
    {
        uint32_t h = 2166136261u;
        h = (h ^ static_cast<uint32_t>(x)) * 16777619u;
        h = (h ^ static_cast<uint32_t>(y)) * 16777619u;
        h = (h ^ attempt) * 16777619u;
        h ^= h >> 15;
        h *= 2246822519u;
        h ^= h >> 13;
        h *= 3266489917u;
        h ^= h >> 16;
        return h;
    }

    //! Number that changes when the tiles of the hatchery or its coops change (does not depend on their order).
    inline uint32_t fingerprint(const std::vector<TileCoord>& roomTiles, const std::vector<TileCoord>& coops,
        const std::vector<TileCoord>& entrances)
    {
        uint32_t sum = static_cast<uint32_t>(roomTiles.size()) * 7919u + static_cast<uint32_t>(coops.size()) * 104729u +
            static_cast<uint32_t>(entrances.size()) * 1299709u;
        for(std::vector<TileCoord>::const_iterator it = roomTiles.begin(); it != roomTiles.end(); ++it)
            sum += hashTile(it->first, it->second, 7u);
        for(std::vector<TileCoord>::const_iterator it = coops.begin(); it != coops.end(); ++it)
            sum += 31u * hashTile(it->first, it->second, 9u);
        for(std::vector<TileCoord>::const_iterator it = entrances.begin(); it != entrances.end(); ++it)
            sum += 37u * hashTile(it->first, it->second, 11u);
        return sum;
    }

    //! Squared distance from a point to a rectangle (0 inside).
    inline double rectDistanceSquared(double x, double y, double minX, double minY, double maxX, double maxY)
    {
        const double dx = std::max(std::max(minX - x, 0.0), x - maxX);
        const double dy = std::max(std::max(minY - y, 0.0), y - maxY);
        return dx * dx + dy * dy;
    }

    //! Squared distance from a point to the segment from (ax, ay) to (bx, by).
    inline double segmentDistanceSquared(double x, double y, double ax, double ay, double bx, double by)
    {
        const double sx = bx - ax;
        const double sy = by - ay;
        const double lengthSquared = sx * sx + sy * sy;
        double t = 0.0;
        if(lengthSquared > 0.0)
            t = std::min(1.0, std::max(0.0, ((x - ax) * sx + (y - ay) * sy) / lengthSquared));
        const double dx = x - (ax + t * sx);
        const double dy = y - (ay + t * sy);
        return dx * dx + dy * dy;
    }

    //! True if a nest can lie at (x, y) on the tile (tileX, tileY) of the hatchery: far enough from the tiles that are
    //! not part of the hatchery, from the coops and their aprons, and from the nests that are taken already.
    inline bool placeIsFree(double x, double y, int tileX, int tileY, const std::set<TileCoord>& roomTiles,
        const std::vector<TileCoord>& coops, const std::vector<TileCoord>& entrances,
        const std::vector<Place>& taken, const Settings& settings)
    {
        for(int dx = -1; dx <= 1; ++dx)
        {
            for(int dy = -1; dy <= 1; ++dy)
            {
                if((dx == 0) && (dy == 0))
                    continue;
                if(roomTiles.count(TileCoord(tileX + dx, tileY + dy)) > 0)
                    continue;

                const double nx = tileX + dx;
                const double ny = tileY + dy;
                if(rectDistanceSquared(x, y, nx - 0.5, ny - 0.5, nx + 0.5, ny + 0.5) < settings.mEdge * settings.mEdge)
                    return false;
            }
        }

        for(std::vector<TileCoord>::const_iterator it = coops.begin(); it != coops.end(); ++it)
        {
            const double cx = it->first;
            const double cy = it->second;
            if(rectDistanceSquared(x, y, cx + coopMinX, cy + coopMinY, cx + coopMaxX, cy + coopMaxY) <
               settings.mCoopClearance * settings.mCoopClearance)
                return false;
            if(rectDistanceSquared(x, y, cx + coopMaxX, cy - settings.mLaneHalfWidth,
                   cx + coopMaxX + settings.mLaneLength, cy + settings.mLaneHalfWidth) <
               settings.mLaneClearance * settings.mLaneClearance)
                return false;

            // The walking strips from the entrances to the middle of the apron of this coop
            const double clear = settings.mPathHalfWidth + settings.mPathClearance;
            for(std::vector<TileCoord>::const_iterator entrance = entrances.begin(); entrance != entrances.end(); ++entrance)
            {
                if(segmentDistanceSquared(x, y, entrance->first, entrance->second, cx + coopMaxX + settings.mLaneLength * 0.5,
                       cy) < clear * clear)
                    return false;
            }
        }

        for(std::vector<Place>::const_iterator it = taken.begin(); it != taken.end(); ++it)
        {
            const double dx = it->mX - x;
            const double dy = it->mY - y;
            if(dx * dx + dy * dy < settings.mSpacing * settings.mSpacing)
                return false;
        }
        return true;
    }

    //! The places of the nests of a hatchery with the given tiles and coop tiles. Without coop there are nests all the
    //! same (a coop that is not built yet does not take the eggs away), and without entrance there is no walking strip.
    //! The tiles can be in any order. When the strips leave room for fewer nests than wanted, there are fewer nests,
    //! but more places are tried while there are fewer nests than coops.
    inline std::vector<Place> compute(const std::vector<TileCoord>& roomTilesIn, const std::vector<TileCoord>& coopsIn,
        const std::vector<TileCoord>& entrancesIn, const Settings& settings)
    {
        std::vector<Place> places;
        const std::set<TileCoord> roomTiles(roomTilesIn.begin(), roomTilesIn.end());
        std::vector<TileCoord> coops(coopsIn.begin(), coopsIn.end());
        std::sort(coops.begin(), coops.end());
        coops.erase(std::unique(coops.begin(), coops.end()), coops.end());

        std::vector<TileCoord> entrances(entrancesIn.begin(), entrancesIn.end());
        std::sort(entrances.begin(), entrances.end());
        entrances.erase(std::unique(entrances.begin(), entrances.end()), entrances.end());

        const uint32_t perNest = std::max<uint32_t>(1, settings.mTilesPerNest);
        uint32_t wanted = std::min(static_cast<uint32_t>(roomTiles.size()) / perNest, settings.mMaxNests);
        wanted = std::max(wanted, static_cast<uint32_t>(coops.size()));

        // Each round (attempt) goes over the tiles in the order of their hash and tries one place on each tile (the place on
        // the tile is moved by up to 0.4 tiles each way, by the hash). More rounds try other places on the same tiles.
        const uint32_t rounds = 6;
        const uint32_t extraRounds = 18;
        for(uint32_t attempt = 0; (places.size() < wanted) &&
            ((attempt < rounds) || ((places.size() < coops.size()) && (attempt < rounds + extraRounds))); ++attempt)
        {
            std::vector<std::pair<uint32_t, TileCoord> > order;
            for(std::set<TileCoord>::const_iterator it = roomTiles.begin(); it != roomTiles.end(); ++it)
                order.push_back(std::make_pair(hashTile(it->first, it->second, attempt), *it));
            std::sort(order.begin(), order.end());

            for(std::vector<std::pair<uint32_t, TileCoord> >::const_iterator it = order.begin();
                (it != order.end()) && (places.size() < wanted); ++it)
            {
                const uint32_t h = it->first;
                const int tileX = it->second.first;
                const int tileY = it->second.second;
                const double x = tileX + (static_cast<int>((h >> 8) % 81u) - 40) / 100.0;
                const double y = tileY + (static_cast<int>((h >> 16) % 81u) - 40) / 100.0;
                if(placeIsFree(x, y, tileX, tileY, roomTiles, coops, entrances, places, settings))
                    places.push_back(Place(x, y, static_cast<double>(h % 360u)));
            }
        }
        return places;
    }
}

#endif // HATCHERYNESTFIELD_H
