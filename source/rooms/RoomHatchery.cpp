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

#include "creatureaction/CreatureAction.h"
#include "creatureaction/CreatureActionEatChicken.h"
#include "creatureaction/CreatureActionSearchFood.h"
#include "entities/BuildingObject.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "entities/ChickenEntity.h"
#include "entities/MapLight.h"
#include "entities/ChickenPose.h"
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
    mCrowInterval(60),
    mLastCrowDay(-1),
    mCoopHenWait(0),
    mCoopRoosterWait(0),
    mFightActive(false),
    mFightFirstWins(true),
    mFightBrawling(false),
    mFightApproach(0),
    mFightTurnsLeft(0)
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

//! \brief Saves the number of the last day the rooster crowed for, so a loaded game does not crow twice.
void RoomHatchery::exportToStream(std::ostream& os) const
{
    Room::exportToStream(os);
    os << "HatcheryDay " << mLastCrowDay << std::endl;
}

//! \brief Loads the line written by exportToStream. A save without it loads as before: the stream is put back
//! and the day is learned again from the first turn.
bool RoomHatchery::importFromStream(std::istream& is)
{
    if(!Room::importFromStream(is))
        return false;

    std::streampos pos = is.tellg();
    std::string tag;
    if(!(is >> tag) || (tag != "HatcheryDay"))
    {
        is.clear();
        is.seekg(pos);
        return true;
    }

    int64_t crowDay;
    if(!(is >> crowDay))
        return false;

    mLastCrowDay = crowDay;
    return true;
}

