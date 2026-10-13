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
#include "giftboxes/GiftBoxBonus.h"
#include "network/ODServer.h"
#include "network/ServerNotification.h"
#include "gamemap/GameMap.h"
#include "ODApplication.h"
#include "rooms/RoomManager.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
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
    Room(gameMap),
    mPrayerManaPending(0.0)
{
    // Placeholder: the crypt look is used until the temple has its own
    setMeshName("Crypt");
}

bool RoomTemple::hasOpenCreatureSpot(Creature* c)
{
    return mCreaturesUsingRoom.size() < getOuterTiles().size();
}

std::vector<Tile*> RoomTemple::getOuterTiles() const
{
    std::vector<Tile*> outerTiles;
    for(Tile* tile : mCoveredTiles)
    {
        if(!isPoolTile(*tile))
            outerTiles.push_back(tile);
    }
    return outerTiles;
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

    // Creatures pray on the outer tiles, the pool is only for sacrifices
    std::vector<Tile*> outerTiles = getOuterTiles();
    if(outerTiles.empty())
        return nullptr;

    // We prefer a tile where nobody prays yet
    std::vector<Tile*> freeTiles;
    for(Tile* tile : outerTiles)
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
        spot = outerTiles[Random::Uint(0, outerTiles.size() - 1)];

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

    // The creature prays. Its keeper gets mana and the creature feels better
    creature.setAnimationState(EntityAnimation::idle_anim);
    creature.jobDone(configManager.getRoomConfigDouble("TemplePrayerWakefulnessPerTurn"));
    // Each creature type has its own prayer mana per second (TemplePrayerMana<ClassName>), the generic value is the fallback.
    // The creature prays once per turn, so the rate is converted to turns. Fractions are kept for the next prayer.
    double defaultManaPerSecond = configManager.getRoomConfigDouble("TemplePrayerManaPerSecond");
    double manaPerSecond = configManager.getRoomConfigDoubleOrDefault(
        "TemplePrayerMana" + creature.getDefinition()->getClassName(), defaultManaPerSecond);
    mPrayerManaPending += manaPerSecond / ODApplication::turnsPerSecond;
    int32_t mana = static_cast<int32_t>(mPrayerManaPending);
    mPrayerManaPending -= mana;
    getGameMap()->addManaToSeat(mana, getSeat()->getId());
    creature.addPrayerRelief(configManager.getRoomConfigInt32("TemplePrayerReliefPerTurn"),
        configManager.getRoomConfigInt32("TemplePrayerReliefMax"));

    return false;
}

namespace
{
//! \brief Splits a recipe like "Troll+Troll=Cultist" into its inputs and its result
bool parseRecipe(const std::string& text, std::vector<std::string>& inputs, std::string& result)
{
    std::string::size_type equalPos = text.find('=');
    if(equalPos == std::string::npos)
        return false;

    result = text.substr(equalPos + 1);
    std::string left = text.substr(0, equalPos);
    inputs.clear();
    std::string::size_type start = 0;
    while(true)
    {
        std::string::size_type plusPos = left.find('+', start);
        if(plusPos == std::string::npos)
        {
            inputs.push_back(left.substr(start));
            break;
        }
        inputs.push_back(left.substr(start, plusPos - start));
        start = plusPos + 1;
    }
    return !result.empty();
}

//! \brief The queue holds the last sacrifices. A recipe matches when its inputs, in the order they were
//! sacrificed, are the newest entries of the queue.
bool matchesRecipe(const std::vector<std::pair<std::string, uint32_t> >& sacrificed,
    const std::vector<std::string>& inputs)
{
    if(inputs.size() > sacrificed.size())
        return false;

    size_t offset = sacrificed.size() - inputs.size();
    for(size_t i = 0; i < inputs.size(); ++i)
    {
        if(sacrificed[offset + i].first != inputs[i])
            return false;
    }
    return true;
}

//! \brief How many sacrifices the pool remembers
const size_t TEMPLE_SACRIFICE_QUEUE_SIZE = 3;
}

bool RoomTemple::isPoolTile(const Tile& tile) const
{
    if(tile.getCoveringRoom() != this)
        return false;

    for(int dx = -1; dx <= 1; ++dx)
    {
        for(int dy = -1; dy <= 1; ++dy)
        {
            Tile* neighbour = getGameMap()->getTile(tile.getX() + dx, tile.getY() + dy);
            if((neighbour == nullptr) || (neighbour->getCoveringRoom() != this))
                return false;
        }
    }
    return true;
}

void RoomTemple::creatureDropped(Creature& creature)
{
    Tile* tile = creature.getPositionTile();
    if((tile != nullptr) && creature.getDefinition()->isWorker() == false &&
       isPoolTile(*tile))
    {
        // The creature is sacrificed during the next upkeep
        mCreaturesToSacrifice.push_back(creature.getName());
        return;
    }

    Room::creatureDropped(creature);
}

void RoomTemple::absorbRoom(Room* r)
{
    Room::absorbRoom(r);

    if(r->getType() != getType())
        return;

    RoomTemple* roomAbs = static_cast<RoomTemple*>(r);
    mSacrificed.insert(mSacrificed.end(), roomAbs->mSacrificed.begin(), roomAbs->mSacrificed.end());
    roomAbs->mSacrificed.clear();
    mCreaturesToSacrifice.insert(mCreaturesToSacrifice.end(), roomAbs->mCreaturesToSacrifice.begin(),
        roomAbs->mCreaturesToSacrifice.end());
    roomAbs->mCreaturesToSacrifice.clear();
}

