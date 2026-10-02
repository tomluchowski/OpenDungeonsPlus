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

#include "gamemap/SandboxMode.h"

#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "network/ODServer.h"
#include "network/ServerNotification.h"
#include "rooms/Room.h"
#include "rooms/RoomPortalWave.h"
#include "rooms/RoomType.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/Random.h"

#include <algorithm>
#include <cctype>

const uint32_t SandboxMode::NB_WAVES = 10;
const uint32_t SandboxMode::NB_HEROES_PER_WAVE = 8;
const uint32_t SandboxMode::MAX_HERO_LEVEL = 10;

namespace
{
const std::string HERO_FACTION = "Hero";
//! \brief The turns between the check for a beaten wave
const int32_t TURNS_BETWEEN_CHECKS = 5;
//! \brief The turns between the end of a wave and the next wave of a continual invasion
const int32_t TURNS_BETWEEN_WAVES = 14;

std::string toLower(const std::string& str)
{
    std::string ret = str;
    for(std::string::iterator it = ret.begin(); it != ret.end(); ++it)
        *it = static_cast<char>(std::tolower(static_cast<unsigned char>(*it)));

    return ret;
}

//! \brief A hero the invasions can bring. The lords are left to the toolbox.
bool isInvasionHero(const std::string& className)
{
    return toLower(className).find("lord") == std::string::npos;
}

bool isDwarf(const std::string& className)
{
    return className.compare(0, 5, "Dwarf") == 0;
}

//! \brief Tells if the creature of the given name is still fighting
bool isCreatureAlive(GameMap& gameMap, const std::string& name)
{
    Creature* creature = gameMap.getCreature(name);
    if(creature == nullptr)
        return false;

    return creature->isAlive();
}
}

SandboxMode::SandboxMode(GameMap& gameMap) :
    mGameMap(gameMap),
    mNbWavesLaunched(0),
    mIsContinual(false),
    mIsWaveActive(false),
    mTurnsBeforeNextWave(0),
    mTurnsBeforeCheck(0)
{
}

void SandboxMode::reset()
{
    mNbWavesLaunched = 0;
    mIsContinual = false;
    mIsWaveActive = false;
    mTurnsBeforeNextWave = 0;
    mTurnsBeforeCheck = 0;
    mWaveHeroes.clear();
    mToolboxHeroes.clear();
}

bool SandboxMode::isHeroSeat(const Seat* seat)
{
    if(seat == nullptr)
        return false;

    if(seat->isRogueSeat())
        return false;

    return (seat->getFaction().compare(HERO_FACTION) == 0);
}

Seat* SandboxMode::getHeroSeat() const
{
    for(Seat* seat : mGameMap.getSeats())
    {
        if(isHeroSeat(seat))
            return seat;
    }

    return nullptr;
}

void SandboxMode::sendMessage(Player* player, const std::string& message) const
{
    ServerNotification* serverNotification = new ServerNotification(
        ServerNotificationType::chatServer, player);
    serverNotification->mPacket << message << EventShortNoticeType::majorGameEvent;
    ODServer::getSingleton().queueServerNotification(serverNotification);
}

void SandboxMode::sendMessage(const std::string& message) const
{
    for(Player* player : mGameMap.getPlayers())
    {
        if(!player->getIsHuman())
            continue;

        sendMessage(player, message);
    }
}

void SandboxMode::takeHero(Player* player, const std::string& className, uint32_t level)
{
    if(!mGameMap.isSandbox())
        return;

    if(player == nullptr)
        return;

    // Only the heroes of the hero faction are in the toolbox
    const std::vector<std::string>& pool = ConfigManager::getSingleton().getFactionSpawnPool(HERO_FACTION);
    if(std::find(pool.begin(), pool.end(), className) == pool.end())
    {
        OD_LOG_ERR("className=" + className + " is not in the hero toolbox");
        return;
    }

    const CreatureDefinition* definition = mGameMap.getClassDescription(className);
    if(definition == nullptr)
    {
        OD_LOG_ERR("Couldn't find the hero class=" + className);
        return;
    }

    Seat* heroSeat = getHeroSeat();
    if(heroSeat == nullptr)
    {
        sendMessage(player, "This level has no hero seat.");
        return;
    }

    // Only one hero per type at a time
    std::map<std::string, std::string>::iterator it = mToolboxHeroes.find(className);
    if(it != mToolboxHeroes.end())
    {
        if(isCreatureAlive(mGameMap, it->second))
        {
            sendMessage(player, "There is already a " + className + " hero in your dungeon.");
            return;
        }

        mToolboxHeroes.erase(it);
    }

    if(level < 1)
        level = 1;
    if(level > MAX_HERO_LEVEL)
        level = MAX_HERO_LEVEL;

    Creature* hero = new Creature(&mGameMap, definition, heroSeat);
    hero->addToGameMap();
    hero->setPosition(Ogre::Vector3(0.0, 0.0, 0.0));
    hero->setLevel(level);
    hero->setHP(hero->getMaxHp());
    hero->addSeatWithVision(player->getSeat(), true);
    mToolboxHeroes[className] = hero->getName();

    player->pickUpEntity(hero);
}

