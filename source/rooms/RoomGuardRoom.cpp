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

#include "rooms/RoomGuardRoom.h"

#include "entities/Creature.h"
#include "entities/Tile.h"
#include "game/Player.h"
#include "gamemap/GameMap.h"
#include "rooms/RoomManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"
#include "utils/Random.h"

#include <algorithm>

const std::string RoomGuardRoomName = "GuardRoom";
const std::string RoomGuardRoomNameDisplay = "Guard room";
const RoomType RoomGuardRoom::mRoomType = RoomType::guardRoom;
const TileVisual RoomGuardRoom::mRoomVisual = TileVisual::guardRoom;

namespace
{
class RoomGuardRoomFactory : public RoomFactory
{
    RoomType getRoomType() const override
    { return RoomGuardRoom::mRoomType; }

    TileVisual getVisualType() const override
    { return RoomGuardRoom::mRoomVisual; }
    
    const std::string& getName() const override
    { return RoomGuardRoomName; }

    const std::string& getNameReadable() const override
    { return RoomGuardRoomNameDisplay; }

    int getCostPerTile() const override
    { return ConfigManager::getSingleton().getRoomConfigInt32("GuardRoomCostPerTile"); }

    void checkBuildRoom(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildRoomDefault(gameMap, RoomGuardRoom::mRoomType, inputManager, inputCommand);
    }

    bool buildRoom(GameMap* gameMap, Player* player, ODPacket& packet) const override
    {
        std::vector<Tile*> tiles;
        if(!getRoomTilesDefault(tiles, gameMap, player, packet))
            return false;

        int32_t pricePerTarget = RoomManager::costPerTile(RoomGuardRoom::mRoomType);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
            return false;

        RoomGuardRoom* room = new RoomGuardRoom(gameMap);
        return buildRoomDefault(gameMap, room, player->getSeat(), tiles);
    }

    void checkBuildRoomEditor(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        checkBuildRoomDefaultEditor(gameMap, RoomGuardRoom::mRoomType, inputManager, inputCommand);
    }

    bool buildRoomEditor(GameMap* gameMap, ODPacket& packet) const override
    {
        RoomGuardRoom* room = new RoomGuardRoom(gameMap);
        return buildRoomDefaultEditor(gameMap, room, packet);
    }

    //! \brief Creates an empty room of this type, for a room that has to be split in two.
    Room* createRoom(GameMap* gameMap) const override
    { return new RoomGuardRoom(gameMap); }

    Room* getRoomFromStream(GameMap* gameMap, std::istream& is) const override
    {
        RoomGuardRoom* room = new RoomGuardRoom(gameMap);
        if(!Room::importRoomFromStream(*room, is))
        {
            OD_LOG_ERR("Error while building a room from the stream");
        }
        return room;
    }

    bool buildRoomOnTiles(GameMap* gameMap, Player* player, const std::vector<Tile*>& tiles, bool noFee =false) const override
    {
        int32_t pricePerTarget = RoomManager::costPerTile(RoomGuardRoom::mRoomType);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!noFee)
            if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
                return false;
        RoomGuardRoom* room = new RoomGuardRoom(gameMap);
        return buildRoomDefault(gameMap, room, player->getSeat(), tiles);
    }
};

// Register the factory
static RoomRegister reg(new RoomGuardRoomFactory);
}


RoomGuardRoom::RoomGuardRoom(GameMap* gameMap) :
    Room(gameMap)
{
    // Placeholder: the training hall look is used until the guard room has its own
    setMeshName("Dojo");
}

bool RoomGuardRoom::hasOpenCreatureSpot(Creature* c)
{
    return mCreaturesUsingRoom.size() < mCoveredTiles.size();
}

void RoomGuardRoom::removeCreatureUsingRoom(Creature* c)
{
    Room::removeCreatureUsingRoom(c);
    mGuardPosts.erase(c);
}

Tile* RoomGuardRoom::getPostForCreature(Creature& creature)
{
    std::map<Creature*, Tile*>::iterator it = mGuardPosts.find(&creature);
    if(it != mGuardPosts.end())
    {
        // The post can be lost if the tile has been sold or taken
        if(it->second->getCoveringRoom() == this)
            return it->second;

        mGuardPosts.erase(it);
    }

    if(mCoveredTiles.empty())
        return nullptr;

    // We prefer a tile that is not guarded yet
    std::vector<Tile*> freeTiles;
    for(Tile* tile : mCoveredTiles)
    {
        bool isTaken = false;
        for(const std::pair<Creature* const, Tile*>& post : mGuardPosts)
        {
            if(post.second != tile)
                continue;

            isTaken = true;
            break;
        }
        if(!isTaken)
            freeTiles.push_back(tile);
    }

    Tile* post = nullptr;
    if(!freeTiles.empty())
        post = freeTiles[Random::Uint(0, freeTiles.size() - 1)];
    else
        post = mCoveredTiles[Random::Uint(0, mCoveredTiles.size() - 1)];

    mGuardPosts[&creature] = post;
    return post;
}

bool RoomGuardRoom::useRoom(Creature& creature, bool forced)
{
    Tile* post = getPostForCreature(creature);
    if(post == nullptr)
        return false;

    Tile* tileCreature = creature.getPositionTile();
    if(tileCreature == nullptr)
    {
        OD_LOG_ERR("room=" + getName() + ", creature=" + creature.getName());
        return false;
    }

    if(tileCreature != post)
    {
        // We walk to the post
        creature.setDestination(post);
        return false;
    }

    // On duty: the guard stands still, watching. Enemies in sight are handled by the
    // creature behaviours that run before this action.
    creature.setAnimationState(EntityAnimation::idle_anim);
    creature.jobDone(ConfigManager::getSingleton().getRoomConfigDouble("GuardRoomWakefulnessPerDuty"));
    creature.setJobCooldown(Random::Uint(ConfigManager::getSingleton().getRoomConfigUInt32("GuardRoomCooldownDutyMin"),
        ConfigManager::getSingleton().getRoomConfigUInt32("GuardRoomCooldownDutyMax")));

    return false;
}
