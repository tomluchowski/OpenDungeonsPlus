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

#include "rooms/RoomTemple.h"

#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/Tile.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "rooms/RoomManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"
#include "utils/Random.h"

#include <algorithm>

const std::string RoomTempleName = "Temple";
const std::string RoomTempleNameDisplay = "Temple";
const RoomType RoomTemple::mRoomType = RoomType::temple;
const TileVisual RoomTemple::mRoomVisual = TileVisual::templeRoom;

namespace
{
class RoomTempleFactory : public RoomFactory
{
    RoomType getRoomType() const override
    { return RoomTemple::mRoomType; }

    TileVisual getVisualType() const override
    { return RoomTemple::mRoomVisual; }

    const std::string& getName() const override
    { return RoomTempleName; }

    const std::string& getNameReadable() const override
    { return RoomTempleNameDisplay; }

    int getCostPerTile() const override
    { return ConfigManager::getSingleton().getRoomConfigInt32("TempleCostPerTile"); }

    void checkBuildRoom(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildRoomDefault(gameMap, RoomTemple::mRoomType, inputManager, inputCommand);
    }

    bool buildRoom(GameMap* gameMap, Player* player, ODPacket& packet) const override
    {
        std::vector<Tile*> tiles;
        if(!getRoomTilesDefault(tiles, gameMap, player, packet))
            return false;

        int32_t pricePerTarget = RoomManager::costPerTile(RoomTemple::mRoomType);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
            return false;

        RoomTemple* room = new RoomTemple(gameMap);
        return buildRoomDefault(gameMap, room, player->getSeat(), tiles);
    }

    void checkBuildRoomEditor(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildRoomDefaultEditor(gameMap, RoomTemple::mRoomType, inputManager, inputCommand);
    }

    bool buildRoomEditor(GameMap* gameMap, ODPacket& packet) const override
    {
        RoomTemple* room = new RoomTemple(gameMap);
        return buildRoomDefaultEditor(gameMap, room, packet);
    }

    //! \brief Creates an empty room of this type, for a room that has to be split in two.
    Room* createRoom(GameMap* gameMap) const override
    { return new RoomTemple(gameMap); }

    Room* getRoomFromStream(GameMap* gameMap, std::istream& is) const override
    {
        RoomTemple* room = new RoomTemple(gameMap);
        if(!Room::importRoomFromStream(*room, is))
        {
            OD_LOG_ERR("Error while building a room from the stream");
        }
        return room;
    }

    bool buildRoomOnTiles(GameMap* gameMap, Player* player, const std::vector<Tile*>& tiles, bool noFee =false) const override
    {
        int32_t pricePerTarget = RoomManager::costPerTile(RoomTemple::mRoomType);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!noFee)
            if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
                return false;
        RoomTemple* room = new RoomTemple(gameMap);
        return buildRoomDefault(gameMap, room, player->getSeat(), tiles);
    }
};

// Register the factory
static RoomRegister reg(new RoomTempleFactory);
}


RoomTemple::RoomTemple(GameMap* gameMap) :
    Room(gameMap)
{
    // Placeholder: the crypt look is used until the temple has its own
    setMeshName("Crypt");
}

bool RoomTemple::hasOpenCreatureSpot(Creature* c)
{
    return mCreaturesUsingRoom.size() < mCoveredTiles.size();
}

void RoomTemple::removeCreatureUsingRoom(Creature* c)
{
    Room::removeCreatureUsingRoom(c);
    mPrayerSpots.erase(c);
}

Tile* RoomTemple::getPrayerSpotForCreature(Creature& creature)
{
    std::map<Creature*, Tile*>::iterator it = mPrayerSpots.find(&creature);
    if(it != mPrayerSpots.end())
    {
        // The spot can be lost if the tile has been sold or taken
        if(it->second->getCoveringRoom() == this)
            return it->second;

        mPrayerSpots.erase(it);
    }

    if(mCoveredTiles.empty())
        return nullptr;

    // We prefer a tile where nobody prays yet
    std::vector<Tile*> freeTiles;
    for(Tile* tile : mCoveredTiles)
    {
        bool isTaken = false;
        for(const std::pair<Creature* const, Tile*>& spot : mPrayerSpots)
        {
            if(spot.second != tile)
                continue;

            isTaken = true;
            break;
        }
        if(!isTaken)
            freeTiles.push_back(tile);
    }

    Tile* spot = nullptr;
    if(!freeTiles.empty())
        spot = freeTiles[Random::Uint(0, freeTiles.size() - 1)];
    else
        spot = mCoveredTiles[Random::Uint(0, mCoveredTiles.size() - 1)];

    mPrayerSpots[&creature] = spot;
    return spot;
}

bool RoomTemple::useRoom(Creature& creature, bool forced)
{
    Tile* spot = getPrayerSpotForCreature(creature);
    if(spot == nullptr)
        return false;

    Tile* tileCreature = creature.getPositionTile();
    if(tileCreature == nullptr)
    {
        OD_LOG_ERR("room=" + getName() + ", creature=" + creature.getName());
        return false;
    }

    if(tileCreature != spot)
    {
        // We walk to the prayer spot
        creature.setDestination(spot);
        return false;
    }

    ConfigManager& configManager = ConfigManager::getSingleton();
    const CreatureRoomAffinity& creatureRoomAffinity = creature.getDefinition()->getRoomAffinity(getType());

    // The creature prays. Its keeper gets mana and the creature feels better
    creature.setAnimationState(EntityAnimation::idle_anim);
    creature.jobDone(configManager.getRoomConfigDouble("TemplePrayerWakefulnessPerTurn"));
    int32_t mana = static_cast<int32_t>(creatureRoomAffinity.getEfficiency() * configManager.getRoomConfigDouble("TemplePrayerManaPerTurn"));
    getGameMap()->addManaToSeat(mana, getSeat()->getId());
    creature.addPrayerRelief(configManager.getRoomConfigInt32("TemplePrayerReliefPerTurn"),
        configManager.getRoomConfigInt32("TemplePrayerReliefMax"));

    return false;
}
