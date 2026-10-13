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

#include "spells/SpellCreateGold.h"

#include "entities/Tile.h"
#include "entities/TreasuryObject.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "modes/InputCommand.h"
#include "modes/InputManager.h"
#include "network/ODClient.h"
#include "spells/SpellType.h"
#include "spells/SpellManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"

const std::string SpellCreateGoldName = "createGold";
const std::string SpellCreateGoldNameDisplay = "Create gold";
const std::string SpellCreateGoldCooldownKey = "CreateGoldCooldown";
const SpellType SpellCreateGold::mSpellType = SpellType::createGold;

namespace
{
class SpellCreateGoldFactory : public SpellFactory
{
    SpellType getSpellType() const override
    { return SpellCreateGold::mSpellType; }

    const std::string& getName() const override
    { return SpellCreateGoldName; }

    const std::string& getCooldownKey() const override
    { return SpellCreateGoldCooldownKey; }

    const std::string& getNameReadable() const override
    { return SpellCreateGoldNameDisplay; }

    virtual void checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    { SpellCreateGold::checkSpellCast(gameMap, inputManager, inputCommand); }

    virtual bool castSpell(GameMap* gameMap, Player* player, ODPacket& packet) const override
    { return SpellCreateGold::castSpell(gameMap, player, packet); }

    Spell* getSpellFromStream(GameMap* gameMap, std::istream &is) const override
    { return SpellCreateGold::getSpellFromStream(gameMap, is); }

    Spell* getSpellFromPacket(GameMap* gameMap, ODPacket &is) const override
    { return SpellCreateGold::getSpellFromPacket(gameMap, is); }
};

// Register the factory
static SpellRegister reg(new SpellCreateGoldFactory);
}

void SpellCreateGold::checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand)
{
    Player* player = gameMap->getLocalPlayer();

    Tile* tile = gameMap->getTile(inputManager.mXPos, inputManager.mYPos);
    if(tile == nullptr)
        return;

    int32_t playerMana = static_cast<int32_t>(player->getSeat()->getMana());
    int32_t price = ConfigManager::getSingleton().getSpellConfigInt32("CreateGoldPrice");
    bool isTileValid = (!tile->isFullTile() && tile->isClaimedForSeat(player->getSeat()));
    if(inputManager.mCommandState == InputCommandState::infoOnly)
    {
        std::string txt = formatCastSpell(SpellType::createGold, price);
        if((playerMana < price) || !isTileValid)
            inputCommand.displayText(Ogre::ColourValue::Red, txt);
        else
            inputCommand.displayText(Ogre::ColourValue::White, txt);

        inputCommand.selectSquaredTiles(inputManager.mXPos, inputManager.mYPos, inputManager.mXPos,
            inputManager.mYPos);
        return;
    }

    if(inputManager.mCommandState == InputCommandState::building)
    {
        std::string txt = formatCastSpell(SpellType::createGold, price);
        inputCommand.displayText(Ogre::ColourValue::White, txt);
        std::vector<Tile*> tiles;
        tiles.push_back(tile);
        inputCommand.selectTiles(tiles);
        return;
    }

    inputCommand.unselectAllTiles();

    if(!isTileValid)
        return;

    ClientNotification *clientNotification = SpellManager::createSpellClientNotification(SpellType::createGold);
    gameMap->tileToPacket(clientNotification->mPacket, tile);

    ODClient::getSingleton().queueClientNotification(clientNotification);
}

bool SpellCreateGold::castSpell(GameMap* gameMap, Player* player, ODPacket& packet)
{
    Tile* tile = gameMap->tileFromPacket(packet);
    if(tile == nullptr)
        return false;

    // The pile can only be picked up on claimed land of the caster
    if(tile->isFullTile() || !tile->isClaimedForSeat(player->getSeat()))
        return false;

    int32_t playerMana = static_cast<int32_t>(player->getSeat()->getMana());
    int32_t manaCost = ConfigManager::getSingleton().getSpellConfigInt32("CreateGoldPrice");
    if(playerMana < manaCost)
        return false;

    if(!player->getSeat()->takeMana(manaCost))
        return false;

    int32_t goldValue = ConfigManager::getSingleton().getSpellConfigInt32("CreateGoldValue");
    TreasuryObject* obj = new TreasuryObject(gameMap, goldValue);
    obj->addToGameMap();
    Ogre::Vector3 spawnPosition(static_cast<Ogre::Real>(tile->getX()),
                                static_cast<Ogre::Real>(tile->getY()),
                                static_cast<Ogre::Real>(0.0));
    obj->createMesh();
    obj->setPosition(spawnPosition);

    return true;
}

Spell* SpellCreateGold::getSpellFromStream(GameMap* gameMap, std::istream &is)
{
    OD_LOG_ERR("SpellCreateGold cannot be read from stream");
    return nullptr;
}

Spell* SpellCreateGold::getSpellFromPacket(GameMap* gameMap, ODPacket &is)
{
    OD_LOG_ERR("SpellCreateGold cannot be read from packet");
    return nullptr;
}
