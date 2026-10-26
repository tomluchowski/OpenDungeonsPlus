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

#include "spells/SpellSoftenRock.h"

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

const std::string SpellSoftenRockName = "softenRock";
const std::string SpellSoftenRockNameDisplay = "Soften rock";
const std::string SpellSoftenRockCooldownKey = "SoftenRockCooldown";
const SpellType SpellSoftenRock::mSpellType = SpellType::softenRock;

namespace
{
class SpellSoftenRockFactory : public SpellFactory
{
    SpellType getSpellType() const override
    { return SpellSoftenRock::mSpellType; }

    const std::string& getName() const override
    { return SpellSoftenRockName; }

    const std::string& getCooldownKey() const override
    { return SpellSoftenRockCooldownKey; }

    const std::string& getNameReadable() const override
    { return SpellSoftenRockNameDisplay; }

    virtual void checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    { SpellSoftenRock::checkSpellCast(gameMap, inputManager, inputCommand); }

    virtual bool castSpell(GameMap* gameMap, Player* player, ODPacket& packet) const override
    { return SpellSoftenRock::castSpell(gameMap, player, packet); }

    Spell* getSpellFromStream(GameMap* gameMap, std::istream &is) const override
    { return SpellSoftenRock::getSpellFromStream(gameMap, is); }

    Spell* getSpellFromPacket(GameMap* gameMap, ODPacket &is) const override
    { return SpellSoftenRock::getSpellFromPacket(gameMap, is); }
};

// Register the factory
static SpellRegister reg(new SpellSoftenRockFactory);
}

bool SpellSoftenRock::canSoftenTile(const GameMap* gameMap, const Tile* tile, const Seat* seat)
{
    // The client only knows what its tiles look like, so this is decided from the tile visual
    // on both sides. Only a full rock wall is undiggable: rock ground can already be walked on.
    if(tile->getTileVisual() != TileVisual::rockFull)
        return false;

    // The rock around the map holds everything in. Opening it would let creatures walk to the
    // edge of the world.
    if((tile->getX() <= 0) || (tile->getY() <= 0) ||
       (tile->getX() >= gameMap->getMapSizeX() - 1) || (tile->getY() >= gameMap->getMapSizeY() - 1))
    {
        return false;
    }

    // The rock has to touch the caster's territory, the way a worker can only claim a wall next
    // to claimed ground: the spell is for widening a dungeon, not for opening a way anywhere.
    for(const Tile* neighbor : tile->getAllNeighbors())
    {
        if(neighbor->isFullTile())
            continue;

        if(!neighbor->isClaimedForSeat(seat))
            continue;

        return true;
    }

    return false;
}

void SpellSoftenRock::checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand)
{
    Player* player = gameMap->getLocalPlayer();
    int32_t pricePerTile = ConfigManager::getSingleton().getSpellConfigInt32("SoftenRockPrice");
    int32_t playerMana = static_cast<int32_t>(player->getSeat()->getMana());
    if(inputManager.mCommandState == InputCommandState::infoOnly)
    {
        if(playerMana < pricePerTile)
        {
            std::string txt = formatCastSpell(SpellType::softenRock, pricePerTile);
            inputCommand.displayText(Ogre::ColourValue::Red, txt);
        }
        else
        {
            std::string txt = formatCastSpell(SpellType::softenRock, pricePerTile);
            inputCommand.displayText(Ogre::ColourValue::White, txt);
        }
        inputCommand.selectSquaredTiles(inputManager.mXPos, inputManager.mYPos, inputManager.mXPos,
            inputManager.mYPos);
        return;
    }

    // Rock walls are what this spell is cast on, and GameMap::playerSelects() only ever selects
    // ground, so the tiles are picked here.
    std::vector<Tile*> tilesRegion = gameMap->rectangularRegion(inputManager.mXPos, inputManager.mYPos,
        inputManager.mLStartDragX, inputManager.mLStartDragY);
    int32_t priceTotal = 0;
    std::vector<Tile*> tiles;
    for(Tile* tile : tilesRegion)
    {
        if(!canSoftenTile(gameMap, tile, player->getSeat()))
            continue;

        int32_t newPrice = priceTotal + pricePerTile;
        if(newPrice > playerMana)
            break;

        tiles.push_back(tile);
        priceTotal = newPrice;
    }

    if(inputManager.mCommandState == InputCommandState::building)
    {
        std::string txt = formatCastSpell(SpellType::softenRock, priceTotal);
        inputCommand.displayText(Ogre::ColourValue::White, txt);
        inputCommand.selectTiles(tiles);
        return;
    }

    inputCommand.unselectAllTiles();

    if(tiles.empty())
        return;

    ClientNotification *clientNotification = SpellManager::createSpellClientNotification(SpellType::softenRock);
    uint32_t nbTiles = tiles.size();
    clientNotification->mPacket << nbTiles;
    for(Tile* tile : tiles)
        gameMap->tileToPacket(clientNotification->mPacket, tile);

    ODClient::getSingleton().queueClientNotification(clientNotification);
}

bool SpellSoftenRock::castSpell(GameMap* gameMap, Player* player, ODPacket& packet)
{
    uint32_t nbTiles;
    OD_ASSERT_TRUE(packet >> nbTiles);
    std::vector<Tile*> tiles;
    while(nbTiles > 0)
    {
        --nbTiles;
        Tile* tile = gameMap->tileFromPacket(packet);
        if(tile == nullptr)
            return false;

        // The client decided from what it saw, which may be a turn old: the server checks again.
        if(!canSoftenTile(gameMap, tile, player->getSeat()))
        {
            OD_LOG_WRN("Cannot soften tile=" + Tile::displayAsString(tile) + " for seatId="
                + Helper::toString(player->getSeat()->getId()));
            continue;
        }

        tiles.push_back(tile);
    }

    if(tiles.empty())
        return false;

    int32_t pricePerTile = ConfigManager::getSingleton().getSpellConfigInt32("SoftenRockPrice");
    int32_t playerMana = static_cast<int32_t>(player->getSeat()->getMana());
    int32_t nbAffordable = (pricePerTile > 0) ? (playerMana / pricePerTile) : static_cast<int32_t>(tiles.size());
    if(nbAffordable <= 0)
        return false;

    if(static_cast<int32_t>(tiles.size()) > nbAffordable)
        tiles.resize(nbAffordable);

    if(!player->getSeat()->takeMana(static_cast<double>(pricePerTile) * static_cast<double>(tiles.size())))
        return false;

    // The walls keep their fullness: they only become earth, which the workers dig out at the
    // usual pace once the player marks them.
    for(Tile* tile : tiles)
    {
        OD_LOG_INF("Softening rock tile=" + Tile::displayAsString(tile) + " for seatId="
            + Helper::toString(player->getSeat()->getId()));
        tile->softenRock();
    }

    return true;
}

Spell* SpellSoftenRock::getSpellFromStream(GameMap* gameMap, std::istream &is)
{
    OD_LOG_ERR("SpellSoftenRock cannot be read from stream");
    return nullptr;
}

Spell* SpellSoftenRock::getSpellFromPacket(GameMap* gameMap, ODPacket &is)
{
    OD_LOG_ERR("SpellSoftenRock cannot be read from packet");
    return nullptr;
}
