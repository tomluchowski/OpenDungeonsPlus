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

#include "spells/SpellPossess.h"

#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "gamemap/Pathfinding.h"
#include "modes/InputCommand.h"
#include "modes/InputManager.h"
#include "network/ClientNotification.h"
#include "network/ODClient.h"
#include "network/ODPacket.h"
#include "spells/SpellManager.h"
#include "spells/SpellType.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

const std::string SpellPossessName = "possess";
const std::string SpellPossessNameDisplay = "Possess";
const std::string SpellPossessCooldownKey = "PossessCooldown";
const SpellType SpellPossess::mSpellType = SpellType::possess;

namespace
{
class SpellPossessFactory : public SpellFactory
{
    SpellType getSpellType() const override
    { return SpellPossess::mSpellType; }

    const std::string& getName() const override
    { return SpellPossessName; }

    const std::string& getCooldownKey() const override
    { return SpellPossessCooldownKey; }

    const std::string& getNameReadable() const override
    { return SpellPossessNameDisplay; }

    virtual void checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    { SpellPossess::checkSpellCast(gameMap, inputManager, inputCommand); }

    virtual bool castSpell(GameMap* gameMap, Player* player, ODPacket& packet) const override
    { return SpellPossess::castSpell(gameMap, player, packet); }

    Spell* getSpellFromStream(GameMap* gameMap, std::istream &is) const override
    { return SpellPossess::getSpellFromStream(gameMap, is); }

    Spell* getSpellFromPacket(GameMap* gameMap, ODPacket &is) const override
    { return SpellPossess::getSpellFromPacket(gameMap, is); }
};

// Register the factory
static SpellRegister reg(new SpellPossessFactory);
}

void SpellPossess::checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand)
{
    Player* player = gameMap->getLocalPlayer();
    int32_t price = ConfigManager::getSingleton().getSpellConfigInt32("PossessPrice");
    int32_t playerMana = static_cast<int32_t>(player->getSeat()->getMana());

    inputCommand.selectSquaredTiles(inputManager.mXPos, inputManager.mYPos, inputManager.mXPos,
        inputManager.mYPos);

    if(player->isPossessing())
    {
        inputCommand.displayText(Ogre::ColourValue::Red, "Already possessing a creature");
        return;
    }

    if(inputManager.mCommandState == InputCommandState::infoOnly)
    {
        std::string txt = formatCastSpell(SpellType::possess, price);
        if(playerMana < price)
            inputCommand.displayText(Ogre::ColourValue::Red, txt);
        else
            inputCommand.displayText(Ogre::ColourValue::White, txt);
        return;
    }

    std::vector<GameEntity*> targets;
    gameMap->playerSelects(targets, inputManager.mXPos, inputManager.mYPos, inputManager.mXPos, inputManager.mYPos,
        SelectionTileAllowed::groundTiles, SelectionEntityWanted::creatureAliveOwned, player);

    // We pick the creature the closest to the mouse
    Creature* target = nullptr;
    double targetDist = 0.0;
    for(GameEntity* entity : targets)
    {
        if(entity->getObjectType() != GameEntityType::creature)
            continue;

        Creature* creature = static_cast<Creature*>(entity);
        if(creature->isKo() || creature->getDefinition()->isChampion())
            continue;

        const Ogre::Vector3& pos = creature->getPosition();
        double dist = Pathfinding::squaredDistance(pos.x, inputManager.mKeeperHandPos.x, pos.y, inputManager.mKeeperHandPos.y);
        if((target != nullptr) && (dist >= targetDist))
            continue;

        target = creature;
        targetDist = dist;
    }

    if(target == nullptr)
    {
        std::string txt = formatCastSpell(SpellType::possess, 0);
        inputCommand.displayText(Ogre::ColourValue::White, txt);
        return;
    }

    std::string txt = formatCastSpell(SpellType::possess, price);
    inputCommand.displayText((playerMana < price) ? Ogre::ColourValue::Red : Ogre::ColourValue::White, txt);

    if(inputManager.mCommandState != InputCommandState::validated)
        return;

    inputCommand.unselectAllTiles();

    ClientNotification *clientNotification = SpellManager::createSpellClientNotification(SpellType::possess);
    clientNotification->mPacket << target->getName();
    ODClient::getSingleton().queueClientNotification(clientNotification);
}

bool SpellPossess::castSpell(GameMap* gameMap, Player* player, ODPacket& packet)
{
    std::string creatureName;
    OD_ASSERT_TRUE(packet >> creatureName);

    if(player->isPossessing())
    {
        OD_LOG_WRN("player " + player->getNick() + " tries to possess " + creatureName + " while possessing "
            + player->getPossessedCreatureName());
        return false;
    }

    Creature* creature = gameMap->getCreature(creatureName);
    if(creature == nullptr)
    {
        OD_LOG_ERR("creatureName=" + creatureName);
        return false;
    }

    // That can happen if the creature is not in perfect synchronization between client and server
    if(!creature->isAlive() || creature->isKo() || !creature->getIsOnMap() || creature->isInPrison() ||
       creature->isPossessed() || creature->getDefinition()->isChampion() || (creature->getSeat() != player->getSeat()))
    {
        OD_LOG_INF("WARNING : " + creatureName + " cannot be possessed by " + player->getNick());
        return false;
    }

    int32_t price = ConfigManager::getSingleton().getSpellConfigInt32("PossessPrice");
    if(!player->getSeat()->takeMana(price))
        return false;

    creature->startPossession(*player);
    return true;
}

Spell* SpellPossess::getSpellFromStream(GameMap* gameMap, std::istream &is)
{
    OD_LOG_ERR("SpellPossess cannot be read from stream");
    return nullptr;
}

Spell* SpellPossess::getSpellFromPacket(GameMap* gameMap, ODPacket &is)
{
    OD_LOG_ERR("SpellPossess cannot be read from packet");
    return nullptr;
}