bool SandboxMode::launchWave(uint32_t waveNumber)
{
    // The hero gate of the map is the wave portal
    RoomPortalWave* portal = nullptr;
    std::vector<Room*> portals = mGameMap.getRoomsByType(RoomType::portalWave);
    for(Room* room : portals)
    {
        if(room->numCoveredTiles() == 0)
            continue;

        portal = static_cast<RoomPortalWave*>(room);
        break;
    }

    if(portal == nullptr)
    {
        sendMessage("This level has no hero gate.");
        return false;
    }

    // The heroes of the wave: one dwarf and random heroes for the rest, all of the wave's level
    std::vector<std::string> pool;
    std::vector<std::string> dwarfs;
    const std::vector<std::string>& heroes = ConfigManager::getSingleton().getFactionSpawnPool(HERO_FACTION);
    for(const std::string& className : heroes)
    {
        if(!isInvasionHero(className))
            continue;

        pool.push_back(className);
        if(isDwarf(className))
            dwarfs.push_back(className);
    }

    if(pool.empty())
    {
        OD_LOG_ERR("No hero available for the invasion");
        return false;
    }

    std::vector<std::pair<std::string, uint32_t> > creatures;
    uint32_t nbToChoose = NB_HEROES_PER_WAVE;
    if(!dwarfs.empty())
    {
        creatures.push_back(std::pair<std::string, uint32_t>(dwarfs[Random::Uint(0, static_cast<uint32_t>(dwarfs.size()) - 1)], waveNumber));
        --nbToChoose;
    }

    while(nbToChoose > 0)
    {
        --nbToChoose;
        creatures.push_back(std::pair<std::string, uint32_t>(pool[Random::Uint(0, static_cast<uint32_t>(pool.size()) - 1)], waveNumber));
    }

    std::vector<std::string> spawnedNames;
    portal->spawnCreatures(creatures, spawnedNames);
    if(spawnedNames.empty())
    {
        sendMessage("The heroes cannot come: too many creatures.");
        return false;
    }

    mWaveHeroes = spawnedNames;
    mIsWaveActive = true;
    mTurnsBeforeCheck = TURNS_BETWEEN_CHECKS;
    if(waveNumber > mNbWavesLaunched)
        mNbWavesLaunched = waveNumber;

    sendMessage("Heroes are invading! Wave " + Helper::toString(waveNumber) + " of "
        + Helper::toString(NB_WAVES) + ".");
    return true;
}

void SandboxMode::startInvasion(Player* player, bool continual)
{
    if(!mGameMap.isSandbox())
        return;

    if(mIsWaveActive || mIsContinual)
    {
        sendMessage(player, "A hero invasion is already under way.");
        return;
    }

    if(continual)
    {
        // A continual invasion that already went through all the waves starts again
        if(mNbWavesLaunched >= NB_WAVES)
            mNbWavesLaunched = 0;

        if(launchWave(mNbWavesLaunched + 1))
            mIsContinual = true;

        return;
    }

    // A single wave. When the last one was beaten, the next ones are as hard as the last
    uint32_t waveNumber = mNbWavesLaunched + 1;
    if(waveNumber > NB_WAVES)
        waveNumber = NB_WAVES;

    launchWave(waveNumber);
}

void SandboxMode::doTurn()
{
    if(mIsWaveActive)
    {
        if(mTurnsBeforeCheck > 0)
        {
            --mTurnsBeforeCheck;
            return;
        }

        mTurnsBeforeCheck = TURNS_BETWEEN_CHECKS;
        for(std::vector<std::string>::iterator it = mWaveHeroes.begin(); it != mWaveHeroes.end();)
        {
            if(isCreatureAlive(mGameMap, *it))
                ++it;
            else
                it = mWaveHeroes.erase(it);
        }

        if(!mWaveHeroes.empty())
            return;

        mIsWaveActive = false;
        sendMessage("Wave " + Helper::toString(mNbWavesLaunched) + " was beaten.");
        if(mNbWavesLaunched >= NB_WAVES)
        {
            if(mIsContinual)
                sendMessage("You survived all " + Helper::toString(NB_WAVES) + " waves!");

            mIsContinual = false;
            return;
        }

        mTurnsBeforeNextWave = TURNS_BETWEEN_WAVES;
        return;
    }

    if(!mIsContinual)
        return;

    if(mTurnsBeforeNextWave > 0)
    {
        --mTurnsBeforeNextWave;
        return;
    }

    if(!launchWave(mNbWavesLaunched + 1))
        mIsContinual = false;
}
