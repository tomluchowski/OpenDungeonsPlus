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

#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/GameEntityType.h"
#include "entities/GiftBoxEntity.h"
#include "entities/PersistentObject.h"
#include "entities/SkillEntity.h"
#include "entities/Tile.h"
#include "network/ODPacket.h"
#include "network/ODServer.h"
#include "network/ServerNotification.h"
#include "rooms/RoomManager.h"
#include "utils/LogManager.h"

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
        checkBuildRoomDefaultEditor(gameMap, RoomDungeonTemple::mRoomType, inputManager, inputCommand);
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

const double RoomDungeonTemple::HEART_HP_PER_TILE = 10000.0;

RoomDungeonTemple::RoomDungeonTemple(GameMap* gameMap) :
    Room(gameMap),
    mTempleObject(nullptr),
    mHeartHP(-1.0),
    mCriticalWarningSent(false)
{
    setMeshName("DungeonTemple");
}

double RoomDungeonTemple::getHP(Tile* tile) const
{
    return mHeartHP < 0.0 ? getHeartMaxHP() : mHeartHP;
}

double RoomDungeonTemple::getHeartMaxHP() const
{
    return HEART_HP_PER_TILE * static_cast<double>(numCoveredTiles());
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
    Room::doUpkeep();
}

void RoomDungeonTemple::exportToStream(std::ostream& os) const
{
    Room::exportToStream(os);
    if(!getGameMap()->isInEditorMode())
        os << "HeartHealth " << getHP(nullptr) << '\n';
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
        if(marker == "HeartHealth")
            mHeartHP = std::min(value, getHeartMaxHP());
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
        default:
            OD_LOG_ERR("room=" + getName() + ", entity=" + carriedEntity->getName());
            return nullptr;
    }
}

void RoomDungeonTemple::notifyCarryingStateChanged(Creature* carrier, GameEntity* carriedEntity)
{
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
