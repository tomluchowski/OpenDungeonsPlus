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

#include "rooms/RoomHatchery.h"
#include <algorithm>
#include <cmath>
#include "game/SkillManager.h"
#include "game/SkillType.h"

#include "creatureaction/CreatureActionEatChicken.h"
#include "creatureaction/CreatureActionSearchFood.h"
#include "entities/BuildingObject.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "entities/ChickenEntity.h"
#include "entities/MapLight.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "gamemap/RoomObjectNavigation.h"
#include "rooms/RoomManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"
#include "utils/MakeUnique.h"
#include "utils/Random.h"

const std::string RoomHatcheryName = "Hatchery";
const std::string RoomHatcheryNameDisplay = "Hatchery room";
const RoomType RoomHatchery::mRoomType = RoomType::hatchery;
const TileVisual RoomHatchery::mRoomVisual= TileVisual::hatcheryRoom;


namespace
{
class RoomHatcheryFactory : public RoomFactory
{
    RoomType getRoomType() const override
    { return RoomHatchery::mRoomType; }

    TileVisual getVisualType() const override
    { return RoomHatchery::mRoomVisual; }
    
    const std::string& getName() const override
    { return RoomHatcheryName; }

    const std::string& getNameReadable() const override
    { return RoomHatcheryNameDisplay; }

    int getCostPerTile() const override
    { return ConfigManager::getSingleton().getRoomConfigInt32("HatcheryCostPerTile"); }

    void checkBuildRoom(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildRoomDefault(gameMap, RoomHatchery::mRoomType, inputManager, inputCommand);
    }

    bool buildRoom(GameMap* gameMap, Player* player, ODPacket& packet) const override
    {
        std::vector<Tile*> tiles;
        if(!getRoomTilesDefault(tiles, gameMap, player, packet))
            return false;

        int32_t pricePerTarget = RoomManager::costPerTile(RoomHatchery::mRoomType);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
            return false;

        RoomHatchery* room = new RoomHatchery(gameMap);
        return buildRoomDefault(gameMap, room, player->getSeat(), tiles);
    }

    void checkBuildRoomEditor(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildRoomDefaultEditor(gameMap, RoomHatchery::mRoomType, inputManager, inputCommand);
    }

    bool buildRoomEditor(GameMap* gameMap, ODPacket& packet) const override
    {
        RoomHatchery* room = new RoomHatchery(gameMap);
        return buildRoomDefaultEditor(gameMap, room, packet);
    }

    //! \brief Creates an empty room of this type, for a room that has to be split in two.
    Room* createRoom(GameMap* gameMap) const override
    { return new RoomHatchery(gameMap); }

    Room* getRoomFromStream(GameMap* gameMap, std::istream& is) const override
    {
        RoomHatchery* room = new RoomHatchery(gameMap);
        if(!Room::importRoomFromStream(*room, is))
        {
            OD_LOG_ERR("Error while building a room from the stream");
        }
        return room;
    }
    
    bool buildRoomOnTiles(GameMap* gameMap, Player* player, const std::vector<Tile*>& tiles, bool noFee =false) const override
    {
        int32_t pricePerTarget = RoomManager::costPerTile(RoomHatchery::mRoomType);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!noFee)
            if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
                return false;

        RoomHatchery* room = new RoomHatchery(gameMap);
        return buildRoomDefault(gameMap, room, player->getSeat(), tiles);
    }
};

// Register the factory
static RoomRegister reg(new RoomHatcheryFactory);
}

RoomHatchery::RoomHatchery(GameMap* gameMap) :
    Room(gameMap),
    mCoopHenWait(0),
    mCoopRoosterWait(0)
{
    setMeshName("Farm");
}

BuildingObject* RoomHatchery::notifyActiveSpotCreated(ActiveSpotPlace place, Tile* tile)
{
    // We add chicken coops on center tiles only
    if(place == ActiveSpotPlace::activeSpotCenter)
        return new BuildingObject(getGameMap(), *this, "ChickenCoop", *tile, 0.0, false);

    return nullptr;
}

