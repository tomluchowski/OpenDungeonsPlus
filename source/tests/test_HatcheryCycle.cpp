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
#include "rooms/HatcheryNestField.h"
#include "rooms/HatcheryRooster.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
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

BOOST_AUTO_TEST_CASE(test_CoopHenCount)
{
    HatcheryCycleSettings settings;
    // By default one hen per coop comes out, at least one
    BOOST_CHECK_EQUAL(HatcheryCycle::coopHenCount(settings, 3), 3u);
    BOOST_CHECK_EQUAL(HatcheryCycle::coopHenCount(settings, 0), 1u);
    settings.mCoopBatch = 2;
    BOOST_CHECK_EQUAL(HatcheryCycle::coopHenCount(settings, 3), 2u);
    BOOST_CHECK_EQUAL(HatcheryCycle::coopHenCount(settings, 1), 1u);
}

BOOST_AUTO_TEST_CASE(test_LayInterval)
{
    HatcheryCycleSettings settings;
    // The factor makes the times a fraction longer: the fraction is rounded up now and then, so the average is exact
    double sum = 0.0;
    const uint32_t nbDraws = 100000;
    for(uint32_t random = 0; random < nbDraws; ++random)
    {
        uint32_t turns = HatcheryCycle::layInterval(settings, random);
        BOOST_CHECK(turns >= settings.mLayMin);
        BOOST_CHECK(turns <= settings.mLayMax + 1);
        sum += turns;
    }
    const double average = (settings.mLayMin + settings.mLayMax) / 2.0 * settings.mLayFactor;
    BOOST_CHECK_CLOSE(sum / nbDraws, average, 0.5);
    settings.mLayFactor = 1.0;
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

//! A hen with the turns until her next egg and the length of the interval that has just run out.
struct SimHen
{
    SimHen(uint32_t timer) : mTimer(timer), mDone(timer) {}
    uint32_t mTimer;
    uint32_t mDone;
};

//! An egg whose timer has run out but whose hen is late at the nest: it appears mTurns turns later and gets the age
//! mLate (the age it would have had on time, see RoomHatchery::releasePendingEggs).
struct SimLateEgg
{
    SimLateEgg(uint32_t turns) : mTurns(turns), mLate(turns) {}
    uint32_t mTurns;
    uint32_t mLate;
};

//! The conditions of one run of the parity model.
struct SimCase
{
    SimCase() :
        mWalkMax(0),
        mNest(true),
        mCare(false),
        mTrample(false),
        mEnemyPercent(5),
        mTramplePercent(30)
    {}

    //! Longest walk to the nest in turns, the walk of an egg is drawn between 0 and this (variable distance).
    uint32_t mWalkMax;
    //! False = no free nest: she sits down where she is, no walk.
    bool mNest;
    //! The care bonuses (light and no enemies, see HatcheryCycle::carePercent) of a claimed, lit hatchery; they are
    //! left out of the 3 percent parity check.
    bool mCare;
    //! Enemies stand in the hatchery mEnemyPercent of the turns: eggs do not hatch then, there is no care, and every
    //! egg is trampled with mTramplePercent per such turn.
    bool mTrample;
    uint32_t mEnemyPercent;
    uint32_t mTramplePercent;
};

//! Turns an egg is late when the hen needs walk + Lay pose turns but had only the interval she has just run through
//! (she sets off when the timer starts at the earliest, see HatcheryCycle::tripDue). The hatching clock of the egg
//! starts with this age, so the rhythm of the cycle does not depend on the way.
uint32_t lateTurns(uint32_t interval, uint32_t walk, const HatcheryCycleSettings& settings)
{
    const uint32_t need = walk + settings.mLayShowTurns;
    return (need > interval) ? need - interval : 0;
}

//! Model of the life cycle with the rules of HatcheryCycle, in the order the hatchery handles them. The egg appears
//! when the laying timer of the hen runs out (late eggs are backdated), it is hatched and grown in the same turn
//! as it is counted. A Python port of this function (the same generators and order) gives the same numbers.
uint32_t simulateNew(uint32_t nbCoops, uint32_t eatPercent, uint32_t nbTurns, const HatcheryCycleSettings& settings,
    const SimCase& simCase)
{
    Lcg demand(12345);
    Lcg rng(777);
    Lcg walkRng(4242);
    Lcg trampleRng(99);
    HatcheryCare care;
    care.mClaimed = true;
    care.mLit = true;
    HatcheryCare careWithEnemy = care;
    careWithEnemy.mEnemies = true;
    const HatcheryCycleSettings caredSettings = simCase.mCare ? HatcheryCycle::withCare(settings, care) : settings;
    const HatcheryCycleSettings enemySettings = simCase.mCare ? HatcheryCycle::withCare(settings, careWithEnemy) : settings;
    std::vector<SimHen> hens;
    std::vector<uint32_t> chicks;
    std::vector<uint32_t> eggs;
    std::vector<SimLateEgg> lateEggs;
    uint32_t coopWait = 0;
    const uint32_t capacity = HatcheryCycle::capacity(nbCoops, nbCoops, settings);
    for(uint32_t i = 0; i < nbCoops; ++i)
        hens.push_back(SimHen(HatcheryCycle::layInterval(caredSettings, rng.next())));
    uint32_t eaten = 0;
    for(uint32_t turn = 0; turn < nbTurns; ++turn)
    {
        const bool enemy = simCase.mTrample && ((trampleRng.next() % 100) < simCase.mEnemyPercent);
        // Enemies in the hatchery: no bonus for the calm
        const HatcheryCycleSettings& laying = enemy ? enemySettings : caredSettings;
        if(((demand.next() % 100) < eatPercent) && !hens.empty())
        {
            hens.erase(hens.begin());
            ++eaten;
        }

        uint32_t population = hens.size() + chicks.size() + eggs.size() + lateEggs.size();

        // Late eggs whose hen has finished appear now, with the age they would have had on time
        std::vector<uint32_t> born;
        std::vector<SimLateEgg> stillLate;
        for(size_t i = 0; i < lateEggs.size(); ++i)
        {
            if(lateEggs[i].mTurns <= 1)
                born.push_back(lateEggs[i].mLate);
            else
            {
                --lateEggs[i].mTurns;
                stillLate.push_back(lateEggs[i]);
            }
        }
        lateEggs = stillLate;

        // Laying: the egg is laid when the timer runs out
        for(size_t i = 0; i < hens.size(); ++i)
        {
            if(hens[i].mTimer > 1)
            {
                --hens[i].mTimer;
                continue;
            }
            const uint32_t interval = HatcheryCycle::layInterval(laying, rng.next());
            const uint32_t done = hens[i].mDone;
            hens[i].mTimer = interval;
            hens[i].mDone = interval;
            if(population >= capacity)
                continue;

            uint32_t walk = 0;
            if(simCase.mNest && (simCase.mWalkMax > 0))
            {
                walk = walkRng.next() % (simCase.mWalkMax + 1);
                // The way has to fit into the time of the interval, otherwise she lays where she sits
                if(!HatcheryCycle::walkFits(walk, done, settings))
                    walk = 0;
            }
            const uint32_t late = lateTurns(done, walk, settings);
            if(late > 0)
                lateEggs.push_back(SimLateEgg(late));
            else
                born.push_back(0);
            ++population;
        }

        // Enemies trample the eggs that lie there
        if(enemy)
        {
            std::vector<uint32_t> kept;
            for(size_t i = 0; i < eggs.size(); ++i)
            {
                if((trampleRng.next() % 100) >= simCase.mTramplePercent)
                    kept.push_back(eggs[i]);
            }
            eggs = kept;
        }
        eggs.insert(eggs.end(), born.begin(), born.end());

        // Hatching (a rooster is there, and no enemy)
        std::vector<uint32_t> stillEggs;
        for(size_t i = 0; i < eggs.size(); ++i)
        {
            if(enemy)
            {
                stillEggs.push_back(eggs[i]);
                continue;
            }
            const uint32_t age = eggs[i] + 1;
            if(age >= settings.mHatchTurns)
                chicks.push_back(age - settings.mHatchTurns);
            else
                stillEggs.push_back(age);
        }
        eggs = stillEggs;

        // Growing
        std::vector<uint32_t> stillChicks;
        for(size_t i = 0; i < chicks.size(); ++i)
        {
            const uint32_t age = chicks[i] + 1;
            if(age >= settings.mGrowTurns)
                hens.push_back(SimHen(HatcheryCycle::layInterval(laying, rng.next())));
            else
                stillChicks.push_back(age);
        }
        chicks = stillChicks;

        // Coop fallback
        HatcheryCounts counts;
        counts.mHens = hens.size();
        counts.mChicks = chicks.size();
        counts.mEggs = eggs.size() + lateEggs.size();
        counts.mRoosters = 1;
        if(HatcheryCycle::needCoopHen(counts, nbCoops))
        {
            ++coopWait;
            if(coopWait >= settings.mCoopWait)
            {
                // One hen per coop comes out
                uint32_t nbFromCoops = HatcheryCycle::coopHenCount(settings, capacity);
                for(uint32_t i = 0; i < nbFromCoops; ++i)
                    hens.push_back(SimHen(HatcheryCycle::layInterval(laying, rng.next())));
                coopWait = 0;
            }
        }
        else
            coopWait = 0;
    }
    return eaten;
}
}

