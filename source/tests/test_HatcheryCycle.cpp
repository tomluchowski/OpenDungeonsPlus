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

// Header-only Boost.Test, see test_DungeonHeartTier.cpp.
// OVERRIDE_BOOST_TEST_INCLUDED_WARNING
#define BOOST_TEST_MODULE HatcheryCycle
#include <boost/test/included/unit_test.hpp>

#include "rooms/HatcheryCycle.h"

#include <cstdlib>
#include <iostream>
#include <vector>

BOOST_AUTO_TEST_CASE(test_Capacity)
{
    HatcheryCycleSettings settings;
    BOOST_CHECK_EQUAL(HatcheryCycle::capacity(9, 3, settings), 3u);
    BOOST_CHECK_EQUAL(HatcheryCycle::capacity(2, 3, settings), 2u);
    BOOST_CHECK_EQUAL(HatcheryCycle::capacity(9, 0, settings), 0u);
    settings.mTilesPerChicken = 4;
    BOOST_CHECK_EQUAL(HatcheryCycle::capacity(9, 3, settings), 2u);
    settings.mTilesPerChicken = 0;
    BOOST_CHECK_EQUAL(HatcheryCycle::capacity(9, 3, settings), 3u);
}

BOOST_AUTO_TEST_CASE(test_Rules)
{
    HatcheryCounts counts;
    BOOST_CHECK(HatcheryCycle::needCoopHen(counts, 2));
    BOOST_CHECK(!HatcheryCycle::needCoopHen(counts, 0));
    BOOST_CHECK(HatcheryCycle::needCoopRooster(counts, 2));
    BOOST_CHECK(!HatcheryCycle::needCoopRooster(counts, 0));
    BOOST_CHECK(!HatcheryCycle::eggsMayHatch(counts));

    // An egg or a chick is a source: no hen from the coop. The rooster does not count.
    counts.mEggs = 1;
    BOOST_CHECK(!HatcheryCycle::needCoopHen(counts, 2));
    counts.mEggs = 0;
    counts.mRoosters = 1;
    BOOST_CHECK(HatcheryCycle::needCoopHen(counts, 2));
    BOOST_CHECK(!HatcheryCycle::needCoopRooster(counts, 2));
    BOOST_CHECK(HatcheryCycle::eggsMayHatch(counts));

    // Full hatchery: hens stop laying. Chicks and eggs fill the capacity, the rooster does not.
    counts.mHens = 1;
    counts.mChicks = 1;
    counts.mEggs = 1;
    BOOST_CHECK(!HatcheryCycle::canLay(counts, 3));
    BOOST_CHECK(HatcheryCycle::canLay(counts, 4));
}

BOOST_AUTO_TEST_CASE(test_LayInterval)
{
    HatcheryCycleSettings settings;
    for(uint32_t random = 0; random < 100; ++random)
    {
        uint32_t turns = HatcheryCycle::layInterval(settings, random);
        BOOST_CHECK(turns >= settings.mLayMin);
        BOOST_CHECK(turns <= settings.mLayMax);
    }
    settings.mLayMin = 0;
    settings.mLayMax = 0;
    BOOST_CHECK_EQUAL(HatcheryCycle::layInterval(settings, 7), 1u);

    HatcheryCycleSettings slow;
    slow.mLayMin = 4;
    slow.mLayMax = 8;
    HatcheryCycleSettings faster = HatcheryCycle::scaled(slow, 0.5);
    BOOST_CHECK_EQUAL(faster.mLayMin, 2u);
    BOOST_CHECK_EQUAL(faster.mLayMax, 4u);
}

