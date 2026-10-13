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

#ifndef SANDBOXMODE_H
#define SANDBOXMODE_H

#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

class GameMap;
class Player;
class Seat;

//! \brief What a bonus objective of a realm asks for
enum class SandboxBonusKind
{
    //! count: slaps given to the creatures of the keeper
    slaps,
    //! Every spell the level allows is researched
    allSpells,
    //! count: creatures that are happy
    happy,
    //! count: rooms built (the dungeon heart and the portals do not count)
    rooms,
    //! count: creatures of at least the level in arg
    levelAtLeast,
    //! count: fighting creatures
    creatures,
    //! count: traps placed
    traps,
    //! count: doors placed
    doors,
    //! count: tiles of the rooms of the type named in arg
    roomTiles,
    //! count: creatures of the class named in arg
    creatureClass,
    //! The keeper owns the creature of the level with the name in arg (a neutral creature that has to be claimed)
    creatureNamed,
    //! count: gold the keeper holds at the same time
    gold,
    //! count: gold tiles the workers of the keeper dug out completely
    goldTiles,
    //! count: different room types the keeper owns (the dungeon heart and the portals do not count)
    roomTypes,
    //! count: prisoners the keeper holds at the same time
    prisoners,
    //! count: times a trap of the keeper fired
    trapsFired
};

//! \brief An objective that gives extra points once, as the two sub-objectives of a realm do
struct SandboxBonus
{
    SandboxBonus() :
        mKind(SandboxBonusKind::slaps),
        mCount(0),
        mPoints(0),
        mAwarded(false)
    {}

    SandboxBonusKind mKind;
    uint32_t mCount;
    uint32_t mPoints;
    //! \brief Room type name (roomTiles), creature class (creatureClass) or level (levelAtLeast)
    std::string mArg;
    //! \brief Text of the objective, shown to the player
    std::string mText;
    bool mAwarded;
};

//! \brief Server side logic of the sandbox mode: a level without a rival keeper where the
//! player builds freely, takes heroes of a chosen level in the hand (hero toolbox) and calls
//! hero invasions on demand through the hero portal of the map (the wave portal).
//! The mode also keeps the score of the sandbox: points for heroes killed, land owned, gold
//! mined, items made, creatures that arrive and creatures converted, plus the bonus objectives
//! of the level. A realm (a sandbox level with a target) is complete once the score reaches the
//! target. The waves are not saved: a loaded game starts again with wave 1. The score, the
//! awarded bonuses and the time for the room unlocks are saved in the [Info] block of the level file.
class SandboxMode
{
public:
    //! \brief Number of waves of a hero invasion
    static const uint32_t NB_WAVES;
    //! \brief Number of heroes in a wave
    static const uint32_t NB_HEROES_PER_WAVE;
    //! \brief Highest level of a hero (the level selector of the toolbox goes from 1 to this)
    static const uint32_t MAX_HERO_LEVEL;

    //! \brief Score values of the sandbox: a hero killed, a tile of land owned (also lost
    //! again when the tile is lost), a gold tile mined, an item made in a workshop, a creature
    //! coming through a portal and a creature converted in the torture chamber
    static const int32_t SCORE_HERO_KILLED;
    static const int32_t SCORE_LAND_TILE;
    static const int32_t SCORE_GOLD_TILE;
    static const int32_t SCORE_ITEM_MADE;
    static const int32_t SCORE_CREATURE_ENTERED;
    static const int32_t SCORE_CREATURE_CONVERTED;

    SandboxMode(GameMap& gameMap);

    //! \brief Forgets the waves, the toolbox heroes, the score and the realm settings of the level
    void reset();

    //! \brief Reads a line of the [Info] block of a level file that belongs to the sandbox mode
    //! (SandboxRealm, SandboxTarget, SandboxBonus, SandboxNext, SandboxState). Returns false if the line is not one of them.
    bool importInfoLine(const std::string& line);

    //! \brief Writes the sandbox lines of the [Info] block
    void exportInfo(std::ostream& os) const;

    //! \brief Puts a hero of the given class and level in the hand of the player. Only one
    //! hero per class can be alive at the same time.
    void takeHero(Player* player, const std::string& className, uint32_t level);

    //! \brief Starts a single wave, or all the waves one after the other if continual is true.
    //! Nothing happens while a wave is still fighting.
    void startInvasion(Player* player, bool continual);

