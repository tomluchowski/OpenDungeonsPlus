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

#include <cmath>
#include <cstdlib>
#include <string>

//! \brief The gold layer of a treasury: one pile per tile whose height follows the gold stored
//! on it, in TreasuryGoldLayer::maxLevel steps, blending into the piles of the neighbouring tiles.
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
    return 0.055f * static_cast<float>(level);
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

//! The part of the pile that does not depend on where it lies in the room. The corners hold the
//! level of the pile at that corner (the lowest level of the up to four tiles that meet there), so
//! two tiles sharing an edge share the heights along it and no tile border is visible.
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

//! Surface height of a pile at (u, v), both 0..1 across the tile (u towards +x, v towards +y).
//! The edges only depend on the corners; the middle rises to the level of the tile itself.
inline float heightAt(const PileShape& shape, float u, float v)
{
    if(shape.mLevel <= 0)
        return 0.0f;

    const float nw = levelHeight(shape.mCorner[0]);
    const float ne = levelHeight(shape.mCorner[1]);
    const float se = levelHeight(shape.mCorner[2]);
    const float sw = levelHeight(shape.mCorner[3]);
    // v grows to the north: top row is north
    const float base = (1.0f - u) * ((1.0f - v) * sw + v * nw) + u * ((1.0f - v) * se + v * ne);

    // 0 on the four edges, 1 in the middle
    const float bump = 16.0f * u * (1.0f - u) * v * (1.0f - v);
    const float peak = levelHeight(shape.mLevel);
    float height = base + (peak - base) * std::sqrt(bump);

    // A few lumps, only on fuller piles, fading out towards the edges
    if(shape.mLevel >= 3)
    {
        const float phase = 1.7f * static_cast<float>(shape.mVariant);
        const float lumps = std::sin(9.0f * u + phase) * std::sin(8.0f * v + 2.0f * phase);
        height += 0.012f * static_cast<float>(shape.mLevel) * bump * lumps;
    }

    // Keep the layer a hair above the floor so it never flickers against it
    if(height < 0.0f)
        height = 0.0f;
    return height + 0.004f;
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
