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

#include "rooms/RoomPrison.h"

#include "creatureaction/CreatureActionUseRoom.h"
#include "entities/BuildingObject.h"
#include "entities/Creature.h"
#include "entities/GameEntityType.h"
#include "entities/SmallSpiderEntity.h"
#include "entities/Tile.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "modes/InputCommand.h"
#include "modes/InputManager.h"
#include "network/ODServer.h"
#include "network/ServerNotification.h"
#include "rooms/RoomManager.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/MakeUnique.h"
#include "utils/Random.h"

const std::string RoomPrisonName = "Prison";
const std::string RoomPrisonNameDisplay = "Prison room";
const RoomType RoomPrison::mRoomType = RoomType::prison;
const TileVisual RoomPrison::mRoomVisual= TileVisual::prisonRoom;



namespace
{
class RoomPrisonFactory : public RoomFactory
{
    RoomType getRoomType() const override
    { return RoomPrison::mRoomType; }

    TileVisual getVisualType() const override
    { return RoomPrison::mRoomVisual; }
    
    const std::string& getName() const override
    { return RoomPrisonName; }

    const std::string& getNameReadable() const override
    { return RoomPrisonNameDisplay; }

    int getCostPerTile() const override
    { return ConfigManager::getSingleton().getRoomConfigInt32("PrisonCostPerTile"); }

    void checkBuildRoom(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        Player* player = gameMap->getLocalPlayer();
        std::vector<Tile*> buildableTiles = gameMap->getBuildableTilesForPlayerInArea(inputManager.mXPos,
        inputManager.mYPos, inputManager.mLStartDragX, inputManager.mLStartDragY, player);
        bool found = false;
        // Check for the creature occupying candidate tiles
        for(std::vector<Tile*>::iterator ii = buildableTiles.begin();  ii<buildableTiles.end() && !found ; ++ii)
        {
            const std::vector<GameEntity*>& entitiesInTile = (*ii)->getEntitiesInTile();
            for( std::vector<GameEntity*>::const_iterator it = entitiesInTile.begin(); it<entitiesInTile.end() && !found; ++it)
            {
                if((*it)->getObjectType() == GameEntityType::creature)
                    found =true;
            }
        }
        // if the creature is found we display message and return
        if(found)
        {
            inputCommand.displayText(Ogre::ColourValue::Red, "Prison Room can only be placed on empty tiles, without Creatures  ");
            return;
        }
        
        checkBuildRoomDefault(gameMap, RoomPrison::mRoomType, inputManager, inputCommand);
    }

    bool checkCreaturesOccupyingPotentialPrison(std::vector<Tile*> tiles, GameMap* gameMap) const
    {
        // check if the potential nearby prison room tiles have creatures
        // on the unused prison part tiles, if so there is potential blunder
        // i.e. the creatures from there could be closed up in a prison physically and wouldn't be
        // incancerated -- that would create a bug in the game mechanics
        std::vector<Tile*> nearbyTiles = gameMap->tilesBorderedByRegion(tiles);
        for(Tile* tt: nearbyTiles)
        {
            if(tt->getCoveringRoom()!=nullptr &&
               tt->getCoveringRoom()->getType()==RoomType::prison)
            {
                RoomPrison* rp = static_cast<RoomPrison*>(tt->getCoveringRoom());
                for(Tile* ss: rp->getUnusedTiles())
                {
                    const std::vector<GameEntity*>& entitiesInTile = ss->getEntitiesInTile();
                    for( GameEntity* ge: entitiesInTile)
                    {
                        if(ge->getObjectType() == GameEntityType::creature)
                        {
                            return false;
                        }
                    }

                }
                for(Tile* ss: rp->getFenceTiles())
                {
                    const std::vector<GameEntity*>& entitiesInTile = ss->getEntitiesInTile();
                    for( GameEntity* ge: entitiesInTile)
                    {
                        if(ge->getObjectType() == GameEntityType::creature)
                        {
                            return false;
                        }
                    }

                }                
            }
        }
        return true;
    }
    
