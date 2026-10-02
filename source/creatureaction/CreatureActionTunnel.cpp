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

#include "creatureaction/CreatureActionTunnel.h"

#include "entities/Creature.h"
#include "entities/Tile.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "rooms/Room.h"
#include "rooms/RoomType.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <cstdlib>

std::function<bool()> CreatureActionTunnel::action()
{
    return std::bind(&CreatureActionTunnel::handleTunnel,
        std::ref(mCreature));
}

bool CreatureActionTunnel::handleTunnel(Creature& creature)
{
    Tile* myTile = creature.getPositionTile();
    if(myTile == nullptr)
    {
        OD_LOG_ERR("name=" + creature.getName() + ", position=" + Helper::toString(creature.getPosition()));
        creature.popAction();
        return false;
    }

    if(creature.hasSlapEffect())
    {
        creature.popAction();
        return true;
    }

    // We look for the closest dungeon heart of an enemy seat
    GameMap* gameMap = creature.getGameMap();
    Tile* target = nullptr;
    int closestDist = 0;
    std::vector<Room*> hearts = gameMap->getRoomsByType(RoomType::dungeonTemple);
    for(Room* heart : hearts)
    {
        if(heart->getSeat()->isAlliedSeat(creature.getSeat()))
            continue;

        std::vector<Tile*> heartTiles = heart->getCoveredTiles();
        if(heartTiles.empty())
            continue;

        Tile* heartTile = heartTiles.front();
        int distX = heartTile->getX() - myTile->getX();
        int distY = heartTile->getY() - myTile->getY();
        int dist = distX * distX + distY * distY;
        if((target == nullptr) || (dist < closestDist))
        {
            target = heartTile;
            closestDist = dist;
        }
    }

    // Nothing to tunnel to, or we can walk there: no need to dig
    if((target == nullptr) || gameMap->pathExists(&creature, myTile, target))
    {
        creature.popAction();
        return true;
    }

    // We try the axis with the biggest distance first, then the other one
    int dx = target->getX() - myTile->getX();
    int dy = target->getY() - myTile->getY();
    int stepX = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
    int stepY = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);
    Tile* tileX = (stepX != 0) ? gameMap->getTile(myTile->getX() + stepX, myTile->getY()) : nullptr;
    Tile* tileY = (stepY != 0) ? gameMap->getTile(myTile->getX(), myTile->getY() + stepY) : nullptr;
    std::vector<Tile*> candidates;
    if(std::abs(dx) >= std::abs(dy))
    {
        candidates.push_back(tileX);
        candidates.push_back(tileY);
    }
    else
    {
        candidates.push_back(tileY);
        candidates.push_back(tileX);
    }

    for(Tile* tile : candidates)
    {
        if(tile == nullptr)
            continue;

        if(creature.canGoThroughTile(tile))
        {
            creature.setDestination(tile);
            return false;
        }

        // Gems cannot be dug out, other walls can unless they are claimed by an enemy
        if((tile->getFullness() <= 0.0) ||
           (tile->getTileVisual() == TileVisual::gemFull) ||
           !tile->isDiggable(creature.getSeat()))
        {
            continue;
        }

        Ogre::Vector3 walkDirection(tile->getX() - myTile->getX(), tile->getY() - myTile->getY(), 0);
        creature.setAnimationState(EntityAnimation::dig_anim, true, walkDirection);
        tile->digOut(creature.getDigRate());
        if(tile->getFullness() <= 0.0)
            creature.setDestination(tile);

        return false;
    }

    // Blocked by solid rock, enemy walls or gems
    creature.popAction();
    return false;
}
