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

#include "spells/SpellArea.h"

#include "entities/Tile.h"
#include "gamemap/GameMap.h"

namespace SpellArea
{
bool isTileInRadius(const Tile* tile, int x, int y, int radius)
{
    int dx = tile->getX() - x;
    int dy = tile->getY() - y;
    return (dx * dx + dy * dy) <= (radius * radius);
}

std::vector<Tile*> getTilesInRadius(GameMap* gameMap, int x, int y, int radius)
{
    std::vector<Tile*> tiles;
    for(int xx = x - radius; xx <= x + radius; ++xx)
    {
        for(int yy = y - radius; yy <= y + radius; ++yy)
        {
            Tile* tile = gameMap->getTile(xx, yy);
            if(tile == nullptr)
                continue;

            if(!isTileInRadius(tile, x, y, radius))
                continue;

            tiles.push_back(tile);
        }
    }
    return tiles;
}
}
