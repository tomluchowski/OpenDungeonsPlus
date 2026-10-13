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

bool HatcheryCycle::needCoopRooster(const HatcheryCounts& counts, uint32_t nbCoops)
{
    return (nbCoops > 0) && (counts.mRoosters == 0);
}

bool HatcheryCycle::canHatch(const HatcheryCounts& counts, bool enemiesPresent)
{
    return eggsMayHatch(counts) && !enemiesPresent;
}

bool HatcheryCycle::wellCared(const HatcheryCare& care)
{
    return care.mClaimed && care.mLit && !care.mEnemies;
}

HatcheryCycleSettings HatcheryCycle::withCare(const HatcheryCycleSettings& settings, const HatcheryCare& care)
{
    if(!wellCared(care))
        return settings;

    uint32_t percent = std::min<uint32_t>(settings.mCareLayPercent, 90);
    return scaled(settings, (100 - percent) / 100.0);
}

bool HatcheryCycle::tramples(const HatcheryCycleSettings& settings, bool enemyCreature, bool isEgg, uint32_t roll)
{
    return enemyCreature && isEgg && (roll < std::min<uint32_t>(settings.mTramplePercent, 100));
}

uint32_t HatcheryCycle::layInterval(const HatcheryCycleSettings& settings, uint32_t random)
{
    uint32_t minTurns = std::max<uint32_t>(1, settings.mLayMin);
    uint32_t maxTurns = std::max(minTurns, settings.mLayMax);
    return minTurns + (random % (maxTurns - minTurns + 1));
}

HatcheryCycleSettings HatcheryCycle::scaled(const HatcheryCycleSettings& settings, double factor)
{
    HatcheryCycleSettings ret = settings;
    ret.mLayMin = std::max<uint32_t>(1, static_cast<uint32_t>(std::lround(settings.mLayMin * factor)));
    ret.mLayMax = std::max(ret.mLayMin, static_cast<uint32_t>(std::lround(settings.mLayMax * factor)));
    return ret;
}
