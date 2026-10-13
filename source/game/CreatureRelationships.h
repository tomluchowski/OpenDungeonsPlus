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

#ifndef CREATURERELATIONSHIPS_H
#define CREATURERELATIONSHIPS_H

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <map>
#include <string>
#include <utility>
#include <vector>

//! \brief Relationship level between two creatures of the same keeper.
//! The numeric order is used on the network, only append new values at the end.
enum class RelationshipTier
{
    nemesis = 0,
    hated,
    neutral,
    friends,
    bestFriends,
    lovers
};

//! \brief Kinds of events that change a relationship value.
enum class RelationshipEvent
{
    //! Both creatures trained in the same room at the same time
    trainingTogether,
    //! Both creatures took part in defeating enemies
    defeatedEnemiesTogether,
    //! The first creature lost against the second one in the arena
    arenaLoss,
    //! The second creature took away the chicken the first one wanted
    chickenSnatched
};

//! \brief Values of the relationship system. The defaults are used when
//! config/relationships.cfg is missing or lacks an entry.
struct RelationshipSettings
{
    //! Lowest and highest value a pair can have
    static const int32_t VALUE_MIN;
    static const int32_t VALUE_MAX;

    //! \brief Creates the settings with the default values of the game.
    RelationshipSettings();

    //! \brief Reads the settings from the key/value pairs of config/relationships.cfg.
    //! Keys that are missing or invalid keep their default; this is logged once
    //! per process.
    static RelationshipSettings fromConfig(const std::map<std::string, std::string>& config);

    int32_t mThresholdFriends;
    int32_t mThresholdBestFriends;
    int32_t mThresholdLovers;
    int32_t mThresholdHated;
    int32_t mThresholdNemesis;

    //! Amount a value moves towards 0 per drift step
    int32_t mDriftAmount;
    //! Number of turns between two drift steps
    int64_t mDriftIntervalTurns;
    //! A pair only drifts when it had no event for this many turns
    int64_t mDriftIdleTurns;

    int32_t mEventTrainingTogether;
    int32_t mEventDefeatedEnemiesTogether;
    int32_t mEventArenaLoss;
    int32_t mEventChickenSnatched;
    //! Training together counts at most once per pair in this number of turns (one training cycle)
    int64_t mTrainingTogetherCooldownTurns;
    //! Creatures that hit the same enemy within this number of turns took part in defeating it
    int64_t mFightParticipantTurns;

    //! Two creatures fight side by side when both fight within this many tiles of each other
    double mCombatRadiusTiles;
    //! Defense added to every defense value of a creature that fights next to a friend / best friend
    double mCombatBonusFriends;
    double mCombatBonusBestFriends;
    //! Defense taken away while a nemesis fights next to the creature
    double mCombatPenaltyNemesis;

    //! Mood points lost for each hated / nemesis creature of the same keeper
    int32_t mMoodPenaltyHated;
    int32_t mMoodPenaltyNemesis;
    //! At most this many hated creatures count for the mood
    int32_t mMoodMaxPairs;

    //! Limits per creature, the weakest relationship of a tier falls back when it is exceeded.
    //! Partners (lovers) are counted separately from the friends.
    int32_t mMaxFriends;
    int32_t mMaxPartners;
    int32_t mMaxNemeses;

    //! Nemesis brawls: how often a pair is checked, the chance for a brawl per check (percent),
    //! the largest distance in tiles at which they notice each other, the health (percent of the
    //! maximum) at which a brawl stops, the longest duration and the change of the value afterwards
    int64_t mBrawlCheckIntervalTurns;
    int32_t mBrawlChancePercent;
    int32_t mBrawlMaxDistanceTiles;
    int32_t mBrawlStopHealthPercent;
    int64_t mBrawlMaxTurns;
    int32_t mBrawlValueChange;

    //! \brief Levels a friend training in the same room must be higher to mentor the creature
    int32_t mMentorMinLevelDiff;

    //! \brief Extra experience in percent a mentored creature gets while training
    int32_t mMentorXpBonusPercent;

    //! \brief Temporary mood points from relationship events (grief, ...) are capped at this size
    int32_t mTempMoodMax;

    //! \brief Points of the temporary mood that fade each turn
    int32_t mTempMoodDecayPerTurn;

    //! \brief Mood points a creature loses when a friend dies
    int32_t mGriefMoodPenalty;

