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


#ifndef MAPHANDLER_H
#define MAPHANDLER_H

#include <string>

class GameMap;

enum class GameEntityType;

//! \brief A small structure storing level info for the player
struct LevelInfo
{
    LevelInfo() :
        mIsSandbox(false)
    {}

    //! \brief The level visible name
    std::string mLevelName;

    //! \brief The level description, player's slot, size, ...
    std::string mLevelDescription;

    //! \brief True for a sandbox level (no goals, hero toolbox and invasions)
    bool mIsSandbox;

    //! \brief The name of a sandbox realm (empty for a sandbox level that is not one) and the level file of the realm after it
    std::string mSandboxRealm;
    std::string mSandboxNext;
};

namespace MapHandler
{
    //! \brief Loads a level or saved game file into the game map, including the optional [Triggers] section.
    //! \param fileName The level file to read.
    //! \param gameMap The game map to fill.
    //! \return False if the file or one of its sections is invalid.
    bool readGameMapFromFile(const std::string& fileName, GameMap& gameMap);

    //! \brief Writes the game map to a level file, including the [Triggers] section if the level has a script.
    //! \param fileName The level file to write.
    //! \param gameMap The game map to save.
    //! \return False if the file could not be written.
    bool writeGameMapToFile(const std::string& fileName, GameMap& gameMap);

    bool readGameEntity(GameMap& gameMap, const std::string& item, GameEntityType type, std::stringstream& levelFile);

    bool loadEquipments(const std::string& fileName, GameMap& gameMap);

    bool loadCreatureDefinition(const std::string& fileName, GameMap& gameMap);

    //! \brief Reads the main user map info. Returns true if the level could be read and levelInfo is set to
    //! corresponding info. Returns false otherwise.
    bool getMapInfo(const std::string& fileName, LevelInfo& levelInfo);

    //! \brief Counts the number of all seats in the levelfile, needed to initialize the structure
    //! everVisitedflagpool plus map Size X and map Size Y

    void getXandYandSeatsNumber(std::stringstream& ss,int& xx, int& yy, int& numberOfSeats);
    //! \brief Level extension constant, used in different GUI modes.
    static const std::string LEVEL_EXTENSION = ".level";
};

#endif // MAPHANDLER_H
