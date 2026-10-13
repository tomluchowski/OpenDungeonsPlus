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

#include "gamemap/LevelScriptRunner.h"

#include "creatureaction/CreatureActionWalkToTile.h"
#include "creaturemood/CreatureMood.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/Tile.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "game/SkillType.h"
#include "gamemap/GameMap.h"
#include "gamemap/LevelScript.h"
#include "goals/Goal.h"
#include "network/ODServer.h"
#include "network/ServerNotification.h"
#include "ODApplication.h"
#include "rooms/Room.h"
#include "rooms/RoomManager.h"
#include "rooms/RoomType.h"
#include "spells/Spell.h"
#include "spells/SpellCallToWar.h"
#include "spells/SpellType.h"
#include "traps/Trap.h"
#include "traps/TrapManager.h"
#include "traps/TrapType.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/MakeUnique.h"

#include <algorithm>
#include <cstdlib>
#include <list>
#include <vector>

namespace
{

//! \brief Converts level seconds to game turns.
int64_t secondsToTurns(int64_t seconds)
{
    return static_cast<int64_t>(static_cast<double>(seconds) * ODApplication::turnsPerSecond);
}

//! \brief True if the creature belongs to the seat; seat -1 matches every creature.
bool isSeatMatching(const Creature* creature, int32_t seatId)
{
    if(seatId < 0)
        return true;

    return (creature->getSeat() != nullptr) && (creature->getSeat()->getId() == seatId);
}

//! Seat -1 matches every tile, seat 0 the tiles that nobody claimed
bool isTileOwnerMatching(const Tile* tile, int32_t seatId)
{
    if(seatId < 0)
        return true;

    if(tile->getSeat() == nullptr)
        return seatId == 0;

    return tile->getSeat()->getId() == seatId;
}

//! The tile kinds of the level scripts (see LevelScript.h)
bool isTileOfKind(const Tile* tile, const std::string& kind)
{
    TileVisual visual = tile->getTileVisual();
    if(kind == "rock")
        return visual == TileVisual::dirtFull;
    if(kind == "path")
        return (visual == TileVisual::dirtGround) || (visual == TileVisual::goldGround) || (visual == TileVisual::gemGround);
    if(kind == "gold")
        return visual == TileVisual::goldFull;
    if(kind == "gems")
        return visual == TileVisual::gemFull;
    if(kind == "claimed")
        return visual == TileVisual::claimedGround;
    if(kind == "wall")
        return visual == TileVisual::claimedFull;
    if(kind == "water")
        return visual == TileVisual::waterGround;
    if(kind == "lava")
        return visual == TileVisual::lavaGround;
    if(kind == "impenetrable")
        return (visual == TileVisual::rockFull) || (visual == TileVisual::rockGround);
    if(kind == "manawell")
        return tile->getType() == TileType::manaWell;
    Room* room = tile->getCoveringRoom();
    if(room == nullptr)
        return false;

    RoomType roomType = RoomManager::getRoomTypeFromRoomName(kind);
    return (roomType != RoomType::nullRoomType) && (room->getType() == roomType);
}

//! An empty class name matches every creature
bool isClassMatching(const Creature* creature, const std::string& className)
{
    return className.empty() || (creature->getDefinition()->getClassName() == className);
}

//! \brief Evaluates one condition of a trigger against the game state.
//! \param gameMap The server game map.
//! \param script The level script (flags, timers, regions).
//! \param cond The condition to test.
//! \return True if the condition holds now.
bool isConditionMet(GameMap& gameMap, const LevelScript& script, const LevelScriptCondition& cond)
{
    switch(cond.mType)
    {
        case LevelScriptConditionType::time:
            return gameMap.getTurnNumber() >= secondsToTurns(cond.mNumber);
        case LevelScriptConditionType::region:
        {
            LevelScriptRegion area(cond.mName, cond.mX1, cond.mY1, cond.mX2, cond.mY2);
            if(!cond.mName.empty())
            {
                const LevelScriptRegion* region = script.getRegion(cond.mName);
                if(region == nullptr)
                {
                    OD_LOG_ERR("Level script: unknown region name=" + cond.mName);
                    return false;
                }
                area = *region;
            }
            int64_t numInArea = 0;
            for(Creature* creature : gameMap.getCreatures())
            {
                if(!creature->isAlive())
                    continue;

                if(!isSeatMatching(creature, cond.mSeatId) || !isClassMatching(creature, cond.mName2))
                    continue;

                Tile* tile = creature->getPositionTile();
                if(tile == nullptr)
                    continue;

                if(area.contains(tile->getX(), tile->getY()))
                    ++numInArea;
            }
            return levelScriptCompare(numInArea, cond.mCompare, cond.mNumber);
        }
        case LevelScriptConditionType::creatures:
        {
            int64_t count = 0;
            for(Creature* creature : gameMap.getCreatures())
            {
                if(!creature->isAlive())
                    continue;

                if(isSeatMatching(creature, cond.mSeatId) && isClassMatching(creature, cond.mName2))
                    ++count;
            }
            return levelScriptCompare(count, cond.mCompare, cond.mNumber);
        }
        case LevelScriptConditionType::room:
        {
            Seat* seat = gameMap.getSeatById(cond.mSeatId);
            if(seat == nullptr)
                return false;

            RoomType roomType = RoomManager::getRoomTypeFromRoomName(cond.mName);
            if(roomType == RoomType::nullRoomType)
            {
                OD_LOG_ERR("Level script: unknown room name=" + cond.mName);
                return false;
            }
            return levelScriptCompare(static_cast<int64_t>(seat->getNbRooms(roomType)), cond.mCompare, cond.mNumber);
        }
        case LevelScriptConditionType::goal:
        {
            Seat* seat = gameMap.getSeatById(cond.mSeatId);
            if(seat == nullptr)
                return false;

            for(unsigned int i = 0; i < seat->numCompletedGoals(); ++i)
            {
                if(seat->getCompletedGoal(i)->getName() == cond.mName)
                    return true;
            }
            return false;
        }
        case LevelScriptConditionType::flag:
        {
            int64_t reference = cond.mName2.empty() ? cond.mNumber : script.getFlag(cond.mName2);
            return levelScriptCompare(script.getFlag(cond.mName), cond.mCompare, reference);
        }
        case LevelScriptConditionType::timer:
            return levelScriptCompare(script.getTimerTurns(cond.mName), cond.mCompare, secondsToTurns(cond.mNumber));
        case LevelScriptConditionType::tileKinds:
        case LevelScriptConditionType::tilesTagged:
        {
            const LevelScriptRegion* region = script.getRegion(cond.mName);
            if(region == nullptr)
            {
                OD_LOG_ERR("Level script: unknown region name=" + cond.mName);
                return false;
            }

            Seat* seat = gameMap.getSeatById(cond.mSeatId);
            Player* player = (seat != nullptr) ? seat->getPlayer() : nullptr;
            if((cond.mType == LevelScriptConditionType::tilesTagged) && (player == nullptr))
                return false;

            int64_t numTiles = 0;
            int64_t numDiggable = 0;
            for(int32_t x = std::min(region->mX1, region->mX2); x <= std::max(region->mX1, region->mX2); ++x)
            {
                for(int32_t y = std::min(region->mY1, region->mY2); y <= std::max(region->mY1, region->mY2); ++y)
                {
                    Tile* tile = gameMap.getTile(x, y);
                    if(tile == nullptr)
                        continue;

                    if(cond.mType == LevelScriptConditionType::tileKinds)
                    {
                        if(isTileOwnerMatching(tile, cond.mSeatId) && isTileOfKind(tile, cond.mName2))
                            ++numTiles;
                    }
                    else
                    {
                        if(tile->isDiggable(seat))
                        {
                            ++numDiggable;
                            if(tile->getMarkedForDigging(player))
                                ++numTiles;
                        }
                    }
                }
            }
            if((cond.mType == LevelScriptConditionType::tilesTagged) && (cond.mNumber < 0))
                return (numDiggable > 0) && (numTiles == numDiggable);

            return levelScriptCompare(numTiles, cond.mCompare, cond.mNumber);
        }
        case LevelScriptConditionType::creatureEvent:
        {
            if(cond.mName2 == "created")
            {
                // The creature (or the party) is there: it was placed by the level or created by an action
                int64_t numCreated = (gameMap.getCreature(cond.mName) != nullptr) ? 1 : 0;
                std::vector<std::string> members = script.getPartyMemberNames(cond.mName);
                for(const std::string& member : members)
                {
                    if(gameMap.getCreature(member) != nullptr)
                        ++numCreated;
                }
                return levelScriptCompare(numCreated, cond.mCompare, cond.mNumber);
            }
            return levelScriptCompare(script.getEventCount(cond.mName, cond.mName2), cond.mCompare, cond.mNumber);
        }
        case LevelScriptConditionType::playerSlaps:
            return levelScriptCompare(script.getSlaps(cond.mSeatId), cond.mCompare, cond.mNumber);
        case LevelScriptConditionType::roomFurniture:
        {
            Seat* seat = gameMap.getSeatById(cond.mSeatId);
            if(seat == nullptr)
                return false;

            RoomType roomType = RoomManager::getRoomTypeFromRoomName(cond.mName);
            if(roomType == RoomType::nullRoomType)
            {
                OD_LOG_ERR("Level script: unknown room name=" + cond.mName);
                return false;
            }

            int64_t numObjects = 0;
            for(Room* room : gameMap.getRoomsByTypeAndSeat(roomType, seat))
                numObjects += static_cast<int64_t>(room->getBuildingObjects().size());

            return levelScriptCompare(numObjects, cond.mCompare, cond.mNumber);
        }
        case LevelScriptConditionType::dungeonBreached:
        {
            Seat* seat = gameMap.getSeatById(cond.mSeatId);
            if(seat == nullptr)
                return false;

            for(Creature* creature : gameMap.getCreatures())
            {
                if(!creature->isAlive() || !creature->getIsOnMap() || (creature->getSeat() == nullptr))
                    continue;

                if(creature->getSeat()->isRogueSeat() || creature->getSeat()->isAlliedSeat(seat))
                    continue;

                Tile* tile = creature->getPositionTile();
                if((tile != nullptr) && tile->isClaimedForSeat(seat))
                    return true;
            }
            return false;
        }
        case LevelScriptConditionType::portalActive:
            return script.isPortalOff(cond.mSeatId) == (cond.mNumber == 0);
        case LevelScriptConditionType::creatureAlive:
        {
            Creature* creature = gameMap.getCreature(cond.mName);
            bool isAlive = (creature != nullptr) && creature->isAlive();
            return isAlive == (cond.mNumber != 0);
        }
        case LevelScriptConditionType::creatureReached:
        {
            Creature* creature = gameMap.getCreature(cond.mName);
            if((creature == nullptr) || !creature->isAlive() || (creature->getPositionTile() == nullptr))
                return false;

            Tile* tile = creature->getPositionTile();
            if(!cond.mName2.empty())
            {
                const LevelScriptRegion* region = script.getRegion(cond.mName2);
                if(region == nullptr)
                {
                    OD_LOG_ERR("Level script: unknown region name=" + cond.mName2);
                    return false;
                }
                return region->contains(tile->getX(), tile->getY());
            }

            Room* room = tile->getCoveringRoom();
            return (room != nullptr) && (room->getType() == RoomType::dungeonTemple) &&
                (room->getSeat() != nullptr) && (room->getSeat()->getId() == cond.mSeatId);
        }
        case LevelScriptConditionType::seatDefeated:
        {
            Player* player = gameMap.getPlayerBySeatId(cond.mSeatId);
            return (player != nullptr) && player->getHasLost();
        }
        case LevelScriptConditionType::ownsCreature:
        {
            Creature* creature = gameMap.getCreature(cond.mName);
            return (creature != nullptr) && creature->isAlive() && isSeatMatching(creature, cond.mSeatId);
        }
        case LevelScriptConditionType::creatureHealth:
        {
            Creature* creature = gameMap.getCreature(cond.mName);
            if((creature == nullptr) || !creature->isAlive() || (creature->getMaxHp() <= 0.0))
                return false;

            int64_t percent = static_cast<int64_t>(100.0 * creature->getHP() / creature->getMaxHp());
            return levelScriptCompare(percent, cond.mCompare, cond.mNumber);
        }
        case LevelScriptConditionType::spellKnown:
        {
            Seat* seat = gameMap.getSeatById(cond.mSeatId);
            SkillType skillType = Skills::fromString(cond.mName);
            return (seat != nullptr) && (skillType != SkillType::nullSkillType) && seat->isSkillDone(skillType);
        }
        case LevelScriptConditionType::trapsBuilt:
        {
            Seat* seat = gameMap.getSeatById(cond.mSeatId);
            if(seat == nullptr)
                return false;

            bool anyType = (cond.mName == "any");
            TrapType trapType = anyType ? TrapType::nullTrapType : TrapManager::getTrapTypeFromTrapName(cond.mName);
            int64_t numTiles = 0;
            for(Trap* trap : gameMap.getTraps())
            {
                if((trap->getSeat() == seat) && (anyType || (trap->getType() == trapType)))
                    numTiles += static_cast<int64_t>(trap->numCoveredTiles());
            }
            return levelScriptCompare(numTiles, cond.mCompare, cond.mNumber);
        }
        case LevelScriptConditionType::roomTiles:
        case LevelScriptConditionType::largestRoom:
        {
            Seat* seat = gameMap.getSeatById(cond.mSeatId);
            if(seat == nullptr)
                return false;

            RoomType roomType = RoomManager::getRoomTypeFromRoomName(cond.mName);
            if(roomType == RoomType::nullRoomType)
            {
                OD_LOG_ERR("Level script: unknown room name=" + cond.mName);
                return false;
            }

            int64_t numTiles = 0;
            for(Room* room : gameMap.getRoomsByTypeAndSeat(roomType, seat))
            {
                int64_t roomTiles = static_cast<int64_t>(room->numCoveredTiles());
                if(cond.mType == LevelScriptConditionType::roomTiles)
                    numTiles += roomTiles;
                else
                    numTiles = std::max(numTiles, roomTiles);
            }
            return levelScriptCompare(numTiles, cond.mCompare, cond.mNumber);
        }
        case LevelScriptConditionType::gold:
        case LevelScriptConditionType::mana:
        case LevelScriptConditionType::kills:
        case LevelScriptConditionType::goldMined:
        {
            Seat* seat = gameMap.getSeatById(cond.mSeatId);
            if(seat == nullptr)
                return false;

            int64_t amount = 0;
            if(cond.mType == LevelScriptConditionType::gold)
                amount = static_cast<int64_t>(seat->getGold());
            else if(cond.mType == LevelScriptConditionType::mana)
                amount = static_cast<int64_t>(seat->getMana());
            else if(cond.mType == LevelScriptConditionType::kills)
                amount = static_cast<int64_t>(seat->getStatistics().mCreaturesKilled);
            else
                amount = static_cast<int64_t>(seat->getGoldMined());

            return levelScriptCompare(amount, cond.mCompare, cond.mNumber);
        }
        case LevelScriptConditionType::happyCreatures:
        case LevelScriptConditionType::angryCreatures:
        case LevelScriptConditionType::creaturesAtLevel:
        {
            int64_t count = 0;
            for(Creature* creature : gameMap.getCreatures())
            {
                if(!creature->isAlive())
                    continue;

                if(!isSeatMatching(creature, cond.mSeatId))
                    continue;

                CreatureMoodLevel mood = creature->getMoodValue();
                if(cond.mType == LevelScriptConditionType::happyCreatures)
                {
                    if(mood == CreatureMoodLevel::Happy)
                        ++count;
                }
                else if(cond.mType == LevelScriptConditionType::angryCreatures)
                {
                    if((mood == CreatureMoodLevel::Angry) || (mood == CreatureMoodLevel::Furious))
                        ++count;
                }
                else if(static_cast<int32_t>(creature->getLevel()) >= cond.mX1)
                {
                    ++count;
                }
            }
            return levelScriptCompare(count, cond.mCompare, cond.mNumber);
        }
        case LevelScriptConditionType::creaturesLost:
        case LevelScriptConditionType::creaturesPickedUp:
        case LevelScriptConditionType::creaturesDropped:
        case LevelScriptConditionType::creaturesSlapped:
        {
            Seat* seat = gameMap.getSeatById(cond.mSeatId);
            if(seat == nullptr)
                return false;

            const SeatStatistics& statistics = seat->getStatistics();
            int64_t amount = 0;
            if(cond.mType == LevelScriptConditionType::creaturesLost)
                amount = static_cast<int64_t>(statistics.mCreaturesLost);
            else if(cond.mType == LevelScriptConditionType::creaturesPickedUp)
                amount = static_cast<int64_t>(statistics.mCreaturesPickedUp);
            else if(cond.mType == LevelScriptConditionType::creaturesDropped)
                amount = static_cast<int64_t>(statistics.mCreaturesDropped);
            else
                amount = static_cast<int64_t>(statistics.mCreaturesSlapped);

            return levelScriptCompare(amount, cond.mCompare, cond.mNumber);
        }
        case LevelScriptConditionType::claimed:
        {
            Seat* seat = gameMap.getSeatById(cond.mSeatId);
            const LevelScriptRegion* region = script.getRegion(cond.mName);
            if(seat == nullptr)
                return false;

            if(region == nullptr)
            {
                OD_LOG_ERR("Level script: unknown region name=" + cond.mName);
                return false;
            }

            int64_t numTiles = 0;
            int64_t numClaimed = 0;
            for(int32_t x = std::min(region->mX1, region->mX2); x <= std::max(region->mX1, region->mX2); ++x)
            {
                for(int32_t y = std::min(region->mY1, region->mY2); y <= std::max(region->mY1, region->mY2); ++y)
                {
                    Tile* tile = gameMap.getTile(x, y);
                    if(tile == nullptr)
                        continue;

                    ++numTiles;
                    if(tile->isClaimedForSeat(seat))
                        ++numClaimed;
                }
            }
            if(cond.mNumber < 0)
                return (numTiles > 0) && (numClaimed == numTiles);

            return numClaimed >= cond.mNumber;
        }
    }
    return false;
}

//! \brief The players an action is meant for: the player of the seat or, with seat -1,
//! every human player that did not lose.
std::vector<Player*> getTargetPlayers(GameMap& gameMap, int32_t seatId)
{
    std::vector<Player*> players;
    if(seatId >= 0)
    {
        Player* player = gameMap.getPlayerBySeatId(seatId);
        if((player != nullptr) && player->getIsHuman())
            players.push_back(player);

        return players;
    }

    for(Player* player : gameMap.getPlayers())
    {
        if(player->getIsHuman() && !player->getHasLost())
            players.push_back(player);
    }
    return players;
}

//! \brief Sends a chat message to the players an action is meant for.
void sendMessage(GameMap& gameMap, int32_t seatId, const std::string& text)
{
    for(Player* player : getTargetPlayers(gameMap, seatId))
    {
        ServerNotification* serverNotification = new ServerNotification(
            ServerNotificationType::chatServer, player);
        serverNotification->mPacket << text << EventShortNoticeType::majorGameEvent;
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }
}

//! \brief Creates the creatures of a spawn action on its tile and, if the action names a target seat,
//! sends them to that dungeon with a call to war.
void spawnCreatures(GameMap& gameMap, const LevelScriptAction& action)
{
    Seat* seat = gameMap.getSeatById(action.mSeatId);
    if(seat == nullptr)
    {
        OD_LOG_ERR("Level script: spawn on unknown seat id=" + Helper::toString(action.mSeatId));
        return;
    }

    Tile* tile = gameMap.getTile(action.mX, action.mY);
    if(tile == nullptr)
    {
        OD_LOG_ERR("Level script: spawn on unknown tile " + Helper::toString(action.mX) + "," + Helper::toString(action.mY));
        return;
    }

    Ogre::Vector3 spawnPosition(static_cast<Ogre::Real>(tile->getX()), static_cast<Ogre::Real>(tile->getY()), 0.0f);
    uint32_t maxCreatures = ConfigManager::getSingleton().getMaxCreaturesPerSeatAbsolute();
    uint32_t numCreatures = seat->getNumCreaturesFighters();
    // Same rule as the wave portals: only the absolute creature limit counts
    for(std::size_t index = 0; index < action.mCreatures.size(); ++index)
    {
        const std::pair<std::string, uint32_t>& p = action.mCreatures[index];
        if(numCreatures >= maxCreatures)
            break;

        const CreatureDefinition* classToSpawn = gameMap.getClassDescription(p.first);
        if(classToSpawn == nullptr)
        {
            OD_LOG_ERR("Level script: wrong creature class=" + p.first);
            continue;
        }

        Creature* newCreature = new Creature(&gameMap, classToSpawn, seat, spawnPosition);
        // A creature that conditions refer to by name gets that name, if it is free
        if((index < action.mCreatureNames.size()) && !action.mCreatureNames[index].empty() &&
           (gameMap.getCreature(action.mCreatureNames[index]) == nullptr))
        {
            newCreature->setName(action.mCreatureNames[index]);
        }
        newCreature->setLevel(p.second);
        newCreature->setHP(newCreature->getMaxHp());

        OD_LOG_INF("Level script spawns a creature class=" + classToSpawn->getClassName()
            + ", name=" + newCreature->getName() + ", seatId=" + Helper::toString(seat->getId()));

        newCreature->addToGameMap();
        if(!action.mParty.empty())
            gameMap.getLevelScript().addPartyMember(action.mParty, newCreature->getName());

        newCreature->createMesh();
        newCreature->setPosition(newCreature->getPosition());
        ++numCreatures;
    }

    if(action.mTargetSeatId < 0)
        return;

    // As the wave portals do, the group is sent to the dungeon of the target seat with a call to war.
    // If there already is one, it is not replaced.
    for(Spell* spell : gameMap.getSpells())
    {
        if(spell->getSpellType() == SpellType::callToWar)
            return;
    }

    for(Room* room : gameMap.getRoomsByType(RoomType::dungeonTemple))
    {
        if(room->getSeat()->getId() != action.mTargetSeatId)
            continue;

        Tile* targetTile = room->getCentralTile();
        if(targetTile == nullptr)
            continue;

        SpellCallToWar* spell = new SpellCallToWar(&gameMap);
        spell->setSeat(seat);
        spell->addToGameMap();
        Ogre::Vector3 targetPosition(static_cast<Ogre::Real>(targetTile->getX()),
                                     static_cast<Ogre::Real>(targetTile->getY()),
                                     static_cast<Ogre::Real>(0.0));
        spell->createMesh();
        spell->setPosition(targetPosition);
        return;
    }
}

//! The number of turns the tiles of a revealed region stay visible: shown once, then they count as seen
static const uint32_t REVEAL_REGION_TURNS = 1;

//! \brief The tiles of the region stay visible to the seat, as the tiles revealed by a
//! tortured creature do: they are shown once and then count as seen.
void revealRegion(GameMap& gameMap, const LevelScript& script, const LevelScriptAction& action)
{
    const LevelScriptRegion* region = script.getRegion(action.mText);
    if(region == nullptr)
    {
        OD_LOG_ERR("Level script: reveal of unknown region name=" + action.mText);
        return;
    }

    std::vector<Tile*> tiles;
    for(int32_t x = std::min(region->mX1, region->mX2); x <= std::max(region->mX1, region->mX2); ++x)
    {
        for(int32_t y = std::min(region->mY1, region->mY2); y <= std::max(region->mY1, region->mY2); ++y)
        {
            Tile* tile = gameMap.getTile(x, y);
            if(tile != nullptr)
                tiles.push_back(tile);
        }
    }

    for(Player* player : getTargetPlayers(gameMap, action.mSeatId))
        player->getSeat()->revealTiles(tiles, REVEAL_REGION_TURNS);
}

//! \brief A room, trap, door or spell becomes available to the seat, as if it was researched.
//! A skill the level had forbidden is allowed again.
void makeSkillAvailable(GameMap& gameMap, const LevelScriptAction& action)
{
    SkillType skillType = Skills::fromString(action.mText);
    if(skillType == SkillType::nullSkillType)
    {
        OD_LOG_ERR("Level script: make of unknown skill name=" + action.mText);
        return;
    }

    for(Player* player : getTargetPlayers(gameMap, action.mSeatId))
    {
        Seat* seat = player->getSeat();
        if(seat->isSkillDone(skillType))
            continue;

        // A forbidden skill cannot be queued, so only the forbidden list needs a change
        const std::vector<SkillType>& notAllowed = seat->getSkillNotAllowed();
        if(std::find(notAllowed.begin(), notAllowed.end(), skillType) != notAllowed.end())
            seat->setSkillAvailability(skillType, true, false);

        seat->addSkill(skillType);
    }
}

//! Builds a wooden bridge of the seat on every lava or water tile of the rectangle (the terrain action with
//! the kind bridge). Tiles with a building are left alone.
void buildBridge(GameMap& gameMap, const LevelScriptAction& action)
{
    Player* player = gameMap.getPlayerBySeatId(action.mSeatId);
    if(player == nullptr)
    {
        OD_LOG_ERR("Level script: a bridge needs the seat of a player");
        return;
    }

    for(int32_t x = std::min(action.mX, action.mX2); x <= std::max(action.mX, action.mX2); ++x)
    {
        for(int32_t y = std::min(action.mY, action.mY2); y <= std::max(action.mY, action.mY2); ++y)
        {
            Tile* tile = gameMap.getTile(x, y);
            if((tile == nullptr) || (tile->getCoveringBuilding() != nullptr))
                continue;

            if((tile->getType() != TileType::lava) && (tile->getType() != TileType::water))
                continue;

            std::vector<Tile*> tiles;
            tiles.push_back(tile);
            RoomManager::buildRoomOnTiles(&gameMap, RoomType::bridgeWooden, player, tiles, true);
        }
    }
}

//! Changes the tiles of the rectangle, as digging or claiming does: the tile is recomputed and
//! sent to every seat that sees it
void alterTerrain(GameMap& gameMap, const LevelScriptAction& action)
{
    if(action.mText == "bridge")
    {
        buildBridge(gameMap, action);
        return;
    }

    TileType type = TileType::dirt;
    double fullness = 0.0;
    bool claim = false;
    if(action.mText == "rock")
        fullness = 100.0;
    else if(action.mText == "path")
        fullness = 0.0;
    else if(action.mText == "claimed")
        claim = true;
    else if(action.mText == "wall")
    {
        fullness = 100.0;
        claim = true;
    }
    else if(action.mText == "gold")
    {
        type = TileType::gold;
        fullness = 100.0;
    }
    else if(action.mText == "gems")
    {
        type = TileType::gem;
        fullness = 100.0;
    }
    else if(action.mText == "water")
        type = TileType::water;
    else if(action.mText == "lava")
        type = TileType::lava;
    else if(action.mText == "impenetrable")
    {
        type = TileType::rock;
        fullness = 100.0;
    }
    else if(action.mText == "manawell")
    {
        // A mana well is open ground. With a seat it starts claimed and feeds that seat's mana
        type = TileType::manaWell;
        claim = (action.mSeatId >= 0);
    }
    else
    {
        OD_LOG_ERR("Level script: unknown terrain kind=" + action.mText);
        return;
    }

    Seat* seat = (action.mSeatId >= 0) ? gameMap.getSeatById(action.mSeatId) : nullptr;
    if(claim && (seat == nullptr))
    {
        OD_LOG_ERR("Level script: terrain " + action.mText + " needs a seat");
        return;
    }

    for(int32_t x = std::min(action.mX, action.mX2); x <= std::max(action.mX, action.mX2); ++x)
    {
        for(int32_t y = std::min(action.mY, action.mY2); y <= std::max(action.mY, action.mY2); ++y)
        {
            Tile* tile = gameMap.getTile(x, y);
            if(tile == nullptr)
                continue;

            // Rooms, traps and doors stay, and a wall does not grow around creatures
            if(tile->getCoveringBuilding() != nullptr)
                continue;

            if((fullness > 0.0 || type == TileType::lava || type == TileType::water) && (tile->numEntitiesInTile() > 0))
                continue;

            tile->setType(type);
            tile->setFullness(fullness);
            if(claim)
                tile->claimTile(seat);
            else
                tile->unclaimTile();

            tile->computeTileVisual();
            tile->setDirtyForAllSeats();
            for(Seat* other : gameMap.getSeats())
                gameMap.refreshFloodFill(other, tile);
        }
    }
}

//! Alliances join the seats into one team. Breaking one puts the seat into a team of its own.
void changeAlliance(GameMap& gameMap, const LevelScriptAction& action)
{
    Seat* first = gameMap.getSeatById(action.mSeatId);
    Seat* second = gameMap.getSeatById(action.mTargetSeatId);
    if((first == nullptr) || (second == nullptr))
    {
        OD_LOG_ERR("Level script: alliance with an unknown seat");
        return;
    }

    std::vector<Seat*> changed;
    if(action.mNumber != 0)
    {
        int oldTeam = first->getTeamId();
        for(Seat* seat : gameMap.getSeats())
        {
            if(seat->getTeamId() != oldTeam)
                continue;

            seat->setTeamId(second->getTeamId());
            changed.push_back(seat);
        }
    }
    else
    {
        first->setTeamId(first->getId());
        changed.push_back(first);
    }

    for(Seat* seat : changed)
    {
        for(Player* player : gameMap.getPlayers())
        {
            if(!player->getIsHuman())
                continue;

            ServerNotification* serverNotification = new ServerNotification(ServerNotificationType::seatTeam, player);
            serverNotification->mPacket << static_cast<int32_t>(seat->getId()) << static_cast<int32_t>(seat->getTeamId());
            ODServer::getSingleton().queueServerNotification(serverNotification);
        }
    }
}

//! A creature comes through a portal of the seat (at the heart if the seat has none)
void generateCreature(GameMap& gameMap, const LevelScriptAction& action)
{
    Seat* seat = gameMap.getSeatById(action.mSeatId);
    if(seat == nullptr)
        return;

    Tile* tile = nullptr;
    std::vector<Room*> portals = gameMap.getRoomsByTypeAndSeat(RoomType::portal, seat);
    if(!portals.empty())
        tile = portals[0]->getCentralTile();

    if(tile == nullptr)
    {
        std::vector<Room*> hearts = gameMap.getRoomsByTypeAndSeat(RoomType::dungeonTemple, seat);
        if(!hearts.empty())
            tile = hearts[0]->getCentralTile();
    }
    if(tile == nullptr)
        return;

    LevelScriptAction spawn = action;
    spawn.mX = tile->getX();
    spawn.mY = tile->getY();
    spawn.mTargetSeatId = -1;
    spawnCreatures(gameMap, spawn);
}

//! The creatures an order or speed action is meant for: names of creatures or tags of parties
std::vector<std::string> resolveCreatureNames(GameMap& gameMap, const LevelScript& script,
                                              const std::vector<std::string>& tokens)
{
    std::vector<std::string> names;
    for(const std::string& token : tokens)
    {
        if(gameMap.getCreature(token) != nullptr)
        {
            names.push_back(token);
            continue;
        }

        std::vector<std::string> members = script.getPartyMemberNames(token);
        names.insert(names.end(), members.begin(), members.end());
    }
    return names;
}

//! Running creatures move this much faster than walking ones (own tuning value)
const double RUN_SPEED_FACTOR = 1.5;

//! The creatures (names or party tags) run or walk again (the speed action)
void changeCreatureSpeed(GameMap& gameMap, const LevelScript& script, const LevelScriptAction& action)
{
    std::vector<std::string> names = resolveCreatureNames(gameMap, script, action.mCreatureNames);
    for(const std::string& name : names)
    {
        Creature* creature = gameMap.getCreature(name);
        if((creature == nullptr) || !creature->isAlive())
            continue;

        creature->setMoveSpeedModifier((action.mNumber != 0) ? RUN_SPEED_FACTOR : 1.0);
    }
}

//! The creatures (names or party tags) get the standing order of the action (the order action)
void giveCreatureOrder(GameMap& gameMap, LevelScript& script, const LevelScriptAction& action)
{
    LevelScriptOrder order;
    order.mJob = action.mText;
    order.mSeatId = action.mSeatId;
    order.mWaypoints = action.mWaypoints;
    std::vector<std::string> names = resolveCreatureNames(gameMap, script, action.mCreatureNames);
    for(const std::string& name : names)
        script.setOrder(name, order);
}

//! The room that covers the tile belongs to the seat: it is worn down at once, as the dance of a worker does
void changeRoomOwner(GameMap& gameMap, const LevelScriptAction& action)
{
    Seat* seat = gameMap.getSeatById(action.mSeatId);
    Tile* tile = gameMap.getTile(action.mX, action.mY);
    if((seat == nullptr) || (tile == nullptr))
    {
        OD_LOG_ERR("Level script: room owner change with an unknown seat or tile");
        return;
    }

    Room* room = tile->getCoveringRoom();
    if((room == nullptr) || (room->getSeat() == seat))
        return;

    if(room->getType() == RoomType::dungeonTemple)
    {
        OD_LOG_WRN("Level script: the dungeon temple cannot change its owner");
        return;
    }

    room->claimForSeat(seat, tile, 1.0e9);
}

//! \brief The creature with that name leaves the map (the remove action). Nothing happens if there is none on the map.
void removeCreature(GameMap& gameMap, const std::string& name)
{
    Creature* creature = gameMap.getCreature(name);
    if((creature == nullptr) || !creature->getIsOnMap())
        return;

    creature->clearDestinations(EntityAnimation::idle_anim, true, true);
    creature->removeFromGameMap();
    creature->deleteYourself();
}

//! \brief Carries out one action of a trigger that fired.
void runAction(GameMap& gameMap, LevelScript& script, const LevelScriptAction& action)
{
    switch(action.mType)
    {
        case LevelScriptActionType::message:
            sendMessage(gameMap, action.mSeatId, action.mText);
            break;
        case LevelScriptActionType::objective:
            sendMessage(gameMap, action.mSeatId, "Objective: " + action.mText);
            break;
        case LevelScriptActionType::spawn:
            spawnCreatures(gameMap, action);
            break;
        case LevelScriptActionType::gold:
        {
            for(Player* player : getTargetPlayers(gameMap, action.mSeatId))
                gameMap.addGoldToSeat(static_cast<int>(action.mNumber), player->getSeat()->getId());

            break;
        }
        case LevelScriptActionType::setFlag:
            script.setFlag(action.mText, action.mNumber);
            break;
        case LevelScriptActionType::addFlag:
            script.setFlag(action.mText, script.getFlag(action.mText) + action.mNumber);
            break;
        case LevelScriptActionType::win:
        {
            for(Player* player : getTargetPlayers(gameMap, action.mSeatId))
                gameMap.addWinningSeat(player->getSeat());

            break;
        }
        case LevelScriptActionType::lose:
        {
            for(Player* player : getTargetPlayers(gameMap, action.mSeatId))
                player->notifyNoMoreDungeonTemple();

            break;
        }
        case LevelScriptActionType::reveal:
            revealRegion(gameMap, script, action);
            break;
        case LevelScriptActionType::make:
            makeSkillAvailable(gameMap, action);
            break;
        case LevelScriptActionType::timeLimit:
            gameMap.setScriptTimeLimit(action.mNumber);
            break;
        case LevelScriptActionType::countdown:
            gameMap.setScriptCountdown(action.mNumber);
            break;
        case LevelScriptActionType::waveCountdown:
            gameMap.setScriptWaveCountdown(action.mNumber);
            break;
        case LevelScriptActionType::startTimer:
            script.startTimer(action.mText, secondsToTurns(action.mNumber));
            break;
        case LevelScriptActionType::alterTerrain:
            alterTerrain(gameMap, action);
            break;
        case LevelScriptActionType::portalStatus:
            script.setPortalOff(action.mSeatId, action.mNumber == 0);
            break;
        case LevelScriptActionType::creatureAvailable:
            script.setCreatureBlocked(action.mSeatId, action.mText, action.mNumber == 0);
            break;
        case LevelScriptActionType::removeCreature:
            removeCreature(gameMap, action.mText);
            break;
        case LevelScriptActionType::alliance:
            changeAlliance(gameMap, action);
            break;
        case LevelScriptActionType::generateCreature:
            generateCreature(gameMap, action);
            break;
        case LevelScriptActionType::creatureOrder:
            giveCreatureOrder(gameMap, script, action);
            break;
        case LevelScriptActionType::creatureSpeed:
            changeCreatureSpeed(gameMap, script, action);
            break;
        case LevelScriptActionType::roomOwner:
            changeRoomOwner(gameMap, action);
            break;
        case LevelScriptActionType::slapLimit:
            script.setSlapLimit(action.mNumber);
            break;
    }
}

} // namespace

