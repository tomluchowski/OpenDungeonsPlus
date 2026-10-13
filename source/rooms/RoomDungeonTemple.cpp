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

#include "rooms/RoomDungeonTemple.h"

#include "entities/BuildingObject.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/GameEntityType.h"
#include "entities/GiftBoxEntity.h"
#include "entities/PersistentObject.h"
#include "entities/SkillEntity.h"
#include "entities/Tile.h"
#include "entities/TreasuryObject.h"
#include "ODApplication.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "modes/InputCommand.h"
#include "modes/InputManager.h"
#include "network/ODClient.h"
#include "network/ODPacket.h"
#include "network/ODServer.h"
#include "network/ServerNotification.h"
#include "rooms/RoomManager.h"
#include "rooms/RoomTreasury.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/Random.h"

#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>

const std::string RoomDungeonTempleName = "DungeonTemple";
const std::string RoomDungeonTempleNameDisplay = "Dungeon temple room";
const RoomType RoomDungeonTemple::mRoomType = RoomType::dungeonTemple;
const TileVisual RoomDungeonTemple::mRoomVisual = TileVisual::dungeonTempleRoom;

namespace
{
//! Tiles from the centre of a heart to the edge of its whole 5x5 area (core and treasury ring).
const int HEART_AREA_RADIUS = 2;

class DungeonHeartObject : public PersistentObject
{
public:
    DungeonHeartObject(GameMap* gameMap, RoomDungeonTemple& room, Tile* tile) :
        PersistentObject(gameMap, room, "DungeonTempleObject", tile, 0.0, false),
        mRoom(room)
    {
        setSeat(room.getSeat());
    }

    bool isAttackable(Tile* tile, Seat* seat) const override
    { return mRoom.canAttackHeart(tile, seat); }

    double getHP(Tile* tile) const override
    { return mRoom.getHP(nullptr); }

    double takeDamage(GameEntity* attacker, double absoluteDamage, double physicalDamage,
        double magicalDamage, double elementDamage, Tile* tile, bool ko) override
    {
        const double damage = mRoom.takeHeartDamage(attacker, absoluteDamage,
            physicalDamage, magicalDamage, elementDamage, tile);
        if(damage > 0.0 && getHP(nullptr) <= 0.0)
            fireEntityDead();
        return damage;
    }

private:
    RoomDungeonTemple& mRoom;
};

class RoomDungeonTempleFactory : public RoomFactory
{
    TileVisual getVisualType() const override
    { return RoomDungeonTemple::mRoomVisual; }

    
    RoomType getRoomType() const override
    { return RoomDungeonTemple::mRoomType; }

    const std::string& getName() const override
    { return RoomDungeonTempleName; }

    const std::string& getNameReadable() const override
    { return RoomDungeonTempleNameDisplay; }

    int getCostPerTile() const override
    { return 0; }

    void checkBuildRoom(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        // Not buildable in game mode
    }

    bool buildRoom(GameMap* gameMap, Player* player, ODPacket& packet) const override
    {
        // Not buildable in game mode
        return false;
    }
    
    bool buildRoomOnTiles(GameMap* gameMap, Player* player, const std::vector<Tile*>& tiles, bool noFee = false) const override
    {
        // Not buildable in game mode
        return false;
    }