namespace
{
//! Small deterministic random generator so both models see the same demand.
struct Lcg
{
    explicit Lcg(uint32_t seed) : mState(seed) {}
    uint32_t next()
    {
        mState = mState * 1664525u + 1013904223u;
        return mState >> 8;
    }
    uint32_t mState;
};

//! Model of the spawning before the life cycle: when below the number of coops, a refill round
//! after refillTurns turns brings the chickens back to the number of coops.
uint32_t simulateOld(uint32_t nbCoops, uint32_t eatPercent, uint32_t nbTurns, uint32_t refillTurns)
{
    Lcg demand(12345);
    uint32_t chickens = nbCoops;
    uint32_t cooldown = 0;
    uint32_t eaten = 0;
    for(uint32_t turn = 0; turn < nbTurns; ++turn)
    {
        if((demand.next() % 100 < eatPercent) && (chickens > 0))
        {
            --chickens;
            ++eaten;
        }
        if(chickens >= nbCoops)
            continue;
        ++cooldown;
        if(cooldown < refillTurns)
            continue;
        chickens = nbCoops;
        cooldown = 0;
    }
    return eaten;
}

struct SimAnimal
{
    SimAnimal(uint32_t timer) : mTimer(timer) {}
    uint32_t mTimer;
};

//! Model of the life cycle with the rules of HatcheryCycle, in the order the hatchery handles them.
uint32_t simulateNew(uint32_t nbCoops, uint32_t eatPercent, uint32_t nbTurns, const HatcheryCycleSettings& settings)
{
    Lcg demand(12345);
    Lcg rng(777);
    std::vector<SimAnimal> hens;
    std::vector<SimAnimal> chicks;
    std::vector<SimAnimal> eggs;
    uint32_t roosters = 1;
    uint32_t coopWait = 0;
    uint32_t roosterWait = 0;
    uint32_t capacity = HatcheryCycle::capacity(nbCoops, nbCoops, settings);
    for(uint32_t i = 0; i < nbCoops; ++i)
        hens.push_back(SimAnimal(HatcheryCycle::layInterval(settings, rng.next())));
    uint32_t eaten = 0;
    for(uint32_t turn = 0; turn < nbTurns; ++turn)
    {
        if((demand.next() % 100 < eatPercent) && !hens.empty())
        {
            hens.erase(hens.begin());
            ++eaten;
        }

        HatcheryCounts counts;
        counts.mHens = hens.size();
        counts.mChicks = chicks.size();
        counts.mEggs = eggs.size();
        counts.mRoosters = roosters;

        // Laying
        for(size_t i = 0; i < hens.size(); ++i)
        {
            if(hens[i].mTimer > 1)
            {
                --hens[i].mTimer;
                continue;
            }
            hens[i].mTimer = HatcheryCycle::layInterval(settings, rng.next());
            if(!HatcheryCycle::canLay(counts, capacity))
                continue;
            eggs.push_back(SimAnimal(0));
            ++counts.mEggs;
        }
        // Hatching
        if(HatcheryCycle::eggsMayHatch(counts))
        {
            std::vector<SimAnimal> stillEggs;
            for(size_t i = 0; i < eggs.size(); ++i)
            {
                ++eggs[i].mTimer;
                if(eggs[i].mTimer >= settings.mHatchTurns)
                    chicks.push_back(SimAnimal(0));
                else
                    stillEggs.push_back(eggs[i]);
            }
            eggs = stillEggs;
        }
        // Growing
        std::vector<SimAnimal> stillChicks;
        for(size_t i = 0; i < chicks.size(); ++i)
        {
            ++chicks[i].mTimer;
            if(chicks[i].mTimer >= settings.mGrowTurns)
                hens.push_back(SimAnimal(HatcheryCycle::layInterval(settings, rng.next())));
            else
                stillChicks.push_back(chicks[i]);
        }
        chicks = stillChicks;

        // Coop fallback
        counts.mHens = hens.size();
        counts.mChicks = chicks.size();
        counts.mEggs = eggs.size();
        counts.mRoosters = roosters;
        if(HatcheryCycle::needCoopHen(counts, nbCoops))
        {
            ++coopWait;
            if(coopWait >= settings.mCoopWait)
            {
                hens.push_back(SimAnimal(HatcheryCycle::layInterval(settings, rng.next())));
                coopWait = 0;
            }
        }
        else
            coopWait = 0;
        if(HatcheryCycle::needCoopRooster(counts, nbCoops))
        {
            ++roosterWait;
            if(roosterWait >= settings.mRoosterWait)
            {
                ++roosters;
                roosterWait = 0;
            }
        }
        else
            roosterWait = 0;
    }
    return eaten;
}
}

