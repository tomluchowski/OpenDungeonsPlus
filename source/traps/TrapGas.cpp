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

#include "traps/TrapGas.h"

#include "entities/Creature.h"
#include "entities/Tile.h"
#include "entities/TrapEntity.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "traps/TrapManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"
#include "utils/Random.h"

const std::string TrapGasName = "Gas";
const std::string TrapGasNameDisplay = "Gas trap";
const TrapType TrapGas::mTrapType = TrapType::gas;

namespace
{
class TrapGasFactory : public TrapFactory
{
    TrapType getTrapType() const override
    { return TrapGas::mTrapType; }

    const std::string& getName() const override
    { return TrapGasName; }

    const std::string& getNameReadable() const override
    { return TrapGasNameDisplay; }

    int getCostPerTile() const override
    { return ConfigManager::getSingleton().getTrapConfigInt32("GasCostPerTile"); }

    const std::string& getMeshName() const override
    {
        static const std::string meshName = "Spiketrap";
        return meshName;
    }

    void checkBuildTrap(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildTrapDefault(gameMap, TrapType::gas, inputManager, inputCommand);
    }

    bool buildTrap(GameMap* gameMap, Player* player, ODPacket& packet) const override
    {
        std::vector<Tile*> tiles;
        if(!getTrapTilesDefault(tiles, gameMap, player, packet))
            return false;

        int32_t pricePerTarget = TrapManager::costPerTile(TrapType::gas);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
            return false;

        TrapGas* trap = new TrapGas(gameMap);
        return buildTrapDefault(gameMap, trap, player->getSeat(), tiles);
    }

    void checkBuildTrapEditor(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildTrapDefaultEditor(gameMap, TrapType::gas, inputManager, inputCommand);
    }

    bool buildTrapEditor(GameMap* gameMap, ODPacket& packet) const override
    {
        TrapGas* trap = new TrapGas(gameMap);
        return buildTrapDefaultEditor(gameMap, trap, packet);
    }

    Trap* getTrapFromStream(GameMap* gameMap, std::istream& is) const override
    {
        TrapGas* trap = new TrapGas(gameMap);
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
        int32_t pricePerTarget = TrapManager::costPerTile(TrapType::gas);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!noFee)
            if(!gameMap->withdrawFromTreasuries(price, seatPtr))
                return false;

        TrapGas* trap = new TrapGas(gameMap);
        return buildTrapDefault(gameMap, trap, seatPtr, tiles);
    }
};

// Register the factory
static TrapRegister reg(new TrapGasFactory);
}

TrapGas::TrapGas(GameMap* gameMap) :
    Trap(gameMap)
{
    mReloadTime = ConfigManager::getSingleton().getTrapConfigUInt32("GasReloadTurns");
    mMinDamage = ConfigManager::getSingleton().getTrapConfigDouble("GasDamagePerHitMin");
    mMaxDamage = ConfigManager::getSingleton().getTrapConfigDouble("GasDamagePerHitMax");
    mNbShootsBeforeDeactivation = ConfigManager::getSingleton().getTrapConfigUInt32("GasNbShootsBeforeDeactivation");
    mRadius = ConfigManager::getSingleton().getTrapConfigUInt32("GasRadius");
    setMeshName("");
}

bool TrapGas::shoot(Tile* tile)
{
    // Pressure trigger: the trap only fires when an enemy creature stands on its tile
    std::vector<Tile*> triggerTiles;
    triggerTiles.push_back(tile);
    std::vector<GameEntity*> triggerCreatures = getGameMap()->getVisibleCreatures(triggerTiles, getSeat(), true);
    if(triggerCreatures.empty())
        return false;

    // The cloud hurts every creature in the area, enemies and allies
    std::vector<Tile*> cloudTiles = getGameMap()->visibleTiles(tile->getX(), tile->getY(), static_cast<int>(mRadius));
    std::vector<GameEntity*> enemyCreatures = getGameMap()->getVisibleCreatures(cloudTiles, getSeat(), true);
    std::vector<GameEntity*> alliedCreatures = getGameMap()->getVisibleCreatures(cloudTiles, getSeat(), false);
    std::vector<GameEntity*> victims;
    victims.insert(victims.end(), enemyCreatures.begin(), enemyCreatures.end());
    victims.insert(victims.end(), alliedCreatures.begin(), alliedCreatures.end());

    for(GameEntity* target : victims)
    {
        Tile* targetTile = target->getCoveredTile(0);
        target->takeDamage(this, 0.0, 0.0, 0.0, Random::Double(mMinDamage, mMaxDamage), targetTile, false);
        target->notifyFightPlayer(targetTile);
    }

    return true;
}

TrapEntity* TrapGas::getTrapEntity(Tile* tile)
{
    return new TrapEntity(getGameMap(), *this, reg.getTrapFactory()->getMeshName(), tile, 0.0, true, isActivated(tile) ? 1.0f : 0.7f);
}