    void checkBuildRoomEditor(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        // The heart is placed as a 5x5 block: the 3x3 core is the heart, the 16 outer tiles
        // are its treasury ring (H6). The centre is the middle of the dragged area; a single
        // click has no drag, so the centre is the hovered tile.
        std::string txt = RoomManager::getRoomReadableName(RoomDungeonTemple::mRoomType);
        inputCommand.displayText(Ogre::ColourValue::White, txt);
        if(inputManager.mCommandState == InputCommandState::infoOnly)
        {
            inputCommand.selectSquaredTiles(inputManager.mXPos, inputManager.mYPos, inputManager.mXPos,
                inputManager.mYPos);
            return;
        }

        const int centreX = (inputManager.mXPos + inputManager.mLStartDragX) / 2;
        const int centreY = (inputManager.mYPos + inputManager.mLStartDragY) / 2;

        std::vector<Tile*> buildableTiles;
        for(int x = centreX - HEART_AREA_RADIUS; x <= centreX + HEART_AREA_RADIUS; x++)
        {
            for(int y = centreY - HEART_AREA_RADIUS; y <= centreY + HEART_AREA_RADIUS; y++)
            {
                Tile* tile = gameMap->getTile(x, y);
                if(tile == nullptr)
                    continue;
                // We accept any tile if there is no building
                if(tile->getIsBuilding())
                    continue;
                buildableTiles.push_back(tile);
            }
        }

        if(buildableTiles.empty())
        {
            inputCommand.unselectAllTiles();
            inputCommand.displayTileBuildFailure(gameMap->getTile(inputManager.mXPos, inputManager.mYPos),
                nullptr);
            return;
        }

        if(inputManager.mCommandState == InputCommandState::building)
        {
            inputCommand.selectTiles(buildableTiles);
            return;
        }

        ClientNotification* clientNotification = RoomManager::createRoomClientNotificationEditor(
            RoomDungeonTemple::mRoomType);
        uint32_t nbTiles = buildableTiles.size();
        int32_t seatId = inputManager.mSeatIdSelected;
        clientNotification->mPacket << seatId;
        clientNotification->mPacket << nbTiles;

        for(Tile* tile : buildableTiles)
            gameMap->tileToPacket(clientNotification->mPacket, tile);

        ODClient::getSingleton().queueClientNotification(clientNotification);
    }

    bool buildRoomEditor(GameMap* gameMap, ODPacket& packet) const override
    {
        RoomDungeonTemple* room = new RoomDungeonTemple(gameMap);
        return buildRoomDefaultEditor(gameMap, room, packet);
    }

    //! \brief Creates an empty room of this type, for a room that has to be split in two.
    Room* createRoom(GameMap* gameMap) const override
    { return new RoomDungeonTemple(gameMap); }

    Room* getRoomFromStream(GameMap* gameMap, std::istream& is) const override
    {
        RoomDungeonTemple* room = new RoomDungeonTemple(gameMap);
        if(!Room::importRoomFromStream(*room, is))
        {
            OD_LOG_ERR("Error while building a room from the stream");
        }
        return room;
    }
};

// Register the factory
static RoomRegister reg(new RoomDungeonTempleFactory);
}

const double RoomDungeonTemple::HEART_MAX_HP = 10000.0;
const double RoomDungeonTemple::HEART_HEAL_PER_SECOND = 2.5;

// The treasury ring of a 5x5 heart stores a fixed amount of gold per ring tile
// (H6/R8). Unlike a treasury room this is not raised by the treasury skill:
// the ring is part of the heart, not a buildable treasury.
static const int treasuryTileCapacity = 1000;

// Tiles from the heart tile to the edge of the 3x3 core; everything beyond it is the ring.
static const int HEART_CORE_RADIUS = 1;

RoomDungeonTemple::RoomDungeonTemple(GameMap* gameMap) :
    Room(gameMap),
    mTempleObject(nullptr),
    mHeartHP(-1.0),
    mCriticalWarningSent(false),
    mGoldChanged(false)
{
    setMeshName("DungeonTemple");
}

double RoomDungeonTemple::getHP(Tile* tile) const
{
    return mHeartHP < 0.0 ? getHeartMaxHP() : mHeartHP;
}

Tile* RoomDungeonTemple::getHeartTile() const
{
    return mTempleObject != nullptr ? mTempleObject->getPositionTile() : nullptr;
}

double RoomDungeonTemple::getHeartMaxHP() const
{
    return HEART_MAX_HP;
}

double RoomDungeonTemple::getHeartHealthFraction() const
{
    const double maxHP = getHeartMaxHP();
    if(maxHP <= 0.0)
        return 0.0;
    return std::max(0.0, std::min(1.0, getHP(nullptr) / maxHP));
}

bool RoomDungeonTemple::canAttackHeart(Tile* tile, Seat* seat) const
{
    return seat != nullptr && getSeat() != nullptr && !getSeat()->isAlliedSeat(seat)
        && mTempleObject != nullptr && tile == mTempleObject->getPositionTile()
        && getHP(nullptr) > 0.0;
}

