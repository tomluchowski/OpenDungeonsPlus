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

#include "creaturemood/CreatureMood.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "game/SeatStatistics.h"
#include "game/SkillType.h"
#include "gamemap/GameMap.h"
#include "ODApplication.h"
#include "network/ODServer.h"
#include "network/ServerNotification.h"
#include "rooms/Room.h"
#include "rooms/RoomManager.h"
#include "rooms/RoomPortalWave.h"
#include "rooms/RoomType.h"
#include "traps/Trap.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/Random.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <set>

const uint32_t SandboxMode::NB_WAVES = 10;
const uint32_t SandboxMode::NB_HEROES_PER_WAVE = 8;
const uint32_t SandboxMode::MAX_HERO_LEVEL = 10;

// The values of the sandbox score
const int32_t SandboxMode::SCORE_HERO_KILLED = 60;
const int32_t SandboxMode::SCORE_LAND_TILE = 4;
const int32_t SandboxMode::SCORE_GOLD_TILE = 2;
const int32_t SandboxMode::SCORE_ITEM_MADE = 8;
const int32_t SandboxMode::SCORE_CREATURE_ENTERED = 15;
const int32_t SandboxMode::SCORE_CREATURE_CONVERTED = 30;

namespace
{
const std::string HERO_FACTION = "Hero";
//! \brief The turns between the check for a beaten wave
const int32_t TURNS_BETWEEN_CHECKS = 5;
//! \brief The turns between the end of a wave and the next wave of a continual invasion
const int32_t TURNS_BETWEEN_WAVES = 14;
//! \brief Seconds the score has to stay at the target before the realm is complete
const uint32_t SECONDS_AT_TARGET = 5;

//! \brief Number of rooms that become available over time
const uint32_t NB_UNLOCK_ROOMS = 13;
//! \brief The rooms that become available one after the other. The Dormitory (the lair) and the Hatchery
//! are available from the start. Room number i (counted from 0) comes after (i + 1) times the interval
//! SandboxRoomUnlockIntervalSeconds of rooms.cfg.
const SkillType ROOM_UNLOCKS[NB_UNLOCK_ROOMS] =
{
    SkillType::roomTreasury,
    SkillType::roomLibrary,
    SkillType::roomTrainingHall,
    SkillType::roomWorkshop,
    SkillType::roomGuardRoom,
    SkillType::roomBridgeWooden,
    SkillType::roomPrison,
    SkillType::roomTorture,
    SkillType::roomCrypt,
    SkillType::roomBridgeStone,
    SkillType::roomCasino,
    SkillType::roomArena,
    SkillType::roomTemple
};

//! \brief The seconds from the start after which room number index of ROOM_UNLOCKS becomes available
int64_t getRoomUnlockSeconds(uint32_t index)
{
    double interval = ConfigManager::getSingleton().getRoomConfigDouble("SandboxRoomUnlockIntervalSeconds");
    return static_cast<int64_t>((static_cast<double>(index) + 1.0) * interval);
}

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

//! \brief The names of the bonus kinds in the level file
const char* const BONUS_KIND_NAMES[] =
{
    "slaps", "allSpells", "happy", "rooms", "levelAtLeast", "creatures", "traps", "doors", "roomTiles", "creatureClass",
    "creatureNamed", "gold", "goldTiles", "roomTypes", "prisoners", "trapsFired"
};
const uint32_t NB_BONUS_KINDS = 16;

bool bonusKindFromString(const std::string& name, SandboxBonusKind& kind)
{
    for(uint32_t i = 0; i < NB_BONUS_KINDS; ++i)
    {
        if(name == BONUS_KIND_NAMES[i])
        {
            kind = static_cast<SandboxBonusKind>(i);
            return true;
        }
    }
    return false;
}

//! \brief Reads a non negative number, false if the text is not one
bool parseNumber(const std::string& text, int64_t& number)
{
    if(text.empty())
        return false;

    for(std::string::const_iterator it = text.begin(); it != text.end(); ++it)
    {
        if(!std::isdigit(static_cast<unsigned char>(*it)))
            return false;
    }

    number = static_cast<int64_t>(Helper::toInt(text));
    return true;
}

bool isSkillNamed(SkillType type, const std::string& prefix)
{
    return Skills::toString(type).compare(0, prefix.size(), prefix) == 0;
}
}