    //! \brief Turns a grieving creature deals more damage to the side of the killer
    int64_t mGriefRageTurns;

    //! \brief Extra damage in percent a grieving creature deals to the side of the killer
    int32_t mGriefRageBonusPercent;

    //! \brief Jealousy: when a creature becomes friends with another one, the friends of the first
    //! creature lose this many points towards the newcomer (0 switches it off). A partner of
    //! the first creature loses mLoversJealousyValueLoss points instead.
    int32_t mJealousyValueLoss;

    //! \brief Points a partner of the first creature loses towards the newcomer (0 switches it off)
    int32_t mLoversJealousyValueLoss;

    //! \brief Lovers: a pair of lovers breaks up when its value falls below mLoversBreakValue (lower than
    //! mThresholdLovers, so the pair stays lovers in between)
    int32_t mLoversBreakValue;

    //! \brief Defense added to every defense value of a creature that fights next to its partner
    double mCombatBonusLovers;

    //! \brief Mood points a creature gets while its partner is alive and in the dungeon
    int32_t mMoodBonusLovers;

    //! \brief Mood points a creature loses when its partner dies (instead of mGriefMoodPenalty)
    int32_t mPartnerGriefMoodPenalty;

    //! \brief Chance (percent) that the partner of a creature that leaves the dungeon unhappy leaves with it
    int32_t mLeavePartnerChancePercent;

    //! Chance (percent) that the best friend of a creature that leaves the dungeon unhappy leaves with it
    int32_t mLeaveTogetherChancePercent;

    //! \brief Mood points a creature gets while a friend sleeps in a bed close by
    int32_t mSleepNextToFriendMood;

    //! \brief Largest distance in tiles between two beds that counts as close by
    int32_t mNeighbourBedTiles;

    //! \brief Mood points a creature gets while a friend eats at the same time close by
    int32_t mEatTogetherMood;

    //! \brief Largest distance in tiles between two eating creatures that counts as together
    int32_t mEatTogetherTiles;

    //! \brief Mood points the friends that see a slap lose
    int32_t mSlapFriendsMoodPenalty;

    //! Start value of a pair of creature classes (sorted pair of class names), see config
    //! entries "Racial_<ClassA>_<ClassB>".
    std::map<std::pair<std::string, std::string>, int32_t> mRacialStart;

    //! \brief Start value for a pair of creature classes, 0 if the table has no entry.
    int32_t getRacialStart(const std::string& classA, const std::string& classB) const;
};

//! \brief A change of the tier of a pair, to be sent to the clients.
struct RelationshipTierChange
{
    std::string mCreatureA;
    std::string mCreatureB;
    RelationshipTier mOldTier;
    RelationshipTier mNewTier;
};

//! \brief Relationship state of one creature that is not part of the pair table and has to be
//! saved with a game: fading grief mood, rage against a seat, a running nemesis brawl. Durations
//! are stored as the number of turns that are left, because the turn counter starts at 0 again
//! when a game is loaded.
struct RelationshipCreatureState
{
    RelationshipCreatureState() :
        mGriefMood(0),
        mRageTurnsLeft(0),
        mRageSeatId(-1),
        mBrawlTurnsLeft(0)
    {}

    //! True if there is nothing to save
    bool isEmpty() const
    { return (mGriefMood == 0) && (mRageTurnsLeft <= 0) && mBrawlOpponent.empty(); }

    std::string mName;
    //! Temporary mood points (negative for grief), fading each turn
    int32_t mGriefMood;
    //! Turns the rage after the death of a friend lasts, and the id of the seat it is against
    int64_t mRageTurnsLeft;
    int32_t mRageSeatId;
    //! Name of the creature of a running brawl (empty if none) and the turns it can still last
    std::string mBrawlOpponent;
    int64_t mBrawlTurnsLeft;
};

//! \brief Writes one tab separated line per creature state (creature name, grief mood, rage turns
//! left, rage seat id, brawl opponent, brawl turns left). States that are empty are skipped.
void writeRelationshipCreatureStates(std::ostream& os, const std::vector<RelationshipCreatureState>& states);

//! \brief Reads lines written by writeRelationshipCreatureStates until the line "[/RelationshipState]".
//! \returns false if the section is invalid.
bool readRelationshipCreatureStates(std::istream& is, std::vector<RelationshipCreatureState>& states);

