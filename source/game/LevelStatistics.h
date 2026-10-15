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

#ifndef LEVELSTATISTICS_H
#define LEVELSTATISTICS_H

#include <cstdint>
#include <vector>

//! \brief Counters of one seat as received with the levelStatistics notification
struct LevelStatisticsSeat
{
    LevelStatisticsSeat() :
        mSeatId(-1), mKeepersDefeated(0), mCreaturesKilled(0), mHeroesDestroyed(0),
        mRoomsCaptured(0), mItemsMade(0), mCreaturesConverted(0)
    {
    }

    int32_t mSeatId;
    uint32_t mKeepersDefeated;
    uint32_t mCreaturesKilled;
    uint32_t mHeroesDestroyed;
    uint32_t mRoomsCaptured;
    uint32_t mItemsMade;
    uint32_t mCreaturesConverted;
};

//! \brief Level debriefing data received from the server (client side)
struct LevelStatistics
{
    LevelStatistics() :
        mElapsedSeconds(0), mLevelWon(false)
    {
    }

    int32_t mElapsedSeconds;
    bool mLevelWon;
    std::vector<LevelStatisticsSeat> mSeats;
};

#endif // LEVELSTATISTICS_H