SandboxMode::SandboxMode(GameMap& gameMap) :
    mGameMap(gameMap)
{
    reset();
}

void SandboxMode::reset()
{
    mNbWavesLaunched = 0;
    mIsContinual = false;
    mIsWaveActive = false;
    mTurnsBeforeNextWave = 0;
    mTurnsBeforeCheck = 0;
    mTurnsElapsed = 0;
    mIsIntroSent = false;
    mWaveHeroes.clear();
    mToolboxHeroes.clear();
    mScore = 0;
    mTarget = 0;
    mRealmId.clear();
    mNextLevel.clear();
    mBonuses.clear();
    mIsRealmComplete = false;
    mSecondsAtTarget = 0;
    mTurnsBeforeSecond = 0;
    mIsBaselineSet = false;
    mLastHeroesKilled = 0;
    mLastItemsMade = 0;
    mLastConverted = 0;
    mLastGoldTiles = 0;
    mLastEntered = 0;
    mLastLandTiles = 0;
    mWaveHeroPoints = 0;
    mIsStatusSent = false;
    mSentScore = 0;
    mSentSecondsLeft = 0;
    mSentBonusMask = 0;
    mSentNextRoom.clear();
}

bool SandboxMode::importInfoLine(const std::string& line)
{
    std::vector<std::string> fields = Helper::split(line, '\t');
    if(fields.empty())
        return false;

    int64_t number = 0;
    if(fields[0] == "SandboxTarget")
    {
        if((fields.size() != 2) || !parseNumber(fields[1], number))
        {
            OD_LOG_ERR("Bad SandboxTarget line: " + line);
            return true;
        }

        mTarget = static_cast<uint32_t>(number);
        return true;
    }

    if(fields[0] == "SandboxRealm")
    {
        if(fields.size() == 2)
            mRealmId = fields[1];

        return true;
    }

    if(fields[0] == "SandboxNext")
    {
        if(fields.size() == 2)
            mNextLevel = fields[1];

        return true;
    }

    if(fields[0] == "SandboxBonus")
    {
        // SandboxBonus <kind> <count> <points> <arg or -> <text>
        SandboxBonus bonus;
        int64_t count = 0;
        int64_t points = 0;
        if((fields.size() != 6) || !bonusKindFromString(fields[1], bonus.mKind) || !parseNumber(fields[2], count) ||
           !parseNumber(fields[3], points))
        {
            OD_LOG_ERR("Bad SandboxBonus line: " + line);
            return true;
        }

        bonus.mCount = static_cast<uint32_t>(count);
        bonus.mPoints = static_cast<uint32_t>(points);
        if(fields[4] != "-")
            bonus.mArg = fields[4];

        bonus.mText = fields[5];
        mBonuses.push_back(bonus);
        return true;
    }

    if(fields[0] == "SandboxState")
    {
        // SandboxState <score> <awarded bonuses as a bit mask> <seconds since the start> <realm complete>
        int64_t score = 0;
        int64_t mask = 0;
        int64_t seconds = 0;
        int64_t complete = 0;
        if((fields.size() != 5) || !parseNumber(fields[1], score) || !parseNumber(fields[2], mask) ||
           !parseNumber(fields[3], seconds) || !parseNumber(fields[4], complete))
        {
            OD_LOG_ERR("Bad SandboxState line: " + line);
            return true;
        }

        mScore = static_cast<int32_t>(score);
        for(uint32_t i = 0; (i < mBonuses.size()) && (i < 32); ++i)
            mBonuses[i].mAwarded = ((mask >> i) & 1) != 0;

        mTurnsElapsed = static_cast<int64_t>(static_cast<double>(seconds) * ODApplication::turnsPerSecond);
        mIsIntroSent = (seconds > 0);
        mIsRealmComplete = (complete != 0);
        return true;
    }

    return false;
}