//! \brief Relationship values between the creatures of a game map.
//!
//! Owned by the game map and only created when the option is switched on. The
//! pairs are stored sparsely: only pairs whose value is not 0 exist. The server
//! map holds the real values; a client map only holds one representative value
//! per tier that was sent by the server (see setTier).
class CreatureRelationships
{
public:
    typedef std::pair<std::string, std::string> Pair;
    //! \brief Returns the gender of the creature with the given name: "Male", "Female" or empty
    typedef std::function<std::string(const std::string&)> GenderLookup;

    //! \brief Creates an empty table with the default settings.
    CreatureRelationships();
    //! \brief Creates an empty table with the given settings.
    explicit CreatureRelationships(const RelationshipSettings& settings);

    //! \brief Returns the settings (thresholds, drift, event amounts) of this table.
    const RelationshipSettings& getSettings() const
    { return mSettings; }

    //! \brief Server side: tells how to find the gender of a creature. Only a pair of one "Male"
    //! and one "Female" creature can become lovers; without a lookup nobody does.
    //! \param lookup The function that returns the gender of the creature with a given name
    void setGenderLookup(const GenderLookup& lookup)
    { mGenderLookup = lookup; }

    //! \brief Single entry point for all gameplay hooks. Changes the value of the
    //! pair according to the amount configured for the event.
    //! When the pair has no value yet, the racial start value of the two classes is applied first
    //! (classes may be left empty to skip that).
    void onRelationshipEvent(RelationshipEvent event, const std::string& creatureA,
        const std::string& creatureB, int64_t turn, const std::string& classA = std::string(),
        const std::string& classB = std::string());

    //! \brief Adds amount to the pair (clamped to -100..100). A value of 0 removes the pair.
    void changeValue(const std::string& creatureA, const std::string& creatureB,
        int32_t amount, int64_t turn);

    //! \brief Returns the value of the pair or 0 if there is none.
    int32_t getValue(const std::string& creatureA, const std::string& creatureB) const;

    //! \brief Tier for a value. Lovers additionally need loversAllowed
    //! (one male and one female creature), otherwise they stay best friends.
    RelationshipTier tierOfValue(int32_t value, bool loversAllowed) const;
    //! \brief Tier of a pair. A pair is lovers when it carries the stored lovers flag and loversAllowed is
    //! set; otherwise it is at best best friends.
    RelationshipTier tierOf(const std::string& creatureA, const std::string& creatureB,
        bool loversAllowed = false) const;
    //! \brief True if the pair is currently lovers (stored flag).
    bool isLovers(const std::string& creatureA, const std::string& creatureB) const;
    //! \brief The lover of the creature or an empty string.
    std::string getPartner(const std::string& creature) const;

    //! \brief True if the pair is at least friends.
    bool isFriend(const std::string& creatureA, const std::string& creatureB) const;
    //! \brief True for the tier nemesis.
    bool isNemesis(const std::string& creatureA, const std::string& creatureB) const;
    //! \brief True for the tiers hated and nemesis.
    bool isHated(const std::string& creatureA, const std::string& creatureB) const;

    //! \brief Defense modifier of a creature that fights next to the creatures in nearbyFighters
    //! (the caller selects them: same keeper, fighting, within the combat radius). The best bonus
    //! of a friend counts once, a nemesis nearby takes the penalty away again.
    double combatModifier(const std::string& creature, const std::vector<std::string>& nearbyFighters) const;

    //! \brief Mood points (zero or negative) the creature gets from the creatures it hates.
    int32_t moodModifier(const std::string& creature) const;

    //! \brief Factor (1.0 or more) for the experience a creature gets while training: a friend among the
    //! trainees that is at least mMentorMinLevelDiff levels higher raises it by mMentorXpBonusPercent percent.
    //! \param creature Name of the training creature
    //! \param level Level of the training creature
    //! \param trainees Name and level of the trainees (the caller selects them: same keeper, same room)
    //! \return 1.0 if no friend mentors the creature, else 1.0 plus the bonus
    double mentoringFactor(const std::string& creature, uint32_t level,
        const std::vector<std::pair<std::string, uint32_t> >& trainees) const;

    //! \brief Returns the strongest best friend (value at least the best friends threshold) of the
    //! creature, or an empty string if it has none.
    //! \param creature The creature to look at
    std::string getBestFriend(const std::string& creature) const;

