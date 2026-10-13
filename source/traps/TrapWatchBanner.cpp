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

#include "traps/TrapWatchBanner.h"

#include "creatureaction/CreatureActionWatchBanner.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "entities/TrapEntity.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "gamemap/Pathfinding.h"
#include "ODApplication.h"
#include "rooms/Room.h"
#include "rooms/RoomType.h"
#include "traps/TrapManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"

const std::string TrapWatchBannerName = "WatchBanner";
const std::string TrapWatchBannerNameDisplay = "Watch banner";
const TrapType TrapWatchBanner::mTrapType = TrapType::watchBanner;

namespace
{
class TrapWatchBannerFactory : public TrapFactory
{
    TrapType getTrapType() const override
    { return TrapWatchBanner::mTrapType; }

    const std::string& getName() const override
    { return TrapWatchBannerName; }

    const std::string& getNameReadable() const override
    { return TrapWatchBannerNameDisplay; }

    int getCostPerTile() const override
    { return ConfigManager::getSingleton().getTrapConfigInt32("WatchBannerCostPerTile"); }

    const std::string& getMeshName() const override
    {
        static const std::string meshName = "Spiketrap";
        return meshName;
    }

    void checkBuildTrap(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildTrapDefault(gameMap, TrapType::watchBanner, inputManager, inputCommand);
    }

    bool buildTrap(GameMap* gameMap, Player* player, ODPacket& packet) const override
    {
        std::vector<Tile*> tiles;
        if(!getTrapTilesDefault(tiles, gameMap, player, packet))
            return false;

        int32_t pricePerTarget = TrapManager::costPerTile(TrapType::watchBanner);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
            return false;

        TrapWatchBanner* trap = new TrapWatchBanner(gameMap);
        return buildTrapDefault(gameMap, trap, player->getSeat(), tiles);
    }

    void checkBuildTrapEditor(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildTrapDefaultEditor(gameMap, TrapType::watchBanner, inputManager, inputCommand);
    }

    bool buildTrapEditor(GameMap* gameMap, ODPacket& packet) const override
    {
        TrapWatchBanner* trap = new TrapWatchBanner(gameMap);
        return buildTrapDefaultEditor(gameMap, trap, packet);
    }

    Trap* getTrapFromStream(GameMap* gameMap, std::istream& is) const override
    {
        TrapWatchBanner* trap = new TrapWatchBanner(gameMap);
        if(!Trap::importTrapFromStream(*trap, is))
        {
            OD_LOG_ERR("Error while building a trap from the stream");
        }
        return trap;
    }

    bool buildTrapOnTiles(GameMap* gameMap, Player* player, const std::vector<Tile*>& tiles, bool noFee = false) const override
    {
        return buildTrapOnTiles(gameMap, player->getSeat(), tiles, noFee);
    }

    bool buildTrapOnTiles(GameMap* gameMap, Seat* seatPtr, const std::vector<Tile*>& tiles, bool noFee = false) const
    {
        int32_t pricePerTarget = TrapManager::costPerTile(TrapType::watchBanner);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!noFee)
            if(!gameMap->withdrawFromTreasuries(price, seatPtr))
                return false;

        TrapWatchBanner* trap = new TrapWatchBanner(gameMap);
        return buildTrapDefault(gameMap, trap, seatPtr, tiles);
    }
};

// Register the factory
static TrapRegister reg(new TrapWatchBannerFactory);
}

TrapWatchBanner::TrapWatchBanner(GameMap* gameMap) :
    Trap(gameMap),
    mNextDistressTurn(0)
{
    setMeshName("");
}

void TrapWatchBanner::doUpkeep()
{
    Trap::doUpkeep();

    int64_t turn = getGameMap()->getTurnNumber();
    if(turn < mNextDistressTurn)
        return;

    // The post notices enemies within its aura and calls the guards of the guard rooms
    int32_t aura = ConfigManager::getSingleton().getTrapConfigInt32("WatchBannerAuraTiles");
    Tile* intruderTile = nullptr;
    for(Tile* postTile : mCoveredTiles)
    {
        if(!isActivated(postTile))
            continue;

        for(Creature* creature : getGameMap()->getCreatures())
        {
            if(!creature->isAlive() || !creature->getIsOnMap() || creature->isInvisible())
                continue;

            if((creature->getSeat() == nullptr) || getSeat()->isAlliedSeat(creature->getSeat()))
                continue;

            Tile* creatureTile = creature->getPositionTile();
            if(creatureTile == nullptr)
                continue;

            if(Pathfinding::squaredDistanceTile(*postTile, *creatureTile) > (aura * aura))
                continue;

            intruderTile = creatureTile;
            break;
        }

        if(intruderTile != nullptr)
            break;
    }

    if(intruderTile == nullptr)
        return;

    mNextDistressTurn = turn + static_cast<int64_t>(
        ConfigManager::getSingleton().getRoomConfigDouble("GuardRoomDistressSeconds") * ODApplication::turnsPerSecond);

    std::vector<Room*> guardRooms = getGameMap()->getRoomsByTypeAndSeat(RoomType::guardRoom, getSeat());
    for(Room* room : guardRooms)
    {
        for(unsigned int i = 0; ; ++i)
        {
            Creature* guard = room->getCreatureUsingRoom(i);
            if(guard == nullptr)
                break;

            CreatureActionWatchBanner::goToIntruder(*guard, intruderTile);
        }
    }
}

TrapEntity* TrapWatchBanner::getTrapEntity(Tile* tile)
{
    return new TrapEntity(getGameMap(), *this, reg.getTrapFactory()->getMeshName(), tile, 0.0, true, isActivated(tile) ? 1.0f : 0.7f);
}
