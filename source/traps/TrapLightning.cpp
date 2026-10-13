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

#include "traps/TrapLightning.h"

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

const std::string TrapLightningName = "Lightning";
const std::string TrapLightningNameDisplay = "Lightning trap";
const TrapType TrapLightning::mTrapType = TrapType::lightning;

namespace
{
class TrapLightningFactory : public TrapFactory
{
    TrapType getTrapType() const override
    { return TrapLightning::mTrapType; }

    const std::string& getName() const override
    { return TrapLightningName; }

    const std::string& getNameReadable() const override
    { return TrapLightningNameDisplay; }

    int getCostPerTile() const override
    { return ConfigManager::getSingleton().getTrapConfigInt32("LightningCostPerTile"); }

    const std::string& getMeshName() const override
    {
        static const std::string meshName = "LightningTrap";
        return meshName;
    }

    void checkBuildTrap(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildTrapDefault(gameMap, TrapType::lightning, inputManager, inputCommand);
    }

    bool buildTrap(GameMap* gameMap, Player* player, ODPacket& packet) const override
    {
        std::vector<Tile*> tiles;
        if(!getTrapTilesDefault(tiles, gameMap, player, packet))
            return false;

        int32_t pricePerTarget = TrapManager::costPerTile(TrapType::lightning);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
            return false;

        TrapLightning* trap = new TrapLightning(gameMap);
        return buildTrapDefault(gameMap, trap, player->getSeat(), tiles);
    }

    void checkBuildTrapEditor(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildTrapDefaultEditor(gameMap, TrapType::lightning, inputManager, inputCommand);
    }

    bool buildTrapEditor(GameMap* gameMap, ODPacket& packet) const override
    {
        TrapLightning* trap = new TrapLightning(gameMap);
        return buildTrapDefaultEditor(gameMap, trap, packet);
    }

    Trap* getTrapFromStream(GameMap* gameMap, std::istream& is) const override
    {
        TrapLightning* trap = new TrapLightning(gameMap);
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
        int32_t pricePerTarget = TrapManager::costPerTile(TrapType::lightning);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!noFee)
            if(!gameMap->withdrawFromTreasuries(price, seatPtr))
                return false;

        TrapLightning* trap = new TrapLightning(gameMap);
        return buildTrapDefault(gameMap, trap, seatPtr, tiles);
    }
};

// Register the factory
static TrapRegister reg(new TrapLightningFactory);
}

TrapLightning::TrapLightning(GameMap* gameMap) :
    Trap(gameMap)
{
    mReloadTime = ConfigManager::getSingleton().getTrapConfigUInt32("LightningReloadTurns");
    mMinDamage = ConfigManager::getSingleton().getTrapConfigDouble("LightningDamagePerHitMin");
    mMaxDamage = ConfigManager::getSingleton().getTrapConfigDouble("LightningDamagePerHitMax");
    mNbShootsBeforeDeactivation = ConfigManager::getSingleton().getTrapConfigUInt32("LightningNbShootsBeforeDeactivation");
    mRange = ConfigManager::getSingleton().getTrapConfigUInt32("LightningRange");
    mStunTurns = ConfigManager::getSingleton().getTrapConfigUInt32("LightningStunTurns");
    setMeshName("");
}

bool TrapLightning::shoot(Tile* tile)
{
    std::vector<Tile*> visibleTiles = getGameMap()->visibleTiles(tile->getX(), tile->getY(), static_cast<int>(mRange));
    std::vector<GameEntity*> enemyCreatures = getGameMap()->getVisibleCreatures(visibleTiles, getSeat(), true);
    if(enemyCreatures.empty())
        return false;

    // The bolt hits one enemy creature in sight
    GameEntity* target = enemyCreatures[Random::Uint(0, enemyCreatures.size() - 1)];
    Tile* targetTile = target->getCoveredTile(0);

    // Water conducts the bolt: the damage doubles for a target standing in water
    double damage = Random::Double(mMinDamage, mMaxDamage);
    if((targetTile != nullptr) && (targetTile->getType() == TileType::water))
        damage *= 2.0;

    target->takeDamage(this, 0.0, 0.0, 0.0, damage, targetTile, false);
    target->notifyFightPlayer(targetTile);

    // The bolt also stuns the creature for a short time
    Creature* creature = dynamic_cast<Creature*>(target);
    if(creature != nullptr)
        creature->stunForTurns(static_cast<int32_t>(mStunTurns));

    return true;
}

TrapEntity* TrapLightning::getTrapEntity(Tile* tile)
{
    return new TrapEntity(getGameMap(), *this, reg.getTrapFactory()->getMeshName(), tile, 0.0, true, isActivated(tile) ? 1.0f : 0.7f);
}
