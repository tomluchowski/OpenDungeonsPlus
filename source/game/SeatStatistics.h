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

#ifndef SEATSTATISTICS_H
#define SEATSTATISTICS_H

#include <cstdint>

//! \brief Per-seat counters shown in the level debriefing. Filled on the server only, never saved:
//! they start at 0 when a map is loaded (also when a saved game is loaded).
struct SeatStatistics
{
    SeatStatistics()
    {
        reset();
    }

    void reset()
    {
        mKeepersDefeated = 0;
        mCreaturesKilled = 0;
        mHeroesDestroyed = 0;
        mRoomsCaptured = 0;
        mItemsMade = 0;
        mCreaturesConverted = 0;
    }

    //! \brief Enemy dungeon hearts destroyed by a final blow of this seat
    uint32_t mKeepersDefeated;
    //! \brief Creatures of non-allied seats (not playing the Hero faction) killed by this seat
    uint32_t mCreaturesKilled;
    //! \brief Creatures of non-allied seats playing the Hero faction killed by this seat
    uint32_t mHeroesDestroyed;
    //! \brief Rooms of non-allied seats whose last tile this seat took over
    uint32_t mRoomsCaptured;
    //! \brief Traps crafted in this seat's workshops
    uint32_t mItemsMade;
    //! \brief Creatures of other seats converted by this seat's torture rooms
    uint32_t mCreaturesConverted;
};

#endif // SEATSTATISTICS_H
