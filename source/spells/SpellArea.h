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

#ifndef SPELLAREA_H
#define SPELLAREA_H

#include <vector>

class GameMap;
class Tile;

//! \brief Helpers for spells that act on all tiles inside a circle around a target tile.
namespace SpellArea
{
//! \brief Tells whether the given tile is inside the circle of the given radius around (x, y).
//! \param tile The tile to test, must not be nullptr.
//! \param x Tile x coordinate of the circle center.
//! \param y Tile y coordinate of the circle center.
//! \param radius Radius of the circle in tiles.
//! \return true if the tile center is at most radius tiles away from (x, y).
bool isTileInRadius(const Tile* tile, int x, int y, int radius);

//! \brief Collects all existing tiles of the game map that are inside the circle around (x, y).
//! \param gameMap The game map to read the tiles from.
//! \param x Tile x coordinate of the circle center.
//! \param y Tile y coordinate of the circle center.
//! \param radius Radius of the circle in tiles.
//! \return The tiles inside the circle; tiles outside the map are left out.
std::vector<Tile*> getTilesInRadius(GameMap* gameMap, int x, int y, int radius);
}

#endif // SPELLAREA_H
