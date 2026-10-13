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

#include "rooms/HatcheryRooster.h"

#include <algorithm>

bool HatcheryRooster::isNight(int64_t turn, const RoosterSettings& settings)
{
    if((settings.mDayTurns == 0) || (turn < 0))
        return false;

    uint32_t nightPercent = std::min<uint32_t>(settings.mNightPercent, 100);
    uint64_t phase = static_cast<uint64_t>(turn) % settings.mDayTurns;
    uint64_t nightStart = static_cast<uint64_t>(settings.mDayTurns) * (100 - nightPercent) / 100;
    return (nightPercent > 0) && (phase >= nightStart);
}

bool HatcheryRooster::isNewDay(int64_t turn, const RoosterSettings& settings)
{
    if((settings.mDayTurns == 0) || (turn < 0))
        return false;

    return (static_cast<uint64_t>(turn) % settings.mDayTurns) == 0;
}

uint32_t HatcheryRooster::crowInterval(const RoosterSettings& settings, uint32_t random)
{
    uint32_t minTurns = std::max<uint32_t>(1, settings.mCrowMin);
    uint32_t maxTurns = std::max(minTurns, settings.mCrowMax);
    return minTurns + (random % (maxTurns - minTurns + 1));
}

RoosterPlan HatcheryRooster::decide(const RoosterContext& context, const RoosterSettings& settings)
{
    RoosterPlan plan;
    plan.mMood = context.mMood;
    plan.mTurns = context.mMoodTurns;

    // Defending the flock comes first
    if(context.mThreat)
    {
        if(context.mMood != RoosterMood::guard)
        {
            plan.mMood = RoosterMood::guard;
            plan.mTurns = settings.mGuardTurns;
        }
        return plan;
    }

    // A new day starts with a crow. At night the rooster sleeps.
    if((context.mMood != RoosterMood::crow) && isNewDay(context.mTurn, settings))
    {
        plan.mMood = RoosterMood::crow;
        plan.mTurns = 4;
        return plan;
    }
    if(isNight(context.mTurn, settings))
    {
        plan.mMood = RoosterMood::roost;
        plan.mTurns = std::max<uint32_t>(1, settings.mDayTurns / 10);
        return plan;
    }

    // The moods go on until their time is over
    if((context.mMoodTurns > 0) && (context.mMood != RoosterMood::strut) &&
       (context.mMood != RoosterMood::roost))
        return plan;

    // After a crow the rooster stays on the roof for a while
    if(context.mMood == RoosterMood::crow)
    {
        plan.mMood = context.mHasCoop ? RoosterMood::perch : RoosterMood::strut;
        plan.mTurns = context.mHasCoop ? std::max<uint32_t>(1, settings.mPerchTurns / 3) : 0;
        return plan;
    }

    if(context.mSinceCrow >= context.mCrowInterval)
    {
        plan.mMood = RoosterMood::crow;
        plan.mTurns = 4;
        return plan;
    }

    // Strutting (also when the night is over): sometimes something else comes to his mind
    plan.mMood = RoosterMood::strut;
    plan.mTurns = 0;
    uint32_t chaseLimit = settings.mChasePercent;
    uint32_t leadLimit = chaseLimit + settings.mLeadPercent;
    uint32_t perchLimit = leadLimit + settings.mPerchPercent;
    uint32_t callLimit = perchLimit + settings.mCallPercent;
    if(context.mHasHen && (context.mRoll < chaseLimit))
    {
        plan.mMood = RoosterMood::chase;
        plan.mTurns = settings.mChaseTurns;
    }
    else if(context.mHasChick && (context.mRoll >= chaseLimit) && (context.mRoll < leadLimit))
    {
        plan.mMood = RoosterMood::lead;
        plan.mTurns = settings.mLeadTurns;
    }
    else if(context.mHasCoop && (context.mRoll >= leadLimit) && (context.mRoll < perchLimit))
    {
        plan.mMood = RoosterMood::perch;
        plan.mTurns = settings.mPerchTurns;
    }
    else if(context.mHasHen && (context.mRoll >= perchLimit) && (context.mRoll < callLimit))
    {
        plan.mMood = RoosterMood::call;
        plan.mTurns = settings.mCallTurns;
    }
    return plan;
}