double RoomDungeonTemple::takeHeartDamage(GameEntity* attacker, double absoluteDamage,
    double physicalDamage, double magicalDamage, double elementDamage,
    Tile* tileTakingDamage)
{
    if(attacker == nullptr || !canAttackHeart(tileTakingDamage, attacker->getSeat()))
        return 0.0;
    // Only fighters damage an enemy heart; workers leave it alone
    if(attacker->getObjectType() == GameEntityType::creature
        && static_cast<Creature*>(attacker)->getDefinition()->isWorker())
        return 0.0;

    if(mHeartHP < 0.0)
        mHeartHP = getHeartMaxHP();
    const double damage = std::max(0.0, absoluteDamage)
        + std::max(0.0, physicalDamage - getPhysicalDefense())
        + std::max(0.0, magicalDamage - getMagicalDefense())
        + std::max(0.0, elementDamage - getElementDefense());
    const double damageDone = std::min(mHeartHP, damage);
    mHeartHP -= damageDone;
    if(mHeartHP <= 0.0)
    {
        // The heart object is removed shortly after death: keep what the defeat notification needs on the owner.
        attacker->getSeat()->getStatistics().mKeepersDefeated++;
        Player* owner = getSeat()->getPlayer();
        if(owner != nullptr)
        {
            Tile* heartTile = mTempleObject->getPositionTile();
            owner->recordHeartDestroyed(attacker->getSeat()->getId(),
                heartTile != nullptr ? heartTile->getX() : -1,
                heartTile != nullptr ? heartTile->getY() : -1);
        }
        fireEntityDead();
    }
    else if(!mCriticalWarningSent && !getGameMap()->isInEditorMode()
        && mHeartHP <= 0.11 * getHeartMaxHP())
    {
        // Warn the owner once, at the first hit after the heart is already at or
        // below 11 % of its durability. The killing hit sends nothing (defeat handles it).
        mCriticalWarningSent = true;
        Player* owner = getSeat()->getPlayer();
        if(owner != nullptr && owner->getIsHuman() && !owner->getHasLost())
        {
            ServerNotification* serverNotification = new ServerNotification(
                ServerNotificationType::chatServer, owner);
            serverNotification->mPacket << "Your dungeon heart is in critical condition!"
                << EventShortNoticeType::majorGameEvent;
            ODServer::getSingleton().queueServerNotification(serverNotification);
        }
    }
    if(getSeat()->getPlayer() != nullptr)
        getGameMap()->playerIsFighting(getSeat()->getPlayer(), tileTakingDamage);
    return damageDone;
}

bool RoomDungeonTemple::removeCoveredTile(Tile* tile)
{
    // The floor of a heart is never released in game mode, not even when the heart is
    // destroyed: the platform stays as a ruin (see doUpkeep).
    if(!getGameMap()->isInEditorMode())
        return false;

    // A ring tile deleted in the editor releases its gold, like a treasury tile.
    RoomTreasuryTileData* roomTreasuryTileData = static_cast<RoomTreasuryTileData*>(mTileData[tile]);
    if(!roomTreasuryTileData->mMeshOfTile.empty())
        removeBuildingObject(tile);

    if(roomTreasuryTileData->mGoldInTile > 0)
    {
        int value = roomTreasuryTileData->mGoldInTile;
        OD_LOG_INF("Room " + getName()
            + ", tile=" + Tile::displayAsString(tile) + " releases gold amount = "
            + Helper::toString(value));
        TreasuryObject* obj = new TreasuryObject(getGameMap(), value);
        obj->addToGameMap();
        Ogre::Vector3 spawnPosition(static_cast<Ogre::Real>(tile->getX()),
                                    static_cast<Ogre::Real>(tile->getY()), 0.0f);
        obj->createMesh();
        obj->setPosition(spawnPosition);
    }

    roomTreasuryTileData->mMeshOfTile.clear();
    roomTreasuryTileData->mGoldInTile = 0;
    return Room::removeCoveredTile(tile);
}