    //! \brief Counts the score, unlocks the rooms and checks if the current wave is beaten and
    //! launches the next one of a continual invasion
    void doTurn();

    //! \brief Tells if the given seat belongs to the hero faction
    static bool isHeroSeat(const Seat* seat);

    inline int32_t getScore() const
    { return mScore; }

    //! \brief The score a realm needs to be complete, 0 for a level without a target
    inline uint32_t getTarget() const
    { return mTarget; }

    //! \brief The name the progress of the player knows this realm by, empty for a level that is not a realm
    inline const std::string& getRealmId() const
    { return mRealmId; }

    //! \brief The level file (relative to the levels folder) of the next realm, empty if there is none
    inline const std::string& getNextLevel() const
    { return mNextLevel; }

    inline const std::vector<SandboxBonus>& getBonuses() const
    { return mBonuses; }

    inline bool isRealmComplete() const
    { return mIsRealmComplete; }

    //! \brief The room of the unlock order that is not available yet and comes next, as a name
    //! for the player. Empty if every room is available. secondsLeft gets the seconds until it is
    //! available (0 when it is already due).
    std::string getNextRoomName(int32_t& secondsLeft) const;

private:
    GameMap& mGameMap;
    //! \brief Number of waves already launched. The next one is mNbWavesLaunched + 1.
    uint32_t mNbWavesLaunched;
    bool mIsContinual;
    //! \brief True from the launch of a wave until its last hero is dead
    bool mIsWaveActive;
    //! \brief Turns before the next wave of a continual invasion is launched
    int32_t mTurnsBeforeNextWave;
    int32_t mTurnsBeforeCheck;
    //! \brief Turns since the sandbox started (restored from a saved game): the room unlocks go by this time
    int64_t mTurnsElapsed;
    //! \brief True once the first message about the starting rooms was sent
    bool mIsIntroSent;
    //! \brief Names of the heroes of the current wave that are still alive
    std::vector<std::string> mWaveHeroes;
    //! \brief Name of the living toolbox hero for each class
    std::map<std::string, std::string> mToolboxHeroes;

    //! \brief The score and the realm settings
    int32_t mScore;
    uint32_t mTarget;
    std::string mRealmId;
    std::string mNextLevel;
    std::vector<SandboxBonus> mBonuses;
    bool mIsRealmComplete;
    //! \brief Seconds the score has been at or above the target
    uint32_t mSecondsAtTarget;
    //! \brief Turns before the next once-per-second check
    int32_t mTurnsBeforeSecond;
    //! \brief The counters of the keepers at the last check, to count what is new
    bool mIsBaselineSet;
    uint32_t mLastHeroesKilled;
    uint32_t mLastItemsMade;
    uint32_t mLastConverted;
    uint32_t mLastGoldTiles;
    uint32_t mLastEntered;
    uint32_t mLastLandTiles;
    //! \brief Points for the heroes killed since the current wave started
    int32_t mWaveHeroPoints;
    //! \brief What was sent to the clients last, to send again only on a change
    bool mIsStatusSent;
    int32_t mSentScore;
    int32_t mSentSecondsLeft;
    uint32_t mSentBonusMask;
    std::string mSentNextRoom;

    Seat* getHeroSeat() const;
    //! \brief The seats of the human keepers
    std::vector<Seat*> getKeeperSeats() const;
    //! \brief Makes the rooms of the unlock order available when their time has come, and all
    //! the traps and doors once a workshop stands. A level that already has everything available
    //! at the start has nothing to unlock.
    void updateUnlocks(const std::vector<Seat*>& keeperSeats);
    //! \brief Adds the points for what happened since the last check
    void updateScore(const std::vector<Seat*>& keeperSeats);
    //! \brief Awards the bonus objectives that are reached
    void updateBonuses(const std::vector<Seat*>& keeperSeats);
    bool isBonusReached(const SandboxBonus& bonus, const Seat* seat) const;
    //! \brief Tells the realm is complete once the score stayed at the target for a moment
    void updateRealmComplete();
    //! \brief Sends the score and the room timer to the clients when they changed
    void sendStatus();
    uint32_t getBonusMask() const;
    //! \brief Launches the given wave. Returns false if it could not be launched.
    bool launchWave(uint32_t waveNumber);
    //! \brief Sends a message to every human player
    void sendMessage(const std::string& message) const;
    void sendMessage(Player* player, const std::string& message) const;
};

#endif // SANDBOXMODE_H
