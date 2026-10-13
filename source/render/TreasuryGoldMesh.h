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

//! \brief Reads the detail option text ("full", "reduced" or "off"); anything else means full.
Detail detailFromString(const std::string& text);
//! \brief The option text of the detail ("full", "reduced" or "off").
const char* detailToString(Detail detail);

//! \brief Sets how much of the gold layer is drawn; the piles are built with this detail from now on.
void setDetail(Detail detail);
//! \brief The detail the gold layer is drawn with at the moment.
Detail getDetail();

//! \brief Returns the name of the mesh to load for the given object mesh name. Names that are not
//! pile names are returned unchanged. Pile meshes are built on demand; the detail "reduced" and "off" use the
//! coarse mesh without coins and gems.
//! A pile without gold (the scattered coins on the bare floor of an empty treasury) is only drawn at the
//! detail "full"; for the other settings an empty string comes back and no mesh is drawn.
//! A pile far from the camera (farAway) uses the reduced mesh at the detail full: coarse surface, no coins and
//! gems, and nothing at all for a tile without gold.
std::string prepareMesh(Ogre::SceneManager* sceneManager, const std::string& meshName, bool farAway = false);
}

#endif // TREASURYGOLDMESH_H