void RoomHatchery::notifyActiveSpotRemoved(ActiveSpotPlace place, Tile* tile)
{
    if(place == ActiveSpotPlace::activeSpotCenter)
    {
        // We remove the chicken coop
        removeBuildingObject(tile);
    }
}

HatcheryCycleSettings RoomHatchery::getCycleSettings() const
{
    const ConfigManager& config = ConfigManager::getSingleton();
    HatcheryCycleSettings settings;
    settings.mLayMin = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryLayMin", settings.mLayMin));
    settings.mLayMax = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryLayMax", settings.mLayMax));
    settings.mHatchTurns = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryHatchTurns", settings.mHatchTurns));
    settings.mGrowTurns = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryGrowTurns", settings.mGrowTurns));
    settings.mRoosterWait = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterSpawnRate", settings.mRoosterWait));
    settings.mTilesPerChicken = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryTilesPerChicken", settings.mTilesPerChicken));
    settings.mCareLightPercent = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryCareLightPercent", settings.mCareLightPercent));
    settings.mCareCalmPercent = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryCareCalmPercent", settings.mCareCalmPercent));
    settings.mTramplePercent = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryTramplePercent", settings.mTramplePercent));

    // The research of the hatchery shortens the waiting times
    double coopWait = config.getRoomConfigDoubleOrDefault("HatcheryChickenSpawnRate", settings.mCoopWait);
    double researchedWait = std::max(1.0, std::round(SkillManager::getResearchValue(
        getSeat(), SkillType::roomHatchery, coopWait)));
    settings.mCoopWait = static_cast<uint32_t>(researchedWait);
    double factor = (coopWait > 0.0) ? (researchedWait / coopWait) : 1.0;
    return HatcheryCycle::scaled(settings, factor);
}

ChickenEntity* RoomHatchery::spawnAnimal(ChickenKind kind, const Ogre::Vector3& position, const HatcheryCycleSettings& settings)
{
    ChickenEntity* chicken = new ChickenEntity(getGameMap(), getName(), kind);
    chicken->addToGameMap();
    chicken->createMesh();
    chicken->setPosition(position);
    if(kind == ChickenKind::hen)
        chicken->setLayTimer(HatcheryCycle::layInterval(settings, Random::Uint(0, 1000)));
    return chicken;
}

bool RoomHatchery::spawnFromCoop(ChickenKind kind, const HatcheryCycleSettings& settings)
{
    if(mCentralActiveSpotTiles.empty())
        return false;

    const std::vector<RoomObjectPath::Obstacle> obstacles = RoomObjectNavigation::collect(*getGameMap(), 0.1f);
    uint32_t first = Random::Uint(0, mCentralActiveSpotTiles.size() - 1);
    for(uint32_t i = 0; i < mCentralActiveSpotTiles.size(); ++i)
    {
        Tile* coopTile = mCentralActiveSpotTiles[(first + i) % mCentralActiveSpotTiles.size()];
        Ogre::Vector2 freePosition;
        if(!RoomObjectNavigation::standingPosition(obstacles,
            Ogre::Vector2(coopTile->getX(), coopTile->getY()), freePosition))
            continue;

        spawnAnimal(kind, Ogre::Vector3(freePosition.x, freePosition.y, 0.0f), settings);
        return true;
    }
    return false;
}

void RoomHatchery::collectEnemies(std::vector<Creature*>& enemies) const
{
    for(Creature* creature : getGameMap()->getCreatures())
    {
        if((creature == nullptr) || !creature->getIsOnMap() || (creature->getSeat() == nullptr))
            continue;

        Tile* tile = creature->getPositionTile();
        if((tile == nullptr) || (tile->getCoveringRoom() != this))
            continue;

        if(!creature->getSeat()->isAlliedSeat(getSeat()))
            enemies.push_back(creature);
    }
}

