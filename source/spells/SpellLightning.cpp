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

#include "spells/SpellLightning.h"

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
#include "spells/SpellType.h"
#include "spells/SpellManager.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

const std::string SpellLightningName = "lightning";
const std::string SpellLightningNameDisplay = "Lightning";
const std::string SpellLightningCooldownKey = "LightningCooldown";
const SpellType SpellLightning::mSpellType = SpellType::lightning;

namespace
{
class SpellLightningFactory : public SpellFactory
{
    SpellType getSpellType() const override
    { return SpellLightning::mSpellType; }

    const std::string& getName() const override
    { return SpellLightningName; }

    const std::string& getCooldownKey() const override
    { return SpellLightningCooldownKey; }

    const std::string& getNameReadable() const override
    { return SpellLightningNameDisplay; }

    virtual void checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    { SpellLightning::checkSpellCast(gameMap, inputManager, inputCommand); }

    virtual bool castSpell(GameMap* gameMap, Player* player, ODPacket& packet) const override
    { return SpellLightning::castSpell(gameMap, player, packet); }

    Spell* getSpellFromStream(GameMap* gameMap, std::istream &is) const override
    { return SpellLightning::getSpellFromStream(gameMap, is); }

    Spell* getSpellFromPacket(GameMap* gameMap, ODPacket &is) const override
    { return SpellLightning::getSpellFromPacket(gameMap, is); }
};

// Register the factory
static SpellRegister reg(new SpellLightningFactory);
}

void SpellLightning::checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand)
{
    Player* player = gameMap->getLocalPlayer();
    int32_t price = ConfigManager::getSingleton().getSpellConfigInt32("LightningPrice");
    int32_t playerMana = static_cast<int32_t>(player->getSeat()->getMana());
    if(inputManager.mCommandState == InputCommandState::infoOnly)
    {
        std::string txt = formatCastSpell(SpellType::lightning, price);
        if(playerMana < price)
            inputCommand.displayText(Ogre::ColourValue::Red, txt);
        else
            inputCommand.displayText(Ogre::ColourValue::White, txt);

        inputCommand.selectSquaredTiles(inputManager.mXPos, inputManager.mYPos, inputManager.mXPos,
            inputManager.mYPos);
        return;
    }

    Tile* tileSelected = gameMap->getTile(inputManager.mXPos, inputManager.mYPos);
    if(tileSelected == nullptr)
        return;

    if(inputManager.mCommandState == InputCommandState::building)
    {
        inputCommand.selectSquaredTiles(inputManager.mXPos, inputManager.mYPos, inputManager.mXPos,
            inputManager.mYPos);
    }

    // We search the closest enemy creature alive
    Creature* closestCreature = tileSelected->getClosestCreature(SelectionEntityWanted::creatureAliveEnemy);
    if(closestCreature == nullptr)
    {
        std::string txt = formatCastSpell(SpellType::lightning, 0);
        inputCommand.displayText(Ogre::ColourValue::White, txt);
        return;
    }

    // The target has to be on land claimed by the caster
    Tile* creatureTile = closestCreature->getPositionTile();
    if((creatureTile == nullptr) || !creatureTile->isClaimedForSeat(player->getSeat()))
    {
        std::string txt = formatCastSpell(SpellType::lightning, 0);
        inputCommand.displayText(Ogre::ColourValue::White, txt);
        return;
    }

    std::string txt = formatCastSpell(SpellType::lightning, price);
    inputCommand.displayText(Ogre::ColourValue::White, txt);

    if(inputManager.mCommandState != InputCommandState::validated)
        return;

    inputCommand.unselectAllTiles();

    ClientNotification *clientNotification = SpellManager::createSpellClientNotification(SpellType::lightning);
    clientNotification->mPacket << closestCreature->getName();
    ODClient::getSingleton().queueClientNotification(clientNotification);
}

bool SpellLightning::castSpell(GameMap* gameMap, Player* player, ODPacket& packet)
{
    std::string creatureName;
    OD_ASSERT_TRUE(packet >> creatureName);

    // We check that the creature is a valid target
    Creature* creature = gameMap->getCreature(creatureName);
    if(creature == nullptr)
    {
        OD_LOG_ERR("creatureName=" + creatureName);
        return false;
    }

    if(creature->getSeat()->isAlliedSeat(player->getSeat()))
    {
        OD_LOG_ERR("creatureName=" + creatureName);
        return false;
    }

    Tile* pos = creature->getPositionTile();
    if(pos == nullptr)
    {
        OD_LOG_ERR("creatureName=" + creatureName);
        return false;
    }

    if(!creature->isAlive())
    {
        // This can happen if the creature was alive on client side but is not since we received the message
        OD_LOG_WRN("creatureName=" + creatureName);
        return false;
    }

    // That can happen if the creature is not in perfect synchronization and is not on a claimed tile on the server gamemap
    if(!pos->isClaimedForSeat(player->getSeat()))
    {
        OD_LOG_WRN("Creature=" + creatureName + ", tile=" + Tile::displayAsString(pos));
        return false;
    }

    int32_t price = ConfigManager::getSingleton().getSpellConfigInt32("LightningPrice");
    if(!player->getSeat()->takeMana(price))
        return false;

    double damage = ConfigManager::getSingleton().getSpellConfigDouble("LightningDamage");
    creature->takeDamage(nullptr, damage, 0.0, 0.0, 0.0, pos, false);
    creature->stun(creature->getDefinition()->getTurnsStunDropped());

    return true;
}

Spell* SpellLightning::getSpellFromStream(GameMap* gameMap, std::istream &is)
{
    OD_LOG_ERR("SpellLightning cannot be read from stream");
    return nullptr;
}

Spell* SpellLightning::getSpellFromPacket(GameMap* gameMap, ODPacket &is)
{
    OD_LOG_ERR("SpellLightning cannot be read from packet");
    return nullptr;
}