//! Balance parity: with the default values the number of edible chickens per minute has to stay the
//! one of the spawning before the life cycle, for the same hatchery size and the same demand.
BOOST_AUTO_TEST_CASE(test_BalanceParity)
{
    HatcheryCycleSettings settings;
    const uint32_t nbTurns = 84000; // about 1000 minutes with 1.4 turns per second
    const uint32_t demands[] = {2, 5, 10, 20};
    const uint32_t coops[] = {1, 2, 4, 8};
    for(uint32_t c = 0; c < 4; ++c)
    {
        for(uint32_t d = 0; d < 4; ++d)
        {
            double oldEaten = simulateOld(coops[c], demands[d], nbTurns, settings.mCoopWait);
            double newEaten = simulateNew(coops[c], demands[d], nbTurns, settings);
            double oldPerMinute = oldEaten / (nbTurns / 1.4 / 60.0);
            double newPerMinute = newEaten / (nbTurns / 1.4 / 60.0);
            std::cout << "parity coops=" << coops[c] << " eatPercent/turn=" << demands[d]
                << " old=" << oldPerMinute << " new=" << newPerMinute << " per minute" << std::endl;
            BOOST_CHECK_CLOSE(newPerMinute, oldPerMinute, 15.0);
        }
    }
}

BOOST_AUTO_TEST_CASE(test_Care)
{
    // The two bonuses (light 10, no enemies 15) add up, but only while all tiles are claimed
    HatcheryCycleSettings settings;
    settings.mLayMin = 8;
    settings.mLayMax = 12;
    HatcheryCare care;
    BOOST_CHECK_EQUAL(HatcheryCycle::carePercent(settings, care), 0u);
    care.mLit = true;
    BOOST_CHECK_EQUAL(HatcheryCycle::carePercent(settings, care), 0u);
    care.mClaimed = true;
    BOOST_CHECK_EQUAL(HatcheryCycle::carePercent(settings, care), 25u);
    care.mEnemies = true;
    BOOST_CHECK_EQUAL(HatcheryCycle::carePercent(settings, care), 10u);
    care.mLit = false;
    BOOST_CHECK_EQUAL(HatcheryCycle::carePercent(settings, care), 0u);
    care.mEnemies = false;
    BOOST_CHECK_EQUAL(HatcheryCycle::carePercent(settings, care), 15u);

    // Without a bonus the settings stay as they are, with care the laying times get shorter
    HatcheryCare none;
    HatcheryCycleSettings plain = HatcheryCycle::withCare(settings, none);
    BOOST_CHECK_EQUAL(plain.mLayMin, 8u);
    BOOST_CHECK_EQUAL(plain.mLayMax, 12u);
    care.mLit = true;
    HatcheryCycleSettings cared = HatcheryCycle::withCare(settings, care);
    BOOST_CHECK_EQUAL(cared.mLayMin, 6u);
    BOOST_CHECK_EQUAL(cared.mLayMax, 9u);
    BOOST_CHECK_EQUAL(cared.mHatchTurns, settings.mHatchTurns);
    settings.mCareLightPercent = 0;
    settings.mCareCalmPercent = 0;
    BOOST_CHECK_EQUAL(HatcheryCycle::withCare(settings, care).mLayMin, 8u);
    settings.mCareLightPercent = 500;
    BOOST_CHECK_EQUAL(HatcheryCycle::carePercent(settings, care), 90u);
    BOOST_CHECK(HatcheryCycle::withCare(settings, care).mLayMin >= 1u);

    // Eggs do not hatch while enemies stand in the hatchery
    HatcheryCounts counts;
    counts.mRoosters = 1;
    BOOST_CHECK(HatcheryCycle::canHatch(counts, false));
    BOOST_CHECK(!HatcheryCycle::canHatch(counts, true));
    counts.mRoosters = 0;
    BOOST_CHECK(!HatcheryCycle::canHatch(counts, false));
}

BOOST_AUTO_TEST_CASE(test_Trample)
{
    HatcheryCycleSettings settings;
    settings.mTramplePercent = 30;
    // Only enemies trample, only eggs get trampled, the dice decide
    BOOST_CHECK(HatcheryCycle::tramples(settings, true, true, 0));
    BOOST_CHECK(HatcheryCycle::tramples(settings, true, true, 29));
    BOOST_CHECK(!HatcheryCycle::tramples(settings, true, true, 30));
    BOOST_CHECK(!HatcheryCycle::tramples(settings, false, true, 0));
    BOOST_CHECK(!HatcheryCycle::tramples(settings, true, false, 0));
    settings.mTramplePercent = 0;
    BOOST_CHECK(!HatcheryCycle::tramples(settings, true, true, 0));
    settings.mTramplePercent = 1000;
    BOOST_CHECK(HatcheryCycle::tramples(settings, true, true, 99));
}
