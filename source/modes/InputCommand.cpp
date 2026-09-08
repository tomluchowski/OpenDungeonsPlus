#include "modes/InputCommand.h"

#include "entities/Tile.h"

#include <OgreColourValue.h>

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
