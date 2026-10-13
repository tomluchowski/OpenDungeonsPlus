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

#ifndef TREASURYCREATURERULES_H
#define TREASURYCREATURERULES_H

#include "render/TreasuryGoldMesh.h"
#include "rooms/TreasuryGoldLayer.h"
#include "rooms/TreasurySettings.h"

#include <cmath>
#include <map>

//! \brief Client side rules for the effects on the treasury gold: how a pile settles when gold is added or taken,
//! how deep objects stand in the gold, when coins slide, roll or sparkle and how many effects a room may show at
//! once. Nothing here touches the renderer, the server or the creature position used for pathing.
//! The tunable numbers live in config/treasury.cfg (see TreasurySettings.h); the constants below are references
//! to the live values, so a value is read when it is used.
namespace TreasuryCreatureRules
{
//! A smooth ramp from 0 at edgeStart to 1 at edgeEnd (cubic, flat at both ends)
inline float smoothStep(float edgeStart, float edgeEnd, float t)
{
    if(t <= edgeStart)
        return 0.0f;
    if(t >= edgeEnd)
        return 1.0f;
    const float x = (t - edgeStart) / (edgeEnd - edgeStart);
    return x * x * (3.0f - 2.0f * x);
}

//! Sparkles and sliding coins on the gold of rich treasuries, and the coins that roll away when gold is taken:
//! seconds between two attempts, how long such an effect counts against the budget of its room, and how many
//! a room may show at once by the "Treasury detail" option. They have a budget of their own.
static const float& ambientInterval = TreasurySettings::current().ambientInterval;
static const float& ambientLifetime = TreasurySettings::current().ambientLifetime;
static const int& ambientBudgetFull = TreasurySettings::current().ambientBudgetFull;
static const int& ambientBudgetReduced = TreasurySettings::current().ambientBudgetReduced;
//! Piles of at least this level slide coins down their slope; the sparkle needs a richer pile
static const int& slideLevel = TreasurySettings::current().slideLevel;
static const int& glintLevel = TreasurySettings::current().glintLevel;

//! Number of ambient effects one room may show at once at the given "Treasury detail" option
inline int ambientBudget(TreasuryGoldMesh::Detail detail)
{
    switch(detail)
    {
        case TreasuryGoldMesh::Detail::full:
            return ambientBudgetFull;
        case TreasuryGoldMesh::Detail::reduced:
            return ambientBudgetReduced;
        default:
            return 0;
    }
}

//! A pile that grows (gold delivered) or sinks (gold taken) is not swapped at once: its height settles over
//! this many seconds. When taking, it dips into a dent first and then smooths out.
static const float& pileSettleTime = TreasurySettings::current().pileSettleTime;
static const float& dentDepth = TreasurySettings::current().dentDepth;
static const float& dentShare = TreasurySettings::current().dentShare;
//! Where gold is taken the pile also gets a local dent (full detail): radius and depth in tile units
static const float& dentRadius = TreasurySettings::current().dentRadius;
static const float& dentLocalDepth = TreasurySettings::current().dentLocalDepth;

//! Height factor of a settling pile (1 = the new pile) at time t, coming from the factor "from" (the old level
//! over the new one, limited). Growing starts low and rises; taking starts high, dips below 1 and returns.
inline float pileSettleScale(float from, bool taken, float t)
{
    if(t >= pileSettleTime)
        return 1.0f;
    const float progress = t / pileSettleTime;
    if(!taken)
        return from + (1.0f - from) * smoothStep(0.0f, 1.0f, progress);
    if(progress < dentShare)
        return from + (dentDepth - from) * smoothStep(0.0f, dentShare, progress);
    return dentDepth + (1.0f - dentDepth) * smoothStep(dentShare, 1.0f, progress);
}

//! Share (0..1) of the full depth of the local dent at time t: it forms quickly (until dentShare of the settle
//! time) and then fills up again, so it is gone when the pile has settled
inline float localDentFactor(float t)
{
    const float progress = t / pileSettleTime;
    return smoothStep(0.0f, dentShare, progress) * (1.0f - smoothStep(dentShare, 1.0f, progress));
}

//! The start height factor of a settling pile is kept in this range, so that any change of the level looks sane
static const float settleFromMin = 0.4f;
static const float settleFromMax = 1.6f;

//! The old level over the new one as a start height factor, kept in a range that looks sane for any change
inline float pileSettleFrom(int oldLevel, int newLevel)
{
    if(oldLevel <= 0 || newLevel <= 0)
        return 1.0f;
    const float ratio = static_cast<float>(oldLevel) / static_cast<float>(newLevel);
    if(ratio < settleFromMin)
        return settleFromMin;
    if(ratio > settleFromMax)
        return settleFromMax;
    return ratio;
}

//! Objects standing in the gold (room objects, gold lying on the floor of a treasury) are drawn partly buried:
//! the deeper the pile of their tile, the larger the share of their height that sinks into the gold. Only a
//! render offset of the object node; position, bounds and paths of the object are not changed.
static const float& buryShareFirst = TreasurySettings::current().buryShareFirst;
static const float& buryShareFull = TreasurySettings::current().buryShareFull;
//! The offset settles at this rate (seconds for roughly 95 percent of a change), as fast as the piles themselves
static const float& buriedSettleTime = TreasurySettings::current().pileSettleTime;
//! A smaller remaining distance is closed at once
static const float buriedSnapDistance = 0.002f;

//! Share of its height (0..1) that an object sinks into a pile of the given level
inline float buryShare(int level)
{
    if(level <= 0)
        return 0.0f;
    const int top = level > TreasuryGoldLayer::maxLevel ? TreasuryGoldLayer::maxLevel : level;
    return buryShareFirst + (buryShareFull - buryShareFirst) * static_cast<float>(top - 1)
        / static_cast<float>(TreasuryGoldLayer::maxLevel - 1);
}

//! Height of the base of an object above the floor, so that the share of its height given by buryShare()
//! is buried below the gold surface. Never below the floor, and always zero where there is no pile.
inline float buriedLift(float surfaceHeight, int level, float objectHeight)
{
    if(level <= 0 || surfaceHeight <= 0.0f)
        return 0.0f;
    const float lift = surfaceHeight - buryShare(level) * (objectHeight > 0.0f ? objectHeight : 0.0f);
    return lift > 0.0f ? lift : 0.0f;
}

//! One step of the smooth move of the offset from current towards target (elapsed seconds)
inline float buriedStep(float current, float target, float elapsed)
{
    const float rate = 1.0f - std::exp(-3.0f * elapsed / buriedSettleTime);
    const float next = current + (target - current) * rate;
    return std::fabs(target - next) < buriedSnapDistance ? target : next;
}

//! The settled piles of a room are drawn as one static batch per room (see TreasuryGoldBatch.h).
//! A changed room is rebuilt at once when it was not rebuilt for this long, otherwise as soon as this time is over
static const float& batchRebuildInterval = TreasurySettings::current().batchRebuildInterval;

//! Level of detail by distance: whether a pile at the given distance (tiles) from the camera uses the reduced mesh.
//! A pile that is reduced already stays so until it is clearly closer again (hysteresis), so it never flips.
static const float& lodFarDistance = TreasurySettings::current().lodFarDistance;
static const float& lodHysteresis = TreasurySettings::current().lodHysteresis;
static const float& lodInterval = TreasurySettings::current().lodInterval;
static const int& lodSwitchesPerUpdate = TreasurySettings::current().lodSwitchesPerUpdate;

//! Piles and floor gold heaps rebuilt per frame after the "Treasury detail" option changed
static const int& rebuildPerFrame = TreasurySettings::current().rebuildPerFrame;

//! True when a pile at the given distance uses the reduced mesh (a reduced pile only leaves it again below the
//! hysteresis distance)
inline bool lodReducedAt(bool currentlyReduced, float distance)
{
    return distance > (currentlyReduced ? lodFarDistance - lodHysteresis : lodFarDistance);
}

//! Counts the splashes shown per room (the room is identified by any pointer)
class SplashBudget
{
public:
    //! Takes one place for the room. False when the room already shows limit splashes.
    bool tryAcquire(const void* room, int limit)
    {
        if(limit <= 0)
            return false;
        int& count = mActive[room];
        if(count >= limit)
            return false;
        ++count;
        return true;
    }

    //! Gives one place of the room back
    void release(const void* room)
    {
        std::map<const void*, int>::iterator it = mActive.find(room);
        if(it == mActive.end())
            return;
        --it->second;
        if(it->second <= 0)
            mActive.erase(it);
    }

    //! Number of places of the room in use
    int active(const void* room) const
    {
        std::map<const void*, int>::const_iterator it = mActive.find(room);
        return it == mActive.end() ? 0 : it->second;
    }

    //! Forgets every place of every room
    void clear()
    {
        mActive.clear();
    }

private:
    std::map<const void*, int> mActive;
};
}

#endif // TREASURYCREATURERULES_H
