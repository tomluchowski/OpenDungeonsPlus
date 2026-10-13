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
#include "rooms/HatcheryRooster.h"

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

//! A hatchery has room for one rooster: two or more fight, the server draws the winner, and a hatchery
//! that lost its rooster gets a new one after the same wait as an empty hatchery gets its hens.
BOOST_AUTO_TEST_CASE(test_RoosterFight)
{
    HatcheryCounts counts;
    BOOST_CHECK(!HatcheryCycle::needFight(counts));
    counts.mRoosters = 1;
    BOOST_CHECK(!HatcheryCycle::needFight(counts));
    counts.mRoosters = 2;
    BOOST_CHECK(HatcheryCycle::needFight(counts));
    // More than two fight pair by pair: while there are two or more there is a fight
    counts.mRoosters = 3;
    BOOST_CHECK(HatcheryCycle::needFight(counts));

    // The winner is one of the two and depends on the random number only (same number, same winner)
    uint32_t firstWins = 0;
    uint32_t secondWins = 0;
    for(uint32_t random = 0; random < 1000; ++random)
    {
        uint32_t winner = HatcheryCycle::fightWinner(random);
        BOOST_CHECK(winner < 2u);
        BOOST_CHECK_EQUAL(winner, HatcheryCycle::fightWinner(random));
        if(winner == 0)
            ++firstWins;
        else
            ++secondWins;
    }
    BOOST_CHECK_EQUAL(firstWins, 500u);
    BOOST_CHECK_EQUAL(secondWins, 500u);

    // The fight is called off when one of them is picked up or gone
    BOOST_CHECK(HatcheryCycle::fightContinues(true, true));
    BOOST_CHECK(!HatcheryCycle::fightContinues(true, false));
    BOOST_CHECK(!HatcheryCycle::fightContinues(false, true));
    BOOST_CHECK(!HatcheryCycle::fightContinues(false, false));

    // Exactly one rooster is left afterwards, so no rooster has to come from a coop
    counts.mRoosters = 2;
    counts.mRoosters -= 1;
    BOOST_CHECK(!HatcheryCycle::needFight(counts));
    BOOST_CHECK(!HatcheryCycle::needCoopRooster(counts, 2));

    // The rooster comes after the wait of the hens: there is no value of its own any more
    HatcheryCycleSettings settings;
    settings.mCoopWait = 21;
    HatcheryCycleSettings scaledSettings = HatcheryCycle::scaled(settings, 0.5);
    BOOST_CHECK_EQUAL(scaledSettings.mCoopWait, 21u);
    BOOST_CHECK(settings.mFightTurns > 0u);
    BOOST_CHECK(settings.mFightApproachTurns > 0u);
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
            if(roosterWait >= settings.mCoopWait)
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

BOOST_AUTO_TEST_CASE(test_RoosterDay)
{
    RoosterSettings settings;
    settings.mDayTurns = 100;
    settings.mNightPercent = 30;
    BOOST_CHECK(!HatcheryRooster::isNight(0, settings));
    BOOST_CHECK(!HatcheryRooster::isNight(69, settings));
    BOOST_CHECK(HatcheryRooster::isNight(70, settings));
    BOOST_CHECK(HatcheryRooster::isNight(99, settings));
    BOOST_CHECK(!HatcheryRooster::isNight(100, settings));
    BOOST_CHECK(HatcheryRooster::isNewDay(0, settings));
    BOOST_CHECK(HatcheryRooster::isNewDay(300, settings));
    BOOST_CHECK(!HatcheryRooster::isNewDay(301, settings));

    // No night and no day length: never night
    settings.mNightPercent = 0;
    BOOST_CHECK(!HatcheryRooster::isNight(99, settings));
    settings.mNightPercent = 30;
    settings.mDayTurns = 0;
    BOOST_CHECK(!HatcheryRooster::isNight(99, settings));
    BOOST_CHECK(!HatcheryRooster::isNewDay(0, settings));
}

BOOST_AUTO_TEST_CASE(test_RoosterCrowInterval)
{
    RoosterSettings settings;
    settings.mCrowMin = 40;
    settings.mCrowMax = 90;
    for(uint32_t random = 0; random < 500; ++random)
    {
        uint32_t interval = HatcheryRooster::crowInterval(settings, random);
        BOOST_CHECK(interval >= 40u);
        BOOST_CHECK(interval <= 90u);
    }
    settings.mCrowMax = 10;
    BOOST_CHECK_EQUAL(HatcheryRooster::crowInterval(settings, 7), 40u);
}

BOOST_AUTO_TEST_CASE(test_RoosterDecide)
{
    RoosterSettings settings;
    settings.mDayTurns = 1000;
    settings.mNightPercent = 30;
    RoosterContext context;
    context.mTurn = 100;
    // He has crowed for the first day already
    context.mCrowDay = 0;
    context.mHasCoop = true;
    context.mHasHen = true;
    context.mHasChick = true;
    context.mCrowInterval = 60;
    context.mRoll = 99;

    // Nothing special: he struts
    RoosterPlan plan = HatcheryRooster::decide(context, settings);
    BOOST_CHECK(plan.mMood == RoosterMood::strut);

    // The dice decide what comes to his mind: chase, lead, perch
    context.mRoll = 0;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::chase);
    context.mRoll = settings.mChasePercent;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::lead);
    context.mRoll = settings.mChasePercent + settings.mLeadPercent;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::perch);
    // He calls the hens to food, only when there is a hen
    context.mRoll = settings.mChasePercent + settings.mLeadPercent + settings.mPerchPercent;
    plan = HatcheryRooster::decide(context, settings);
    BOOST_CHECK(plan.mMood == RoosterMood::call);
    BOOST_CHECK_EQUAL(plan.mTurns, settings.mCallTurns);
    context.mHasHen = false;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::strut);
    context.mHasHen = true;
    context.mRoll = settings.mChasePercent + settings.mLeadPercent + settings.mPerchPercent + settings.mCallPercent;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::strut);

    // Without a hen, chick or coop those moods are not chosen
    context.mHasHen = false;
    context.mRoll = 0;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::strut);
    context.mHasHen = true;

    // A crow when the time is up, then he sits on the roof for a while
    context.mRoll = 99;
    context.mSinceCrow = 60;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::crow);
    context.mSinceCrow = 0;
    context.mMood = RoosterMood::crow;
    context.mMoodTurns = 0;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::perch);
    context.mHasCoop = false;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::strut);
    context.mHasCoop = true;

    // A mood with turns left goes on
    context.mMood = RoosterMood::perch;
    context.mMoodTurns = 5;
    context.mRoll = 0;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::perch);

    // A threat always wins, also against the mood in progress
    context.mThreat = true;
    plan = HatcheryRooster::decide(context, settings);
    BOOST_CHECK(plan.mMood == RoosterMood::guard);
    BOOST_CHECK_EQUAL(plan.mTurns, settings.mGuardTurns);
    context.mMood = RoosterMood::guard;
    context.mMoodTurns = 3;
    plan = HatcheryRooster::decide(context, settings);
    BOOST_CHECK(plan.mMood == RoosterMood::guard);
    BOOST_CHECK_EQUAL(plan.mTurns, 3u);
    context.mThreat = false;

    // Night: he sleeps, even in the middle of a chase. A new day starts with a crow.
    context.mMood = RoosterMood::chase;
    context.mMoodTurns = 5;
    context.mTurn = 800;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::roost);
    context.mMood = RoosterMood::roost;
    context.mTurn = 2000;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::crow);
    // After the night he gets up (he has crowed for the new day by now)
    context.mCrowDay = 2;
    context.mTurn = 2100;
    context.mRoll = 99;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::strut);
}

