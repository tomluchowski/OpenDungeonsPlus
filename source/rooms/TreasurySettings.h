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

//! \brief The tunable values of the treasury gold layer and of the effects on it (config/treasury.cfg).
//!
//! The default of every member is the value the game used before the values moved into the configuration
//! file, so a missing file or a missing key changes nothing. The key of a member is its name with a capital
//! first letter ("levelHeight" is "LevelHeight"). Nothing here depends on the renderer or the server; the
//! pure rules in TreasuryGoldLayer.h and TreasuryCreatureRules.h read the values through current().
struct TreasurySettings
{
    // Gold layer (TreasuryGoldLayer.h)
    //! Height of a pile corner per fill step, in tile units
    float levelHeight = 0.055f;
    //! Coins lying on top of a full pile, and the lowest fill step that carries any
    int maxTopCoins = 8;
    int topCoinMinLevel = 2;
    //! Gems in the fullest piles (1..4, the pile has four variants)
    int maxGems = 2;
    //! Coins spilled per open edge of a full pile
    int spillCoinsFull = 1;
    //! Coins scattered on the bare floor of an empty tile
    int scatterCoins = 3;

    // Creatures and effects on the gold (TreasuryCreatureRules.h)
    //! From this fill step on the gold is deep enough for creatures to splash and clink in it
    int deepLevel = 4;
    //! Distance a creature walks between two splashes, in tiles
    float stepDistance = 0.55f;
    //! Seconds a splash counts against the budget of its room
    float splashLifetime = 1.4f;
    //! Seconds a worker spends climbing the pile, pouring out the gold and coming back down
    float pourDuration = 1.4f;
    //! Splashes a room may show at once at the full and at the reduced detail
    int splashBudgetFull = 6;
    int splashBudgetReduced = 2;
    //! Gold dust puffs a room may show at once at the full and at the reduced detail (0 turns the floating motes off),
    //! seconds between two attempts to start a puff and how long a puff counts against the budget of its room
    int dustBudgetFull = 0;
    int dustBudgetReduced = 0;
    float dustInterval = 0.7f;
    float dustLifetime = 3.0f;
    //! A keeper is rich when it holds this share of the treasury capacity and at least this much gold (dust over
    //! its portal and its heart)
    float portalRichShare = 0.5f;
    int portalRichMinGold = 500;
    //! Seconds between two attempts to start a sparkle, sliding coins or rolling coins, and how long such an
    //! effect counts against the budget of its room
    float ambientInterval = 0.9f;
    float ambientLifetime = 2.5f;
    //! Sparkles, sliding and rolling coins a room may show at once at the full and at the reduced detail
    int ambientBudgetFull = 3;
    int ambientBudgetReduced = 1;
    //! Piles of at least this fill step slide coins down their slope; the sparkle needs the richer pile
    int slideLevel = 3;
    int glintLevel = 5;
    //! Seconds a pile needs to settle to its new height after gold was added or taken (objects buried in the
    //! gold settle at the same rate), the height factor of the whole pile at the deepest point of the dip when
    //! it settles at the reduced detail and the share of the settle time after which that dip is deepest
    float pileSettleTime = 0.7f;
    float dentDepth = 0.88f;
    float dentShare = 0.35f;
    //! Local dent where gold is taken (full detail): radius in tile units and depth below the surface
    float dentRadius = 0.3f;
    float dentLocalDepth = 0.08f;
    //! Share of its height that an object sinks into a pile of the first fill step and into a full pile
    float buryShareFirst = 0.1f;
    float buryShareFull = 0.5f;

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