BOOST_AUTO_TEST_CASE(test_Walk)
{
    // The walk window follows the real distance at the walking speed of a hen (0.4 tiles per second, 1.4 turns per second)
    const double tilesPerTurn = 0.4 / 1.4;
    BOOST_CHECK_EQUAL(HatcheryCycle::walkTurns(0.0, tilesPerTurn), 0u);
    BOOST_CHECK_EQUAL(HatcheryCycle::walkTurns(-1.0, tilesPerTurn), 0u);
    BOOST_CHECK_EQUAL(HatcheryCycle::walkTurns(1.0, 0.0), 0u);
    BOOST_CHECK_EQUAL(HatcheryCycle::walkTurns(0.2, tilesPerTurn), 1u);
    BOOST_CHECK_EQUAL(HatcheryCycle::walkTurns(1.0, tilesPerTurn), 4u);
    BOOST_CHECK_EQUAL(HatcheryCycle::walkTurns(2.1, tilesPerTurn), 8u);
    BOOST_CHECK(HatcheryCycle::walkTurns(2.1, tilesPerTurn) > HatcheryCycle::walkTurns(1.0, tilesPerTurn));

    // She sets off when the turns left until the egg are as many as the walk and the Lay pose (2 turns)
    HatcheryCycleSettings settings;
    BOOST_CHECK(HatcheryCycle::tripDue(6, 4, settings));
    BOOST_CHECK(!HatcheryCycle::tripDue(7, 4, settings));
    BOOST_CHECK(HatcheryCycle::tripDue(2, 0, settings));
    BOOST_CHECK(!HatcheryCycle::tripDue(3, 0, settings));

    // A nest is used when the walk and the Lay pose (2 turns) fit into the turns left until the egg: the window is the
    // real way in the time she has, not a fixed number of turns
    BOOST_CHECK(HatcheryCycle::walkFits(3, 5, settings));
    BOOST_CHECK(!HatcheryCycle::walkFits(4, 5, settings));
    BOOST_CHECK(HatcheryCycle::walkFits(0, 2, settings));
    BOOST_CHECK(!HatcheryCycle::walkFits(0, 1, settings));
    BOOST_CHECK(HatcheryCycle::walkFits(12, 14, settings));
    BOOST_CHECK(!HatcheryCycle::walkFits(12, 13, settings));
}