uint32_t SandboxMode::getBonusMask() const
{
    uint32_t mask = 0;
    for(uint32_t i = 0; (i < mBonuses.size()) && (i < 32); ++i)
    {
        if(mBonuses[i].mAwarded)
            mask |= (1u << i);
    }
    return mask;
}

void SandboxMode::exportInfo(std::ostream& os) const
{
    if(!mRealmId.empty())
        os << "SandboxRealm\t" << mRealmId << "\n";

    if(mTarget > 0)
        os << "SandboxTarget\t" << mTarget << "\n";

    for(std::vector<SandboxBonus>::const_iterator it = mBonuses.begin(); it != mBonuses.end(); ++it)
    {
        os << "SandboxBonus\t" << BONUS_KIND_NAMES[static_cast<uint32_t>(it->mKind)] << "\t" << it->mCount << "\t"
           << it->mPoints << "\t" << (it->mArg.empty() ? std::string("-") : it->mArg) << "\t" << it->mText << "\n";
    }

    if(!mNextLevel.empty())
        os << "SandboxNext\t" << mNextLevel << "\n";

    int64_t seconds = static_cast<int64_t>(static_cast<double>(mTurnsElapsed) / ODApplication::turnsPerSecond);
    if((mScore != 0) || (seconds > 0) || (getBonusMask() != 0) || mIsRealmComplete)
    {
        os << "SandboxState\t" << (mScore < 0 ? 0 : mScore) << "\t" << getBonusMask() << "\t" << seconds << "\t"
           << (mIsRealmComplete ? 1 : 0) << "\n";
    }
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

std::vector<Seat*> SandboxMode::getKeeperSeats() const
{
    std::vector<Seat*> keeperSeats;
    for(Seat* seat : mGameMap.getSeats())
    {
        if(seat->isRogueSeat() || isHeroSeat(seat))
            continue;

        if((seat->getPlayer() == nullptr) || !seat->getPlayer()->getIsHuman())
            continue;

        keeperSeats.push_back(seat);
    }

    return keeperSeats;
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
    // The hero portal of the map is the wave portal
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
        sendMessage("This level has no hero portal.");
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
    mWaveHeroPoints = 0;
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

std::string SandboxMode::getNextRoomName(int32_t& secondsLeft) const
{
    secondsLeft = 0;
    std::vector<Seat*> keeperSeats = getKeeperSeats();
    if(keeperSeats.empty())
        return std::string();

    for(uint32_t i = 0; i < NB_UNLOCK_ROOMS; ++i)
    {
        bool isDone = true;
        for(const Seat* seat : keeperSeats)
        {
            if(!seat->isSkillDone(ROOM_UNLOCKS[i]))
                isDone = false;
        }

        if(isDone)
            continue;

        int64_t seconds = static_cast<int64_t>(static_cast<double>(mTurnsElapsed) / ODApplication::turnsPerSecond);
        if(seconds < getRoomUnlockSeconds(i))
            secondsLeft = static_cast<int32_t>(getRoomUnlockSeconds(i) - seconds);

        return Skills::skillTypeToPlayerVisibleString(ROOM_UNLOCKS[i]);
    }

    return std::string();
}

void SandboxMode::updateUnlocks(const std::vector<Seat*>& keeperSeats)
{
    if(keeperSeats.empty())
        return;

    // The rooms the level already gives at the start (or that were researched) are skipped
    bool hasRoomToUnlock = false;
    for(uint32_t i = 0; i < NB_UNLOCK_ROOMS; ++i)
    {
        for(const Seat* seat : keeperSeats)
        {
            if(!seat->isSkillDone(ROOM_UNLOCKS[i]))
                hasRoomToUnlock = true;
        }
    }

    if(hasRoomToUnlock && !mIsIntroSent)
    {
        mIsIntroSent = true;
        sendMessage("A Dormitory and a Hatchery are yours to begin with. Build them to start your dungeon, "
            "so that your minions may sleep and eat. The other rooms become available over time.");
    }

    // The next rooms are available once their time has come. The seats tell their player with the usual notice
    int64_t seconds = static_cast<int64_t>(static_cast<double>(mTurnsElapsed) / ODApplication::turnsPerSecond);
    for(uint32_t i = 0; i < NB_UNLOCK_ROOMS; ++i)
    {
        if(seconds < getRoomUnlockSeconds(i))
            break;

        for(Seat* seat : keeperSeats)
            seat->addSkill(ROOM_UNLOCKS[i]);
    }

    // Every trap and door is available as soon as a workshop stands
    for(Seat* seat : keeperSeats)
    {
        if(seat->getNbRooms(RoomType::workshop) == 0)
            continue;

        uint32_t nbAdded = 0;
        for(uint32_t i = 1; i < static_cast<uint32_t>(SkillType::countSkill); ++i)
        {
            SkillType type = static_cast<SkillType>(i);
            if(!isSkillNamed(type, "trap") || seat->isSkillNotAllowed(type) || Skills::isRewardSkill(type))
                continue;

            if(seat->addSkill(type, false))
                ++nbAdded;
        }

        if(nbAdded > 0)
            sendMessage(seat->getPlayer(), "Your workshop gives you every trap and door.");
    }
}

void SandboxMode::updateScore(const std::vector<Seat*>& keeperSeats)
{
    uint32_t heroesKilled = 0;
    uint32_t itemsMade = 0;
    uint32_t converted = 0;
    uint32_t goldTiles = 0;
    uint32_t entered = 0;
    uint32_t landTiles = 0;
    for(const Seat* seat : keeperSeats)
    {
        const SeatStatistics& statistics = seat->getStatistics();
        heroesKilled += statistics.mHeroesDestroyed;
        itemsMade += statistics.mItemsMade;
        converted += statistics.mCreaturesConverted;
        goldTiles += statistics.mGoldTilesMined;
        entered += statistics.mCreaturesEntered;
        landTiles += seat->getNumClaimedTiles();
    }

    if(!mIsBaselineSet)
    {
        // What the keepers have at the start does not count. The seat counters of the map are
        // computed during the first turns, so the start is taken a few turns later.
        if(mTurnsElapsed < 3)
            return;

        mIsBaselineSet = true;
        mLastHeroesKilled = heroesKilled;
        mLastItemsMade = itemsMade;
        mLastConverted = converted;
        mLastGoldTiles = goldTiles;
        mLastEntered = entered;
        mLastLandTiles = landTiles;
        return;
    }

    int32_t heroPoints = (static_cast<int32_t>(heroesKilled) - static_cast<int32_t>(mLastHeroesKilled)) * SCORE_HERO_KILLED;
    int32_t points = heroPoints
        + (static_cast<int32_t>(itemsMade) - static_cast<int32_t>(mLastItemsMade)) * SCORE_ITEM_MADE
        + (static_cast<int32_t>(converted) - static_cast<int32_t>(mLastConverted)) * SCORE_CREATURE_CONVERTED
        + (static_cast<int32_t>(goldTiles) - static_cast<int32_t>(mLastGoldTiles)) * SCORE_GOLD_TILE
        + (static_cast<int32_t>(entered) - static_cast<int32_t>(mLastEntered)) * SCORE_CREATURE_ENTERED
        + (static_cast<int32_t>(landTiles) - static_cast<int32_t>(mLastLandTiles)) * SCORE_LAND_TILE;

    mLastHeroesKilled = heroesKilled;
    mLastItemsMade = itemsMade;
    mLastConverted = converted;
    mLastGoldTiles = goldTiles;
    mLastEntered = entered;
    mLastLandTiles = landTiles;

    if(mIsWaveActive)
        mWaveHeroPoints += heroPoints;

    mScore += points;
    if(mScore < 0)
        mScore = 0;
}

bool SandboxMode::isBonusReached(const SandboxBonus& bonus, const Seat* seat) const
{
    switch(bonus.mKind)
    {
        case SandboxBonusKind::slaps:
            return seat->getStatistics().mCreaturesSlapped >= bonus.mCount;
        case SandboxBonusKind::allSpells:
        {
            uint32_t nbSpells = 0;
            for(uint32_t i = 1; i < static_cast<uint32_t>(SkillType::countSkill); ++i)
            {
                SkillType type = static_cast<SkillType>(i);
                if(!isSkillNamed(type, "spell") || seat->isSkillNotAllowed(type) || Skills::isRewardSkill(type))
                    continue;

                if(!seat->isSkillDone(type))
                    return false;

                ++nbSpells;
            }
            return nbSpells > 0;
        }
        case SandboxBonusKind::happy:
        case SandboxBonusKind::levelAtLeast:
        case SandboxBonusKind::creatureClass:
        {
            uint32_t minLevel = 0;
            if(bonus.mKind == SandboxBonusKind::levelAtLeast)
                minLevel = static_cast<uint32_t>(Helper::toInt(bonus.mArg));

            uint32_t nbCreatures = 0;
            for(Creature* creature : mGameMap.getCreatures())
            {
                if(!creature->isAlive() || (creature->getSeat() != seat))
                    continue;

                if(bonus.mKind == SandboxBonusKind::happy)
                {
                    if(creature->getMoodValue() == CreatureMoodLevel::Happy)
                        ++nbCreatures;
                }
                else if(bonus.mKind == SandboxBonusKind::levelAtLeast)
                {
                    if(creature->getLevel() >= minLevel)
                        ++nbCreatures;
                }
                else if(creature->getDefinition()->getClassName() == bonus.mArg)
                {
                    ++nbCreatures;
                }
            }
            return nbCreatures >= bonus.mCount;
        }
        case SandboxBonusKind::creatureNamed:
        {
            Creature* creature = mGameMap.getCreature(bonus.mArg);
            return (creature != nullptr) && creature->isAlive() && (creature->getSeat() == seat);
        }
        case SandboxBonusKind::creatures:
            return static_cast<uint32_t>(seat->getNumCreaturesFighters()) >= bonus.mCount;
        case SandboxBonusKind::gold:
            return seat->getGold() >= static_cast<double>(bonus.mCount);
        case SandboxBonusKind::goldTiles:
            return seat->getStatistics().mGoldTilesMined >= bonus.mCount;
        case SandboxBonusKind::trapsFired:
            return seat->getStatistics().mTrapsFired >= bonus.mCount;
        case SandboxBonusKind::prisoners:
        {
            uint32_t nbPrisoners = 0;
            for(Creature* creature : mGameMap.getCreatures())
            {
                if(creature->isAlive() && (creature->getSeatPrison() == seat))
                    ++nbPrisoners;
            }
            return nbPrisoners >= bonus.mCount;
        }
        case SandboxBonusKind::roomTypes:
        {
            std::set<RoomType> types;
            for(Room* room : mGameMap.getRooms())
            {
                if((room->getSeat() != seat) || (room->numCoveredTiles() == 0))
                    continue;

                RoomType type = room->getType();
                if((type != RoomType::dungeonTemple) && (type != RoomType::portal) && (type != RoomType::portalWave))
                    types.insert(type);
            }
            return types.size() >= bonus.mCount;
        }
        case SandboxBonusKind::rooms:
        case SandboxBonusKind::roomTiles:
        {
            RoomType wanted = RoomType::nullRoomType;
            if(bonus.mKind == SandboxBonusKind::roomTiles)
                wanted = RoomManager::getRoomTypeFromRoomName(bonus.mArg);

            uint32_t count = 0;
            for(Room* room : mGameMap.getRooms())
            {
                if((room->getSeat() != seat) || (room->numCoveredTiles() == 0))
                    continue;

                RoomType type = room->getType();
                if(bonus.mKind == SandboxBonusKind::roomTiles)
                {
                    if(type == wanted)
                        count += room->numCoveredTiles();
                }
                else if((type != RoomType::dungeonTemple) && (type != RoomType::portal) && (type != RoomType::portalWave))
                {
                    ++count;
                }
            }
            return count >= bonus.mCount;
        }
        case SandboxBonusKind::traps:
        case SandboxBonusKind::doors:
        {
            bool wantDoors = (bonus.mKind == SandboxBonusKind::doors);
            uint32_t count = 0;
            for(Trap* trap : mGameMap.getTraps())
            {
                if((trap->getSeat() == seat) && (trap->isDoor() == wantDoors))
                    ++count;
            }
            return count >= bonus.mCount;
        }
    }
    return false;
}

void SandboxMode::updateBonuses(const std::vector<Seat*>& keeperSeats)
{
    for(SandboxBonus& bonus : mBonuses)
    {
        if(bonus.mAwarded)
            continue;

        bool isReached = false;
        for(const Seat* seat : keeperSeats)
        {
            if(isBonusReached(bonus, seat))
                isReached = true;
        }

        if(!isReached)
            continue;

        bonus.mAwarded = true;
        mScore += static_cast<int32_t>(bonus.mPoints);
        sendMessage("Bonus objective reached: " + bonus.mText + " (+" + Helper::toString(bonus.mPoints) + " points)");
    }
}

void SandboxMode::updateRealmComplete()
{
    if((mTarget == 0) || mIsRealmComplete)
        return;

    if(mScore < static_cast<int32_t>(mTarget))
    {
        mSecondsAtTarget = 0;
        return;
    }

    ++mSecondsAtTarget;
    if(mSecondsAtTarget < SECONDS_AT_TARGET)
        return;

    mIsRealmComplete = true;
    std::string text = "Congratulations, Keeper! You have reached " + Helper::toString(mTarget)
        + " points and mastered this realm.";
    if(!mNextLevel.empty())
        text += " Do you want to proceed to the next realm? If you stay, you can keep building here.";

    sendMessage(text);
    for(Player* player : mGameMap.getPlayers())
    {
        if(!player->getIsHuman())
            continue;

        ServerNotification* serverNotification = new ServerNotification(
            ServerNotificationType::sandboxRealmComplete, player);
        serverNotification->mPacket << mRealmId << mNextLevel << text;
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }
}

void SandboxMode::sendStatus()
{
    int32_t secondsLeft = 0;
    std::string nextRoom = getNextRoomName(secondsLeft);
    uint32_t bonusMask = getBonusMask();
    if(mIsStatusSent && (mSentScore == mScore) && (mSentSecondsLeft == secondsLeft) && (mSentBonusMask == bonusMask) &&
       (mSentNextRoom == nextRoom))
    {
        return;
    }

    mIsStatusSent = true;
    mSentScore = mScore;
    mSentSecondsLeft = secondsLeft;
    mSentBonusMask = bonusMask;
    mSentNextRoom = nextRoom;

    for(Player* player : mGameMap.getPlayers())
    {
        if(!player->getIsHuman())
            continue;

        ServerNotification* serverNotification = new ServerNotification(
            ServerNotificationType::sandboxStatus, player);
        serverNotification->mPacket << mScore << static_cast<int32_t>(mTarget) << nextRoom << secondsLeft;
        serverNotification->mPacket << static_cast<uint32_t>(mBonuses.size());
        for(const SandboxBonus& bonus : mBonuses)
            serverNotification->mPacket << bonus.mText << static_cast<int32_t>(bonus.mPoints) << bonus.mAwarded;

        ODServer::getSingleton().queueServerNotification(serverNotification);
    }
}

void SandboxMode::doTurn()
{
    ++mTurnsElapsed;
    std::vector<Seat*> keeperSeats = getKeeperSeats();
    updateScore(keeperSeats);

    --mTurnsBeforeSecond;
    if(mTurnsBeforeSecond <= 0)
    {
        mTurnsBeforeSecond = static_cast<int32_t>(ODApplication::turnsPerSecond);
        updateUnlocks(keeperSeats);
        updateBonuses(keeperSeats);
        updateRealmComplete();
        sendStatus();
    }

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
        std::string beaten = "Wave " + Helper::toString(mNbWavesLaunched) + " was beaten.";
        if(mWaveHeroPoints > 0)
            beaten += " Reward: " + Helper::toString(mWaveHeroPoints) + " points.";

        sendMessage(beaten);
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