bool RoomHatchery::isEnemyOnEggTile(const ChickenEntity& egg, const std::vector<Creature*>& enemies) const
{
    for(Creature* enemy : enemies)
    {
        if(enemy->getPositionTile() == egg.getPositionTile())
            return true;
    }
    return false;
}

bool RoomHatchery::isLit() const
{
    double radius = ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryCareLightRadius", 8.0);
    double radiusSquared = radius * radius;
    for(MapLight* light : getGameMap()->getMapLights())
    {
        for(Tile* tile : mCoveredTiles)
        {
            double dx = light->getPosition().x - tile->getX();
            double dy = light->getPosition().y - tile->getY();
            if((dx * dx + dy * dy) <= radiusSquared)
                return true;
        }
    }
    return false;
}

HatcheryCare RoomHatchery::getCare(const std::vector<Creature*>& enemies) const
{
    HatcheryCare care;
    care.mClaimed = true;
    for(Tile* tile : mCoveredTiles)
    {
        if(!tile->isClaimedForSeat(getSeat()))
        {
            care.mClaimed = false;
            break;
        }
    }
    care.mLit = isLit();
    care.mEnemies = !enemies.empty();
    return care;
}

void RoomHatchery::doUpkeep()
{
    Room::doUpkeep();

    if(mCoveredTiles.empty())
        return;

    const HatcheryCycleSettings settings = getCycleSettings();

    // Sort the animals of the hatchery by kind. Eaten or dying ones do not count.
    std::vector<ChickenEntity*> hens;
    std::vector<ChickenEntity*> chicks;
    std::vector<ChickenEntity*> eggs;
    HatcheryCounts counts;
    for(Tile* tile : mCoveredTiles)
    {
        std::vector<GameEntity*> entities;
        tile->fillWithEntities(entities, SelectionEntityWanted::chicken, getSeat()->getPlayer());
        for(GameEntity* entity : entities)
        {
            ChickenEntity* chicken = static_cast<ChickenEntity*>(entity);
            if(!chicken->isFree())
                continue;

            switch(chicken->getKind())
            {
                case ChickenKind::hen:
                    hens.push_back(chicken);
                    break;
                case ChickenKind::chick:
                    chicks.push_back(chicken);
                    break;
                case ChickenKind::egg:
                    eggs.push_back(chicken);
                    break;
                case ChickenKind::rooster:
                    ++counts.mRoosters;
                    break;
            }
        }
    }
    counts.mHens = hens.size();
    counts.mChicks = chicks.size();
    counts.mEggs = eggs.size();

    // Breeding needs care: hens lay faster in a claimed, lit hatchery without enemies
    std::vector<Creature*> enemies;
    collectEnemies(enemies);
    const HatcheryCare care = getCare(enemies);
    const HatcheryCycleSettings layingSettings = HatcheryCycle::withCare(settings, care);

    // Enemy creatures and heroes trample eggs close to them, creatures of the keeper never do
    double trampleRadius = ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryTrampleRadius", 0.6);
    for(Creature* enemy : enemies)
    {
        const Ogre::Vector2 enemyPos(enemy->getPosition().x, enemy->getPosition().y);
        std::vector<ChickenEntity*>::iterator eggIt = eggs.begin();
        while(eggIt != eggs.end())
        {
            ChickenEntity* egg = *eggIt;
            const Ogre::Vector2 eggPos(egg->getPosition().x, egg->getPosition().y);
            if((enemyPos.distance(eggPos) <= trampleRadius) &&
               HatcheryCycle::tramples(settings, true, true, Random::Uint(0, 99)) && egg->trample(enemy))
            {
                --counts.mEggs;
                eggIt = eggs.erase(eggIt);
            }
            else
                ++eggIt;
        }
    }

    // Hens lay eggs while the hatchery is not full
    uint32_t capacity = HatcheryCycle::capacity(mCoveredTiles.size(), mNumActiveSpots, settings);
    for(ChickenEntity* hen : hens)
    {
        if(!hen->countDownLay())
            continue;

        hen->setLayTimer(HatcheryCycle::layInterval(layingSettings, Random::Uint(0, 1000)));
        if(!HatcheryCycle::canLay(counts, capacity))
            continue;

        spawnAnimal(ChickenKind::egg, hen->getPosition(), settings);
        ++counts.mEggs;
    }

    // Eggs hatch while there is a rooster and no enemy stands on that egg's tile
    if(HatcheryCycle::canHatch(counts, false))
    {
        for(ChickenEntity* egg : eggs)
        {
            if(isEnemyOnEggTile(*egg, enemies))
                continue;

            if(egg->incrementAge() < settings.mHatchTurns)
                continue;

            egg->setKind(ChickenKind::chick);
            --counts.mEggs;
            ++counts.mChicks;
        }
    }

    // Chicks grow up
    for(ChickenEntity* chick : chicks)
    {
        if(chick->incrementAge() < settings.mGrowTurns)
            continue;

        chick->setKind(ChickenKind::hen);
        chick->setLayTimer(HatcheryCycle::layInterval(layingSettings, Random::Uint(0, 1000)));
        --counts.mChicks;
        ++counts.mHens;
    }

    // Coops are the last resort: only when there is no hen, chick or egg at all
    if(HatcheryCycle::needCoopHen(counts, mNumActiveSpots))
    {
        ++mCoopHenWait;
        if((mCoopHenWait >= settings.mCoopWait) && spawnFromCoop(ChickenKind::hen, settings))
            mCoopHenWait = 0;
    }
    else
        mCoopHenWait = 0;

    // The same for the rooster
    if(HatcheryCycle::needCoopRooster(counts, mNumActiveSpots))
    {
        ++mCoopRoosterWait;
        if((mCoopRoosterWait >= settings.mRoosterWait) && spawnFromCoop(ChickenKind::rooster, settings))
            mCoopRoosterWait = 0;
    }
    else
        mCoopRoosterWait = 0;
}

