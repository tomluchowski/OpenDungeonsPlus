#include "entities/TileLava.h"
#include "entities/RockLava.h"
#include "gamemap/GameMap.h"
#include "utils/Helper.h"

#include "utils/LogManager.h"
#include "utils/Random.h"


TileLava::TileLava( GameMap* gameMap, int x, int y ):Tile(gameMap,x,y,TileType::lava,0),rl(nullptr)
{

}


bool TileLava::addItselfToContainer(TileContainer *tc)
{

    bool result = Tile::addItselfToContainer(tc);
    getGameMap()->addClientUpkeepEntity(this);

    if(!getIsOnServerMap())
        return result;

    getGameMap()->addActiveObject(this); 
    return result;

}


void TileLava::doUpkeep()
{

    if (rl == nullptr)
    {
        rl = new RockLava(getGameMap());
        rl->setInitialX(getX());
        rl->setInitialY(getY());
        rl->setName("MyFunkyRockLava_"+Helper::toString(getX()) + "_" +Helper::toString(getY())) ;
        rl->restartPosition();
        rl->addToGameMap();
        
        rl->createMesh();

    }



}

void TileLava::removeItselfFromContainer()
{
    getGameMap()->removeActiveObject(this);
}

TileLava::~TileLava()
{

}
