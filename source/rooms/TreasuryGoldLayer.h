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

#ifndef TREASURYGOLDLAYER_H
#define TREASURYGOLDLAYER_H

#include "rooms/TreasurySettings.h"

#include <cmath>
#include <cstdlib>
#include <string>

//! \brief The gold layer of a treasury: one pile per tile whose height follows the gold stored
//! on it, in TreasuryGoldLayer::maxLevel steps.
//!
//! Nothing here depends on the renderer. The server turns the gold of a tile and of its
//! neighbours into a mesh name (the name of the pile object it already replicates to the
//! clients, so no extra packet is needed and old clients or saves are not affected), and the
//! client builds the mesh behind that name from the same description.
namespace TreasuryGoldLayer
{
//! Number of non-empty fill steps. Level 0 is an empty tile (no pile at all).
static const int maxLevel = 7;

//! Height of a corner of the pile at the given level, in tile units
inline float levelHeight(int level)
{
    if(level <= 0)
        return 0.0f;
    if(level > maxLevel)
        level = maxLevel;
    return TreasurySettings::current().levelHeight * static_cast<float>(level);
}

//! Fill step of a tile: 0 when empty, else 1..maxLevel by the share of the capacity stored.
inline int levelForGold(int gold, int capacity)
{
    if(gold <= 0)
        return 0;
    if(capacity <= 0 || gold >= capacity)
        return maxLevel;
    // Rounded up, so any gold at all shows a pile
    int level = static_cast<int>((static_cast<long long>(gold) * maxLevel + capacity - 1) / capacity);
    if(level < 1)
        level = 1;
    if(level > maxLevel)
        level = maxLevel;
    return level;
}

//! The part of the pile that does not depend on where it lies in the room. The corner levels
//! remain in mesh names so existing room updates and saved pile objects keep their format.
struct PileShape
{
    PileShape() :
        mLevel(0),
        mVariant(0)
    {
        for(int i = 0; i < 4; ++i)
            mCorner[i] = 0;
    }

    int mLevel;
    //! North-west, north-east, south-east, south-west (north is towards +y)
    int mCorner[4];
    //! Selects one of a few lumpy variants so neighbouring piles do not look alike
    int mVariant;
};

static const int variantCount = 4;
static const char* const meshNamePrefix = "TreasuryGold_";

//! Mesh name of a pile shape, e.g. "TreasuryGold_5_5577_2"
inline std::string meshName(const PileShape& shape)
{
    std::string name = meshNamePrefix;
    name += static_cast<char>('0' + shape.mLevel);
    name += '_';
    for(int i = 0; i < 4; ++i)
        name += static_cast<char>('0' + shape.mCorner[i]);
    name += '_';
    name += static_cast<char>('0' + shape.mVariant);
    return name;
}

//! Reads a name made by meshName(). Fails for any other name.
inline bool parseMeshName(const std::string& name, PileShape& shape)
{
    const std::string prefix = meshNamePrefix;
    // prefix + level + '_' + four corners + '_' + variant
    if(name.size() != prefix.size() + 8 || name.compare(0, prefix.size(), prefix) != 0)
        return false;
    const char* text = name.c_str() + prefix.size();
    if(text[1] != '_' || text[6] != '_')
        return false;
    int values[6] = {text[0] - '0', text[2] - '0', text[3] - '0', text[4] - '0', text[5] - '0', text[7] - '0'};
    for(int i = 0; i < 5; ++i)
    {
        if(values[i] < 0 || values[i] > maxLevel)
            return false;
    }
    if(values[5] < 0 || values[5] >= variantCount)
        return false;
    shape.mLevel = values[0];
    for(int i = 0; i < 4; ++i)
        shape.mCorner[i] = values[1 + i];
    shape.mVariant = values[5];
    return true;
}

//! Radius (tile units, the tile is 1 wide) of the round heap of a pile: a little gold is a small heap in the middle
//! of the tile; even a full tile leaves floor visible around its uneven edge
inline float pileRadius(int level)
{
    if(level <= 1)
        return 0.16f;
    if(level >= maxLevel)
        return 0.46f;
    return 0.16f + 0.30f * static_cast<float>(level - 1) / static_cast<float>(maxLevel - 1);
}

//! Radius of the heap of the shape in the direction of (u, v) seen from the middle of the tile: slightly uneven,
//! never above pileRadius()
inline float pileRadiusAt(const PileShape& shape, float u, float v)
{
    const float angle = std::atan2(v - 0.5f, u - 0.5f);
    return pileRadius(shape.mLevel) * (0.92f + 0.08f * std::sin(3.0f * angle + 1.7f * static_cast<float>(shape.mVariant)));
}

//! Surface height of a pile at (u, v), both 0..1 across the tile (u towards +x, v towards +y).
//! The heap rises from the middle of the tile and runs out flat at its irregular radius.
inline float heightAt(const PileShape& shape, float u, float v)
{
    if(shape.mLevel <= 0)
        return 0.0f;

    // 1 in the middle, 0 at and beyond the radius of the heap (which never exceeds the tile edges)
    const float du = u - 0.5f;
    const float dv = v - 0.5f;
    const float ratio = std::sqrt(du * du + dv * dv) / pileRadiusAt(shape, u, v);
    float bump = 0.0f;
    if(ratio < 1.0f)
    {
        const float rest = 1.0f - ratio * ratio;
        bump = rest * std::sqrt(rest);
    }
    const float peak = levelHeight(shape.mLevel);
    // The rise starts flat at the foot of the heap.
    float height = peak * bump;

    // A few lumps, only on fuller piles, fading out towards the edges
    if(shape.mLevel >= 3)
    {
        const float phase = 1.7f * static_cast<float>(shape.mVariant);
        const float lumps = std::sin(9.0f * u + phase) * std::sin(8.0f * v + 2.0f * phase);
        height += 0.005f * static_cast<float>(shape.mLevel) * bump * lumps;
    }

    // Keep the layer a hair above the floor so it never flickers against it
    if(height < 0.0f)
        height = 0.0f;
    return height + 0.004f;
}

//! Small details lying on a pile (coins, gems, spilled coins at open edges). Their number and place follow
//! from the shape alone, so every client builds the same pile from its name.
//! The numbers come from config/treasury.cfg (see TreasurySettings.h); the references follow the live values.
static const int& maxTopCoins = TreasurySettings::current().maxTopCoins;
static const int& maxGems = TreasurySettings::current().maxGems;
//! Coins lying at the foot of the pile on every open edge of a full pile (per edge, four edges)
inline int maxSpillCoins()
{
    return 4 * TreasurySettings::current().spillCoinsFull;
}
//! Coins scattered on the bare floor of a tile without gold
static const int& scatterCoins = TreasurySettings::current().scatterCoins;

//! Pseudo random number 0..1 from two integers (the same everywhere, no random state)
inline float hash01(int a, int b)
{
    unsigned int h = static_cast<unsigned int>(a) * 73856093u ^ static_cast<unsigned int>(b) * 19349663u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFu) / 65535.0f;
}