bool RoomHatchery::hasOpenCreatureSpot(Creature* c)
{
    return mNumActiveSpots > mCreaturesUsingRoom.size();
}

bool RoomHatchery::useRoom(Creature& creature, bool forced)
{
    // Check if the creature needs to eat
    if(creature.getHunger() <= 5.0)
    {
        creature.popAction();
        return true;
    }

    // We look for the closest chicken (if any). We consider chickens
    // on the hatchery only
    // Because TilesWithinSightRadius are sorted by distance, we use them rather
    // than covered tiles.
    ChickenEntity* chickenClosest = nullptr;
    for(Tile* tile : creature.getTilesWithinSightRadius())
    {
        if(tile->getCoveringRoom() != this)
            continue;

        std::vector<GameEntity*> chickens;
        tile->fillWithEntities(chickens, SelectionEntityWanted::chicken, getSeat()->getPlayer());
        if(chickens.empty())
            continue;

        for(GameEntity* chickenEnt : chickens)
        {
            ChickenEntity* chicken = static_cast<ChickenEntity*>(chickenEnt);
            if(!chicken->isEdible())
                continue;
            if(chicken->getLockEat(creature))
                continue;

            chickenClosest = chicken;
            break;
        }
        if(chickenClosest != nullptr)
            break;
    }

    // If we cannot find any available chicken, nothing to do
    if(chickenClosest == nullptr)
        return false;

    creature.pushAction(Utils::make_unique<CreatureActionEatChicken>(creature, *chickenClosest));
    return true;
}

void RoomHatchery::handleCreatureUsingAbsorbedRoom(Creature& creature)
{
    creature.clearDestinations(EntityAnimation::idle_anim, true, true);
    creature.clearActionQueue();
    creature.pushAction(Utils::make_unique<CreatureActionSearchFood>(creature, true));
}

void RoomHatchery::creatureDropped(Creature& creature)
{
    creature.pushAction(Utils::make_unique<CreatureActionSearchFood>(creature, true));
}
