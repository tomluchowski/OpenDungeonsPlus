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

#include "giftboxes/GiftBoxBonus.h"

#include "ODApplication.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/Tile.h"
#include "entities/TreasuryObject.h"
#include "game/Seat.h"
#include "game/SkillType.h"
#include "gamemap/GameMap.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/Random.h"

#include <cmath>
#include <iostream>

// Proposed values, they need to be approved
static const uint32_t DEFAULT_AMOUNT_MANA = 50000;
static const uint32_t DEFAULT_AMOUNT_GOLD = 10000;
static const uint32_t DEFAULT_AMOUNT_LEVELS = 1;
static const uint32_t DEFAULT_AMOUNT_WORKERS = 10;
//! Workers received with the upgraded Summon Worker spell start at this level
static const uint32_t LEVEL_WORKERS_UPGRADED = 4;
//! Skill level of the Summon Worker spell from which it counts as upgraded
static const uint32_t SKILL_LEVEL_UPGRADED = 2;
//! Stun Workers: the enemy workers stay down for this many seconds
static const double STUN_WORKERS_SECONDS = 5.0;

//! \brief Creates a bonus box of the given type and amount with the mystery box mesh
GiftBoxBonus::GiftBoxBonus(GameMap* gameMap, const std::string& baseName, GiftBoxType type, uint32_t amount) :
    GiftBoxEntity(gameMap, baseName, "MysteryBox", type),
    mAmount(amount)
{
    mPrevAnimationState = "Loop";
    mPrevAnimationStateLoop = true;
}

//! \brief Creates an empty bonus box of the given type, filled afterwards when it is loaded from a stream
GiftBoxBonus::GiftBoxBonus(GameMap* gameMap, GiftBoxType type) :
    GiftBoxEntity(gameMap, type),
    mAmount(0)
{
}

//! \brief Returns the name of the bonus of the given type
std::string GiftBoxBonus::getDisplayName(GiftBoxType type)
{
    switch(type)
    {
        case GiftBoxType::mana:
            return "Mana Boost";
        case GiftBoxType::gold:
            return "Increase Gold";
        case GiftBoxType::revealMap:
            return "Reveal Map";
        case GiftBoxType::levelUp:
            return "Increase Level";
        case GiftBoxType::healAll:
            return "Heal All";
        case GiftBoxType::makeSafe:
            return "Make Safe";
        case GiftBoxType::weakenWalls:
            return "Weaken Walls";
        case GiftBoxType::stunWorkers:
            return "Stun Workers";
        case GiftBoxType::receiveWorkers:
            return "Receive Workers";
        case GiftBoxType::makeHappy:
            return "Make Happy";
        case GiftBoxType::makeUnhappy:
            return "Make Unhappy";
        case GiftBoxType::killCreatures:
            return "Kill Creatures";
        default:
            return "Special";
    }
}

//! \brief Returns a short text saying what the bonus of the given type does
std::string GiftBoxBonus::getDescription(GiftBoxType type)
{
    switch(type)
    {
        case GiftBoxType::mana:
            return "Gives a large amount of mana.";
        case GiftBoxType::gold:
            return "Gives a large amount of gold.";
        case GiftBoxType::revealMap:
            return "Reveals the whole map.";
        case GiftBoxType::levelUp:
            return "All your creatures gain a level.";
        case GiftBoxType::healAll:
            return "Heals all your creatures completely.";
        case GiftBoxType::makeSafe:
            return "Reinforces the walls around your dungeon.";
        case GiftBoxType::weakenWalls:
            return "Turns the reinforced walls of the enemy back to earth.";
        case GiftBoxType::stunWorkers:
            return "Stuns all enemy workers for a few seconds.";
        case GiftBoxType::receiveWorkers:
            return "Gives you free workers.";
        case GiftBoxType::makeHappy:
            return "Removes all the annoyance of your creatures.";
        case GiftBoxType::makeUnhappy:
            return "Makes all enemy creatures unhappy.";
        case GiftBoxType::killCreatures:
            return "Kills an enemy creature you can see.";
        default:
            return "";
    }
}

