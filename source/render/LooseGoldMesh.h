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

#ifndef LOOSEGOLDMESH_H
#define LOOSEGOLDMESH_H

#include <string>

namespace Ogre
{
class SceneManager;
}

//! \brief Client side meshes for gold outside the treasury: a small coin heap for gold lying on the
//! floor and a sack with coins for gold carried by a worker. Both come in four sizes that follow the
//! four classic stack meshes the server already names, so no new data is sent. The meshes are built
//! here the first time they are needed. Nothing is changed when the treasury detail option is off.
namespace LooseGoldMesh
{
//! Number of sizes (1 = least gold)
static const int sizeCount = 4;

//! Size 1..sizeCount of a classic stack mesh name ("GoldstackLv1".."GoldstackLv4"), 0 for any other name
int sizeFromStackName(const std::string& meshName);

//! Returns the name of the coin heap mesh for the stack mesh name (built on demand). Names that
//! are not stack names, and the detail option "off", return the name unchanged.
std::string prepareHeap(Ogre::SceneManager* sceneManager, const std::string& meshName);

//! Returns the name of the sack mesh for the stack mesh name (built on demand), or an empty string
//! when no sack is to be used (not a stack name, or the detail option is "off").
std::string prepareSack(Ogre::SceneManager* sceneManager, const std::string& meshName);
}

#endif // LOOSEGOLDMESH_H