BOOST_AUTO_TEST_CASE(test_LateEgg)
{
    // The egg is on time when walk and pose fit into the interval she has run through, otherwise it is that late
    HatcheryCycleSettings settings;
    BOOST_CHECK_EQUAL(lateTurns(5, 3, settings), 0u);
    BOOST_CHECK_EQUAL(lateTurns(3, 3, settings), 2u);
    BOOST_CHECK_EQUAL(lateTurns(3, 0, settings), 0u);
    BOOST_CHECK_EQUAL(lateTurns(7, 8, settings), 3u);
    // A way that fits (HatcheryCycle::walkFits) is never late
    for(uint32_t interval = settings.mLayMin; interval <= settings.mLayMax; ++interval)
    {
        for(uint32_t walk = 0; walk <= 20; ++walk)
        {
            if(HatcheryCycle::walkFits(walk, interval, settings))
                BOOST_CHECK_EQUAL(lateTurns(interval, walk, settings), 0u);
        }
    }
}

//! Balance parity: with the default values the number of edible chickens per minute has to stay the one of the
//! spawning before the life cycle, for the same hatchery size and the same demand: within 3 percent of the old
//! number, in every case below that has no care bonus (the bonuses of a claimed, lit, calm hatchery are printed but
//! not limited, they speed the hatchery up on purpose). The egg appears when the laying timer of the hen runs out: the
//! hen uses the nest only when the real way and the Lay pose fit into the time of her laying interval (walkFits),
//! otherwise she lays where she sits, so the rate does not depend on how far the nests are (a way that is longer than
//! the interval would delay the eggs and break the parity: 6.5 percent with ways up to 12 turns, 17 percent up to 20).
//! The laying timer carries the factor HatcheryCycleSettings::mLayFactor that balances the cycle against the old
//! spawning. A Python port of the model (the same generators and order) gives the worst deviation per case without
//! bonus: 2.44 percent (no walk, ways of 0 to 6 or 0 to 60 turns, no free nest), 2.80 (enemies trample, 5 percent of
//! the turns, 30 percent per egg). With the bonuses (light 10, calm 15) it is 7.0 percent, which is intended.
// Historical fits-or-floor model only; required nest waiting and actual-arrival parity must be checked separately.
BOOST_AUTO_TEST_CASE(test_BalanceParity)
{
    HatcheryCycleSettings settings;
    const uint32_t nbTurns = 84000; // about 1000 minutes with 1.4 turns per second
    const uint32_t demands[] = {2, 5, 10, 20};
    const uint32_t coops[] = {1, 2, 4, 8};

    std::vector<std::string> names;
    std::vector<SimCase> cases;
    SimCase noWalk;
    names.push_back("no walk");
    cases.push_back(noWalk);
    SimCase walk = noWalk;
    walk.mWalkMax = 6;
    names.push_back("variable walk 0 to 6 turns");
    cases.push_back(walk);
    SimCase farNests = noWalk;
    farNests.mWalkMax = 60;
    names.push_back("variable walk 0 to 60 turns (far nests)");
    cases.push_back(farNests);
    SimCase noNest = noWalk;
    noNest.mNest = false;
    names.push_back("no free nest");
    cases.push_back(noNest);
    SimCase careNoWalk = noWalk;
    careNoWalk.mCare = true;
    names.push_back("care bonus");
    cases.push_back(careNoWalk);
    SimCase careWalk = walk;
    careWalk.mCare = true;
    names.push_back("care bonus and variable walk");
    cases.push_back(careWalk);
    SimCase trample = walk;
    trample.mTrample = true;
    names.push_back("enemies trample eggs");
    cases.push_back(trample);

    for(size_t k = 0; k < cases.size(); ++k)
    {
        for(uint32_t c = 0; c < 4; ++c)
        {
            for(uint32_t d = 0; d < 4; ++d)
            {
                const double oldEaten = simulateOld(coops[c], demands[d], nbTurns, settings.mCoopWait);
                const double newEaten = simulateNew(coops[c], demands[d], nbTurns, settings, cases[k]);
                const double oldPerMinute = oldEaten / (nbTurns / 1.4 / 60.0);
                const double newPerMinute = newEaten / (nbTurns / 1.4 / 60.0);
                const double deviation = std::abs(newPerMinute - oldPerMinute) / oldPerMinute * 100.0;
                std::cout << "parity " << names[k] << " coops=" << coops[c] << " eatPercent/turn=" << demands[d]
                    << " old=" << oldPerMinute << " new=" << newPerMinute << " per minute, deviation "
                    << deviation << " percent" << std::endl;
                // The care bonuses make a cared-for hatchery lay faster on purpose: only printed, not limited
                if(!cases[k].mCare)
                {
                    BOOST_CHECK_MESSAGE(deviation <= 3.0, names[k] << " coops=" << coops[c] << " demand=" << demands[d]
                        << ": " << deviation << " percent");
                }
            }
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

    // Without a bonus the settings stay as they are, with care the laying times get shorter (through the factor)
    HatcheryCare none;
    HatcheryCycleSettings plain = HatcheryCycle::withCare(settings, none);
    BOOST_CHECK_EQUAL(plain.mLayMin, 8u);
    BOOST_CHECK_EQUAL(plain.mLayMax, 12u);
    BOOST_CHECK_EQUAL(plain.mLayFactor, settings.mLayFactor);
    care.mLit = true;
    HatcheryCycleSettings cared = HatcheryCycle::withCare(settings, care);
    BOOST_CHECK_EQUAL(cared.mLayMin, 8u);
    BOOST_CHECK_EQUAL(cared.mLayMax, 12u);
    BOOST_CHECK_CLOSE(cared.mLayFactor, settings.mLayFactor * 0.75, 0.0001);
    BOOST_CHECK_EQUAL(cared.mHatchTurns, settings.mHatchTurns);
    settings.mCareLightPercent = 0;
    settings.mCareCalmPercent = 0;
    BOOST_CHECK_EQUAL(HatcheryCycle::withCare(settings, care).mLayFactor, settings.mLayFactor);
    settings.mCareLightPercent = 500;
    BOOST_CHECK_EQUAL(HatcheryCycle::carePercent(settings, care), 90u);
    BOOST_CHECK_CLOSE(HatcheryCycle::withCare(settings, care).mLayFactor, settings.mLayFactor * 0.1, 0.0001);
    BOOST_CHECK(HatcheryCycle::layInterval(HatcheryCycle::withCare(settings, care), 0) >= 1u);

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

BOOST_AUTO_TEST_CASE(test_NestPlace)
{
    // Two nests with three places each (as in the coop mesh)
    std::vector<bool> occupied(6, false);
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(occupied, 3), 0);

    // The nest with the fewest eggs comes first, a free place before a full nest
    occupied[0] = true;
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(occupied, 3), 3);
    occupied[3] = true;
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(occupied, 3), 1);
    occupied[1] = true;
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(occupied, 3), 4);

    // A nest that is full is skipped even when it comes first
    occupied[2] = true;
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(occupied, 3), 4);

    // The gap of a picked up egg is filled again
    occupied[4] = true;
    occupied[5] = true;
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(occupied, 3), -1);
    occupied[1] = false;
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(occupied, 3), 1);

    // All places taken (or no nest at all): the egg lies on the ground
    std::vector<bool> full(6, true);
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(full, 3), -1);
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(std::vector<bool>(), 3), -1);
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(occupied, 0), -1);

    // A nest where only the last place is free
    std::vector<bool> last(3, true);
    last[2] = false;
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(last, 3), 2);
}