void LevelScriptRunner::doTurn(GameMap& gameMap)
{
    LevelScript& script = gameMap.getLevelScript();
    int64_t turn = gameMap.getTurnNumber();
    script.advanceTimers();
    for(LevelScriptTrigger& trigger : script.getTriggers())
    {
        if(trigger.mTimesFired > 0)
        {
            if(!trigger.mRepeat)
                continue;

            if(turn < trigger.mLastFiredTurn + secondsToTurns(trigger.mCooldownSeconds))
                continue;
        }

        bool isMet = true;
        for(const LevelScriptCondition& cond : trigger.mConditions)
        {
            if(!isConditionMet(gameMap, script, cond))
            {
                isMet = false;
                break;
            }
        }

        if(!isMet)
            continue;

        OD_LOG_INF("Level script trigger fired: " + trigger.mName);
        ++trigger.mTimesFired;
        trigger.mLastFiredTurn = turn;
        for(const LevelScriptAction& action : trigger.mActions)
            runAction(gameMap, script, action);
    }
}

namespace
{

//! The creature walks to the tile. False if there is no way.
bool walkToTile(Creature& creature, Tile* destination)
{
    std::list<Tile*> tilePath = creature.getGameMap()->path(&creature, destination);
    if(tilePath.empty())
        return false;

    std::vector<Ogre::Vector2> vectorPath;
    Creature::tileToVector2(tilePath, vectorPath, true, 0.0);
    if(vectorPath.empty())
        return false;

    creature.setWalkPath(EntityAnimation::walk_anim, EntityAnimation::idle_anim, true, true, vectorPath, true);
    creature.pushAction(Utils::make_unique<CreatureActionWalkToTile>(creature));
    return true;
}

//! Walks to the nearest of the tiles. False if none can be reached.
bool walkToNearest(Creature& creature, Tile* myTile, const std::vector<Tile*>& tiles)
{
    Tile* chosenTile = nullptr;
    std::list<Tile*> tilePath = creature.getGameMap()->findBestPath(&creature, myTile, tiles, chosenTile);
    if(tilePath.empty() || (chosenTile == nullptr))
        return false;

    std::vector<Ogre::Vector2> vectorPath;
    Creature::tileToVector2(tilePath, vectorPath, true, 0.0);
    if(vectorPath.empty())
        return false;

    creature.setWalkPath(EntityAnimation::walk_anim, EntityAnimation::idle_anim, true, true, vectorPath, true);
    creature.pushAction(Utils::make_unique<CreatureActionWalkToTile>(creature));
    return true;
}

//! True if the tiles are at most that many tiles apart in both directions
bool isNear(const Tile* first, const Tile* second, int32_t distance)
{
    return (std::abs(first->getX() - second->getX()) <= distance) && (std::abs(first->getY() - second->getY()) <= distance);
}

} // namespace