    //! \brief Lists the creatures that are friends (or better) of the creature.
    void getFriends(const std::string& creature, std::vector<std::string>& friends) const;

    //! \brief Lists the creatures that have a value with the creature, with the value.
    void getPartners(const std::string& creature, std::vector<std::pair<std::string, int32_t> >& partners) const;

    //! \brief Lists the pairs of nemeses.
    void getNemesisPairs(std::vector<Pair>& pairs) const;

    //! \brief Removes every pair of the creature (death, leaving, conversion).
    void removeCreature(const std::string& creature);

    //! \brief Low-frequency update, called once per turn on the server. It only
    //! works every mDriftIntervalTurns turns: pairs without a recent event move
    //! by mDriftAmount towards 0.
    void doTurn(int64_t turn);

    //! \brief Moves the tier changes recorded since the last call to changes.
    void takeTierChanges(std::vector<RelationshipTierChange>& changes);

    //! \brief Client side: stores the tier sent by the server.
    void setTier(const std::string& creatureA, const std::string& creatureB, RelationshipTier tier);

    //! \brief Returns the number of pairs that are stored (pairs with value 0 do not exist).
    size_t getNbPairs() const
    { return mPairs.size(); }

    //! \brief Lists every pair that is not neutral with its tier.
    void getTiers(std::vector<RelationshipTierChange>& tiers) const;

    //! \brief Writes the pairs, one line per pair: creatureA, creatureB, value and
    //! the number of turns since the last event (tab separated).
    void writeToStream(std::ostream& os, int64_t turn) const;

    //! \brief Reads lines written by writeToStream until the line "[/Relationships]".
    //! \returns false if the section is invalid.
    bool readFromStream(std::istream& is, int64_t turn);

private:
    struct PairData
    {
        int32_t mValue;
        int64_t mLastEventTurn;
        //! \brief The pair is lovers; stays set down to mLoversBreakValue
        bool mLovers;
    };

    //! \brief Builds the key of a pair; the order of the two names does not matter.
    static Pair makePair(const std::string& creatureA, const std::string& creatureB);
    //! \brief Stores the value (clamped; 0 removes the pair). isEvent tells that the value came from
    //! an event, which restarts the drift idle time of the pair.
    void setValue(const Pair& pair, int32_t value, int64_t turn, bool isEvent);
    //! \brief Lets the weakest relationship of a tier fall back while the creature has too many of it
    void enforceLimits(const std::string& creature, int64_t turn);
    //! \brief Friends of creature get jealous of newFriend, which just became its friend
    //! \param creature The creature whose friends get jealous
    //! \param newFriend The creature that just became a friend of creature
    //! \param turn The current turn
    void applyJealousy(const std::string& creature, const std::string& newFriend, int64_t turn);
    //! \brief Records a tier change of the pair if the old and the new tier differ.
    void recordTierChange(const Pair& pair, RelationshipTier oldTier, RelationshipTier newTier);

    //! \brief Tier of the stored data of a pair: lovers if the flag is set, else the tier of the value
    static RelationshipTier tierOfData(const CreatureRelationships& self, const PairData& data);
    //! \brief True if one creature is "Male" and the other "Female"
    bool haveLoversGenders(const Pair& pair) const;
    //! \brief True if the creature can have another partner of the given value (free slot or weaker partner)
    bool canTakeLover(const std::string& creature, int32_t value) const;
    //! \brief Pair becomes lovers when it qualifies
    void updateLovers(const Pair& pair);
    //! \brief Ends the lovers state of the pair (sets the flag false, records the tier change)
    void clearLovers(const Pair& pair, PairData& data);

    //! \brief Returns one value that lies in the given tier (used by clients that only know the tier).
    int32_t representativeValue(RelationshipTier tier) const;

    RelationshipSettings mSettings;
    std::map<Pair, PairData> mPairs;
    //! Turn of the last counted training event per pair
    std::map<Pair, int64_t> mLastTrainingTurn;
    std::vector<RelationshipTierChange> mTierChanges;
    int64_t mLastDriftTurn;

    //! \brief Finds the gender of a creature by its name (empty: nobody can become lovers)
    GenderLookup mGenderLookup;
};

