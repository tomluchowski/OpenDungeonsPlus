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

#ifndef SELECTIONSIZE_H
#define SELECTIONSIZE_H

#include <algorithm>
#include <vector>

//! \brief Width and height of the bounding rectangle of the given tiles (anything with getX()/getY()).
//! Returns false and sets both to 0 when the list is empty.
template<typename TileType>
bool getSelectionSize(const std::vector<TileType*>& tiles, int& width, int& height)
{
    width = 0;
    height = 0;
    if(tiles.empty())
        return false;

    int minX = tiles[0]->getX();
    int maxX = minX;
    int minY = tiles[0]->getY();
    int maxY = minY;
    for(size_t i = 1; i < tiles.size(); ++i)
    {
        minX = std::min(minX, tiles[i]->getX());
        maxX = std::max(maxX, tiles[i]->getX());
        minY = std::min(minY, tiles[i]->getY());
        maxY = std::max(maxY, tiles[i]->getY());
    }
    width = maxX - minX + 1;
    height = maxY - minY + 1;
    return true;
}

#endif // SELECTIONSIZE_H