void RoomDungeonTemple::doUpkeep()
{
    if(getHP(nullptr) <= 0.0 && mTempleObject != nullptr && mTempleObject->notifyRemoveAsked())
    {
        // The heart is destroyed: only the heart object is released. The floor tiles stay
        // covered by this room as an inert ruin, so the platform keeps its look. The room
        // no longer counts as a dungeon temple because getHP() is 0 (see
        // GameMap::getRoomsByType and Seat::computeSeatBeginTurn), which starts the
        // existing last-temple defeat path.
        removeAllBuildingObjects();
        mTempleObject = nullptr;
    }
    // A living heart heals up to its maximum; a destroyed one (0) never does
    if(mHeartHP > 0.0 && mHeartHP < getHeartMaxHP())
    {
        mHeartHP = std::min(getHeartMaxHP(),
            mHeartHP + HEART_HEAL_PER_SECOND / ODApplication::turnsPerSecond);
        // Healing above the critical level re-arms the warning for the next drop below it
        if(mHeartHP > 0.11 * getHeartMaxHP())
            mCriticalWarningSent = false;
    }
    Room::doUpkeep();

    // Refresh the gold meshes of the ring tiles that changed since the last upkeep
    if(mGoldChanged)
    {
        for(std::pair<Tile* const, TileData*>& p : mTileData)
        {
            if(!isTreasuryTile(p.first))
                continue;
            RoomTreasuryTileData* roomTreasuryTileData = static_cast<RoomTreasuryTileData*>(p.second);
            updateTreasuryMeshesForTile(p.first, roomTreasuryTileData);
        }
        mGoldChanged = false;
    }
}

void RoomDungeonTemple::exportToStream(std::ostream& os) const
{
    Room::exportToStream(os);
    if(!getGameMap()->isInEditorMode())
        os << "HeartHealth10000 " << getHP(nullptr) << '\n';
}

bool RoomDungeonTemple::importFromStream(std::istream& is)
{
    if(!Room::importFromStream(is))
        return false;
    // Old maps and saves have no room-level health record: their heart starts
    // undamaged, without interpreting the following room as health.
    mHeartHP = getHeartMaxHP();
    is >> std::ws;
    if(is.peek() == 'H')
    {
        std::string marker;
        double value;
        if(!(is >> marker >> value) || !std::isfinite(value) || value < 0.0)
            return false;
        if(marker == "HeartHealth10000")
            mHeartHP = std::min(value, getHeartMaxHP());
        else if(marker == "HeartHealth")
        {
            // Saves from when the heart had 10000 health per room tile: keep the same share of
            // the fixed maximum.
            const double oldMaxHP = 10000.0 * static_cast<double>(numCoveredTiles());
            mHeartHP = oldMaxHP > 0.0
                ? std::min(1.0, value / oldMaxHP) * getHeartMaxHP() : 0.0;
        }
        else if(marker == "HeartHP")
        {
            // Saves written before the heart had its own health: the value was measured against
            // the durability of the floor tiles. Keep the same share of the new maximum.
            const double floorDurability = Building::getHP(nullptr);
            mHeartHP = floorDurability > 0.0
                ? std::min(1.0, value / floorDurability) * getHeartMaxHP() : 0.0;
        }
        else
            return false;
    }
    return true;
}

Tile* RoomDungeonTemple::getRingCenterTile() const
{
    Tile* center = getHeartTile();
    if(center == nullptr)
        center = getCentralTile();
    return center;
}

bool RoomDungeonTemple::isTreasuryTile(Tile* tile) const
{
    if(tile == nullptr)
        return false;

    Tile* center = getRingCenterTile();
    if(center == nullptr)
        return false;

    // The 3x3 core (Chebyshev distance <= 1 from the heart) is the heart itself;
    // only the outer ring of a 5x5 heart stores gold.
    const int dx = tile->getX() - center->getX();
    const int dy = tile->getY() - center->getY();
    if(dx >= -HEART_CORE_RADIUS && dx <= HEART_CORE_RADIUS
        && dy >= -HEART_CORE_RADIUS && dy <= HEART_CORE_RADIUS)
        return false;

    for(Tile* covered : mCoveredTiles)
    {
        if(covered == tile)
            return true;
    }
    return false;
}