    bool buildRoom(GameMap* gameMap, Player* player, ODPacket& packet) const override
    {
        std::vector<Tile*> tiles;
        if(!getRoomTilesDefault(tiles, gameMap, player, packet))
            return false;
        if(!checkCreaturesOccupyingPotentialPrison(tiles,gameMap))
        {
            ServerNotification *serverNotification = new ServerNotification(
                ServerNotificationType::displayText, player->getSeat()->getPlayer());
            std::string msg = "Prison can be extended when there are \n no creatures at prison's fence and unusued tiles";
            serverNotification->mPacket << msg;
            ODServer::getSingleton().queueServerNotification(serverNotification);
            return false;
        }
        int32_t pricePerTarget = RoomManager::costPerTile(RoomPrison::mRoomType);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
            return false;

        RoomPrison* room = new RoomPrison(gameMap);
        return buildRoomDefault(gameMap, room, player->getSeat(), tiles);
    }

    void checkBuildRoomEditor(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildRoomDefaultEditor(gameMap, RoomPrison::mRoomType, inputManager, inputCommand);
    }

    bool buildRoomEditor(GameMap* gameMap, ODPacket& packet) const override
    {
        RoomPrison* room = new RoomPrison(gameMap);
        return buildRoomDefaultEditor(gameMap, room, packet);
    }

    Room* getRoomFromStream(GameMap* gameMap, std::istream& is) const override
    {
        RoomPrison* room = new RoomPrison(gameMap);
        if(!Room::importRoomFromStream(*room, is))
        {
            OD_LOG_ERR("Error while building a room from the stream");
        }
        return room;
    }

    bool buildRoomOnTiles(GameMap* gameMap, Player* player, const std::vector<Tile*>& tiles, bool noFee =false) const override
    {
        int32_t pricePerTarget = RoomManager::costPerTile(RoomPrison::mRoomType);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!checkCreaturesOccupyingPotentialPrison(tiles,gameMap))
        {
            ServerNotification *serverNotification = new ServerNotification(
                ServerNotificationType::displayText, player->getSeat()->getPlayer());
            std::string msg = "Prison can be extended when there are \n no creatures at prison's fence and unusued tiles";
            serverNotification->mPacket << msg;
            ODServer::getSingleton().queueServerNotification(serverNotification);            
            return false;
        }
        if(!noFee)
            if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
                return false;

        RoomPrison* room = new RoomPrison(gameMap);
        return buildRoomDefault(gameMap, room, player->getSeat(), tiles);
    }
};

// Register the factory
static RoomRegister reg(new RoomPrisonFactory);
}

static const int32_t OFFSET_TILE_X = 0;
static const int32_t OFFSET_TILE_Y = -1;

RoomPrison::RoomPrison(GameMap* gameMap) :
    FencedRoom(gameMap)
{
    setMeshName("PrisonGround");
}

BuildingObject* RoomPrison::notifyActiveSpotCreated(ActiveSpotPlace place, Tile* tile)
{
    switch(place)
    {
        case ActiveSpotPlace::activeSpotCenter:
        {
            // Prison do not have central active spot
            return nullptr;
        }
        case ActiveSpotPlace::activeSpotLeft:
        {
            return new BuildingObject(getGameMap(), *this, "Skull", *tile, 90.0, false);
        }
        case ActiveSpotPlace::activeSpotRight:
        {
            return new BuildingObject(getGameMap(), *this, "Skull", *tile, 270.0, false);
        }
        case ActiveSpotPlace::activeSpotTop:
        {
            return new BuildingObject(getGameMap(), *this, "Skull", *tile, 0.0, false);
        }
        case ActiveSpotPlace::activeSpotBottom:
        {
            return new BuildingObject(getGameMap(), *this, "Skull", *tile, 180.0, false);
        }
        default:
            break;
    }
    return nullptr;
}