BOOST_AUTO_TEST_CASE(test_RoosterNewDayCrow)
{
    RoosterSettings settings;
    settings.mDayTurns = 1000;
    settings.mNightPercent = 30;

    // The day of a turn, none without a day length
    BOOST_CHECK_EQUAL(HatcheryRooster::dayNumber(0, settings), 0);
    BOOST_CHECK_EQUAL(HatcheryRooster::dayNumber(999, settings), 0);
    BOOST_CHECK_EQUAL(HatcheryRooster::dayNumber(1000, settings), 1);
    BOOST_CHECK_EQUAL(HatcheryRooster::dayNumber(2500, settings), 2);
    BOOST_CHECK_EQUAL(HatcheryRooster::dayNumber(-1, settings), -1);
    RoosterSettings noDay = settings;
    noDay.mDayTurns = 0;
    BOOST_CHECK_EQUAL(HatcheryRooster::dayNumber(500, noDay), -1);
    BOOST_CHECK(!HatcheryRooster::newDayCrowOwed(500, -1, noDay));

    // The crow is owed from the first turn of a new day until he has crowed for it, not only on that turn
    BOOST_CHECK(!HatcheryRooster::newDayCrowOwed(999, 0, settings));
    BOOST_CHECK(HatcheryRooster::newDayCrowOwed(1000, 0, settings));
    BOOST_CHECK(HatcheryRooster::newDayCrowOwed(1001, 0, settings));
    BOOST_CHECK(HatcheryRooster::newDayCrowOwed(1400, 0, settings));
    BOOST_CHECK(!HatcheryRooster::newDayCrowOwed(1400, 1, settings));
    // Several days missed (the hatchery was not looked at): one crow settles it
    BOOST_CHECK(HatcheryRooster::newDayCrowOwed(3500, 0, settings));
    BOOST_CHECK(!HatcheryRooster::newDayCrowOwed(3500, 3, settings));

    RoosterContext context;
    context.mTurn = 1001;
    context.mCrowDay = 0;
    context.mCrowInterval = 60;
    context.mRoll = 99;

    // He was busy on the first turn of the day: he crows as soon as he is free
    RoosterPlan plan = HatcheryRooster::decide(context, settings);
    BOOST_CHECK(plan.mMood == RoosterMood::crow);
    BOOST_CHECK_EQUAL(plan.mTurns, settings.mCrowTurns);
    context.mTurn = 1350;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::crow);

    // Having crowed for the day, he does not crow again for it
    context.mCrowDay = 1;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::strut);

    // A threat comes first, the crow follows when it is gone (the day is still owed)
    context.mCrowDay = 0;
    context.mThreat = true;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::guard);
    context.mThreat = false;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::crow);

    // The crow also interrupts a mood that still has turns left
    context.mMood = RoosterMood::chase;
    context.mMoodTurns = 5;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::crow);

    // A crow in progress is not started again
    context.mMood = RoosterMood::crow;
    context.mMoodTurns = 2;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::crow);
    BOOST_CHECK_EQUAL(HatcheryRooster::decide(context, settings).mTurns, 2u);

    // The sleep period is the day divided by the divisor, the crow length comes from the settings
    settings.mCrowTurns = 7;
    settings.mRoostDivisor = 20;
    context.mMood = RoosterMood::strut;
    context.mMoodTurns = 0;
    context.mCrowDay = 1;
    context.mTurn = 1800;
    plan = HatcheryRooster::decide(context, settings);
    BOOST_CHECK(plan.mMood == RoosterMood::roost);
    BOOST_CHECK_EQUAL(plan.mTurns, 50u);
    context.mTurn = 1400;
    context.mSinceCrow = 60;
    plan = HatcheryRooster::decide(context, settings);
    BOOST_CHECK(plan.mMood == RoosterMood::crow);
    BOOST_CHECK_EQUAL(plan.mTurns, 7u);
}
