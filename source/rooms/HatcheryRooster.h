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
    crow,   //! Jumps on a coop roof, crows from there (after a random time, see mCrowMin) and jumps down again
    chase,  //! Runs after a hen
    guard   //! Defends the flock against a threat
};

//! \brief Times and chances for the rooster, read from the config.
struct RoosterSettings
{
    RoosterSettings() :
        mCrowMin(40),
        mCrowMax(90),
        mChasePercent(4),
        mChaseTurns(10),
        mGuardTurns(6),
        mCrowTurns(4),
        mGuardFar(2.2),
        mGuardNear(0.9),
        mGuardApproachGap(1.8),
        mCatchDistance(0.55),
        mWalkGap(0.3),
        mHopDistance(0.6),
        mFightStandFactor(0.5),
        mChickPeepChance(12)
    {}

    //! Turns between two crows (random value in [mCrowMin, mCrowMax]).
    uint32_t mCrowMin;
    uint32_t mCrowMax;
    //! Chance (percent per turn) to start chasing a hen.
    uint32_t mChasePercent;
    //! Length in turns of a chase and a guard.
    uint32_t mChaseTurns;
    uint32_t mGuardTurns;
    //! Turns a crow lasts.
    uint32_t mCrowTurns;
    //! Guarding: farther than mGuardFar the rooster runs up to the creature (to mGuardApproachGap from it), between
    //! mGuardFar and mGuardNear he puffs up and pecks, closer than mGuardNear he runs off (tiles).
    double mGuardFar;
    double mGuardNear;
    double mGuardApproachGap;
    //! Distance (tiles) at which a chasing rooster has caught the hen.
    double mCatchDistance;
    //! Distance (tiles) the rooster stops from his goal when he walks to a hen or a roof, and the distance from the
    //! roof place within which he hops up at once.
    double mWalkGap;
    double mHopDistance;
    //! Fraction of the fighting reach at which a rooster stops in front of the other one.
    double mFightStandFactor;
    //! One in this many turns a chick of the hatchery peeps (at most one peep per turn and hatchery).
    uint32_t mChickPeepChance;
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
    //! Turns until the next crow for a random number.
    static uint32_t crowInterval(const RoosterSettings& settings, uint32_t random);

    //! Chooses what the rooster does now. A mood with turns left is kept, except that a threat always
    //! wins.
    static RoosterPlan decide(const RoosterContext& context, const RoosterSettings& settings);
};

#endif // HATCHERYROOSTER_H
