/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "modes/InputCommand.h"

#include "entities/GameEntity.h"
#include "entities/Tile.h"

#include <OgreColourValue.h>

#include <algorithm>
#include <string>

void InputCommand::displayTileBuildFailure(const Tile* tile, Seat* seat)
{
    std::string reason;
    if(tile == nullptr)
        reason = "Point at a tile inside the map.";
    else if(tile->isFullTile())
        reason = "Dig out this tile before building.";
    else if(tile->getIsBuilding())
        reason = "A room or trap already occupies this tile.";
    else if(!tile->isClaimedForSeat(seat))
        reason = "Claim this ground before building.";
    else
        reason = "No buildable tiles in this selection.";

    displayText(Ogre::ColourValue::Red, reason);
}

void InputCommand::selectTilesOfEntities(const std::vector<GameEntity*>& entities)
{
    std::vector<Tile*> tiles;
    for(GameEntity* entity : entities)
    {
        Tile* tile = entity->getPositionTile();
        if(tile != nullptr && std::find(tiles.begin(), tiles.end(), tile) == tiles.end())
            tiles.push_back(tile);
    }
    selectTiles(tiles);
}
