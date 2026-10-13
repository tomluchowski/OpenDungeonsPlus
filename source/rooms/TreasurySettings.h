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

#ifndef TREASURYSETTINGS_H
#define TREASURYSETTINGS_H

#include <cstdlib>
#include <map>
#include <string>

//! \brief The tunable values of the treasury gold layer (config/treasury.cfg).
//!
//! The default of every member is the value the game used before the values moved into the configuration
//! file, so a missing file or a missing key changes nothing. The key of a member is its name with a capital
//! first letter ("levelHeight" is "LevelHeight"). Nothing here depends on the renderer or the server; the
//! pure rules in TreasuryGoldLayer.h read the values through current().
struct TreasurySettings
{
    // Gold layer (TreasuryGoldLayer.h)
    //! Height of a pile corner per fill step, in tile units
    float levelHeight = 0.055f;

    // Draw batches (TreasuryGoldBatch.h)
    //! Seconds between two rebuilds of the draw batch of a changed room
    float batchRebuildInterval = 0.25f;

    // Level of detail by distance to the camera: piles farther away than lodFarDistance (tiles) use the reduced
    // mesh (no coins and gems), they return to the full mesh below lodFarDistance - lodHysteresis, so they never
    // flip back and forth. The check runs every lodInterval seconds and switches at most lodSwitchesPerUpdate
    // piles per run, so a camera jump is spread over several runs.
    float lodFarDistance = 22.0f;
    float lodHysteresis = 4.0f;
    float lodInterval = 0.5f;
    int lodSwitchesPerUpdate = 10;

    //! Piles drawn again per frame after the "Treasury detail" option changed
    int rebuildPerFrame = 40;

    typedef std::map<std::string, std::string> Config;

    //! \brief The values of the game. Written once when the configuration is loaded.
    static TreasurySettings& current()
    {
        static TreasurySettings settings;
        return settings;
    }

    //! \brief The value of the key as a number limited to [low, high], or the given value when the key is missing.
    static float readFloat(const Config& config, const char* key, float value, float low, float high)
    {
        Config::const_iterator it = config.find(key);
        if(it == config.end())
            return value;

        float parsed = static_cast<float>(std::atof(it->second.c_str()));
        if(parsed < low)
            parsed = low;
        else if(parsed > high)
            parsed = high;
        return parsed;
    }

    //! \brief The value of the key as an integer limited to [low, high], or the given value when the key is missing.
    static int readInt(const Config& config, const char* key, int value, int low, int high)
    {
        Config::const_iterator it = config.find(key);
        if(it == config.end())
            return value;

        int parsed = std::atoi(it->second.c_str());
        if(parsed < low)
            parsed = low;
        else if(parsed > high)
            parsed = high;
        return parsed;
    }

    //! \brief Values missing in the configuration keep their default, values out of range are limited.
    static TreasurySettings fromConfig(const Config& config)
    {
        TreasurySettings s;
        s.levelHeight = readFloat(config, "LevelHeight", s.levelHeight, 0.01f, 0.2f);
        s.batchRebuildInterval = readFloat(config, "BatchRebuildInterval", s.batchRebuildInterval, 0.05f, 5.0f);
        s.lodFarDistance = readFloat(config, "LodFarDistance", s.lodFarDistance, 1.0f, 500.0f);
        s.lodHysteresis = readFloat(config, "LodHysteresis", s.lodHysteresis, 0.0f, 100.0f);
        s.lodInterval = readFloat(config, "LodInterval", s.lodInterval, 0.1f, 10.0f);
        s.lodSwitchesPerUpdate = readInt(config, "LodSwitchesPerUpdate", s.lodSwitchesPerUpdate, 1, 100);
        s.rebuildPerFrame = readInt(config, "RebuildPerFrame", s.rebuildPerFrame, 1, 1000);
        return s;
    }
};

#endif // TREASURYSETTINGS_H