void RoomTemple::doUpkeep()
{
    Room::doUpkeep();

    std::vector<std::string> creaturesToSacrifice;
    creaturesToSacrifice.swap(mCreaturesToSacrifice);
    for(const std::string& name : creaturesToSacrifice)
    {
        Creature* creature = getGameMap()->getCreature(name);
        if((creature == nullptr) || !creature->isAlive() || (creature->getSeat() != getSeat()))
            continue;

        // The creature may have been picked up again
        Tile* tile = creature->getPositionTile();
        if((tile == nullptr) || !isPoolTile(*tile))
            continue;

        sacrificeCreature(*creature);
    }
}

void RoomTemple::sacrificeCreature(Creature& creature)
{
    ConfigManager& configManager = ConfigManager::getSingleton();
    Tile* tile = creature.getPositionTile();
    std::string className = creature.getDefinition()->getClassName();
    uint32_t level = creature.getLevel();

    OD_LOG_INF("creature=" + creature.getName() + " is sacrificed in room=" + getName());
    creature.clearActionQueue();
    creature.removeFromGameMap();
    creature.deleteYourself();

    // The pool remembers the last sacrifices only
    mSacrificed.push_back(std::pair<std::string, uint32_t>(className, level));
    if(mSacrificed.size() > TEMPLE_SACRIFICE_QUEUE_SIZE)
        mSacrificed.erase(mSacrificed.begin());

    // The first recipe that matches the newest sacrifices wins
    int32_t nbRecipes = configManager.getRoomConfigInt32("TempleRecipeCount");
    for(int32_t i = 1; i <= nbRecipes; ++i)
    {
        std::vector<std::string> inputs;
        std::string result;
        if(!parseRecipe(configManager.getRoomConfigString("TempleRecipe" + Helper::toString(i)), inputs, result))
        {
            OD_LOG_ERR("room=" + getName() + ", wrong recipe number=" + Helper::toString(i));
            continue;
        }

        if(!matchesRecipe(mSacrificed, inputs))
            continue;

        // The level of the new creature is the average level of the sacrificed creatures, rounded down
        uint32_t totalLevel = 0;
        for(size_t k = mSacrificed.size() - inputs.size(); k < mSacrificed.size(); ++k)
            totalLevel += mSacrificed[k].second;

        uint32_t averageLevel = totalLevel / static_cast<uint32_t>(inputs.size());
        mSacrificed.clear();
        giveSacrificeResult(result, averageLevel, *tile);
        return;
    }
}

void RoomTemple::giveSacrificeResult(const std::string& result, uint32_t averageLevel, Tile& tile)
{
    ConfigManager& configManager = ConfigManager::getSingleton();
    std::string message;
    if(result == "ManaBoost")
    {
        // The special is a gift box that has to be brought to the dungeon temple
        GiftBoxBonus* giftBox = new GiftBoxBonus(getGameMap(), "TempleSpecial", GiftBoxType::mana,
            GiftBoxBonus::getDefaultAmount(GiftBoxType::mana));
        giftBox->setSeat(getSeat());
        giftBox->addToGameMap();
        giftBox->createMesh();
        giftBox->setPosition(Ogre::Vector3(static_cast<Ogre::Real>(tile.getX()),
            static_cast<Ogre::Real>(tile.getY()), 0.0f));
        message = "The gods accepted your sacrifice and send you a special";
    }
    else
    {
        // The result is a creature. The fork specific name Workers gives the workers of the keeper
        int32_t nbCreatures = 1;
        const CreatureDefinition* classToSpawn = nullptr;
        if(result == "Workers")
        {
            nbCreatures = configManager.getRoomConfigInt32("TempleWorkersGiven");
            classToSpawn = getSeat()->getWorkerClassToSpawn();
        }
        else
            classToSpawn = getGameMap()->getClassDescription(result);

        if(classToSpawn == nullptr)
        {
            OD_LOG_ERR("room=" + getName() + ", unknown recipe result=" + result);
            return;
        }

        int32_t maxCreatures = configManager.getMaxCreaturesPerSeatAbsolute();
        for(int32_t i = 0; i < nbCreatures; ++i)
        {
            int32_t numCreatures = getGameMap()->getCreaturesBySeat(getSeat()).size();
            if(numCreatures >= maxCreatures)
                break;

            Creature* newCreature = new Creature(getGameMap(), classToSpawn, getSeat());
            if(averageLevel > 1 && !classToSpawn->isWorker())
                newCreature->setLevel(averageLevel);

            newCreature->addToGameMap();
            newCreature->setPosition(Ogre::Vector3(static_cast<Ogre::Real>(tile.getX()),
                static_cast<Ogre::Real>(tile.getY()), 0.0f));
            newCreature->createMesh();
        }
        message = "The gods accepted your sacrifice and send you a new creature";
    }

    if((getSeat()->getPlayer() != nullptr) &&
       getSeat()->getPlayer()->getIsHuman() &&
       !getSeat()->getPlayer()->getHasLost())
    {
        ServerNotification *serverNotification = new ServerNotification(
            ServerNotificationType::chatServer, getSeat()->getPlayer());
        serverNotification->mPacket << message << EventShortNoticeType::aboutCreatures;
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }
}