void RoomPrison::absorbRoom(Room *r)
{
    if(r->getType() != getType())
    {
        OD_LOG_ERR("Trying to merge incompatible rooms: " + getName() + ", type=" + RoomManager::getRoomNameFromRoomType(getType()) + ", with " + r->getName() + ", type=" + RoomManager::getRoomNameFromRoomType(r->getType()));
        return;
    }

    RoomPrison* rc = static_cast<RoomPrison*>(r);
    mPendingPrisoners.insert(mPendingPrisoners.end(), rc->mPendingPrisoners.begin(), rc->mPendingPrisoners.end());
    rc->mPendingPrisoners.clear();
    mFenceTiles.insert(mFenceTiles.end(), rc->mFenceTiles.begin(), rc->mFenceTiles.end());
    rc->mFenceTiles.clear();
    mFencingObjects.insert( rc->mFencingObjects.begin(), rc->mFencingObjects.end());
    rc->mFencingObjects.clear();
    
    Room::absorbRoom(r);
}

void RoomPrison::doUpkeep()
{
    Room::doUpkeep();

    if(mCoveredTiles.empty())
        return;

    // We check if we have enough room for all prisoners
    uint32_t nbCreatures = 0;
    for(Tile* tile : mCoveredTiles)
    {
        for(GameEntity* entity : tile->getEntitiesInTile())
        {
            if(entity->getObjectType() != GameEntityType::creature)
                continue;

            Creature* creature = static_cast<Creature*>(entity);
            if(creature->getSeatPrison() != getSeat())
                continue;

            Tile* creatureTile = creature->getPositionTile();
            if(creatureTile == nullptr)
            {
                OD_LOG_ERR("creatureName=" + creature->getName() + ", position=" + Helper::toString(creature->getPosition()));
                continue;
            }

            if(nbCreatures >= mCentralActiveSpotTiles.size())
            {
                // We have more prisoner than room. We free them
                creature->clearActionQueue();
                continue;
            }

            // If the creature is dead (by slapping for example), we remove it without
            // spawning any creature
            if(!creature->isAlive())
            {
                creature->clearActionQueue();
                continue;
            }

            ++nbCreatures;
            // We slightly damage the prisoner
            double damage = ConfigManager::getSingleton().getRoomConfigDouble("PrisonDamagePerTurn");
            creature->takeDamage(this, damage, 0.0, 0.0, 0.0, creatureTile, false);
            creature->increaseTurnsPrison();

            if(creature->isAlive())
                continue;

            // The creature is dead. We can release it
            OD_LOG_INF("creature=" + creature->getName() + " died in prison=" + getName());

            if((getSeat()->getPlayer() != nullptr) &&
               getSeat()->getPlayer()->getIsHuman() &&
               !getSeat()->getPlayer()->getHasLost())
            {
                ServerNotification *serverNotification = new ServerNotification(
                    ServerNotificationType::chatServer, getSeat()->getPlayer());
                std::string msg = "A creature died starving in your prison";
                serverNotification->mPacket << msg << EventShortNoticeType::aboutCreatures;
                ODServer::getSingleton().queueServerNotification(serverNotification);
            }

            creature->clearActionQueue();
            creature->removeFromGameMap();
            creature->deleteYourself();

            const std::string& className = ConfigManager::getSingleton().getRoomConfigString("PrisonSpawnClass");
            const CreatureDefinition* classToSpawn = getGameMap()->getClassDescription(className);
            if(classToSpawn == nullptr)
            {
                OD_LOG_ERR("className=" + className);
                continue;
            }
            // Create a new creature and copy over the class-based creature parameters.
            Creature* newCreature = new Creature(getGameMap(), classToSpawn, getSeat());

            // Add the creature to the gameMap and create meshes so it is visible.
            newCreature->addToGameMap();
            newCreature->setPosition(Ogre::Vector3(creatureTile->getX(), creatureTile->getY(), 0.0f));
            newCreature->createMesh();
        }
    }
}

void RoomPrison::updateActiveSpots(GameMap* gameMap)
{
    deleteFenceMeshes();    
    splitTilesIntoClasses();
    putFenceMeshes();
    Room::updateActiveSpots(gameMap);    
}


