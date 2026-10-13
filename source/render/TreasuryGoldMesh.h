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

#ifndef TREASURYGOLDMESH_H
#define TREASURYGOLDMESH_H

#include <string>

namespace Ogre
{
class SceneManager;
}

//! \brief Client side meshes of the treasury gold layer (see TreasuryGoldLayer.h). The server only
//! replicates the name of the pile; the mesh behind the name is built here the first time it is needed.
namespace TreasuryGoldMesh
{
//! How much of the gold layer is drawn ("Treasury detail" option)
enum class Detail
{
    full,
    reduced,
    off
};

Detail detailFromString(const std::string& text);
const char* detailToString(Detail detail);

void setDetail(Detail detail);
Detail getDetail();

//! \brief Returns the name of the mesh to load for the given object mesh name. Names that are not
//! pile names are returned unchanged. Pile meshes are built on demand, or replaced by the classic gold
//! stacks when the detail is off.
std::string prepareMesh(Ogre::SceneManager* sceneManager, const std::string& meshName);
}

#endif // TREASURYGOLDMESH_H