int RoomDungeonTemple::getTotalGoldStorage() const
{
    int numTreasuryTiles = 0;
    for(Tile* tile : mCoveredTiles)
    {
        if(isTreasuryTile(tile))
            numTreasuryTiles++;
    }
    return numTreasuryTiles * treasuryTileCapacity;
}

int RoomDungeonTemple::getTotalGoldStored() const
{
    int totalGold = 0;
    for(const std::pair<Tile* const, TileData*>& p : mTileData)
    {
        if(!isTreasuryTile(p.first))
            continue;
        RoomTreasuryTileData* roomTreasuryTileData = static_cast<RoomTreasuryTileData*>(p.second);
        totalGold += roomTreasuryTileData->mGoldInTile;
    }
    return totalGold;
}

int RoomDungeonTemple::depositGold(int gold, Tile* tile)
{
    int goldDeposited, goldToDeposit = gold, emptySpace;

    // Start by trying to deposit the gold in the requested tile, if it is part of the ring.
    // The core tiles of the heart are never filled.
    if(isTreasuryTile(tile))
    {
        RoomTreasuryTileData* roomTreasuryTileData = static_cast<RoomTreasuryTileData*>(mTileData[tile]);
        emptySpace = std::max(0, treasuryTileCapacity - roomTreasuryTileData->mGoldInTile);
        goldDeposited = std::min(emptySpace, goldToDeposit);
        roomTreasuryTileData->mGoldInTile += goldDeposited;
        goldToDeposit -= goldDeposited;
    }

    // If there is still gold left, fill the remaining ring tiles.
    for(std::pair<Tile* const, TileData*>& p : mTileData)
    {
        if(goldToDeposit <= 0)
            break;

        if(!isTreasuryTile(p.first))
            continue;
        if(p.second->mHP <= 0)
            continue;

        RoomTreasuryTileData* roomTreasuryTileData = static_cast<RoomTreasuryTileData*>(p.second);
        emptySpace = std::max(0, treasuryTileCapacity - roomTreasuryTileData->mGoldInTile);
        goldDeposited = std::min(emptySpace, goldToDeposit);
        roomTreasuryTileData->mGoldInTile += goldDeposited;
        goldToDeposit -= goldDeposited;
    }

    // Return the amount we were actually able to deposit
    // (i.e. the amount we wanted to deposit minus the amount we were unable to deposit).
    int wasDeposited = gold - goldToDeposit;
    // If we couldn't deposit anything, we do not notify
    if(wasDeposited == 0)
        return wasDeposited;

    mGoldChanged = true;

    // Tells the client to play a deposit gold sound. For now, we only send it to the players
    // with vision on tile
    fireRoomSound(*tile, "Treasury/DepositGold");

    return wasDeposited;
}

int RoomDungeonTemple::withdrawGold(int gold)
{
    int withdrawalAmount = 0;
    for(std::pair<Tile* const, TileData*>& p : mTileData)
    {
        if(withdrawalAmount >= gold)
            break;

        if(!isTreasuryTile(p.first))
            continue;

        RoomTreasuryTileData* roomTreasuryTileData = static_cast<RoomTreasuryTileData*>(p.second);
        if(roomTreasuryTileData->mGoldInTile <= 0)
            continue;

        // Check to see if the current ring tile has enough gold to fill the amount we still
        // need to pick up.
        int goldStillNeeded = gold - withdrawalAmount;
        if(roomTreasuryTileData->mGoldInTile >= goldStillNeeded)
        {
            withdrawalAmount += goldStillNeeded;
            roomTreasuryTileData->mGoldInTile -= goldStillNeeded;
        }
        else
        {
            // There is not enough to satisfy the request so take everything there is and move
            // on to the next tile.
            withdrawalAmount += roomTreasuryTileData->mGoldInTile;
            roomTreasuryTileData->mGoldInTile = 0;
        }
    }

    if(withdrawalAmount > 0)
        mGoldChanged = true;

    return withdrawalAmount;
}

