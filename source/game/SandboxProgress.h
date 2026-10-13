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

#ifndef SANDBOXPROGRESS_H
#define SANDBOXPROGRESS_H

#include "utils/ResourceManager.h"

#include <boost/filesystem.hpp>

#include <fstream>
#include <set>
#include <string>

//! \brief Which sandbox realms the player completed. A realm that another realm names as its next one
//! can only be started once that other realm is complete. The progress is a text file in the user data
//! folder with the file name (without the extension) of one completed realm per line.
namespace SandboxProgress
{
    const std::string PROGRESS_FILE = "sandbox-progress.txt";

    //! \brief The file name without folder and extension, the name the progress file lists
    inline std::string getLevelStem(const std::string& levelFile)
    {
        return boost::filesystem::path(levelFile).stem().string();
    }

    inline std::set<std::string> loadCompleted()
    {
        std::set<std::string> completed;
        std::ifstream file((ResourceManager::getSingleton().getUserDataPath() + PROGRESS_FILE).c_str());
        std::string line;
        while(std::getline(file, line))
        {
            while(!line.empty() && ((line[line.size() - 1] == '\r') || (line[line.size() - 1] == ' ')))
                line.erase(line.size() - 1);

            if(!line.empty() && (line[0] != '#'))
                completed.insert(line);
        }
        return completed;
    }

    inline void markCompleted(const std::string& levelFile)
    {
        std::set<std::string> completed = loadCompleted();
        if(!completed.insert(getLevelStem(levelFile)).second)
            return;

        std::ofstream file((ResourceManager::getSingleton().getUserDataPath() + PROGRESS_FILE).c_str());
        file << "# Sandbox realms you completed. One per line. Add a line to open the realm that follows it.\n";
        for(std::set<std::string>::const_iterator it = completed.begin(); it != completed.end(); ++it)
            file << *it << "\n";
    }
}

#endif // SANDBOXPROGRESS_H
