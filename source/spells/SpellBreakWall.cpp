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

#include "spells/SpellBreakWall.h"

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

const std::string SpellBreakWallName = "breakWall";
const std::string SpellBreakWallNameDisplay = "Break wall";
const std::string SpellBreakWallCooldownKey = "BreakWallCooldown";
const SpellType SpellBreakWall::mSpellType = SpellType::breakWall;

namespace
{
class SpellBreakWallFactory : public SpellFactory
{
    SpellType getSpellType() const override
    { return SpellBreakWall::mSpellType; }

    const std::string& getName() const override
    { return SpellBreakWallName; }

    const std::string& getCooldownKey() const override
    { return SpellBreakWallCooldownKey; }

    const std::string& getNameReadable() const override
    { return SpellBreakWallNameDisplay; }

    virtual void checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    { SpellBreakWall::checkSpellCast(gameMap, inputManager, inputCommand); }

    virtual bool castSpell(GameMap* gameMap, Player* player, ODPacket& packet) const override
    { return SpellBreakWall::castSpell(gameMap, player, packet); }

    Spell* getSpellFromStream(GameMap* gameMap, std::istream &is) const override
    { return SpellBreakWall::getSpellFromStream(gameMap, is); }

    Spell* getSpellFromPacket(GameMap* gameMap, ODPacket &is) const override
    { return SpellBreakWall::getSpellFromPacket(gameMap, is); }
};

// Register the factory
static SpellRegister reg(new SpellBreakWallFactory);
}

bool SpellBreakWall::canBreakTile(Tile* tile, Seat* seat)
{
    return getRefusalReason(tile, seat).empty();
}

std::string SpellBreakWall::getRefusalReason(Tile* tile, Seat* seat)
{
    // The client only knows what its tiles look like, so this is decided from the tile visual
    // on both sides
    if(tile->getTileVisual() != TileVisual::claimedFull)
        return "point at a reinforced wall";

    // The caster's own walls, and its allies', can already be dug by its workers
    Seat* tileSeat = tile->getSeat();
    if((tileSeat == nullptr) || !tileSeat->canOwnedTileBeClaimedBy(seat))
        return "the wall must belong to an enemy";

    if(!seat->hasVisionOnTile(tile))
        return "you must see the wall";

    return std::string();
}

void SpellBreakWall::checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand)
{
    Player* player = gameMap->getLocalPlayer();
    int32_t pricePerTile = ConfigManager::getSingleton().getSpellConfigInt32("BreakWallPrice");
    int32_t playerMana = static_cast<int32_t>(player->getSeat()->getMana());
    // The player is told why the wall pointed at cannot be broken rather than seeing nothing
    // happen
    Tile* tilePointed = gameMap->getTile(inputManager.mXPos, inputManager.mYPos);
    std::string refusalReason;
    if(tilePointed != nullptr)
        refusalReason = getRefusalReason(tilePointed, player->getSeat());

    if(inputManager.mCommandState == InputCommandState::infoOnly)
    {
        std::string txt = formatCastSpell(SpellType::breakWall, pricePerTile);
        if(!refusalReason.empty())
            inputCommand.displayText(Ogre::ColourValue::Red, txt + ": " + refusalReason);
        else if(playerMana < pricePerTile)
            inputCommand.displayText(Ogre::ColourValue::Red, txt);
        else
            inputCommand.displayText(Ogre::ColourValue::White, txt);

        inputCommand.selectSquaredTiles(inputManager.mXPos, inputManager.mYPos, inputManager.mXPos,
            inputManager.mYPos);
        return;
    }

    // Walls are what this spell is cast on, and GameMap::playerSelects() only ever selects
    // ground, so the tiles are picked here.
    std::vector<Tile*> tilesRegion = gameMap->rectangularRegion(inputManager.mXPos, inputManager.mYPos,
        inputManager.mLStartDragX, inputManager.mLStartDragY);
    int32_t priceTotal = 0;
    std::vector<Tile*> tiles;
    for(Tile* tile : tilesRegion)
    {
        if(!canBreakTile(tile, player->getSeat()))
            continue;

        int32_t newPrice = priceTotal + pricePerTile;
        if(newPrice > playerMana)
            break;

        tiles.push_back(tile);
        priceTotal = newPrice;
    }

    if(inputManager.mCommandState == InputCommandState::building)
    {
        std::string txt = formatCastSpell(SpellType::breakWall, priceTotal);
        if(tiles.empty() && !refusalReason.empty())
            inputCommand.displayText(Ogre::ColourValue::Red, txt + ": " + refusalReason);
        else
            inputCommand.displayText(Ogre::ColourValue::White, txt);
        inputCommand.selectTiles(tiles);
        return;
    }

    inputCommand.unselectAllTiles();

    if(tiles.empty())
        return;

    ClientNotification *clientNotification = SpellManager::createSpellClientNotification(SpellType::breakWall);
    uint32_t nbTiles = tiles.size();
    clientNotification->mPacket << nbTiles;
    for(Tile* tile : tiles)
        gameMap->tileToPacket(clientNotification->mPacket, tile);

    ODClient::getSingleton().queueClientNotification(clientNotification);
}

bool SpellBreakWall::castSpell(GameMap* gameMap, Player* player, ODPacket& packet)
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
        if(!canBreakTile(tile, player->getSeat()))
        {
            OD_LOG_WRN("Cannot break tile=" + Tile::displayAsString(tile) + " for seatId="
                + Helper::toString(player->getSeat()->getId()));
            continue;
        }

        tiles.push_back(tile);
    }

    if(tiles.empty())
        return false;

    int32_t pricePerTile = ConfigManager::getSingleton().getSpellConfigInt32("BreakWallPrice");
    int32_t playerMana = static_cast<int32_t>(player->getSeat()->getMana());
    int32_t nbAffordable = (pricePerTile > 0) ? (playerMana / pricePerTile) : static_cast<int32_t>(tiles.size());
    if(nbAffordable <= 0)
        return false;

    if(static_cast<int32_t>(tiles.size()) > nbAffordable)
        tiles.resize(nbAffordable);

    if(!player->getSeat()->takeMana(static_cast<double>(pricePerTile) * static_cast<double>(tiles.size())))
        return false;

    // The walls keep their fullness: they only lose their owner, and the workers dig them out at
    // the usual pace once the player marks them.
    for(Tile* tile : tiles)
    {
        OD_LOG_INF("Breaking wall tile=" + Tile::displayAsString(tile) + " for seatId="
            + Helper::toString(player->getSeat()->getId()));
        tile->unclaimTile();
    }

    return true;
}

Spell* SpellBreakWall::getSpellFromStream(GameMap* gameMap, std::istream &is)
{
    OD_LOG_ERR("SpellBreakWall cannot be read from stream");
    return nullptr;
}

Spell* SpellBreakWall::getSpellFromPacket(GameMap* gameMap, ODPacket &is)
{
    OD_LOG_ERR("SpellBreakWall cannot be read from packet");
    return nullptr;
}
