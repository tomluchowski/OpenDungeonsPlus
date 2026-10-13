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

#ifndef HATCHERYROOSTER_H
#define HATCHERYROOSTER_H

#include <cstdint>

//! \brief What the rooster of a hatchery is doing.
enum class RoosterMood : uint32_t
{
    strut,  //! Walks around proudly
    perch,  //! Sits on a coop roof as lookout
    crow,   //! Crows (from the roof if possible)
    chase,  //! Runs after a hen
    guard,  //! Defends the flock against a threat
    lead,   //! Leads the chicks
    roost   //! Sleeps (on the roof if possible)
};

//! \brief Times, chances and the length of the day for the rooster, read from the config.
struct RoosterSettings
{
    RoosterSettings() :
        mCrowMin(40),
        mCrowMax(90),
        mChasePercent(4),
        mLeadPercent(3),
        mPerchPercent(4),
        mPerchTurns(25),
        mChaseTurns(10),
        mGuardTurns(6),
        mLeadTurns(8),
        mDayTurns(1680),
        mNightPercent(30)
    {}

    //! Turns between two crows (random value in [mCrowMin, mCrowMax]).
    uint32_t mCrowMin;
    uint32_t mCrowMax;
    //! Chance (percent per turn) to start chasing a hen, leading the chicks or sitting on a roof.
    uint32_t mChasePercent;
    uint32_t mLeadPercent;
    uint32_t mPerchPercent;
    //! Length in turns of a stay on the roof, a chase, a guard and a lead.
    uint32_t mPerchTurns;
    uint32_t mChaseTurns;
    uint32_t mGuardTurns;
    uint32_t mLeadTurns;
    //! Turns of a whole day and the part of it (percent, at its end) that is night.
    uint32_t mDayTurns;
    uint32_t mNightPercent;
};

//! \brief What the rooster sees around him.
struct RoosterContext
{
    RoosterContext() :
        mTurn(0),
        mMood(RoosterMood::strut),
        mMoodTurns(0),
        mSinceCrow(0),
        mCrowInterval(60),
        mHasCoop(false),
        mHasHen(false),
        mHasChick(false),
        mThreat(false),
        mRoll(0)
    {}

    int64_t mTurn;
    RoosterMood mMood;
    //! Turns left of the current mood.
    uint32_t mMoodTurns;
    uint32_t mSinceCrow;
    //! Turns until the next crow, a random value in the range of the settings.
    uint32_t mCrowInterval;
    bool mHasCoop;
    bool mHasHen;
    bool mHasChick;
    //! A creature that is after the chickens is close.
    bool mThreat;
    //! Random number in [0, 99].
    uint32_t mRoll;
};

struct RoosterPlan
{
    RoosterMood mMood;
    uint32_t mTurns;
};

//! \brief Rules of the rooster, without a dependency on the game map so they can be tested alone.
class HatcheryRooster
{
public:
    //! True during the last part of each day.
    static bool isNight(int64_t turn, const RoosterSettings& settings);

    //! True on the first turn of a day.
    static bool isNewDay(int64_t turn, const RoosterSettings& settings);

    //! Turns until the next crow for a random number.
    static uint32_t crowInterval(const RoosterSettings& settings, uint32_t random);

    //! Chooses what the rooster does now. A mood with turns left is kept, except that a threat always
    //! wins and that the night and a new day send him to the roof.
    static RoosterPlan decide(const RoosterContext& context, const RoosterSettings& settings);
};

#endif // HATCHERYROOSTER_H
