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

#include "rooms/HatcheryCycle.h"

#include <algorithm>
#include <cmath>

uint32_t HatcheryCycle::capacity(uint32_t nbTiles, uint32_t nbCoops, const HatcheryCycleSettings& settings)
{
    uint32_t tilesPerChicken = std::max<uint32_t>(1, settings.mTilesPerChicken);
    return std::min(nbCoops, nbTiles / tilesPerChicken);
}

bool HatcheryCycle::canLay(const HatcheryCounts& counts, uint32_t capacity)
{
    return counts.population() < capacity;
}

bool HatcheryCycle::eggsMayHatch(const HatcheryCounts& counts)
{
    return counts.mRoosters > 0;
}

bool HatcheryCycle::needCoopHen(const HatcheryCounts& counts, uint32_t nbCoops)
{
    return (nbCoops > 0) && (counts.population() == 0);
}

uint32_t HatcheryCycle::coopHenCount(const HatcheryCycleSettings& settings, uint32_t capacity)
{
    uint32_t count = (settings.mCoopBatch == 0) ? capacity : std::min(settings.mCoopBatch, capacity);
    return std::max<uint32_t>(1, count);
}

bool HatcheryCycle::needCoopRooster(const HatcheryCounts& counts, uint32_t nbCoops)
{
    return (nbCoops > 0) && (counts.mRoosters == 0);
}

bool HatcheryCycle::needFight(const HatcheryCounts& counts)
{
    return counts.mRoosters >= 2;
}

uint32_t HatcheryCycle::fightWinner(uint32_t random)
{
    return random % 2;
}

bool HatcheryCycle::fightContinues(bool firstPresent, bool secondPresent)
{
    return firstPresent && secondPresent;
}

bool HatcheryCycle::canHatch(const HatcheryCounts& counts, bool enemiesPresent)
{
    return eggsMayHatch(counts) && !enemiesPresent;
}

uint32_t HatcheryCycle::walkTurns(double distance, double tilesPerTurn)
{
    if((distance <= 0.0) || (tilesPerTurn <= 0.0))
        return 0;

    return static_cast<uint32_t>(std::ceil(distance / tilesPerTurn));
}

bool HatcheryCycle::tripDue(uint32_t remaining, uint32_t walk, const HatcheryCycleSettings& settings)
{
    return remaining <= walk + settings.mLayShowTurns;
}

bool HatcheryCycle::walkFits(uint32_t walk, uint32_t remaining, const HatcheryCycleSettings& settings)
{
    return walk + settings.mLayShowTurns <= remaining;
}

uint32_t HatcheryCycle::carePercent(const HatcheryCycleSettings& settings, const HatcheryCare& care)
{
    if(!care.mClaimed)
        return 0;

    uint32_t percent = 0;
    if(care.mLit)
        percent += settings.mCareLightPercent;
    if(!care.mEnemies)
        percent += settings.mCareCalmPercent;
    return std::min<uint32_t>(percent, 90);
}

HatcheryCycleSettings HatcheryCycle::withCare(const HatcheryCycleSettings& settings, const HatcheryCare& care)
{
    uint32_t percent = carePercent(settings, care);
    if(percent == 0)
        return settings;

    HatcheryCycleSettings ret = settings;
    ret.mLayFactor = settings.mLayFactor * ((100 - percent) / 100.0);
    return ret;
}

bool HatcheryCycle::tramples(const HatcheryCycleSettings& settings, bool enemyCreature, bool isEgg, uint32_t roll)
{
    return enemyCreature && isEgg && (roll < std::min<uint32_t>(settings.mTramplePercent, 100));
}

int32_t HatcheryCycle::pickNestPlace(const std::vector<bool>& occupied, uint32_t slotsPerNest)
{
    if(slotsPerNest == 0)
        return -1;

    const uint32_t nbNests = static_cast<uint32_t>(occupied.size()) / slotsPerNest;
    int32_t bestNest = -1;
    uint32_t bestCount = 0;
    for(uint32_t nest = 0; nest < nbNests; ++nest)
    {
        uint32_t count = 0;
        for(uint32_t slot = 0; slot < slotsPerNest; ++slot)
        {
            if(occupied[nest * slotsPerNest + slot])
                ++count;
        }
        if(count >= slotsPerNest)
            continue;
        if((bestNest < 0) || (count < bestCount))
        {
            bestNest = static_cast<int32_t>(nest);
            bestCount = count;
        }
    }
    if(bestNest < 0)
        return -1;

    for(uint32_t slot = 0; slot < slotsPerNest; ++slot)
    {
        const uint32_t index = static_cast<uint32_t>(bestNest) * slotsPerNest + slot;
        if(!occupied[index])
            return static_cast<int32_t>(index);
    }
    return -1;
}

uint32_t HatcheryCycle::layInterval(const HatcheryCycleSettings& settings, uint32_t random)
{
    uint32_t minTurns = std::max<uint32_t>(1, settings.mLayMin);
    uint32_t maxTurns = std::max(minTurns, settings.mLayMax);
    const uint32_t span = maxTurns - minTurns + 1;
    const double value = (minTurns + (random % span)) * std::max(0.0, settings.mLayFactor);
    const uint32_t whole = static_cast<uint32_t>(value);
    // The fraction decides by chance whether the time is rounded up, so the average time is exact
    const bool roundUp = static_cast<double>((random / span) % 1000) < (value - whole) * 1000.0;
    return std::max<uint32_t>(1, whole + (roundUp ? 1 : 0));
}

HatcheryCycleSettings HatcheryCycle::scaled(const HatcheryCycleSettings& settings, double factor)
{
    HatcheryCycleSettings ret = settings;
    ret.mLayMin = std::max<uint32_t>(1, static_cast<uint32_t>(std::lround(settings.mLayMin * factor)));
    ret.mLayMax = std::max(ret.mLayMin, static_cast<uint32_t>(std::lround(settings.mLayMax * factor)));
    return ret;
}
