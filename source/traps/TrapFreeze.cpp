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

#include "traps/TrapFreeze.h"

#include "creatureeffect/CreatureEffectFrozen.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "entities/TrapEntity.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "traps/TrapManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"

const std::string TrapFreezeName = "Freeze";
const std::string TrapFreezeNameDisplay = "Freeze trap";
const TrapType TrapFreeze::mTrapType = TrapType::freeze;

namespace
{
class TrapFreezeFactory : public TrapFactory
{
    TrapType getTrapType() const override
    { return TrapFreeze::mTrapType; }

    const std::string& getName() const override
    { return TrapFreezeName; }

    const std::string& getNameReadable() const override
    { return TrapFreezeNameDisplay; }

    int getCostPerTile() const override
    { return ConfigManager::getSingleton().getTrapConfigInt32("FreezeCostPerTile"); }

    const std::string& getMeshName() const override
    {
        static const std::string meshName = "Spiketrap";
        return meshName;
    }

    void checkBuildTrap(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildTrapDefault(gameMap, TrapType::freeze, inputManager, inputCommand);
    }

    bool buildTrap(GameMap* gameMap, Player* player, ODPacket& packet) const override
    {
        std::vector<Tile*> tiles;
        if(!getTrapTilesDefault(tiles, gameMap, player, packet))
            return false;

        int32_t pricePerTarget = TrapManager::costPerTile(TrapType::freeze);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
            return false;

        TrapFreeze* trap = new TrapFreeze(gameMap);
        return buildTrapDefault(gameMap, trap, player->getSeat(), tiles);
    }

    void checkBuildTrapEditor(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildTrapDefaultEditor(gameMap, TrapType::freeze, inputManager, inputCommand);
    }

    bool buildTrapEditor(GameMap* gameMap, ODPacket& packet) const override
    {
        TrapFreeze* trap = new TrapFreeze(gameMap);
        return buildTrapDefaultEditor(gameMap, trap, packet);
    }

    Trap* getTrapFromStream(GameMap* gameMap, std::istream& is) const override
    {
        TrapFreeze* trap = new TrapFreeze(gameMap);
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
        int32_t pricePerTarget = TrapManager::costPerTile(TrapType::freeze);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!noFee)
            if(!gameMap->withdrawFromTreasuries(price, seatPtr))
                return false;

        TrapFreeze* trap = new TrapFreeze(gameMap);
        return buildTrapDefault(gameMap, trap, seatPtr, tiles);
    }
};

// Register the factory
static TrapRegister reg(new TrapFreezeFactory);
}

TrapFreeze::TrapFreeze(GameMap* gameMap) :
    Trap(gameMap)
{
    mReloadTime = ConfigManager::getSingleton().getTrapConfigUInt32("FreezeReloadTurns");
    mNbShootsBeforeDeactivation = ConfigManager::getSingleton().getTrapConfigUInt32("FreezeNbShootsBeforeDeactivation");
    mFreezeTurns = static_cast<int32_t>(ConfigManager::getSingleton().getTrapConfigUInt32("FreezeDurationTurns"));
    mShatterHpPercent = ConfigManager::getSingleton().getTrapConfigDouble("FreezeShatterHpPercent");
    setMeshName("");
}

bool TrapFreeze::shoot(Tile* tile)
{
    // Pressure trigger: the trap only fires when an enemy creature stands on its tile
    std::vector<Tile*> triggerTiles;
    triggerTiles.push_back(tile);
    std::vector<GameEntity*> triggerCreatures = getGameMap()->getVisibleCreatures(triggerTiles, getSeat(), true);

    bool fired = false;
    for(GameEntity* target : triggerCreatures)
    {
        Creature* creature = dynamic_cast<Creature*>(target);
        if((creature == nullptr) || !creature->isAlive() || creature->isFrozen())
            continue;

        fired = true;
        Tile* targetTile = target->getCoveredTile(0);
        if(creature->getHP() <= creature->getMaxHp() * mShatterHpPercent / 100.0)
        {
            // A creature that is low on health shatters
            creature->takeDamage(this, creature->getHP(), 0.0, 0.0, 0.0, targetTile, false);
        }
        else
        {
            creature->addCreatureEffect(new CreatureEffectFrozen(mFreezeTurns));
        }
        creature->notifyFightPlayer(targetTile);
    }

    return fired;
}

TrapEntity* TrapFreeze::getTrapEntity(Tile* tile)
{
    return new TrapEntity(getGameMap(), *this, reg.getTrapFactory()->getMeshName(), tile, 0.0, true, isActivated(tile) ? 1.0f : 0.7f);
}