    //! Piles and floor gold heaps drawn again per frame after the "Treasury detail" option changed
    int rebuildPerFrame = 12;

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
        s.maxTopCoins = readInt(config, "MaxTopCoins", s.maxTopCoins, 0, 40);
        s.topCoinMinLevel = readInt(config, "TopCoinMinLevel", s.topCoinMinLevel, 1, 7);
        s.maxGems = readInt(config, "MaxGems", s.maxGems, 1, 4);
        s.spillCoinsFull = readInt(config, "SpillCoinsFull", s.spillCoinsFull, 0, 4);
        s.scatterCoins = readInt(config, "ScatterCoins", s.scatterCoins, 0, 8);
        s.deepLevel = readInt(config, "DeepLevel", s.deepLevel, 1, 7);
        s.stepDistance = readFloat(config, "StepDistance", s.stepDistance, 0.1f, 5.0f);
        s.splashLifetime = readFloat(config, "SplashLifetime", s.splashLifetime, 0.1f, 10.0f);
        s.pourDuration = readFloat(config, "PourDuration", s.pourDuration, 0.2f, 10.0f);
        s.splashBudgetFull = readInt(config, "SplashBudgetFull", s.splashBudgetFull, 0, 50);
        s.splashBudgetReduced = readInt(config, "SplashBudgetReduced", s.splashBudgetReduced, 0, 50);
        s.dustBudgetFull = readInt(config, "DustBudgetFull", s.dustBudgetFull, 0, 50);
        s.dustBudgetReduced = readInt(config, "DustBudgetReduced", s.dustBudgetReduced, 0, 50);
        s.dustInterval = readFloat(config, "DustInterval", s.dustInterval, 0.1f, 30.0f);
        s.dustLifetime = readFloat(config, "DustLifetime", s.dustLifetime, 0.1f, 30.0f);
        s.portalRichShare = readFloat(config, "PortalRichShare", s.portalRichShare, 0.0f, 1.0f);
        s.portalRichMinGold = readInt(config, "PortalRichMinGold", s.portalRichMinGold, 0, 1000000);
        s.ambientInterval = readFloat(config, "AmbientInterval", s.ambientInterval, 0.1f, 30.0f);
        s.ambientLifetime = readFloat(config, "AmbientLifetime", s.ambientLifetime, 0.1f, 30.0f);
        s.ambientBudgetFull = readInt(config, "AmbientBudgetFull", s.ambientBudgetFull, 0, 50);
        s.ambientBudgetReduced = readInt(config, "AmbientBudgetReduced", s.ambientBudgetReduced, 0, 50);
        s.slideLevel = readInt(config, "SlideLevel", s.slideLevel, 1, 7);
        s.glintLevel = readInt(config, "GlintLevel", s.glintLevel, 1, 7);
        s.pileSettleTime = readFloat(config, "PileSettleTime", s.pileSettleTime, 0.1f, 5.0f);
        s.dentDepth = readFloat(config, "DentDepth", s.dentDepth, 0.3f, 1.0f);
        s.dentShare = readFloat(config, "DentShare", s.dentShare, 0.05f, 0.95f);
        s.dentRadius = readFloat(config, "DentRadius", s.dentRadius, 0.05f, 0.5f);
        s.dentLocalDepth = readFloat(config, "DentLocalDepth", s.dentLocalDepth, 0.0f, 0.3f);
        s.buryShareFirst = readFloat(config, "BuryShareFirst", s.buryShareFirst, 0.0f, 1.0f);
        s.buryShareFull = readFloat(config, "BuryShareFull", s.buryShareFull, 0.0f, 1.0f);
        s.batchRebuildInterval = readFloat(config, "BatchRebuildInterval", s.batchRebuildInterval, 0.05f, 5.0f);
        s.lodFarDistance = readFloat(config, "LodFarDistance", s.lodFarDistance, 1.0f, 500.0f);
        s.lodHysteresis = readFloat(config, "LodHysteresis", s.lodHysteresis, 0.0f, 100.0f);
        s.lodInterval = readFloat(config, "LodInterval", s.lodInterval, 0.1f, 10.0f);
        s.lodSwitchesPerUpdate = readInt(config, "LodSwitchesPerUpdate", s.lodSwitchesPerUpdate, 1, 100);
        s.rebuildPerFrame = readInt(config, "RebuildPerFrame", s.rebuildPerFrame, 1, 500);
        return s;
    }
};

#endif // TREASURYSETTINGS_H
