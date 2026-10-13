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
#include <vector>

namespace Ogre
{
class ManualObject;
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

//! A dynamic copy of the pile with the given name (full detail only, null for any other case) that can be dented:
//! the dent lies at (u, v) across the tile (0..1) with the given radius in tile units. The caller owns the object
//! (SceneManager::destroyManualObject) and draws it in place of the entity of the pile while the dent lasts.
Ogre::ManualObject* createDentedPile(Ogre::SceneManager* sceneManager, const std::string& meshName, float u, float v,
    float radius);
//! Rewrites a dented pile with the dent at the given depth (tile units, 0 = no dent)
void updateDentedPile(Ogre::ManualObject* object, const std::string& meshName, float u, float v, float radius,
    float depth);

//! \brief The piles drawn on this client, by tile, so objects can stand in the gold and effects can find the piles.
//! Names that are not pile names, and every pile while the detail is off, are ignored.
//! The room (any pointer identifying it, may be null) lets effects share a budget per room.
//! Returns the level this tile had when it was last registered (-1 when it never was, or the pile is
//! ignored), so the caller can tell a pile that grew from one that was taken from.
int registerPile(const std::string& entityName, float x, float y, const std::string& meshName,
    const void* room = nullptr);
//! Forgets the pile, if the tile still holds the pile of that entity (a newer pile stays)
void unregisterPile(const std::string& entityName, float x, float y);
//! Forgets every registered pile (the scene is cleared)
void clearPiles();

//! Height of the gold surface at the given map position, 0 when there is no pile. The level of the
//! pile of that tile (0 when none) is returned in level.
float surfaceHeight(float x, float y, int& level);

//! A pile: its tile, its level and the room it was registered for
struct FullPile
{
    int mX;
    int mY;
    const void* mRoom;
    int mLevel;
};

//! Lists the completely filled piles (the sources of the gold dust over full treasuries)
void collectFullPiles(std::vector<FullPile>& piles);
//! Lists the piles of at least the given level (sparkles and sliding coins need a deep pile)
void collectPiles(std::vector<FullPile>& piles, int minLevel);
}

#endif // TREASURYGOLDMESH_H
