#include "rooms/FencedRoom.h"

#include "creatureaction/CreatureActionUseRoom.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "game/Seat.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/MakeUnique.h"



FencedRoom::FencedRoom(GameMap* gameMap):
    Room(gameMap)
{

}



Tile* FencedRoom::getActualRoomTile(int index)
{
    if(index >= static_cast<int>(mActualTiles.size()))
    {
        OD_LOG_ERR("name=" + getName() + ", index=" + Helper::toString(index) + ", size=" + Helper::toString(mActualTiles.size()));
        return nullptr;
    }

    return mActualTiles[index];
}

void FencedRoom::splitTilesIntoClasses()
{


    std::vector<Tile*> allTiles = getCoveredTiles();
    // check for the actual prison tiles -- those are the middle ones e.g.
    // the tiles which has all 8 neigbours being the prison roomType
    mActualTiles.clear();
    for(Tile* tt : allTiles)
    {
        if(tt->getEightNeighbouringRoomTypeCount(getType()) == 8)
            mActualTiles.push_back(tt);
    }
    std::vector<Tile*> diff;
    std::sort(mActualTiles.begin(),mActualTiles.end());
    std::sort(allTiles.begin(),allTiles.end());
    std::set_difference(allTiles.begin(),allTiles.end(),mActualTiles.begin(),mActualTiles.end(), std::inserter(diff,diff.begin()));
    mFenceTiles.clear();
    for(Tile* tt: diff)
    {
        bool fencing = false;
        for(Tile* ss: mActualTiles)
            if(std::abs(ss->getX() - tt->getX() ) <= 1 &&  (std::abs(ss->getY() - tt->getY() ) <= 1   ))
                fencing = true;
        if(fencing)
                mFenceTiles.push_back(tt);

    }
    // check for unused tiles ( those would be neither used nor fencing tiles
    //-- too alone to consist a valid prison/ arena/ fencing tile
    mUnusedTiles.clear();
    std::sort(mFenceTiles.begin(),mFenceTiles.end());
    std::set_difference(diff.begin(),diff.end(),mFenceTiles.begin(),mFenceTiles.end(), std::inserter(mUnusedTiles ,mUnusedTiles.begin()));    


    
}


double FencedRoom::getCreatureSpeed(const Creature* creature, Tile* tile) const
{
    if( std::find(mActualTiles.begin(),mActualTiles.end(), tile ) != mActualTiles.end())
    {
        Tile* creatureTile = creature->getPositionTile();
        if(std::find(mActualTiles.begin(),mActualTiles.end(), creatureTile ) != mActualTiles.end())
            return creature->getMoveSpeedGround();
        else
            return 0.0;

    }
    else
        return creature->getMoveSpeedGround();
}

void FencedRoom::creatureDropped(Creature& creature)
{
    // Owned and enemy creatures can be tortured
    if((getSeat() != creature.getSeat()) && (getSeat()->isAlliedSeat(creature.getSeat())))
        return;

    if(!hasOpenCreatureSpot(&creature))
        return;

    Tile* tile = creature.getPositionTile();
    
    // We only push the use room action. We do not want this creature to be
    // considered as searching for a job
    if( std::find(mActualTiles.begin(),mActualTiles.end(), tile ) != mActualTiles.end())
    {
        creature.clearActionQueue();
        creature.pushAction(Utils::make_unique<CreatureActionUseRoom>(creature, *this, true));
    }
}

void FencedRoom::setupRoom(const std::string& name, Seat* seat, const std::vector<Tile*>& tiles)
{
    Room::setupRoom(name, seat, tiles);
    
    std::vector<Creature*> creatures;
    
    for(Creature* creature : getGameMap()->getCreatures())
    {
        for(Tile* tile: getActualRoomTiles())
        {
            if(getGameMap()->pathExists(creature, tile, creature->getPositionTile()))
            {
                creatures.push_back(creature);
                break;
            }
        }
    }
    // We check if a creature from the given seat has a path through the door and stop it if there is
    for(Creature* creature : creatures)
        creature->checkWalkPathValid();    
}


Tile* FencedRoom::getGateTile()
{
    for(Tile* tile: mFenceTiles)
    {
        for(Tile* neighbour: tile->getAllNeighbors())
        {
            if(std::find(mActualTiles.begin(), mActualTiles.end(), neighbour)!=mActualTiles.end())
                return tile;
        }
    }
    return nullptr;
}
