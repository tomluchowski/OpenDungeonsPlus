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
    //! Partners belong to the lovers tier, which is not active yet.
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
    //! creature lose this many points towards the newcomer (0 switches it off). The break-up of
    //! partners needs the lovers tier and is not part of it yet.
    int32_t mJealousyValueLoss;

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

    //! \brief Creates an empty table with the default settings.
    CreatureRelationships();
    //! \brief Creates an empty table with the given settings.
    explicit CreatureRelationships(const RelationshipSettings& settings);

    //! \brief Returns the settings (thresholds, drift, event amounts) of this table.
    const RelationshipSettings& getSettings() const
    { return mSettings; }

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
    //! \brief Tier of the pair (neutral if there is none); see tierOfValue for loversAllowed.
    RelationshipTier tierOf(const std::string& creatureA, const std::string& creatureB,
        bool loversAllowed = false) const;
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

    //! \brief Records a tier change of the pair if the old and the new value are in different tiers.
    void recordTierChange(const Pair& pair, int32_t oldValue, int32_t newValue);
    //! \brief Returns one value that lies in the given tier (used by clients that only know the tier).
    int32_t representativeValue(RelationshipTier tier) const;

    RelationshipSettings mSettings;
    std::map<Pair, PairData> mPairs;
    //! Turn of the last counted training event per pair
    std::map<Pair, int64_t> mLastTrainingTurn;
    std::vector<RelationshipTierChange> mTierChanges;
    int64_t mLastDriftTurn;
};

#endif // CREATURERELATIONSHIPS_H
