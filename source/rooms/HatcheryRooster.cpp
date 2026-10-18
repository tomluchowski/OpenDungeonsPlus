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

    // The moods go on until their time is over
    if((context.mMoodTurns > 0) && (context.mMood != RoosterMood::strut))
        return plan;

    if(context.mSinceCrow >= context.mCrowInterval)
    {
        plan.mMood = RoosterMood::crow;
        plan.mTurns = settings.mCrowTurns;
        return plan;
    }

    // Strutting: sometimes something else comes to his mind
    plan.mMood = RoosterMood::strut;
    plan.mTurns = 0;
    uint32_t chaseLimit = settings.mChasePercent;
    if(context.mHasHen && (context.mRoll < chaseLimit))
    {
        plan.mMood = RoosterMood::chase;
        plan.mTurns = settings.mChaseTurns;
    }
    return plan;
}