void RoomPrison::deleteFenceMeshes()
{
    for (Tile* tile: mFenceTiles)
    {
        auto it = mFencingObjects.find(tile);
        if(it == mFencingObjects.end())
            continue;

        BuildingObject* obj = it->second;
        obj->removeFromGameMap();
        obj->deleteYourself();
        mFencingObjects.erase(it);
    }
}


void RoomPrison::putFenceMeshes()
{
    std::vector<std::pair<int,int>> mFenceCoords;
    std::vector<std::pair<int,int>> mActualTilesCoords;
    
    for(Tile* tt : mFenceTiles)
    {
        mFenceCoords.push_back(std::make_pair(tt->getX(),tt->getY()));
    }
    for(Tile* tt : mActualTiles)
    {
        mActualTilesCoords.push_back(std::make_pair(tt->getX(),tt->getY()));
    }    
    for (Tile* tt: mFenceTiles)
    {
        int xx = tt->getX();
        int yy = tt->getY();
        std::pair<int,int> east = std::make_pair( xx + 1, yy);
        std::pair<int,int> west = std::make_pair( xx - 1, yy);
        std::pair<int,int> north = std::make_pair( xx , yy + 1);
        std::pair<int,int> south = std::make_pair( xx , yy - 1);
        auto fenceEast = std::find(mFenceCoords.begin(), mFenceCoords.end(), east);
        auto fenceWest = std::find(mFenceCoords.begin(), mFenceCoords.end(), west);
        auto fenceSouth = std::find(mFenceCoords.begin(), mFenceCoords.end(), south);
        auto fenceNorth = std::find(mFenceCoords.begin(), mFenceCoords.end(), north);
        auto actualEast = std::find(mActualTilesCoords.begin(), mActualTilesCoords.end(), east);
        auto actualWest = std::find(mActualTilesCoords.begin(), mActualTilesCoords.end(), west);
        auto actualSouth = std::find(mActualTilesCoords.begin(), mActualTilesCoords.end(), south);
        auto actualNorth = std::find(mActualTilesCoords.begin(), mActualTilesCoords.end(), north);        
        BuildingObject* ro = nullptr;        
        if(fenceEast != mFenceCoords.end() && fenceWest !=mFenceCoords.end())
            if(actualSouth!=mActualTilesCoords.end())
                ro = createFencingMesh( FencingDirection::EW, tt);
            else
                ro = createFencingMesh( FencingDirection::WE, tt);
        
        else if(fenceSouth != mFenceCoords.end() && fenceNorth !=mFenceCoords.end())
            if(actualWest!=mActualTilesCoords.end())
                ro = createFencingMesh( FencingDirection::SN, tt);
            else
                ro = createFencingMesh( FencingDirection::NS, tt);
        
        else if(fenceEast != mFenceCoords.end() && fenceSouth !=mFenceCoords.end()
                && actualWest!=mActualTilesCoords.end() && actualNorth!=mActualTilesCoords.end())
            ro = createFencingMesh(  FencingDirection::SE , tt);
        else if(fenceEast != mFenceCoords.end() && fenceNorth !=mFenceCoords.end()
                && actualWest!=mActualTilesCoords.end() && actualSouth!=mActualTilesCoords.end())            
            ro = createFencingMesh(  FencingDirection::NE , tt);
        else if(fenceWest != mFenceCoords.end() && fenceSouth !=mFenceCoords.end()
                && actualEast!=mActualTilesCoords.end() && actualNorth!=mActualTilesCoords.end())
            ro = createFencingMesh(  FencingDirection::SW , tt);
        else if(fenceWest != mFenceCoords.end() && fenceNorth !=mFenceCoords.end()
                && actualEast!=mActualTilesCoords.end() && actualSouth!=mActualTilesCoords.end())
            ro = createFencingMesh(  FencingDirection::NW , tt);        
        if(ro != nullptr)
        {
            // The room wants to build a room onject. We add it to the gamemap
            addFencingObject(tt, ro,getGameMap());
            ro->createMesh();
        }
    }

}

