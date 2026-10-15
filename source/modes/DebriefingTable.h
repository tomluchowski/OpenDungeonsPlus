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

#ifndef DEBRIEFINGTABLE_H
#define DEBRIEFINGTABLE_H

#include "game/LevelStatistics.h"

#include <cstdint>
#include <string>
#include <vector>

//! \brief Layout of the statistics table of the debriefing window
namespace DebriefingTableSettings
{
    const float ROW_HEIGHT = 26.0f;
    //! Share of the table width taken by the row labels; the seat columns share the rest
    const float LABEL_WIDTH = 0.4f;
    const size_t ROW_COUNT = 6;
}

//! \brief One number of the table: the text of the value and the seat it belongs to
struct DebriefingTableCell
{
    int32_t mSeatId;
    std::string mText;
};

//! \brief One row of the table: the label and one cell per seat, in the order of the statistics
struct DebriefingTableRow
{
    std::string mLabel;
    std::vector<DebriefingTableCell> mCells;
};

inline const char* debriefingTableLabel(size_t row)
{
    switch(row)
    {
        case 0:
            return "Enemy keepers defeated";
        case 1:
            return "Enemy creatures killed";
        case 2:
            return "Heroes destroyed";
        case 3:
            return "Rooms captured";
        case 4:
            return "Items made";
        default:
            return "Creatures converted";
    }
}

inline uint32_t debriefingTableValue(const LevelStatisticsSeat& seat, size_t row)
{
    switch(row)
    {
        case 0:
            return seat.mKeepersDefeated;
        case 1:
            return seat.mCreaturesKilled;
        case 2:
            return seat.mHeroesDestroyed;
        case 3:
            return seat.mRoomsCaptured;
        case 4:
            return seat.mItemsMade;
        default:
            return seat.mCreaturesConverted;
    }
}

//! \brief Turns the statistics into the rows of the table. Without seats there are no rows.
inline std::vector<DebriefingTableRow> buildDebriefingTable(const LevelStatistics& statistics)
{
    std::vector<DebriefingTableRow> rows;
    if(statistics.mSeats.empty())
        return rows;
    for(size_t row = 0; row < DebriefingTableSettings::ROW_COUNT; ++row)
    {
        DebriefingTableRow tableRow;
        tableRow.mLabel = debriefingTableLabel(row);
        for(size_t seat = 0; seat < statistics.mSeats.size(); ++seat)
        {
            DebriefingTableCell cell;
            cell.mSeatId = statistics.mSeats[seat].mSeatId;
            cell.mText = std::to_string(debriefingTableValue(statistics.mSeats[seat], row));
            tableRow.mCells.push_back(cell);
        }
        rows.push_back(tableRow);
    }
    return rows;
}

//! \brief CEGUI area string (relative to the table area) of the label of a row
inline std::string debriefingLabelArea(size_t row)
{
    return "{{0,0},{0," + std::to_string(row * DebriefingTableSettings::ROW_HEIGHT) + "},{"
        + std::to_string(DebriefingTableSettings::LABEL_WIDTH) + ",0},{0,"
        + std::to_string((row + 1) * DebriefingTableSettings::ROW_HEIGHT) + "}}";
}

//! \brief CEGUI area string of the cell of a seat column; the columns share the width right of the labels
inline std::string debriefingCellArea(size_t row, size_t column, size_t columnCount)
{
    const float columnWidth = (1.0f - DebriefingTableSettings::LABEL_WIDTH) / static_cast<float>(columnCount);
    const float left = DebriefingTableSettings::LABEL_WIDTH + columnWidth * static_cast<float>(column);
    return "{{" + std::to_string(left) + ",0},{0," + std::to_string(row * DebriefingTableSettings::ROW_HEIGHT)
        + "},{" + std::to_string(left + columnWidth) + ",0},{0,"
        + std::to_string((row + 1) * DebriefingTableSettings::ROW_HEIGHT) + "}}";
}

//! \brief Colour property value (AARRGGBB, upper case hex) for a seat colour given as ARGB
inline std::string debriefingColourText(uint32_t argb)
{
    const char* digits = "0123456789ABCDEF";
    std::string result;
    for(int shift = 28; shift >= 0; shift -= 4)
        result += digits[(argb >> shift) & 0xF];
    return result;
}

//! \brief "Level won" comes from the received statistics when there are some, else from the client's own value
inline bool debriefingLevelWon(bool hasStatistics, const LevelStatistics& statistics, bool fallback)
{
    return hasStatistics ? statistics.mLevelWon : fallback;
}

//! \brief Elapsed seconds come from the received statistics when there are some, else from the client's own value
inline int64_t debriefingSeconds(bool hasStatistics, const LevelStatistics& statistics, int64_t fallback)
{
    return hasStatistics ? static_cast<int64_t>(statistics.mElapsedSeconds) : fallback;
}

inline std::string debriefingOutcomeText(bool levelWon)
{
    return levelWon ? "Level won: Yes" : "Level won: No";
}

#endif // DEBRIEFINGTABLE_H
