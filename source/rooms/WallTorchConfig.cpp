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

#include "rooms/WallTorchConfig.h"

#include "utils/ConfigManager.h"
#include "utils/LogManager.h"

#include <algorithm>
#include <set>
#include <string>

namespace
{
//! Reads a value of the rooms configuration. A missing key gives the default and is logged once.
double readValue(const std::string& key, double defaultValue)
{
    static std::set<std::string> reported;
    ConfigManager& config = ConfigManager::getSingleton();
    if(!config.hasRoomConfig(key))
    {
        if(reported.insert(key).second)
            OD_LOG_WRN("Wall torch setting " + key + " is missing in the rooms configuration, using the default");

        return defaultValue;
    }

    return config.getRoomConfigDouble(key);
}

uint32_t readCount(const std::string& key, uint32_t defaultValue, uint32_t minValue)
{
    double value = readValue(key, static_cast<double>(defaultValue));
    if(value < static_cast<double>(minValue))
        return minValue;

    return static_cast<uint32_t>(value);
}

double readClamped(const std::string& key, double defaultValue, double minValue, double maxValue)
{
    return std::max(minValue, std::min(maxValue, readValue(key, defaultValue)));
}
}

WallTorchConfig::WallTorchConfig() :
    mActiveLights(4),
    mActiveLightsReduced(2),
    mLightRadius(6.0),
    mLightColorR(1.0),
    mLightColorG(0.62),
    mLightColorB(0.28),
    mLightIntensity(1.0),
    mFlickerStrength(0.25),
    mFlickerSpeed(2.3)
{
}

WallTorchConfig WallTorchConfig::load()
{
    WallTorchConfig result;
    result.mPlacement.mRoomSideDivisor = readCount("WallTorchRoomSideDivisor", 4, 1);
    result.mPlacement.mCorridorSpacing = readCount("WallTorchCorridorSpacing", 4, 1);
    result.mPlacement.mMinDistance = readCount("WallTorchMinDistance", 3, 1);
    result.mActiveLights = readCount("WallTorchActiveLights", 4, 0);
    result.mActiveLightsReduced = readCount("WallTorchActiveLightsReduced", 2, 0);
    result.mLightRadius = readClamped("WallTorchLightRadius", 6.0, 0.5, 64.0);
    result.mLightColorR = readClamped("WallTorchLightColorR", 1.0, 0.0, 1.0);
    result.mLightColorG = readClamped("WallTorchLightColorG", 0.62, 0.0, 1.0);
    result.mLightColorB = readClamped("WallTorchLightColorB", 0.28, 0.0, 1.0);
    result.mLightIntensity = readClamped("WallTorchLightIntensity", 1.0, 0.0, 10.0);
    result.mFlickerStrength = readClamped("WallTorchFlickerStrength", 0.25, 0.0, 1.0);
    result.mFlickerSpeed = readClamped("WallTorchFlickerSpeed", 2.3, 0.0, 20.0);
    return result;
}