BuildingObject* RoomPrison::createFencingMesh(FencingDirection hd, Tile*  tt)
{
    switch(hd)
    {
    case FencingDirection::NS :
        return new BuildingObject(getGameMap(), *this, "FenceStraight", *tt, 90.0, false);
    case FencingDirection::EW :
        return new BuildingObject(getGameMap(), *this, "FenceStraight", *tt, 0.0, false);
    case FencingDirection::SN :
        return new BuildingObject(getGameMap(), *this, "FenceStraight", *tt, 270.0, false);
    case FencingDirection::WE :
        return new BuildingObject(getGameMap(), *this, "FenceStraight", *tt, 180.0, false);
    case FencingDirection::SE :
        return new BuildingObject(getGameMap(), *this, "FenceCorner", *tt, 270.0, false);
    case FencingDirection::NE :
        return new BuildingObject(getGameMap(), *this, "FenceCorner", *tt, 00.0, false);
    case FencingDirection::NW :
        return new BuildingObject(getGameMap(), *this, "FenceCorner", *tt, 90.0, false);
    case FencingDirection::SW :
        return new BuildingObject(getGameMap(), *this, "FenceCorner", *tt, 180.0, false);        
    default:
        break;


    }
    return nullptr;

}


void RoomPrison::addFencingObject(Tile* targetTile, BuildingObject* obj, GameMap* gameMap)
{
    if(obj == nullptr)
        return;
    if(gameMap == nullptr)
        gameMap = getGameMap();
    
    // The object position has been already set in the building object constructor
    mFencingObjects[targetTile] = obj;
    obj->addToGameMap(gameMap);
    obj->setPosition(obj->getPosition(),gameMap);
}


void RoomPrison::exportToStream(std::ostream& os) const
{
    Room::exportToStream(os);

    std::vector<Creature*> creatures;
    for(Tile* tile : mCoveredTiles)
    {
        for(GameEntity* entity : tile->getEntitiesInTile())
        {
            if(entity->getObjectType() != GameEntityType::creature)
                continue;

            Creature* creature = static_cast<Creature*>(entity);
            if(creature->getSeatPrison() != getSeat())
                continue;

            creatures.push_back(creature);
        }
    }

    uint32_t nb = creatures.size();
    os << nb;
    for(Creature* creature : creatures)
    {
        os << "\t" << creature->getName();
    }
    os << "\n";
}

bool RoomPrison::importFromStream(std::istream& is)
{
    if(!Room::importFromStream(is))
        return false;

    uint32_t nb;
    if(!(is >> nb))
        return false;
    while(nb > 0)
    {
        std::string creatureName;
        if(!(is >> creatureName))
            return false;

        mPrisonersLoad.push_back(creatureName);
        nb--;
    }

    return true;
}

void RoomPrison::restoreInitialEntityState()
{
    for(const std::string& creatureName : mPrisonersLoad)
    {
        Creature* creature = getGameMap()->getCreature(creatureName);
        if(creature == nullptr)
        {
            OD_LOG_ERR("creatureName=" + creatureName);
            continue;
        }

        creature->clearActionQueue();
        creature->pushAction(Utils::make_unique<CreatureActionUseRoom>(*creature, *this, true));
    }
}

bool RoomPrison::hasOpenCreatureSpot(Creature* creature)
{
    // We count current prisoners + prisoners on their way
    uint32_t nbCreatures = countPrisoners();
    nbCreatures += mPendingPrisoners.size();
    if(nbCreatures >= mCentralActiveSpotTiles.size())
        return false;

    return true;
}

bool RoomPrison::addCreatureUsingRoom(Creature* creature)
{
    if(!Room::addCreatureUsingRoom(creature))
        return false;

    creature->setInJail(this);

    return true;
}

void RoomPrison::removeCreatureUsingRoom(Creature* creature)
{
    Room::removeCreatureUsingRoom(creature);

    creature->setInJail(nullptr);
}

bool RoomPrison::hasCarryEntitySpot(GameEntity* carriedEntity)
{
    if(carriedEntity->getObjectType() != GameEntityType::creature)
        return false;

    Creature* creature = static_cast<Creature*>(carriedEntity);

    // Only ko to death enemy creatures should be carried to prison
    if(creature->getKoTurnCounter() >= 0)
       return false;

    // Only enemy creatures should be brought to prison
    if(getSeat()->isAlliedSeat(creature->getSeat()))
       return false;

    return hasOpenCreatureSpot(creature);
}

