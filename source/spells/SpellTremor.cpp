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

#include "spells/SpellTremor.h"

#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "modes/InputCommand.h"
#include "modes/InputManager.h"
#include "network/ODClient.h"
#include "spells/SpellArea.h"
#include "spells/SpellType.h"
#include "spells/SpellManager.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

const std::string SpellTremorName = "tremor";
const std::string SpellTremorNameDisplay = "Tremor";
const std::string SpellTremorCooldownKey = "TremorCooldown";
const SpellType SpellTremor::mSpellType = SpellType::tremor;

namespace
{
class SpellTremorFactory : public SpellFactory
{
    SpellType getSpellType() const override
    { return SpellTremor::mSpellType; }

    const std::string& getName() const override
    { return SpellTremorName; }

    const std::string& getCooldownKey() const override
    { return SpellTremorCooldownKey; }

    const std::string& getNameReadable() const override
    { return SpellTremorNameDisplay; }

    virtual void checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    { SpellTremor::checkSpellCast(gameMap, inputManager, inputCommand); }

    virtual bool castSpell(GameMap* gameMap, Player* player, ODPacket& packet) const override
    { return SpellTremor::castSpell(gameMap, player, packet); }

    Spell* getSpellFromStream(GameMap* gameMap, std::istream &is) const override
    { return SpellTremor::getSpellFromStream(gameMap, is); }

    Spell* getSpellFromPacket(GameMap* gameMap, ODPacket &is) const override
    { return SpellTremor::getSpellFromPacket(gameMap, is); }
};

// Register the factory
static SpellRegister reg(new SpellTremorFactory);
}

void SpellTremor::checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand)
{
    Player* player = gameMap->getLocalPlayer();
    int32_t price = ConfigManager::getSingleton().getSpellConfigInt32("TremorPrice");
    int radius = static_cast<int>(ConfigManager::getSingleton().getSpellConfigUInt32("TremorRadiusTiles"));
    int32_t playerMana = static_cast<int32_t>(player->getSeat()->getMana());

    Tile* tileSelected = gameMap->getTile(inputManager.mXPos, inputManager.mYPos);
    if(tileSelected == nullptr)
        return;

    std::vector<Tile*> tiles = SpellArea::getTilesInRadius(gameMap, inputManager.mXPos, inputManager.mYPos, radius);
    inputCommand.selectTiles(tiles);

    std::string txt = formatCastSpell(SpellType::tremor, price);
    if(playerMana < price)
        inputCommand.displayText(Ogre::ColourValue::Red, txt);
    else
        inputCommand.displayText(Ogre::ColourValue::White, txt);

    if(inputManager.mCommandState != InputCommandState::validated)
        return;

    inputCommand.unselectAllTiles();

    ClientNotification *clientNotification = SpellManager::createSpellClientNotification(SpellType::tremor);
    clientNotification->mPacket << inputManager.mXPos << inputManager.mYPos;
    ODClient::getSingleton().queueClientNotification(clientNotification);
}

bool SpellTremor::castSpell(GameMap* gameMap, Player* player, ODPacket& packet)
{
    int x;
    int y;
    OD_ASSERT_TRUE(packet >> x >> y);

    Tile* tileTarget = gameMap->getTile(x, y);
    if(tileTarget == nullptr)
    {
        OD_LOG_ERR("x=" + Helper::toString(x) + ", y=" + Helper::toString(y));
        return false;
    }

    int32_t price = ConfigManager::getSingleton().getSpellConfigInt32("TremorPrice");
    if(!player->getSeat()->takeMana(price))
        return false;

    fireSpellEffect(*tileTarget, "Tremor", "");

    int radius = static_cast<int>(ConfigManager::getSingleton().getSpellConfigUInt32("TremorRadiusTiles"));
    double damage = ConfigManager::getSingleton().getSpellConfigDouble("TremorDamage");
    int32_t nbTurns = static_cast<int32_t>(ConfigManager::getSingleton().getSpellConfigUInt32("TremorNbTurns"));

    // Enemy creatures in the area are hurt and knocked down. We copy the list because
    // a creature killed by the damage is removed from the game map.
    std::vector<Creature*> creatures = gameMap->getCreatures();
    for(Creature* creature : creatures)
    {
        if(!creature->isAlive())
            continue;

        if(creature->getSeat()->isAlliedSeat(player->getSeat()))
            continue;

        Tile* pos = creature->getPositionTile();
        if((pos == nullptr) || !SpellArea::isTileInRadius(pos, x, y, radius))
            continue;

        creature->takeDamage(nullptr, damage, 0.0, 0.0, 0.0, pos, false);
        creature->stun(nbTurns);
    }

    // Claimed enemy ground and walls turn back to dirt. Rooms, traps, doors and bridges are not affected.
    std::vector<Tile*> tiles = SpellArea::getTilesInRadius(gameMap, x, y, radius);
    for(Tile* tile : tiles)
    {
        if(!tile->isClaimed())
            continue;

        if(tile->getSeat()->isAlliedSeat(player->getSeat()))
            continue;

        if((tile->getCoveringBuilding() != nullptr) || tile->getHasBridge())
            continue;

        // We notify the previous owner that the tile is lost, like when it is claimed by an enemy
        tile->getSeat()->notifyTileClaimedByEnemy(tile);
        tile->unclaimTile();
    }

    return true;
}

Spell* SpellTremor::getSpellFromStream(GameMap* gameMap, std::istream &is)
{
    OD_LOG_ERR("SpellTremor cannot be read from stream");
    return nullptr;
}

Spell* SpellTremor::getSpellFromPacket(GameMap* gameMap, ODPacket &is)
{
    OD_LOG_ERR("SpellTremor cannot be read from packet");
    return nullptr;
}