void RoomDungeonTemple::updateTreasuryMeshesForTile(Tile* tile, RoomTreasuryTileData* roomTreasuryTileData)
{
    int gold = roomTreasuryTileData->mGoldInTile;
    OD_ASSERT_TRUE_MSG(gold >= 0, "room=" + getName() + ", gold=" + Helper::toString(gold));

    // If the tile was and is empty, nothing to do
    if(roomTreasuryTileData->mMeshOfTile.empty() && (gold == 0))
        return;

    // If the tile was not empty but is now, we remove it
    if(gold == 0)
    {
        roomTreasuryTileData->mMeshOfTile.clear();
        removeBuildingObject(tile);
        return;
    }

    // If the mesh has not changed we do not need to do anything.
    std::string newMeshName = TreasuryObject::getMeshNameForGold(gold);
    if(roomTreasuryTileData->mMeshOfTile.compare(newMeshName) == 0)
        return;

    // If the mesh has changed we need to destroy the existing gold stack if there was one
    if(!roomTreasuryTileData->mMeshOfTile.empty())
        removeBuildingObject(tile);

    if(gold > 0)
    {
        const double offset = 0.2;
        double posX = static_cast<double>(tile->getX());
        double posY = static_cast<double>(tile->getY());
        double posZ = 0;
        posX += Random::Double(-offset, offset);
        posY += Random::Double(-offset, offset);
        double angle = Random::Double(0.0, 360);
        BuildingObject* ro = new BuildingObject(getGameMap(), *this, newMeshName, tile, posX, posY, posZ, angle, false);
        addBuildingObject(tile, ro);
    }

    roomTreasuryTileData->mMeshOfTile = newMeshName;
}

void RoomDungeonTemple::splitRoom(Room& newRoom, const std::vector<Tile*>& tiles)
{
    // The tiles took a copy of their gold with them. This room counts the gold of every ring
    // tile it holds data for, not only the ones it still covers, so leaving the copy behind
    // would have the same gold counted by both rooms.
    for(Tile* tile : tiles)
    {
        std::map<Tile*, TileData*>::iterator it = mTileData.find(tile);
        if(it == mTileData.end())
            continue;

        RoomTreasuryTileData* roomTreasuryTileData = static_cast<RoomTreasuryTileData*>(it->second);
        roomTreasuryTileData->mGoldInTile = 0;
        roomTreasuryTileData->mMeshOfTile.clear();
    }

    mGoldChanged = true;
}

RoomTreasuryTileData* RoomDungeonTemple::createTileData(Tile* tile)
{
    return new RoomTreasuryTileData;
}

void RoomDungeonTemple::updateActiveSpots(GameMap* gameMap)
{
    if(gameMap == nullptr)
    {
        gameMap = getGameMap();
    }
    // Room::updateActiveSpots(); <<-- Disabled on purpose.
    // We don't update the active spots the same way as only the central tile is needed.
    if (getGameMap()->isInEditorMode())
        updateTemplePosition();
    else
    {
        // A destroyed heart (ruin) has no temple object and must not get a new one
        if(mTempleObject == nullptr && getHP(nullptr) > 0.0)
        {
            // We check if the temple already exists (that can happen if it has
            // been restored after restoring a saved game)
            if(mBuildingObjects.empty())
                updateTemplePosition();
            else
            {
                for(auto& p : mBuildingObjects)
                {
                    if(p.second == nullptr)
                        continue;

                    // We take the first BuildingObject. Note that we cannot use
                    // the central tile because after saving a game, the central tile may
                    // not be the same if some tiles have been destroyed
                    mTempleObject = p.second;
                    break;
                }
            }
        }
    }
}

void RoomDungeonTemple::updateTemplePosition()
{
    // Only the server game map should load objects.
    if (!getIsOnServerMap())
        return;

    // Delete all previous rooms meshes and recreate a central one.
    removeAllBuildingObjects();
    mTempleObject = nullptr;

    Tile* centralTile = getCentralTile();
    if (centralTile == nullptr)
        return;

    mTempleObject = new DungeonHeartObject(getGameMap(), *this, centralTile);
    addBuildingObject(centralTile, mTempleObject);
}

void RoomDungeonTemple::destroyMeshLocal(NodeType nt)
{
    Room::destroyMeshLocal();
    mTempleObject = nullptr;
}

