#include "entities/GameEntityType.h"
#include "entities/RockLava.h"
#include "entities/Tile.h"
#include "gamemap/GameMap.h"
#include "render/RenderManager.h"
#include "network/ODServer.h"
#include "network/ServerNotification.h"
#include "utils/LogManager.h"
#include "utils/Random.h"

static const Ogre::Vector3 gravity( 0.0f, 0.0f, -9.81f) ; 

static const Ogre::Real mInitialZ(-0.5f);

RockLava::RockLava(GameMap* gameMap) :
    RenderedMovableEntity(gameMap, "" ,"Rocklava", 0.1, false),
    dormant(true),
    velocity(Ogre::Vector3 (0.05f, 0.04f, Random::gaussianRandomDouble() * 3.0 + 9.0))
    {
    }

void RockLava::update(Ogre::Real timeSinceLastFrame)
{
    if(!dormant)
    {
        Ogre::Vector3 newPosition  = getPosition();
        velocity += gravity * timeSinceLastFrame;
        newPosition  += velocity * timeSinceLastFrame;
        setPosition(newPosition);
        
    }


    if(getPosition().z < mInitialZ)
        dormant = true;

    
}

void RockLava::setPosition(const Ogre::Vector3& v, GameMap *gameMap )
{


    MovableGameEntity::setPosition(v, gameMap);
    RenderManager::getSingleton().rrPitchAroundAxis(this, Ogre::Degree(5));


}


GameEntityType RockLava::getObjectType() const
{

    return GameEntityType::lavaRock;
}


void RockLava::restoreEntityState()
{
    
    mPosition = Ogre::Vector3 (static_cast<Ogre::Real>(mInitialX), static_cast<Ogre::Real>(mInitialY),  static_cast<Ogre::Real>(mInitialZ));
}



RockLava* RockLava::getLavaRockEntityFromPacket(GameMap* gameMap, ODPacket& is)
{

    RockLava* entity = new RockLava(gameMap);
    entity->importFromPacket(is);
    return entity;
}

void RockLava::restartPosition()
{

    velocity = Ogre::Vector3 (0.05f, 0.04f, Random::gaussianRandomDouble() * 1.0 + 10.0);
    Ogre::Vector3 spawnPosition(static_cast<Ogre::Real>(mInitialX), static_cast<Ogre::Real>(mInitialY),  static_cast<Ogre::Real>(mInitialZ));
    
    setPosition(spawnPosition);



}


void RockLava::exportToPacket(ODPacket& os, const Seat* seat) const
{
    RenderedMovableEntity::exportToPacket(os, seat);
    os << mInitialX;
    os << mInitialY;
    os << velocity;
    os << dormant;

}

void RockLava::importFromPacket(ODPacket& is)
{
    RenderedMovableEntity::importFromPacket(is);
    OD_ASSERT_TRUE(is >> mInitialX);
    OD_ASSERT_TRUE(is >> mInitialY);
    OD_ASSERT_TRUE(is >> velocity);    
    OD_ASSERT_TRUE(is >> dormant);
}



void RockLava::clientUpkeep()
{
    Tile* tile = getGameMap()->getTile(mInitialX, mInitialY);

    
    if(dormant && !tile->getHasBridge() && !Random::Int(0,100))
    {

        dormant = false;
    }


    
}