//! \brief Returns the amount a box of the given type carries by default
uint32_t GiftBoxBonus::getDefaultAmount(GiftBoxType type)
{
    switch(type)
    {
        case GiftBoxType::mana:
            return DEFAULT_AMOUNT_MANA;
        case GiftBoxType::gold:
            return DEFAULT_AMOUNT_GOLD;
        case GiftBoxType::levelUp:
            return DEFAULT_AMOUNT_LEVELS;
        case GiftBoxType::receiveWorkers:
            return DEFAULT_AMOUNT_WORKERS;
        case GiftBoxType::revealMap:
        case GiftBoxType::healAll:
        case GiftBoxType::makeSafe:
        case GiftBoxType::weakenWalls:
        case GiftBoxType::stunWorkers:
        case GiftBoxType::makeHappy:
        case GiftBoxType::makeUnhappy:
        case GiftBoxType::killCreatures:
            return 0;
        default:
            OD_LOG_ERR("Unexpected GiftBoxType=" + Helper::toString(static_cast<uint32_t>(type)));
            return 0;
    }
}

//! \brief Writes the box and its amount to the stream
void GiftBoxBonus::exportToStream(std::ostream& os) const
{
    GiftBoxEntity::exportToStream(os);
    os << mAmount << "\t";
}

//! \brief Reads the box and its amount from the stream. Returns false if the data is incomplete
bool GiftBoxBonus::importFromStream(std::istream& is)
{
    if(!GiftBoxEntity::importFromStream(is))
        return false;
    if(!(is >> mAmount))
        return false;

    return true;
}

//! \brief Server side. Gives the bonus of this box to the seat that delivered it
void GiftBoxBonus::applyEffect()
{
    Seat* seat = getSeat();
    if(seat == nullptr)
    {
        OD_LOG_ERR("null Seat for giftbox=" + getName() + " pos=" + Helper::toString(getPosition()));
        return;
    }

    applyBonus(getGameMap(), seat, getGiftBoxType(), mAmount, getPositionTile());
}

