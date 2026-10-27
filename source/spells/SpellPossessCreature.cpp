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

#include "spells/SpellPossessCreature.h"

#include "entities/Creature.h"
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

const std::string SpellPossessCreatureName = "possessCreature";
const std::string SpellPossessCreatureNameDisplay = "Possess creature";
const std::string SpellPossessCreatureCooldownKey = "PossessCreatureCooldown";
const SpellType SpellPossessCreature::mSpellType = SpellType::possessCreature;

namespace
{
class SpellPossessCreatureFactory : public SpellFactory
{
    SpellType getSpellType() const override
    { return SpellPossessCreature::mSpellType; }

    const std::string& getName() const override
    { return SpellPossessCreatureName; }

    const std::string& getCooldownKey() const override
    { return SpellPossessCreatureCooldownKey; }

    const std::string& getNameReadable() const override
    { return SpellPossessCreatureNameDisplay; }

    virtual void checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    { SpellPossessCreature::checkSpellCast(gameMap, inputManager, inputCommand); }

    virtual bool castSpell(GameMap* gameMap, Player* player, ODPacket& packet) const override
    { return SpellPossessCreature::castSpell(gameMap, player, packet); }

    Spell* getSpellFromStream(GameMap* gameMap, std::istream &is) const override
    { return SpellPossessCreature::getSpellFromStream(gameMap, is); }

    Spell* getSpellFromPacket(GameMap* gameMap, ODPacket &is) const override
    { return SpellPossessCreature::getSpellFromPacket(gameMap, is); }
};

// Register the factory
static SpellRegister reg(new SpellPossessCreatureFactory);
}

void SpellPossessCreature::checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand)
{
    Player* player = gameMap->getLocalPlayer();
    int32_t price = ConfigManager::getSingleton().getSpellConfigInt32("PossessCreaturePrice");
    int32_t playerMana = static_cast<int32_t>(player->getSeat()->getMana());
    if(inputManager.mCommandState == InputCommandState::infoOnly)
    {
        std::string txt = formatCastSpell(SpellType::possessCreature, price);
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

    // Only one of the player's own creatures can be taken over, never an ally's
    Creature* closestCreature = tileSelected->getClosestCreature(SelectionEntityWanted::creatureAliveOwned);
    if(closestCreature == nullptr)
    {
        std::string txt = formatCastSpell(SpellType::possessCreature, 0);
        inputCommand.displayText(Ogre::ColourValue::White, txt);
        return;
    }

    std::string txt = formatCastSpell(SpellType::possessCreature, price);
    inputCommand.displayText(Ogre::ColourValue::White, txt);

    if(inputManager.mCommandState != InputCommandState::validated)
        return;

    inputCommand.unselectAllTiles();

    ClientNotification *clientNotification = SpellManager::createSpellClientNotification(SpellType::possessCreature);
    clientNotification->mPacket << closestCreature->getName();
    ODClient::getSingleton().queueClientNotification(clientNotification);
}

bool SpellPossessCreature::castSpell(GameMap* gameMap, Player* player, ODPacket& packet)
{
    std::string creatureName;
    OD_ASSERT_TRUE(packet >> creatureName);

    // Possession is a first person view on the screen of a human player: an AI keeper has
    // nothing to see through the creature's eyes
    if(!player->getIsHuman())
        return false;

    Creature* creature = gameMap->getCreature(creatureName);
    if(creature == nullptr)
    {
        OD_LOG_ERR("creatureName=" + creatureName);
        return false;
    }

    if(creature->getSeat() != player->getSeat())
    {
        OD_LOG_ERR("creatureName=" + creatureName);
        return false;
    }

    // The client may have seen the creature as it was a turn ago: it may have been picked up,
    // knocked out or killed since
    if(!creature->canBePossessed())
    {
        OD_LOG_WRN("Creature cannot be possessed creatureName=" + creatureName);
        return false;
    }

    if(player->getPossessedCreature() != nullptr)
    {
        OD_LOG_WRN("Player is already possessing creature=" + player->getPossessedCreature()->getName());
        return false;
    }

    int32_t price = ConfigManager::getSingleton().getSpellConfigInt32("PossessCreaturePrice");
    if(!player->getSeat()->takeMana(price))
        return false;

    creature->startPossession(*player);
    return true;
}

Spell* SpellPossessCreature::getSpellFromStream(GameMap* gameMap, std::istream &is)
{
    OD_LOG_ERR("SpellPossessCreature cannot be read from stream");
    return nullptr;
}

Spell* SpellPossessCreature::getSpellFromPacket(GameMap* gameMap, ODPacket &is)
{
    OD_LOG_ERR("SpellPossessCreature cannot be read from packet");
    return nullptr;
}