BOOST_AUTO_TEST_CASE(test_NestPlaceSingleSlot)
{
    // The nests of the nest field hold one egg each: the first free one (the list is ordered by the distance to the hen)
    std::vector<bool> occupied(4, false);
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(occupied, 1), 0);
    occupied[0] = true;
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(occupied, 1), 1);
    occupied[1] = true;
    occupied[2] = true;
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(occupied, 1), 3);
    occupied[3] = true;
    BOOST_CHECK_EQUAL(HatcheryCycle::pickNestPlace(occupied, 1), -1);
}

//! A block of tiles for the nest field tests
static std::vector<HatcheryNestField::TileCoord> nestTestBlock(int x0, int y0, int width, int height)
{
    std::vector<HatcheryNestField::TileCoord> tiles;
    for(int x = x0; x < x0 + width; ++x)
    {
        for(int y = y0; y < y0 + height; ++y)
            tiles.push_back(HatcheryNestField::TileCoord(x, y));
    }
    return tiles;
}

BOOST_AUTO_TEST_CASE(test_NestField)
{
    const HatcheryNestField::Settings settings;
    const std::vector<HatcheryNestField::TileCoord> tiles = nestTestBlock(0, 0, 5, 5);
    std::vector<HatcheryNestField::TileCoord> coops;
    coops.push_back(HatcheryNestField::TileCoord(1, 2));
    coops.push_back(HatcheryNestField::TileCoord(3, 2));
    // Two entrances (the middle of the west and of the east edge), the way to the coops must stay free of nests
    std::vector<HatcheryNestField::TileCoord> entrances;
    entrances.push_back(HatcheryNestField::TileCoord(0, 2));
    entrances.push_back(HatcheryNestField::TileCoord(4, 2));
    const std::vector<HatcheryNestField::Place> places = HatcheryNestField::compute(tiles, coops, entrances, settings);

    // One nest per coop at least, about one per three tiles, never more than the size asks for
    BOOST_CHECK(places.size() >= coops.size());
    BOOST_CHECK(places.size() <= tiles.size() / settings.mTilesPerNest);

    for(uint32_t i = 0; i < places.size(); ++i)
    {
        const double x = places[i].mX;
        const double y = places[i].mY;

        // On a tile of the hatchery, away from the edge to the tiles outside of it (the block is 0..4)
        BOOST_CHECK(x >= -0.5 + settings.mEdge - 1e-9);
        BOOST_CHECK(x <= 4.5 - settings.mEdge + 1e-9);
        BOOST_CHECK(y >= -0.5 + settings.mEdge - 1e-9);
        BOOST_CHECK(y <= 4.5 - settings.mEdge + 1e-9);

        // Away from the coops and their aprons
        for(uint32_t c = 0; c < coops.size(); ++c)
        {
            const double cx = coops[c].first;
            const double cy = coops[c].second;
            BOOST_CHECK(HatcheryNestField::rectDistanceSquared(x, y, cx + HatcheryNestField::coopMinX,
                cy + HatcheryNestField::coopMinY, cx + HatcheryNestField::coopMaxX, cy + HatcheryNestField::coopMaxY) >=
                settings.mCoopClearance * settings.mCoopClearance - 1e-9);
            BOOST_CHECK(HatcheryNestField::rectDistanceSquared(x, y, cx + HatcheryNestField::coopMaxX,
                cy - settings.mLaneHalfWidth, cx + HatcheryNestField::coopMaxX + settings.mLaneLength,
                cy + settings.mLaneHalfWidth) >= settings.mLaneClearance * settings.mLaneClearance - 1e-9);

            // Away from the walking strips from the entrances to the middle of the apron
            const double clear = settings.mPathHalfWidth + settings.mPathClearance;
            for(uint32_t e = 0; e < entrances.size(); ++e)
            {
                BOOST_CHECK(HatcheryNestField::segmentDistanceSquared(x, y, entrances[e].first, entrances[e].second,
                    cx + HatcheryNestField::coopMaxX + settings.mLaneLength * 0.5, cy) >= clear * clear - 1e-9);
            }
        }

        // Away from each other
        for(uint32_t j = 0; j < i; ++j)
        {
            const double dx = places[j].mX - x;
            const double dy = places[j].mY - y;
            BOOST_CHECK(dx * dx + dy * dy >= settings.mSpacing * settings.mSpacing - 1e-9);
        }
    }

    // The places depend on the tiles only, not on their order (the server lists the tiles in the order of the room, which changes when rooms merge)
    std::vector<HatcheryNestField::TileCoord> reversedTiles(tiles.rbegin(), tiles.rend());
    std::vector<HatcheryNestField::TileCoord> reversedCoops(coops.rbegin(), coops.rend());
    std::vector<HatcheryNestField::TileCoord> reversedEntrances(entrances.rbegin(), entrances.rend());
    const std::vector<HatcheryNestField::Place> again = HatcheryNestField::compute(reversedTiles, reversedCoops,
        reversedEntrances, settings);
    BOOST_REQUIRE_EQUAL(again.size(), places.size());
    for(uint32_t i = 0; i < places.size(); ++i)
    {
        BOOST_CHECK_EQUAL(again[i].mX, places[i].mX);
        BOOST_CHECK_EQUAL(again[i].mY, places[i].mY);
    }
    BOOST_CHECK_EQUAL(HatcheryNestField::fingerprint(tiles, coops, entrances),
        HatcheryNestField::fingerprint(reversedTiles, reversedCoops, reversedEntrances));

    // A change of the tiles changes the fingerprint, so the places are computed again
    std::vector<HatcheryNestField::TileCoord> moreTiles = tiles;
    moreTiles.push_back(HatcheryNestField::TileCoord(5, 2));
    BOOST_CHECK(HatcheryNestField::fingerprint(moreTiles, coops, entrances) != HatcheryNestField::fingerprint(tiles, coops, entrances));

    // A new entrance (a dug corridor, a door) changes the fingerprint too, and the places are computed again
    std::vector<HatcheryNestField::TileCoord> moreEntrances = entrances;
    moreEntrances.push_back(HatcheryNestField::TileCoord(2, 0));
    BOOST_CHECK(HatcheryNestField::fingerprint(tiles, coops, moreEntrances) != HatcheryNestField::fingerprint(tiles, coops, entrances));
    BOOST_CHECK(HatcheryNestField::fingerprint(tiles, coops, std::vector<HatcheryNestField::TileCoord>()) !=
        HatcheryNestField::fingerprint(tiles, coops, entrances));

    // Without an entrance the strips do not exist, so the entrances change the places (a door at the west wall of a
    // 5x5 hatchery with a coop at (3, 2) lies on the way to its apron)
    std::vector<HatcheryNestField::TileCoord> westDoor;
    westDoor.push_back(HatcheryNestField::TileCoord(0, 2));
    std::vector<HatcheryNestField::TileCoord> oneCoop;
    oneCoop.push_back(HatcheryNestField::TileCoord(3, 2));
    const std::vector<HatcheryNestField::Place> withoutDoor = HatcheryNestField::compute(tiles, oneCoop,
        std::vector<HatcheryNestField::TileCoord>(), settings);
    const std::vector<HatcheryNestField::Place> withDoor = HatcheryNestField::compute(tiles, oneCoop, westDoor, settings);
    BOOST_CHECK(!withoutDoor.empty());
    BOOST_CHECK(!withDoor.empty());
    for(uint32_t i = 0; i < withDoor.size(); ++i)
    {
        const double clear = settings.mPathHalfWidth + settings.mPathClearance;
        BOOST_CHECK(HatcheryNestField::segmentDistanceSquared(withDoor[i].mX, withDoor[i].mY, 0.0, 2.0,
            3.0 + HatcheryNestField::coopMaxX + settings.mLaneLength * 0.5, 2.0) >= clear * clear - 1e-9);
    }

    // The size caps the number of nests, the coops come on top
    HatcheryNestField::Settings capped = settings;
    capped.mMaxNests = 2;
    BOOST_CHECK(HatcheryNestField::compute(nestTestBlock(0, 0, 9, 9), std::vector<HatcheryNestField::TileCoord>(),
        std::vector<HatcheryNestField::TileCoord>(), capped).size() <= 2u);

    // A hatchery without a tile has no nest
    BOOST_CHECK(HatcheryNestField::compute(std::vector<HatcheryNestField::TileCoord>(), coops, entrances, settings).empty());
}

