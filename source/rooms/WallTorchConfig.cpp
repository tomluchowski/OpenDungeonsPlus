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
//! Smallest value of the placement settings (divisor, spacing, distance)
const uint32_t MIN_PLACEMENT_VALUE = 1;
//! Smallest number of torches with light or sound
const uint32_t MIN_LIGHT_COUNT = 0;
//! Smallest range of the torch light in tiles
const double MIN_LIGHT_RADIUS = 0.5;
//! Largest range of the torch light in tiles
const double MAX_LIGHT_RADIUS = 64.0;
//! Smallest value of the colour parts, the intensity, the flicker strength and the flicker speed
const double MIN_FRACTION = 0.0;
//! Largest value of the colour parts and the flicker strength
const double MAX_FRACTION = 1.0;
//! Largest intensity of the torch light
const double MAX_LIGHT_INTENSITY = 10.0;
//! Largest speed of the torch flicker
const double MAX_FLICKER_SPEED = 20.0;

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

//! Reads a count of the rooms configuration. A missing key gives the default, a value below minValue gives minValue.
uint32_t readCount(const std::string& key, uint32_t defaultValue, uint32_t minValue)
{
    double value = readValue(key, static_cast<double>(defaultValue));
    if(value < static_cast<double>(minValue))
        return minValue;

    return static_cast<uint32_t>(value);
}

//! Reads a value of the rooms configuration and keeps it between minValue and maxValue. A missing key gives the default.
double readClamped(const std::string& key, double defaultValue, double minValue, double maxValue)
{
    return std::max(minValue, std::min(maxValue, readValue(key, defaultValue)));
}
}

//! \brief Creates the configuration with the default values (the same as in config/rooms.cfg)
WallTorchConfig::WallTorchConfig() :
    mActiveLights(DEFAULT_TORCH_ACTIVE_LIGHTS),
    mActiveLightsReduced(DEFAULT_TORCH_ACTIVE_LIGHTS_REDUCED),
    mSoundLoops(DEFAULT_TORCH_SOUND_LOOPS),
    mLightRadius(DEFAULT_TORCH_LIGHT_RADIUS),
    mLightColorR(DEFAULT_TORCH_LIGHT_COLOR_R),
    mLightColorG(DEFAULT_TORCH_LIGHT_COLOR_G),
    mLightColorB(DEFAULT_TORCH_LIGHT_COLOR_B),
    mLightIntensity(DEFAULT_TORCH_LIGHT_INTENSITY),
    mFlickerStrength(DEFAULT_TORCH_FLICKER_STRENGTH),
    mFlickerSpeed(DEFAULT_TORCH_FLICKER_SPEED)
{
}

WallTorchConfig WallTorchConfig::load()
{
    WallTorchConfig result;
    result.mPlacement.mRoomSideDivisor = readCount("WallTorchRoomSideDivisor", DEFAULT_TORCH_ROOM_SIDE_DIVISOR, MIN_PLACEMENT_VALUE);
    result.mPlacement.mCorridorSpacing = readCount("WallTorchCorridorSpacing", DEFAULT_TORCH_CORRIDOR_SPACING, MIN_PLACEMENT_VALUE);
    result.mPlacement.mMinDistance = readCount("WallTorchMinDistance", DEFAULT_TORCH_MIN_DISTANCE, MIN_PLACEMENT_VALUE);
    result.mActiveLights = readCount("WallTorchActiveLights", DEFAULT_TORCH_ACTIVE_LIGHTS, MIN_LIGHT_COUNT);
    result.mActiveLightsReduced = readCount("WallTorchActiveLightsReduced", DEFAULT_TORCH_ACTIVE_LIGHTS_REDUCED, MIN_LIGHT_COUNT);
    result.mSoundLoops = readCount("WallTorchSoundLoops", DEFAULT_TORCH_SOUND_LOOPS, MIN_LIGHT_COUNT);
    result.mLightRadius = readClamped("WallTorchLightRadius", DEFAULT_TORCH_LIGHT_RADIUS, MIN_LIGHT_RADIUS, MAX_LIGHT_RADIUS);
    result.mLightColorR = readClamped("WallTorchLightColorR", DEFAULT_TORCH_LIGHT_COLOR_R, MIN_FRACTION, MAX_FRACTION);
    result.mLightColorG = readClamped("WallTorchLightColorG", DEFAULT_TORCH_LIGHT_COLOR_G, MIN_FRACTION, MAX_FRACTION);
    result.mLightColorB = readClamped("WallTorchLightColorB", DEFAULT_TORCH_LIGHT_COLOR_B, MIN_FRACTION, MAX_FRACTION);
    result.mLightIntensity = readClamped("WallTorchLightIntensity", DEFAULT_TORCH_LIGHT_INTENSITY, MIN_FRACTION, MAX_LIGHT_INTENSITY);
    result.mFlickerStrength = readClamped("WallTorchFlickerStrength", DEFAULT_TORCH_FLICKER_STRENGTH, MIN_FRACTION, MAX_FRACTION);
    result.mFlickerSpeed = readClamped("WallTorchFlickerSpeed", DEFAULT_TORCH_FLICKER_SPEED, MIN_FRACTION, MAX_FLICKER_SPEED);
    return result;
}