//! \brief Server side. Applies the bonus of the given type and amount to the seat. Gold the
//! treasuries cannot hold is left on the given tile
void GiftBoxBonus::applyBonus(GameMap* gameMap, Seat* seat, GiftBoxType type, uint32_t amount, Tile* positionTile)
{
    switch(type)
    {
        case GiftBoxType::mana:
        {
            gameMap->addManaToSeat(static_cast<int>(amount), seat->getId());
            break;
        }
        case GiftBoxType::gold:
        {
            // What the treasuries cannot hold is left on the ground where the box was
            int notStored = gameMap->addGoldToSeat(static_cast<int>(amount), seat->getId());
            Tile* tile = positionTile;
            if((notStored > 0) && (tile != nullptr))
            {
                TreasuryObject* obj = new TreasuryObject(gameMap, notStored);
                obj->addToGameMap();
                Ogre::Vector3 spawnPosition(static_cast<Ogre::Real>(tile->getX()),
                                            static_cast<Ogre::Real>(tile->getY()), 0.0f);
                obj->createMesh();
                obj->setPosition(spawnPosition);
            }
            break;
        }
        case GiftBoxType::revealMap:
        {
            seat->revealMapPermanently();
            break;
        }
        case GiftBoxType::levelUp:
        {
            std::vector<Creature*> creatures = gameMap->getCreaturesBySeat(seat);
            for(Creature* creature : creatures)
            {
                if(!creature->isAlive())
                    continue;

                creature->setLevel(creature->getLevel() + amount);
            }
            break;
        }
        case GiftBoxType::healAll:
        {
            std::vector<Creature*> creatures = gameMap->getCreaturesBySeat(seat);
            for(Creature* creature : creatures)
            {
                if(!creature->isAlive())
                    continue;

                creature->heal(creature->getMaxHp());
            }
            break;
        }
        case GiftBoxType::makeSafe:
        {
            // Every wall next to claimed ground of the seat that is not claimed yet becomes a reinforced wall
            for(int x = 0; x < gameMap->getMapSizeX(); ++x)
            {
                for(int y = 0; y < gameMap->getMapSizeY(); ++y)
                {
                    Tile* tile = gameMap->getTile(x, y);
                    if((tile == nullptr) || tile->isClaimed() || !tile->isWallClaimable(seat))
                        continue;

                    tile->claimTile(seat);
                }
            }
            break;
        }
        case GiftBoxType::weakenWalls:
        {
            // Reinforced enemy walls turn back to earth. Rooms, traps, doors and bridges are not affected
            for(int x = 0; x < gameMap->getMapSizeX(); ++x)
            {
                for(int y = 0; y < gameMap->getMapSizeY(); ++y)
                {
                    Tile* tile = gameMap->getTile(x, y);
                    if((tile == nullptr) || !tile->isFullTile() || !tile->isClaimed())
                        continue;

                    if(tile->getSeat()->isAlliedSeat(seat))
                        continue;

                    if((tile->getCoveringBuilding() != nullptr) || tile->getHasBridge())
                        continue;

                    tile->getSeat()->notifyTileClaimedByEnemy(tile);
                    tile->unclaimTile();
                }
            }
            break;
        }
        case GiftBoxType::stunWorkers:
        {
            int32_t nbTurns = static_cast<int32_t>(std::ceil(STUN_WORKERS_SECONDS * ODApplication::turnsPerSecond));
            std::vector<Creature*> creatures = gameMap->getCreatures();
            for(Creature* creature : creatures)
            {
                if(!creature->isAlive() || !creature->getDefinition()->isWorker())
                    continue;

                if(creature->getSeat()->isAlliedSeat(seat))
                    continue;

                creature->stun(nbTurns);
            }
            break;
        }
        case GiftBoxType::receiveWorkers:
        {
            const CreatureDefinition* workerClass = seat->getWorkerClassToSpawn();
            Tile* tile = positionTile;
            if((workerClass == nullptr) || (tile == nullptr))
            {
                OD_LOG_ERR("No worker class or tile for special type=" + Helper::toString(static_cast<uint32_t>(type)));
                break;
            }

            bool upgraded = (seat->getSkillLevel(SkillType::spellSummonWorker) >= SKILL_LEVEL_UPGRADED);
            for(uint32_t i = 0; i < amount; ++i)
            {
                Creature* newCreature = new Creature(gameMap, workerClass, seat);
                newCreature->addToGameMap();
                if(upgraded)
                    newCreature->setLevel(LEVEL_WORKERS_UPGRADED);

                Ogre::Vector3 spawnPosition(static_cast<Ogre::Real>(tile->getX()),
                                            static_cast<Ogre::Real>(tile->getY()), 0.0f);
                newCreature->addParticleEffect("SummonWorker", 3);
                newCreature->createMesh();
                newCreature->setPosition(spawnPosition);
            }
            break;
        }
        case GiftBoxType::makeHappy:
        {
            std::vector<Creature*> creatures = gameMap->getCreaturesBySeat(seat);
            for(Creature* creature : creatures)
            {
                if(creature->isAlive())
                    creature->removeAnnoyance();
            }
            break;
        }
        case GiftBoxType::makeUnhappy:
        {
            std::vector<Creature*> creatures = gameMap->getCreatures();
            for(Creature* creature : creatures)
            {
                if(!creature->isAlive() || creature->getSeat()->isRogueSeat())
                    continue;

                if(creature->getSeat()->isAlliedSeat(seat))
                    continue;

                creature->makeUnhappy();
            }
            break;
        }
        case GiftBoxType::killCreatures:
        {
            // One visible enemy creature is chosen at random
            std::vector<Creature*> victims;
            std::vector<Creature*> creatures = gameMap->getCreatures();
            for(Creature* creature : creatures)
            {
                if(!creature->isAlive() || creature->getSeat()->isAlliedSeat(seat))
                    continue;

                Tile* pos = creature->getPositionTile();
                if((pos == nullptr) || !seat->hasVisionOnTile(pos))
                    continue;

                victims.push_back(creature);
            }
            if(victims.empty())
                break;

            Creature* victim = victims[Random::Int(0, static_cast<int>(victims.size()) - 1)];
            victim->takeDamage(nullptr, victim->getHP() + 1.0, 0.0, 0.0, 0.0, victim->getPositionTile(), false);
            break;
        }
        default:
            OD_LOG_ERR("Unexpected GiftBoxType=" + Helper::toString(static_cast<uint32_t>(type)));
            break;
    }
}