BOOST_AUTO_TEST_CASE(test_NestFieldFeathers)
{
    const HatcheryNestField::Settings settings;
    const std::vector<HatcheryNestField::TileCoord> tiles = nestTestBlock(0, 0, 7, 7);
    std::vector<HatcheryNestField::TileCoord> coops;
    coops.push_back(HatcheryNestField::TileCoord(1, 3));
    coops.push_back(HatcheryNestField::TileCoord(4, 3));
    std::vector<HatcheryNestField::TileCoord> entrances;
    entrances.push_back(HatcheryNestField::TileCoord(0, 3));
    const std::vector<HatcheryNestField::Place> nests = HatcheryNestField::compute(tiles, coops, entrances, settings);
    const std::vector<HatcheryNestField::Place> feathers = HatcheryNestField::computeFeathers(tiles, coops, entrances,
        nests, settings);

    // About one place per six tiles, at least two, at most eight
    BOOST_CHECK(feathers.size() >= settings.mMinFeathers);
    BOOST_CHECK(feathers.size() <= settings.mMaxFeathers);
    BOOST_CHECK(feathers.size() <= std::max<uint32_t>(tiles.size() / settings.mTilesPerFeather, settings.mMinFeathers));

    for(uint32_t i = 0; i < feathers.size(); ++i)
    {
        const double x = feathers[i].mX;
        const double y = feathers[i].mY;

        // On the hatchery (the block is 0..6), away from the walls
        BOOST_CHECK(x >= -0.5 + settings.mEdge - 1e-9);
        BOOST_CHECK(x <= 6.5 - settings.mEdge + 1e-9);
        BOOST_CHECK(y >= -0.5 + settings.mEdge - 1e-9);
        BOOST_CHECK(y <= 6.5 - settings.mEdge + 1e-9);

        // Not at the coops, their aprons or the walking strip from the entrance
        for(uint32_t c = 0; c < coops.size(); ++c)
        {
            const double cx = coops[c].first;
            const double cy = coops[c].second;
            BOOST_CHECK(HatcheryNestField::rectDistanceSquared(x, y, cx + HatcheryNestField::coopMinX,
                cy + HatcheryNestField::coopMinY, cx + HatcheryNestField::coopMaxX, cy + HatcheryNestField::coopMaxY) >=
                settings.mCoopClearance * settings.mCoopClearance - 1e-9);
            BOOST_CHECK(HatcheryNestField::rectDistanceSquared(x, y, cx + HatcheryNestField::coopMaxX,
                cy - settings.mLaneHalfWidth, cx + HatcheryNestField::coopMaxX + settings.mLaneLength,
                cy + settings.mLaneHalfWidth) >= settings.mLaneClearance * settings.mLaneClearance - 1e-9);
            const double clear = settings.mPathHalfWidth + settings.mPathClearance;
            BOOST_CHECK(HatcheryNestField::segmentDistanceSquared(x, y, entrances[0].first, entrances[0].second,
                cx + HatcheryNestField::coopMaxX + settings.mLaneLength * 0.5, cy) >= clear * clear - 1e-9);
        }

        // Away from the nests and from each other
        for(uint32_t n = 0; n < nests.size(); ++n)
        {
            const double dx = nests[n].mX - x;
            const double dy = nests[n].mY - y;
            BOOST_CHECK(dx * dx + dy * dy >= settings.mFeatherNestClearance * settings.mFeatherNestClearance - 1e-9);
        }
        for(uint32_t j = 0; j < i; ++j)
        {
            const double dx = feathers[j].mX - x;
            const double dy = feathers[j].mY - y;
            BOOST_CHECK(dx * dx + dy * dy >= settings.mFeatherSpacing * settings.mFeatherSpacing - 1e-9);
        }
    }

    // The places do not depend on the order of the tiles
    std::vector<HatcheryNestField::TileCoord> reversedTiles(tiles.rbegin(), tiles.rend());
    std::vector<HatcheryNestField::TileCoord> reversedCoops(coops.rbegin(), coops.rend());
    const std::vector<HatcheryNestField::Place> again = HatcheryNestField::computeFeathers(reversedTiles, reversedCoops,
        entrances, nests, settings);
    BOOST_REQUIRE_EQUAL(again.size(), feathers.size());
    for(uint32_t i = 0; i < feathers.size(); ++i)
    {
        BOOST_CHECK_EQUAL(again[i].mX, feathers[i].mX);
        BOOST_CHECK_EQUAL(again[i].mY, feathers[i].mY);
    }

    // A hatchery without a tile has no feathers, one without a coop has them all the same
    BOOST_CHECK(HatcheryNestField::computeFeathers(std::vector<HatcheryNestField::TileCoord>(), coops, entrances, nests,
        settings).empty());
    BOOST_CHECK(HatcheryNestField::computeFeathers(tiles, std::vector<HatcheryNestField::TileCoord>(),
        std::vector<HatcheryNestField::TileCoord>(), std::vector<HatcheryNestField::Place>(), settings).size() >= 2u);
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
    RoosterContext context;
    context.mTurn = 100;
    context.mHasCoop = true;
    context.mHasHen = true;
    context.mCrowInterval = 60;
    context.mRoll = 99;

    // Nothing special: he struts
    RoosterPlan plan = HatcheryRooster::decide(context, settings);
    BOOST_CHECK(plan.mMood == RoosterMood::strut);

    // The dice decide what comes to his mind: chase (no roof sitting by chance, no calling of the hens, no leading of the chicks)
    context.mRoll = 0;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::chase);
    // Past the chase chance he only struts: the rooster never calls the hens or chicks to him
    context.mRoll = settings.mChasePercent;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::strut);
    for(uint32_t roll = settings.mChasePercent; roll < 100; ++roll)
    {
        context.mRoll = roll;
        BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::strut);
    }

    // Without a hen that mood is not chosen
    context.mHasHen = false;
    context.mRoll = 0;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::strut);
    context.mHasHen = true;

    // A crow when the time is up, then he goes on strutting (he does not stay on the roof)
    context.mRoll = 99;
    context.mSinceCrow = 60;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::crow);
    context.mSinceCrow = 0;
    context.mMood = RoosterMood::crow;
    context.mMoodTurns = 0;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::strut);

    // A mood with turns left goes on
    context.mMood = RoosterMood::crow;
    context.mMoodTurns = 5;
    context.mRoll = 0;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::crow);

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

    // A mood that is over (no turns left) is followed by strutting, whatever the turn is
    context.mMood = RoosterMood::chase;
    context.mMoodTurns = 0;
    context.mTurn = 800;
    context.mRoll = 99;
    context.mSinceCrow = 0;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::strut);
}

BOOST_AUTO_TEST_CASE(test_RoosterCrowTimer)
{
    RoosterSettings settings;
    settings.mCrowTurns = 7;
    RoosterContext context;
    context.mCrowInterval = 60;
    context.mRoll = 99;

    // The time of the day does not matter: only the crow timer starts a crow
    for(int64_t turn = 0; turn < 5000; turn += 250)
    {
        context.mTurn = turn;
        context.mSinceCrow = 0;
        BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::strut);
        context.mSinceCrow = 60;
        RoosterPlan plan = HatcheryRooster::decide(context, settings);
        BOOST_CHECK(plan.mMood == RoosterMood::crow);
        BOOST_CHECK_EQUAL(plan.mTurns, 7u);
    }

    // A crow in progress is not started again
    context.mMood = RoosterMood::crow;
    context.mMoodTurns = 2;
    BOOST_CHECK(HatcheryRooster::decide(context, settings).mMood == RoosterMood::crow);
    BOOST_CHECK_EQUAL(HatcheryRooster::decide(context, settings).mTurns, 2u);
}