Tile* RoomPrison::askSpotForCarriedEntity(GameEntity* carriedEntity)
{
    if(carriedEntity->getObjectType() != GameEntityType::creature)
    {
        OD_LOG_ERR("room=" + getName() + ", entity=" + carriedEntity->getName());
        return nullptr;
    }

    Creature* creature = static_cast<Creature*>(carriedEntity);
    mPendingPrisoners.push_back(creature);
    return getGateTile();
}

void RoomPrison::notifyCarryingStateChanged(Creature* carrier, GameEntity* carriedEntity)
{
    // The carrier has brought the enemy creature
    if(carriedEntity->getObjectType() != GameEntityType::creature)
    {
        OD_LOG_ERR("room=" + getName() + ", entity=" + carriedEntity->getName());
        return;
    }

    Creature* prisonerCreature = static_cast<Creature*>(carriedEntity);

    // We check if we were waiting for this creature
    // We release the pending prisoners before pushing the action room to make sure
    // the place is free when we add the creature
    auto it = std::find(mPendingPrisoners.begin(), mPendingPrisoners.end(), prisonerCreature);
    if(it == mPendingPrisoners.end())
    {
        OD_LOG_ERR("room=" + getName() + ", unexpected creature=" + prisonerCreature->getName());
        return;
    }

    mPendingPrisoners.erase(it);

    prisonerCreature->clearActionQueue();
    prisonerCreature->pushAction(Utils::make_unique<CreatureActionUseRoom>(*prisonerCreature, *this, true));
    prisonerCreature->resetKoTurns();
}

uint32_t RoomPrison::countPrisoners()
{
    uint32_t nbCreatures = 0;
    for(Tile* tile : mCoveredTiles)
    {
        for(GameEntity* entity : tile->getEntitiesInTile())
        {
            if(entity->getObjectType() != GameEntityType::creature)
                continue;

            Creature* creature = static_cast<Creature*>(entity);
            if(creature->getSeatPrison() != getSeat())
                continue;

            ++nbCreatures;
        }
    }

    return nbCreatures;
}

bool RoomPrison::isInContainment(Creature& creature)
{
    if(creature.getSeatPrison() != getSeat())
        return false;

    return true;
}

bool RoomPrison::useRoom(Creature& creature, bool forced)
{
    if(Random::Uint(1, 4) > 1)
        return false;

    Tile* creatureTile = creature.getPositionTile();
    if(creatureTile == nullptr)
    {
        OD_LOG_ERR("room=" + getName() + ", creatureName=" + creature.getName() + ", position=" + Helper::toString(creature.getPosition()));
        return false;
    }

    std::vector<Tile*> availableTiles;
    for(Tile* tile : creatureTile->getAllNeighbors())
    {
        if(std::find(mActualTiles.begin(), mActualTiles.end(), tile)!=mActualTiles.end())
            availableTiles.push_back(tile);
    }

    if(availableTiles.empty())
        return false;

    uint32_t index = Random::Uint(0, availableTiles.size() - 1);
    Tile* tileDest = availableTiles[index];
    Ogre::Vector2 v (static_cast<Ogre::Real>(tileDest->getX()), static_cast<Ogre::Real>(tileDest->getY()));
    std::vector<Ogre::Vector2> path;
    path.push_back(v);
    creature.setWalkPath(EntityAnimation::flee_anim, EntityAnimation::idle_anim, true, true, path,true);

    uint32_t nbTurns = Random::Uint(3, 6);
    creature.setJobCooldown(nbTurns);

    return false;
}

Tile* RoomPrison::getGateTile()
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


double RoomPrison::getCreatureSpeed(const Creature* creature, Tile* tile) const
{
    if( std::find(mActualTiles.begin(),mActualTiles.end(), tile ) != mActualTiles.end())
        return 0.0;
    else
        return creature->getMoveSpeedGround();
}
