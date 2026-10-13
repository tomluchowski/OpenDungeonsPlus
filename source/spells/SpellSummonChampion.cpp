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

#include "spells/SpellSummonChampion.h"

#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/Tile.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "modes/InputCommand.h"
#include "modes/InputManager.h"
#include "network/ODClient.h"
#include "spells/SpellManager.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

const std::string SpellSummonChampionName = "summonChampion";
const std::string SpellSummonChampionNameDisplay = "Summon champion";
const std::string SpellSummonChampionCooldownKey = "SummonChampionCooldown";
const std::string SpellSummonChampionCreatureClass = "Champion";
const SpellType SpellSummonChampion::mSpellType = SpellType::summonChampion;

namespace
{
class SpellSummonChampionFactory : public SpellFactory
{
    SpellType getSpellType() const override
    { return SpellSummonChampion::mSpellType; }

    const std::string& getName() const override
    { return SpellSummonChampionName; }

    const std::string& getCooldownKey() const override
    { return SpellSummonChampionCooldownKey; }

    const std::string& getNameReadable() const override
    { return SpellSummonChampionNameDisplay; }

    virtual void checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    { SpellSummonChampion::checkSpellCast(gameMap, inputManager, inputCommand); }

    virtual bool castSpell(GameMap* gameMap, Player* player, ODPacket& packet) const override
    { return SpellSummonChampion::castSpell(gameMap, player, packet); }

    Spell* getSpellFromStream(GameMap* gameMap, std::istream &is) const override
    { return SpellSummonChampion::getSpellFromStream(gameMap, is); }

    Spell* getSpellFromPacket(GameMap* gameMap, ODPacket &is) const override
    { return SpellSummonChampion::getSpellFromPacket(gameMap, is); }
};

// Register the factory
static SpellRegister reg(new SpellSummonChampionFactory);
}

bool SpellSummonChampion::hasChampion(GameMap* gameMap, const Seat* seat)
{
    for(Creature* creature : gameMap->getCreaturesBySeat(seat))
    {
        if(creature->getDefinition()->isChampion() && creature->isAlive())
            return true;
    }
    return false;
}

void SpellSummonChampion::checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand)
{
    Player* player = gameMap->getLocalPlayer();

    Tile* tile = gameMap->getTile(inputManager.mXPos, inputManager.mYPos);
    if(tile == nullptr)
        return;

    int32_t playerMana = static_cast<int32_t>(player->getSeat()->getMana());
    int32_t price = ConfigManager::getSingleton().getSpellConfigInt32("SummonChampionPrice");
    bool isTileValid = (!tile->isFullTile() && tile->isClaimedForSeat(player->getSeat()) &&
        !hasChampion(gameMap, player->getSeat()));
    if(inputManager.mCommandState == InputCommandState::infoOnly)
    {
        std::string txt = formatCastSpell(SpellType::summonChampion, price);
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
        std::string txt = formatCastSpell(SpellType::summonChampion, price);
        inputCommand.displayText(Ogre::ColourValue::White, txt);
        std::vector<Tile*> tiles;
        tiles.push_back(tile);
        inputCommand.selectTiles(tiles);
        return;
    }

    inputCommand.unselectAllTiles();

    if(!isTileValid)
        return;

    ClientNotification *clientNotification = SpellManager::createSpellClientNotification(SpellType::summonChampion);
    gameMap->tileToPacket(clientNotification->mPacket, tile);

    ODClient::getSingleton().queueClientNotification(clientNotification);
}

bool SpellSummonChampion::castSpell(GameMap* gameMap, Player* player, ODPacket& packet)
{
    Tile* tile = gameMap->tileFromPacket(packet);
    if(tile == nullptr)
        return false;

    if(tile->isFullTile() || !tile->isClaimedForSeat(player->getSeat()))
        return false;

    if(hasChampion(gameMap, player->getSeat()))
        return false;

    const CreatureDefinition* definition = ConfigManager::getSingleton().getCreatureDefinition(SpellSummonChampionCreatureClass);
    if(definition == nullptr)
    {
        OD_LOG_ERR("No creature definition for the champion, class=" + SpellSummonChampionCreatureClass);
        return false;
    }

    int32_t price = ConfigManager::getSingleton().getSpellConfigInt32("SummonChampionPrice");
    if(!player->getSeat()->takeMana(price))
        return false;

    Creature* champion = new Creature(gameMap, definition, player->getSeat());
    OD_LOG_INF("Summoning the champion, name=" + champion->getName() + ", seatId=" + Helper::toString(player->getSeat()->getId()));
    champion->addToGameMap();
    Ogre::Vector3 spawnPosition(static_cast<Ogre::Real>(tile->getX()),
                                static_cast<Ogre::Real>(tile->getY()),
                                static_cast<Ogre::Real>(0.0));
    champion->addParticleEffect("SummonWorker", 3);
    champion->createMesh();
    champion->setPosition(spawnPosition);

    return true;
}

Spell* SpellSummonChampion::getSpellFromStream(GameMap* gameMap, std::istream &is)
{
    OD_LOG_ERR("SpellSummonChampion cannot be read from stream");
    return nullptr;
}

Spell* SpellSummonChampion::getSpellFromPacket(GameMap* gameMap, ODPacket &is)
{
    OD_LOG_ERR("SpellSummonChampion cannot be read from packet");
    return nullptr;
}
