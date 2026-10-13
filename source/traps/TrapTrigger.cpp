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

#include "traps/TrapTrigger.h"

#include "entities/Creature.h"
#include "entities/Tile.h"
#include "entities/TrapEntity.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "traps/TrapManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"

#include <set>

namespace
{
//! Tiles whose trap has been set off by the trigger chain currently running
std::set<Tile*> triggerChainTiles;
}

const std::string TrapTriggerName = "Trigger";
const std::string TrapTriggerNameDisplay = "Trigger trap";
const TrapType TrapTrigger::mTrapType = TrapType::trigger;

namespace
{
class TrapTriggerFactory : public TrapFactory
{
    TrapType getTrapType() const override
    { return TrapTrigger::mTrapType; }

    const std::string& getName() const override
    { return TrapTriggerName; }

    const std::string& getNameReadable() const override
    { return TrapTriggerNameDisplay; }

    int getCostPerTile() const override
    { return ConfigManager::getSingleton().getTrapConfigInt32("TriggerCostPerTile"); }

    const std::string& getMeshName() const override
    {
        static const std::string meshName = "TriggerTrap";
        return meshName;
    }

    void checkBuildTrap(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildTrapDefault(gameMap, TrapType::trigger, inputManager, inputCommand);
    }

    bool buildTrap(GameMap* gameMap, Player* player, ODPacket& packet) const override
    {
        std::vector<Tile*> tiles;
        if(!getTrapTilesDefault(tiles, gameMap, player, packet))
            return false;

        int32_t pricePerTarget = TrapManager::costPerTile(TrapType::trigger);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
            return false;

        TrapTrigger* trap = new TrapTrigger(gameMap);
        return buildTrapDefault(gameMap, trap, player->getSeat(), tiles);
    }

    void checkBuildTrapEditor(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildTrapDefaultEditor(gameMap, TrapType::trigger, inputManager, inputCommand);
    }

    bool buildTrapEditor(GameMap* gameMap, ODPacket& packet) const override
    {
        TrapTrigger* trap = new TrapTrigger(gameMap);
        return buildTrapDefaultEditor(gameMap, trap, packet);
    }

    Trap* getTrapFromStream(GameMap* gameMap, std::istream& is) const override
    {
        TrapTrigger* trap = new TrapTrigger(gameMap);
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
        int32_t pricePerTarget = TrapManager::costPerTile(TrapType::trigger);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!noFee)
            if(!gameMap->withdrawFromTreasuries(price, seatPtr))
                return false;

        TrapTrigger* trap = new TrapTrigger(gameMap);
        return buildTrapDefault(gameMap, trap, seatPtr, tiles);
    }
};

// Register the factory
static TrapRegister reg(new TrapTriggerFactory);
}

TrapTrigger::TrapTrigger(GameMap* gameMap) :
    Trap(gameMap)
{
    mReloadTime = ConfigManager::getSingleton().getTrapConfigUInt32("TriggerReloadTurns");
    mNbShootsBeforeDeactivation = ConfigManager::getSingleton().getTrapConfigUInt32("TriggerNbShootsBeforeDeactivation");
    setMeshName("");
}

bool TrapTrigger::shoot(Tile* tile)
{
    // Pressure trigger: the trap only fires when an enemy creature stands on its tile,
    // unless another trigger trap sets it off
    if(!mForcedTrigger)
    {
        std::vector<Tile*> triggerTiles;
        triggerTiles.push_back(tile);
        std::vector<GameEntity*> triggerCreatures = getGameMap()->getVisibleCreatures(triggerTiles, getSeat(), true);
        if(triggerCreatures.empty())
            return false;

        // A new chain starts with this trap
        triggerChainTiles.clear();
        triggerChainTiles.insert(tile);
    }

    bool fired = false;
    for(int dx = -1; dx <= 1; ++dx)
    {
        for(int dy = -1; dy <= 1; ++dy)
        {
            if((dx == 0) && (dy == 0))
                continue;

            Tile* neighbor = getGameMap()->getTile(tile->getX() + dx, tile->getY() + dy);
            if(neighbor == nullptr)
                continue;

            Trap* trap = neighbor->getCoveringTrap();
            if((trap == nullptr) || (trap->getSeat() != getSeat()))
                continue;

            // Doors only fire their weapon, if they have one
            if(trap->isDoor() && (trap->getType() != TrapType::doorRuned))
                continue;

            // Every trap is set off only once per chain, so trigger traps cannot loop
            if(!triggerChainTiles.insert(neighbor).second)
                continue;

            // The linked trap is highlighted for the players that see it, even if it cannot fire now
            trap->fireTrapEffect(TrapEffectKind::linked, neighbor, 1.0);
            if(trap->forceTrigger(neighbor))
                fired = true;
        }
    }

    return fired;
}

TrapEntity* TrapTrigger::getTrapEntity(Tile* tile)
{
    return new TrapEntity(getGameMap(), *this, reg.getTrapFactory()->getMeshName(), tile, 0.0, true, isActivated(tile) ? 1.0f : 0.7f);
}