//! \brief What decides whether a creature may be part of a relationship (see Creature::canHaveRelationships).
//! The option must be on, the creature must be on the server map and belong to a keeper seat
//! (no rogue, no hero), and it must not be a worker or a prisoner.
bool relationshipsAllowed(bool optionOnServerMap, bool hasSeat, bool rogueSeat, bool heroFaction,
    bool worker, bool inPrison);

//! \brief A gameplay event only changes a pair when both creatures may have relationships and
//! belong to the same seat (see Creature::reportRelationshipEvent).
bool relationshipEventAllowed(bool firstAllowed, bool secondAllowed, bool sameSeat);

//! \brief True if one of the coworkers (the caller selects them: same seat, not the creature itself)
//! is hated by the creature or is its nemesis (see Room::hasHatedCoworker). Without relationships
//! (creatureAllowed false) nobody is hated.
bool anyHatedCoworker(const CreatureRelationships& relationships, bool creatureAllowed,
    const std::string& creature, const std::vector<std::string>& coworkers);

//! \brief The creature stays in a room it does not like only if no other suitable room exists
//! (see CreatureActionSearchJob).
bool useDislikedRoomAsFallback(bool haveDislikedRoom, size_t nbOtherSuitableRooms);

//! \brief State of a creature that is checked before a nemesis brawl (see Creature::canStartBrawl).
struct BrawlCandidateState
{
    BrawlCandidateState() :
        mAllowed(false),
        mOnMap(false),
        mAlive(false),
        mKo(false),
        mBrawling(false),
        mHasTile(false),
        mFighting(false),
        mInArenaOrCasino(false),
        mHp(0.0),
        mMaxHp(0.0)
    {}

    //! The creature may have relationships at all
    bool mAllowed;
    //! The creature is on the map
    bool mOnMap;
    bool mAlive;
    //! The creature is knocked out
    bool mKo;
    //! The creature already brawls
    bool mBrawling;
    //! The creature stands on a tile
    bool mHasTile;
    //! Has a fight or friendly fight action queued
    bool mFighting;
    //! The creature stands in an arena or a casino
    bool mInArenaOrCasino;
    //! Current and maximum health
    double mHp;
    double mMaxHp;
};

//! \brief True if the creature may start a brawl: not busy, not hurt (health must be more than
//! the stop percentage plus the start margin of the maximum).
bool canStartBrawl(const BrawlCandidateState& state, const RelationshipSettings& settings);

//! \brief State of one fighter of a running brawl (see Creature::updateBrawl).
struct BrawlFighterState
{
    BrawlFighterState() :
        mAlive(false),
        mKo(false),
        mHp(0.0),
        mMaxHp(0.0)
    {}

    bool mAlive;
    //! The fighter is knocked out
    bool mKo;
    //! Current and maximum health
    double mHp;
    double mMaxHp;
};

//! \brief True if a running brawl ends: someone died or is knocked out, someone is at or
//! below the stop percentage of the maximum health, the brawl lasted mBrawlMaxTurns turns or
//! the fight action of the creature is gone.
bool shouldStopBrawl(const BrawlFighterState& first, const BrawlFighterState& second, int64_t turnsSinceStart,
    bool stillFighting, const RelationshipSettings& settings);

//! \brief Brawls are only looked for every mBrawlCheckIntervalTurns turns.
bool isBrawlCheckTurn(int64_t turn, const RelationshipSettings& settings);

//! \brief True if the two creatures (distance given as tile differences) are close enough to notice each other.
bool isWithinBrawlDistance(double dx, double dy, const RelationshipSettings& settings);

//! \brief True if a roll of 0 to 99 starts a brawl.
bool brawlChanceHit(int32_t roll, const RelationshipSettings& settings);

//! \brief Mood points a creature loses when a friend of it dies: the partner grieves more than a friend.
//! \param partner True if the dead creature was the partner of the mourner.
//! \param settings The settings of the relationships.
int32_t getGriefMoodPenalty(bool partner, const RelationshipSettings& settings);

//! \brief Chance in percent that the partner or the best friend of a creature that leaves the dungeon unhappy
//! leaves with it.
//! \param partner True if the one that may follow is the partner, false if it is the best friend.
//! \param settings The settings of the relationships.
int32_t getLeaveTogetherChancePercent(bool partner, const RelationshipSettings& settings);

#endif // CREATURERELATIONSHIPS_H