bool LevelScriptRunner::doCreatureOrder(Creature& creature)
{
    GameMap* gameMap = creature.getGameMap();
    if((gameMap == nullptr) || !creature.getIsOnServerMap())
        return false;

    LevelScript& script = gameMap->getLevelScript();
    const LevelScriptOrder* order = script.getOrder(creature.getName());
    if(order == nullptr)
        return false;

    Tile* myTile = creature.getPositionTile();
    if(myTile == nullptr)
        return false;

    // Copy: the script can change the order below
    LevelScriptOrder current = *order;
    const std::string& name = creature.getName();
    if(current.mJob == "wait")
        return true;

    if(current.mJob == "goto")
    {
        if(current.mIndex >= current.mWaypoints.size())
        {
            script.advanceOrder(name);
            return true;
        }

        Tile* target = gameMap->getTile(current.mWaypoints[current.mIndex].first, current.mWaypoints[current.mIndex].second);
        if((target == nullptr) || isNear(myTile, target, 1) || !gameMap->pathExists(&creature, myTile, target))
        {
            script.advanceOrder(name);
            return true;
        }

        if(!walkToTile(creature, target))
            script.advanceOrder(name);

        return true;
    }

    Seat* targetSeat = gameMap->getSeatById(current.mSeatId);
    if(current.mJob == "killplayer")
    {
        if(targetSeat == nullptr)
        {
            script.clearOrder(name);
            return false;
        }

        std::vector<Tile*> tiles;
        for(Room* room : gameMap->getRoomsByTypeAndSeat(RoomType::dungeonTemple, targetSeat))
        {
            for(Tile* tile : room->getCoveredTiles())
                tiles.push_back(tile);
        }
        if(tiles.empty())
        {
            // The dungeon is gone: the order is done
            script.clearOrder(name);
            return false;
        }

        // Near the heart the creature stays and fights what comes
        for(Tile* tile : tiles)
        {
            if(isNear(myTile, tile, 4))
                return true;
        }

        return walkToNearest(creature, myTile, tiles);
    }

    if(current.mJob == "killcreatures")
    {
        Creature* nearest = nullptr;
        int32_t nearestDistance = 0;
        for(Creature* other : gameMap->getCreatures())
        {
            if(!other->isAlive() || !other->getIsOnMap() || (other->getSeat() == nullptr) || (other == &creature))
                continue;

            if(other->getSeat()->isAlliedSeat(creature.getSeat()))
                continue;

            if((current.mSeatId >= 0) && (other->getSeat()->getId() != current.mSeatId))
                continue;

            Tile* otherTile = other->getPositionTile();
            if(otherTile == nullptr)
                continue;

            int32_t distance = std::abs(otherTile->getX() - myTile->getX()) + std::abs(otherTile->getY() - myTile->getY());
            if((nearest == nullptr) || (distance < nearestDistance))
            {
                nearest = other;
                nearestDistance = distance;
            }
        }
        if(nearest == nullptr)
            return false;

        Tile* target = nearest->getPositionTile();
        if(isNear(myTile, target, 2) || !gameMap->pathExists(&creature, myTile, target))
            return false;

        return walkToTile(creature, target);
    }

    if(current.mJob == "stealgold")
    {
        std::vector<Tile*> tiles;
        std::vector<Room*> treasuries;
        if(targetSeat != nullptr)
            treasuries = gameMap->getRoomsByTypeAndSeat(RoomType::treasury, targetSeat);

        for(Room* room : treasuries)
        {
            for(Tile* tile : room->getCoveredTiles())
                tiles.push_back(tile);
        }
        if(tiles.empty())
        {
            script.clearOrder(name);
            return false;
        }

        Room* room = myTile->getCoveringRoom();
        if((room != nullptr) && (room->getType() == RoomType::treasury) && (room->getSeat() == targetSeat))
        {
            int32_t amount = static_cast<int32_t>(creature.getDefinition()->getStealGold());
            if(amount <= 0)
                amount = 200;

            int32_t gold = room->withdrawGold(amount);
            creature.addGoldCarried(gold);
            if(gold > 0)
                gameMap->getLevelScript().recordEvent(name, "steals");

            // The theft is done: the creature stays where it is
            LevelScriptOrder done;
            done.mJob = "wait";
            script.setOrder(name, done);
            return true;
        }

        return walkToNearest(creature, myTile, tiles);
    }

    return false;
}