HatcheryCycleSettings RoomHatchery::getCycleSettings() const
{
    const ConfigManager& config = ConfigManager::getSingleton();
    HatcheryCycleSettings settings;
    settings.mLayMin = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryLayMin", settings.mLayMin));
    settings.mLayMax = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryLayMax", settings.mLayMax));
    settings.mHatchTurns = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryHatchTurns", settings.mHatchTurns));
    settings.mGrowTurns = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryGrowTurns", settings.mGrowTurns));
    settings.mTilesPerChicken = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryTilesPerChicken", settings.mTilesPerChicken));
    settings.mCareLightPercent = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryCareLightPercent", settings.mCareLightPercent));
    settings.mCareCalmPercent = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryCareCalmPercent", settings.mCareCalmPercent));
    settings.mTramplePercent = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryTramplePercent", settings.mTramplePercent));
    settings.mFightTurns = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryFightTurns", settings.mFightTurns));
    settings.mFightApproachTurns = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryFightApproachTurns", settings.mFightApproachTurns));

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

        ChickenEntity* animal = spawnAnimal(kind, Ogre::Vector3(freePosition.x, freePosition.y, 0.0f), settings);
        animal->playPose(ChickenPose::emerge, 2);
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
    mRoosterSettings = getRoosterSettings();

    // Sort the animals of the hatchery by kind. Eaten or dying ones do not count.
    std::vector<ChickenEntity*> hens;
    std::vector<ChickenEntity*> chicks;
    std::vector<ChickenEntity*> eggs;
    std::vector<ChickenEntity*> roosters;
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
                    roosters.push_back(chicken);
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
        hen->playPose(ChickenPose::lay, 2);
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
            {
                // The egg wobbles shortly before it hatches
                if(egg->getAge() + 1 == settings.mHatchTurns)
                    egg->playPose(ChickenPose::wobble, 0);
                continue;
            }

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

    // A hatchery has one rooster only: two of them fight until one is dead
    updateFight(roosters, settings, counts);

    // Behaviour for the eyes: sleeping at night, sitting calmly in a full hatchery, the chick line, the rooster
    const RoosterSettings& roosterSettings = mRoosterSettings;
    bool night =HatcheryRooster::isNight(getGameMap()->getTurnNumber(), roosterSettings);
    bool full = (capacity > 0) && !HatcheryCycle::canLay(counts, capacity);
    for(ChickenEntity* hen : hens)
        hen->setCalm(night || full);
    for(ChickenEntity* chick : chicks)
        chick->setCalm(night);
    ChickenEntity* rooster = roosters.empty() ? nullptr : roosters.front();
    updateChickLine(hens, chicks, rooster, night);
    for(ChickenEntity* oneRooster : roosters)
    {
        if(!oneRooster->isFighting())
            updateRooster(oneRooster, hens, chicks, roosterSettings);
    }

    // The hens run to the rooster while he calls them to food, otherwise they go their own way
    ChickenEntity* caller = nullptr;
    for(ChickenEntity* oneRooster : roosters)
    {
        if((oneRooster->getMood() == RoosterMood::call) && !oneRooster->isOnRoof())
            caller = oneRooster;
    }
    for(ChickenEntity* hen : hens)
    {
        if((caller != nullptr) && !hen->isBusy() && !night)
            hen->setFollowTarget(Ogre::Vector2(caller->getPosition().x, caller->getPosition().y), roosterSettings.mCallFollowGap);
        else
            hen->clearFollowTarget();
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

    // The same for the rooster, after the same wait as for the hens. It does not count towards the capacity of
    // the hens, and no rooster comes while a fight is going on
    if(HatcheryCycle::needCoopRooster(counts, mNumActiveSpots) && !mFightActive)
    {
        ++mCoopRoosterWait;
        if((mCoopRoosterWait >= settings.mCoopWait) && spawnFromCoop(ChickenKind::rooster, settings))
            mCoopRoosterWait = 0;
    }
    else
        mCoopRoosterWait = 0;
}

void RoomHatchery::updateFight(std::vector<ChickenEntity*>& roosters, const HatcheryCycleSettings& settings,
    HatcheryCounts& counts)
{
    ChickenEntity* first = nullptr;
    ChickenEntity* second = nullptr;
    if(mFightActive)
    {
        for(ChickenEntity* rooster : roosters)
        {
            if(rooster->getName() == mFightFirst)
                first = rooster;
            else if(rooster->getName() == mFightSecond)
                second = rooster;
        }

        // One of them was picked up or is gone: the fight is called off
        if(!HatcheryCycle::fightContinues(first != nullptr, second != nullptr))
        {
            endFight(first, second, false, counts);
            return;
        }
    }
    else
    {
        if(!HatcheryCycle::needFight(counts) || (roosters.size() < 2))
            return;

        // The first two roosters fight, if there are more the next pair follows when this one is over
        first = roosters[0];
        second = roosters[1];
        mFightActive = true;
        mFightFirst = first->getName();
        mFightSecond = second->getName();
        mFightFirstWins = (HatcheryCycle::fightWinner(Random::Uint(0, 1000)) == 0);
        mFightBrawling = false;
        mFightApproach = 0;
        mFightTurnsLeft = std::max<uint32_t>(1, settings.mFightTurns);
        for(ChickenEntity* fighter : roosters)
        {
            if((fighter != first) && (fighter != second))
                continue;

            fighter->setFighting(true);
            fighter->setRoomDriven(true);
            fighter->setMood(RoosterMood::strut, 0);
            climbDown(fighter);
        }
        fireAnimalSound(*first, "Hatchery/Cluck");
        first->notifyFight(mFightSecond, 0);
    }

    const double reach = ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryFightReach", 1.0);
    const Ogre::Vector2 firstPos(first->getPosition().x, first->getPosition().y);
    const Ogre::Vector2 secondPos(second->getPosition().x, second->getPosition().y);
    if(!mFightBrawling)
    {
        // They walk up to each other
        ++mFightApproach;
        if((firstPos.distance(secondPos) <= reach) || (mFightApproach >= settings.mFightApproachTurns))
            mFightBrawling = true;
        else
        {
            if(!first->isMoving())
                first->walkToward(secondPos, reach * mRoosterSettings.mFightStandFactor, ChickenPose::chase);
            if(!second->isMoving())
                second->walkToward(firstPos, reach * mRoosterSettings.mFightStandFactor, ChickenPose::chase);
            return;
        }
    }

    // They brawl: wings beating, pecking, puffed up. The client adds the clouds of feathers.
    if(!first->isBusy())
        first->playPose(ChickenPose::fight, 3);
    if(!second->isBusy())
        second->playPose(ChickenPose::fight, 3);
    if(mFightTurnsLeft > 0)
        --mFightTurnsLeft;
    if(mFightTurnsLeft > 0)
        return;

    endFight(first, second, true, counts);
    std::vector<ChickenEntity*>::iterator it = roosters.begin();
    while(it != roosters.end())
    {
        if((*it)->isFree())
            ++it;
        else
            it = roosters.erase(it);
    }
}

void RoomHatchery::endFight(ChickenEntity* first, ChickenEntity* second, bool finished, HatcheryCounts& counts)
{
    mFightActive = false;
    if(!finished)
    {
        // The survivor (if any) goes on as before
        ChickenEntity* survivors[2] = {first, second};
        for(uint32_t i = 0; i < 2; ++i)
        {
            if(survivors[i] == nullptr)
                continue;

            survivors[i]->setFighting(false);
            survivors[i]->setRoomDriven(false);
            survivors[i]->setAnimationState(EntityAnimation::idle_anim, true);
        }
        ChickenEntity* sender = (first != nullptr) ? first : second;
        if(sender != nullptr)
            sender->notifyFight((sender == first) ? mFightSecond : mFightFirst, 2);
        return;
    }

    ChickenEntity* winner = mFightFirstWins ? first : second;
    ChickenEntity* loser = mFightFirstWins ? second : first;
    winner->notifyFight(loser->getName(), 1);
    fireAnimalSound(*loser, "Hatchery/Cluck");
    if(loser->loseFight() && (counts.mRoosters > 0))
        --counts.mRoosters;

    // The winner stands up straight and crows
    winner->setFighting(false);
    winner->setRoomDriven(false);
    winner->setMood(RoosterMood::strut, 0);
    winner->resetSinceCrow();
    winner->playPose(ChickenPose::crow, 3);
    fireAnimalSound(*winner, "Hatchery/Crow");
}

RoosterSettings RoomHatchery::getRoosterSettings() const
{
    const ConfigManager& config = ConfigManager::getSingleton();
    RoosterSettings settings;
    settings.mCrowMin = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterCrowMin", settings.mCrowMin));
    settings.mCrowMax = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterCrowMax", settings.mCrowMax));
    settings.mChasePercent = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterChasePercent", settings.mChasePercent));
    settings.mLeadPercent = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterLeadPercent", settings.mLeadPercent));
    settings.mPerchPercent = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterPerchPercent", settings.mPerchPercent));
    settings.mPerchTurns = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterPerchTurns", settings.mPerchTurns));
    settings.mChaseTurns = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterChaseTurns", settings.mChaseTurns));
    settings.mGuardTurns = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterGuardTurns", settings.mGuardTurns));
    settings.mLeadTurns = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterLeadTurns", settings.mLeadTurns));
    settings.mCallPercent = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterCallPercent", settings.mCallPercent));
    settings.mCallTurns = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterCallTurns", settings.mCallTurns));
    settings.mDayTurns = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryDayTurns", settings.mDayTurns));
    settings.mNightPercent = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryNightPercent", settings.mNightPercent));
    settings.mCrowTurns = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterCrowTurns", settings.mCrowTurns));
    settings.mRoostDivisor = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterRoostDivisor", settings.mRoostDivisor));
    settings.mGuardFar = config.getRoomConfigDoubleOrDefault("HatcheryRoosterGuardFar", settings.mGuardFar);
    settings.mGuardNear = config.getRoomConfigDoubleOrDefault("HatcheryRoosterGuardNear", settings.mGuardNear);
    settings.mGuardApproachGap = config.getRoomConfigDoubleOrDefault("HatcheryRoosterGuardApproachGap", settings.mGuardApproachGap);
    settings.mCatchDistance = config.getRoomConfigDoubleOrDefault("HatcheryRoosterCatchDistance", settings.mCatchDistance);
    settings.mWalkGap = config.getRoomConfigDoubleOrDefault("HatcheryRoosterWalkGap", settings.mWalkGap);
    settings.mHopDistance = config.getRoomConfigDoubleOrDefault("HatcheryRoosterHopDistance", settings.mHopDistance);
    settings.mCallFollowGap = config.getRoomConfigDoubleOrDefault("HatcheryCallFollowGap", settings.mCallFollowGap);
    settings.mSnuggleGap = config.getRoomConfigDoubleOrDefault("HatcheryChickSnuggleGap", settings.mSnuggleGap);
    settings.mLeadScratchChance = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterLeadScratchChance", settings.mLeadScratchChance));
    settings.mCallScratchChance = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryRoosterCallScratchChance", settings.mCallScratchChance));
    settings.mFightStandFactor = config.getRoomConfigDoubleOrDefault("HatcheryFightStandFactor", settings.mFightStandFactor);
    return settings;
}

Tile* RoomHatchery::getNearestCoop(const Ogre::Vector2& position) const
{
    Tile* nearest = nullptr;
    float nearestDistance = 0.0f;
    for(Tile* coopTile : mCentralActiveSpotTiles)
    {
        float distance = position.squaredDistance(Ogre::Vector2(coopTile->getX(), coopTile->getY()));
        if((nearest == nullptr) || (distance < nearestDistance))
        {
            nearest = coopTile;
            nearestDistance = distance;
        }
    }
    return nearest;
}

Ogre::Vector2 RoomHatchery::getPerchSpot(const Tile& coopTile) const
{
    // The coop mesh reaches from -0.2 to +0.8 along the tile, its roof is above the middle
    double offset = ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryCoopPerchOffset", 0.3);
    return Ogre::Vector2(coopTile.getX() + static_cast<Ogre::Real>(offset), coopTile.getY());
}

bool RoomHatchery::getGroundSpot(const Tile& coopTile, Ogre::Vector2& spot) const
{
    const std::vector<RoomObjectPath::Obstacle> obstacles = RoomObjectNavigation::collect(*getGameMap(), 0.1f);
    return RoomObjectNavigation::standingPosition(obstacles, Ogre::Vector2(coopTile.getX(), coopTile.getY()), spot);
}

bool RoomHatchery::findThreat(const ChickenEntity& rooster, double radius, Ogre::Vector2& position) const
{
    const Ogre::Vector2 roosterPos(rooster.getPosition().x, rooster.getPosition().y);
    float nearestDistance = static_cast<float>(radius * radius);
    bool found = false;
    for(Creature* creature : getGameMap()->getCreatures())
    {
        if((creature == nullptr) || !creature->getIsOnMap() || (creature->getSeat() == nullptr))
            continue;

        Tile* tile = creature->getPositionTile();
        if((tile == nullptr) || (tile->getCoveringRoom() != this))
            continue;

        bool enemy = !creature->getSeat()->isAlliedSeat(getSeat());
        if(!enemy && !creature->isActionInList(CreatureActionType::eatChicken))
            continue;

        const Ogre::Vector2 creaturePos(creature->getPosition().x, creature->getPosition().y);
        float distance = roosterPos.squaredDistance(creaturePos);
        if(distance > nearestDistance)
            continue;

        nearestDistance = distance;
        position = creaturePos;
        found = true;
    }
    return found;
}

void RoomHatchery::climbDown(ChickenEntity* rooster)
{
    if(!rooster->isOnRoof())
        return;

    Tile* coopTile = getNearestCoop(Ogre::Vector2(rooster->getPosition().x, rooster->getPosition().y));
    Ogre::Vector2 spot(rooster->getPosition().x, rooster->getPosition().y);
    if(coopTile != nullptr)
        getGroundSpot(*coopTile, spot);
    rooster->hopDown(spot);
}

void RoomHatchery::roostOnRoof(ChickenEntity* rooster, const std::string& pose, bool hopFromFar)
{
    const Ogre::Vector2 position(rooster->getPosition().x, rooster->getPosition().y);
    Tile* coopTile = getNearestCoop(position);
    if(coopTile == nullptr)
    {
        // No coop: he sits on the ground
        rooster->setAnimationState(pose, true);
        return;
    }

    if(rooster->isOnRoof())
    {
        rooster->setAnimationState(pose, true);
        return;
    }

    const Ogre::Vector2 spot = getPerchSpot(*coopTile);
    if(hopFromFar || (position.distance(spot) < mRoosterSettings.mHopDistance))
    {
        double roofHeight = ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryCoopRoofHeight", 0.95);
        rooster->hopToRoof(Ogre::Vector3(spot.x, spot.y, static_cast<Ogre::Real>(roofHeight)));
        rooster->setAnimationState(pose, true);
        return;
    }

    if(!rooster->isMoving() && !rooster->walkToward(spot, mRoosterSettings.mWalkGap, ChickenPose::strut))
    {
        // No way to the coop: forget about the roof
        rooster->setMood(RoosterMood::strut, 0);
        rooster->setRoomDriven(false);
    }
}

void RoomHatchery::beginRoosterMood(ChickenEntity* rooster, const RoosterPlan& plan)
{
    rooster->setMood(plan.mMood, plan.mTurns);
    bool roofMood = (plan.mMood == RoosterMood::perch) || (plan.mMood == RoosterMood::roost) ||
        (plan.mMood == RoosterMood::crow);
    if(!roofMood)
        climbDown(rooster);

    rooster->setRoomDriven((plan.mMood != RoosterMood::strut) && (plan.mMood != RoosterMood::lead));
    if(plan.mMood == RoosterMood::crow)
    {
        rooster->resetSinceCrow();
        // Whatever the reason for the crow, it counts for the day it happens on
        mLastCrowDay = std::max(mLastCrowDay, HatcheryRooster::dayNumber(getGameMap()->getTurnNumber(), getRoosterSettings()));
        mCrowInterval = HatcheryRooster::crowInterval(getRoosterSettings(), Random::Uint(0, 1000));
    }
}

void RoomHatchery::actRoosterMood(ChickenEntity* rooster, const std::vector<ChickenEntity*>& hens,
    const RoosterSettings& settings, const Ogre::Vector2& threat)
{
    const Ogre::Vector2 position(rooster->getPosition().x, rooster->getPosition().y);
    switch(rooster->getMood())
    {
        case RoosterMood::strut:
            break;
        case RoosterMood::perch:
            roostOnRoof(rooster, ChickenPose::perch, false);
            break;
        case RoosterMood::roost:
            roostOnRoof(rooster, ChickenPose::roost, false);
            break;
        case RoosterMood::crow:
            // He crows from the roof when it is close
            roostOnRoof(rooster, ChickenPose::crow, true);
            rooster->playPose(ChickenPose::crow, 2);
            break;
        case RoosterMood::lead:
            // Scratches the ground now and then, the chicks come along
            if(!rooster->isMoving() && (Random::Uint(1, std::max<uint32_t>(1, settings.mLeadScratchChance)) == 1))
                rooster->playPose(ChickenPose::lead, 2);
            break;
        case RoosterMood::call:
            // Scratches the ground while the hens and chicks gather around him
            if(!rooster->isMoving() && (Random::Uint(1, std::max<uint32_t>(1, settings.mCallScratchChance)) == 1))
                rooster->playPose(ChickenPose::lead, 2);
            break;
        case RoosterMood::chase:
        {
            ChickenEntity* target = nullptr;
            float nearestDistance = 0.0f;
            for(ChickenEntity* hen : hens)
            {
                if(hen->isBusy())
                    continue;

                float distance = position.squaredDistance(Ogre::Vector2(hen->getPosition().x, hen->getPosition().y));
                if((target == nullptr) || (distance < nearestDistance))
                {
                    target = hen;
                    nearestDistance = distance;
                }
            }
            if(target == nullptr)
            {
                rooster->setMood(RoosterMood::strut, 0);
                rooster->setRoomDriven(false);
                break;
            }

            // Caught: he jumps on her for a moment, feathers fly and she cackles
            if(nearestDistance < static_cast<float>(settings.mCatchDistance * settings.mCatchDistance))
            {
                rooster->playPose(ChickenPose::mount, 2);
                target->playPose(ChickenPose::cackle, 3);
                rooster->setMood(RoosterMood::strut, 0);
                rooster->setRoomDriven(false);
                break;
            }

            if(!rooster->walkToward(Ogre::Vector2(target->getPosition().x, target->getPosition().y), settings.mWalkGap, ChickenPose::chase))
            {
                rooster->setMood(RoosterMood::strut, 0);
                rooster->setRoomDriven(false);
            }
            break;
        }
        case RoosterMood::guard:
        {
            // Runs to the creature, puffs up and runs off when it gets too close
            double distance = position.distance(threat);
            if(distance > settings.mGuardFar)
            {
                if(!rooster->isMoving())
                    rooster->walkToward(threat, settings.mGuardApproachGap, ChickenPose::strut);
            }
            else if(distance > settings.mGuardNear)
            {
                if(!rooster->isMoving())
                    rooster->setAnimationState(ChickenPose::guard, true);
            }
            else if(!rooster->isMoving())
            {
                // Run to a place of the hatchery away from it
                Tile* away = mCoveredTiles[Random::Uint(0, mCoveredTiles.size() - 1)];
                rooster->walkToward(Ogre::Vector2(away->getX(), away->getY()), 0.0, ChickenPose::flee);
            }
            break;
        }
    }
}

void RoomHatchery::updateRooster(ChickenEntity* rooster, const std::vector<ChickenEntity*>& hens,
    const std::vector<ChickenEntity*>& chicks, const RoosterSettings& settings)
{
    rooster->setHomeSeat(getSeat());
    // The first time he is seen the day counts as crowed, a hatchery that is built or loaded does not start with a crow
    if(mLastCrowDay < 0)
        mLastCrowDay = HatcheryRooster::dayNumber(getGameMap()->getTurnNumber(), settings);
    rooster->incrementSinceCrow();
    rooster->countDownMood();

    // He stays in his pose while it lasts
    if(rooster->isBusy())
        return;

    double guardRadius = ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryRoosterGuardRadius", 4.0);
    Ogre::Vector2 threat(0.0f, 0.0f);
    RoosterContext context;
    context.mTurn = getGameMap()->getTurnNumber();
    context.mMood = rooster->getMood();
    context.mMoodTurns = rooster->getMoodTurns();
    context.mSinceCrow = rooster->getSinceCrow();
    context.mCrowInterval = mCrowInterval;
    context.mHasCoop = !mCentralActiveSpotTiles.empty();
    context.mHasHen = !hens.empty();
    context.mHasChick = !chicks.empty();
    context.mThreat = findThreat(*rooster, guardRadius, threat);
    context.mRoll = Random::Uint(0, 99);
    context.mCrowDay = mLastCrowDay;

    RoosterPlan plan = HatcheryRooster::decide(context, settings);
    if(plan.mMood != rooster->getMood())
        beginRoosterMood(rooster, plan);

    // A hen that is chased or a rooster that is picked up leaves the chase
    actRoosterMood(rooster, hens, settings, threat);
}

void RoomHatchery::updateChickLine(const std::vector<ChickenEntity*>& hens, const std::vector<ChickenEntity*>& chicks,
    ChickenEntity* rooster, bool night)
{
    if(chicks.empty())
        return;

    double gap = ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryChickGap", 0.35);
    const Ogre::Vector2 first(chicks.front()->getPosition().x, chicks.front()->getPosition().y);

    // The animal in front of the line: the rooster when he leads, otherwise the nearest hen (or the rooster)
    ChickenEntity* leader = nullptr;
    if((rooster != nullptr) && ((rooster->getMood() == RoosterMood::lead) || (rooster->getMood() == RoosterMood::call)))
        leader = rooster;
    else
    {
        float nearestDistance = 0.0f;
        for(ChickenEntity* hen : hens)
        {
            float distance = first.squaredDistance(Ogre::Vector2(hen->getPosition().x, hen->getPosition().y));
            if((leader == nullptr) || (distance < nearestDistance))
            {
                leader = hen;
                nearestDistance = distance;
            }
        }
        if((leader == nullptr) && (rooster != nullptr) && !rooster->isOnRoof())
            leader = rooster;
    }
    if(leader == nullptr)
    {
        for(ChickenEntity* chick : chicks)
            chick->clearFollowTarget();
        return;
    }

    const Ogre::Vector2 leaderPos(leader->getPosition().x, leader->getPosition().y);

    // At night every chick snuggles up to the hen
    if(night)
    {
        for(ChickenEntity* chick : chicks)
            chick->setFollowTarget(leaderPos, mRoosterSettings.mSnuggleGap);
        return;
    }

    // Each chick follows the one in front of it: they walk in a line behind the hen
    std::vector<ChickenEntity*> remaining = chicks;
    Ogre::Vector2 previous = leaderPos;
    while(!remaining.empty())
    {
        std::vector<ChickenEntity*>::iterator nearest = remaining.begin();
        float nearestDistance = previous.squaredDistance(Ogre::Vector2((*nearest)->getPosition().x, (*nearest)->getPosition().y));
        for(std::vector<ChickenEntity*>::iterator it = remaining.begin(); it != remaining.end(); ++it)
        {
            float distance = previous.squaredDistance(Ogre::Vector2((*it)->getPosition().x, (*it)->getPosition().y));
            if(distance < nearestDistance)
            {
                nearest = it;
                nearestDistance = distance;
            }
        }
        (*nearest)->setFollowTarget(previous, gap);
        previous = Ogre::Vector2((*nearest)->getPosition().x, (*nearest)->getPosition().y);
        remaining.erase(nearest);
    }
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