bool RoomDungeonTemple::hasCarryEntitySpot(GameEntity* carriedEntity)
{
    switch(carriedEntity->getObjectType())
    {
        case GameEntityType::giftBoxEntity:
        case GameEntityType::skillEntity:
            return true;
        case GameEntityType::treasuryObject:
            // A 5x5 heart has space for more gold until its ring is full; a 3x3 heart
            // never accepts gold (it has no ring)
            if(getTotalGoldStored() >= getTotalGoldStorage())
                return false;
            return true;
        default:
            return false;
    }
}

Tile* RoomDungeonTemple::askSpotForCarriedEntity(GameEntity* carriedEntity)
{
    switch(carriedEntity->getObjectType())
    {
        case GameEntityType::giftBoxEntity:
        case GameEntityType::skillEntity:
            return getCentralTile();
        case GameEntityType::treasuryObject:
        {
            if(!hasCarryEntitySpot(carriedEntity))
                return nullptr;
            // Any ring tile works: the deposit is handled by the covering room and fills the
            // whole ring
            for(Tile* tile : mCoveredTiles)
            {
                if(isTreasuryTile(tile))
                    return tile;
            }
            return nullptr;
        }
        default:
            OD_LOG_ERR("room=" + getName() + ", entity=" + carriedEntity->getName());
            return nullptr;
    }
}

void RoomDungeonTemple::notifyCarryingStateChanged(Creature* carrier, GameEntity* carriedEntity)
{
    // A gold delivery is deposited through the covering room (the treasury ring), the same
    // way a treasury room handles it: the TreasuryObject handles itself
    if(carriedEntity->getObjectType() == GameEntityType::treasuryObject)
        return;

    // We check if the carrier is at the expected destination. If not on the wanted tile,
    // we don't accept the entity
    // Note that if the wanted tile were to move during the transport, the carried entity
    // will be dropped at its original destination and will become available again so there
    // should be no problem
    Tile* carrierTile = carrier->getPositionTile();
    if(carrierTile != getCentralTile())
        return;

    switch(carriedEntity->getObjectType())
    {
        case GameEntityType::giftBoxEntity:
        {
            // We apply the gift box effect
            GiftBoxEntity* giftBox = static_cast<GiftBoxEntity*>(carriedEntity);
            giftBox->applyEffect();
            giftBox->removeEntityFromPositionTile();
            giftBox->removeFromGameMap();
            giftBox->deleteYourself();
            return;
        }
        case GameEntityType::skillEntity:
        {
            // We notify the player that the skill is now available and we delete the skillEntity
            SkillEntity* skillEntity = static_cast<SkillEntity*>(carriedEntity);
            getSeat()->addSkillPoints(skillEntity->getSkillPoints());
            skillEntity->removeEntityFromPositionTile();
            skillEntity->removeFromGameMap();
            skillEntity->deleteYourself();
            return;
        }
        default:
            OD_LOG_ERR("room=" + getName() + ", entity=" + carriedEntity->getName());
            return;
    }
}

void RoomDungeonTemple::restoreInitialEntityState()
{
    // A destroyed heart (ruin) has no temple object, only its floor has to be restored
    if(mTempleObject == nullptr && getHP(nullptr) <= 0.0)
    {
        Room::restoreInitialEntityState();
        return;
    }

    // We need to use seats with vision before calling Room::restoreInitialEntityState
    // because it will empty the list
    if(mTempleObject == nullptr)
    {
        OD_LOG_ERR("roomDungeonTemple=" + getName());
        return;
    }

    Tile* tileTempleObject = mTempleObject->getPositionTile();
    if(tileTempleObject == nullptr)
    {
        OD_LOG_ERR("roomDungeonTemple=" + getName() + ", mTempleObject=" + mTempleObject->getName());
        return;
    }
    TileData* tileData = mTileData[tileTempleObject];
    if(tileData == nullptr)
    {
        OD_LOG_ERR("roomDungeonTemple=" + getName() + ", tile=" + Tile::displayAsString(tileTempleObject));
        return;
    }

    if(!tileData->mSeatsVision.empty())
        mTempleObject->notifySeatsWithVision(tileData->mSeatsVision);

    // If there are no covered tile, the temple object is not working
    if(numCoveredTiles() == 0)
        mTempleObject->notifyRemoveAsked();

    Room::restoreInitialEntityState();
}