//! Single coins on top of the heap: none on a thin layer, then growing evenly up to maxTopCoins on a full one
//! (a sea of coins on the fullest piles)
inline int topCoinCount(const PileShape& shape)
{
    const int first = TreasurySettings::current().topCoinMinLevel;
    const int most = TreasurySettings::current().maxTopCoins;
    if(shape.mLevel < first || most <= 0)
        return 0;
    if(shape.mLevel >= maxLevel)
        return most;
    // From a few coins on the first covered step in even steps to the full number
    const int span = maxLevel - first;
    const int count = 1 + (most - 1) * (shape.mLevel - first + 1) / (span + 1);
    return count > most ? most : count;
}

//! Gems scattered in rich piles: none below level 5, one in some level 5 piles, one or two in level 6 ones, one
//! up to maxGems in full ones (the variant of the pile decides, so rich rooms show several gems)
inline int gemCount(const PileShape& shape)
{
    if(shape.mLevel < 5)
        return 0;
    if(shape.mLevel < maxLevel - 1)
        return (shape.mVariant % 2 == 0) ? 1 : 0;
    if(shape.mLevel < maxLevel)
        return 1 + (shape.mVariant % 2);
    return 1 + (shape.mVariant % TreasurySettings::current().maxGems);
}

//! Edge 0 north, 1 east, 2 south, 3 west. It is open when both of its corners are lower than the pile
//! itself, i.e. it faces a wall, the outside of the room or a lower pile.
inline bool edgeOpen(const PileShape& shape, int edge)
{
    return shape.mCorner[edge] < shape.mLevel && shape.mCorner[(edge + 1) % 4] < shape.mLevel;
}

//! Slight overflow of a nearly full pile at its open edges: one coin (level 6) or spillCoinsFull (full) per open edge
inline int spillCoinsPerEdge(const PileShape& shape)
{
    if(shape.mLevel >= maxLevel)
        return TreasurySettings::current().spillCoinsFull;
    return shape.mLevel == maxLevel - 1 ? 1 : 0;
}

//! Whether an empty tile of a treasury gets a few scattered coins (a quarter of them, so the bare floor
//! of an empty room is dotted with coins without an object on every tile)
inline bool hasFloorScatter(int x, int y)
{
    return (x * 3 + y * 5) % 4 == 0;
}

//! Level of detail by distance: whether a pile at the given distance (tiles) from the camera uses the reduced mesh.
//! A pile that is reduced already stays so until it is clearly closer again (hysteresis), so it never flips.
inline bool lodReducedAt(bool currentlyReduced, float distance)
{
    const TreasurySettings& settings = TreasurySettings::current();
    float limit = settings.lodFarDistance;
    if(currentlyReduced)
        limit -= settings.lodHysteresis;
    return distance > limit;
}

//! The pot mesh used when the treasury detail option is off (one of the four classic stacks)
inline const char* classicMeshForLevel(int level)
{
    if(level <= 2)
        return "GoldstackLv1";
    if(level <= 4)
        return "GoldstackLv2";
    if(level <= 6)
        return "GoldstackLv3";
    return "GoldstackLv4";
}
}

#endif // TREASURYGOLDLAYER_H
