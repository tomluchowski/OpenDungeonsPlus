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

#include "game/CreatureRelationships.h"

#include "utils/LogManager.h"

#include <algorithm>
#include <cstdlib>
#include <istream>
#include <ostream>
#include <set>
#include <sstream>

const int32_t RelationshipSettings::VALUE_MIN = -100;
const int32_t RelationshipSettings::VALUE_MAX = 100;

namespace
{
    //! Saved creature states: number of tab separated fields of a line and the position of each of them
    static const size_t STATE_FIELD_COUNT = 6;
    static const size_t STATE_FIELD_GRIEF = 1;
    static const size_t STATE_FIELD_RAGE_TURNS = 2;
    static const size_t STATE_FIELD_RAGE_SEAT = 3;
    static const size_t STATE_FIELD_BRAWL_OPPONENT = 4;
    static const size_t STATE_FIELD_BRAWL_TURNS = 5;
    //! A saved grief mood is limited to this many points in both directions, whatever the file says
    static const int64_t STATE_GRIEF_LIMIT = 100000;
    //! A creature only starts a brawl if its health is this many percent above the health that stops the brawl
    static const int32_t BRAWL_START_HEALTH_MARGIN_PERCENT = 25;
    //! Factor between a fraction and percent, used to compare health with the percent settings of the brawl
    static const double BRAWL_PERCENT_FACTOR = 100.0;

    //! Default values used when config/relationships.cfg is missing or lacks an entry
    static const int32_t DEFAULT_THRESHOLD_FRIENDS = 50;
    static const int32_t DEFAULT_THRESHOLD_BEST_FRIENDS = 80;
    static const int32_t DEFAULT_THRESHOLD_LOVERS = 90;
    static const int32_t DEFAULT_THRESHOLD_HATED = -50;
    static const int32_t DEFAULT_THRESHOLD_NEMESIS = -80;
    static const int32_t DEFAULT_DRIFT_AMOUNT = 1;
    static const int64_t DEFAULT_DRIFT_INTERVAL_TURNS = 30;
    static const int64_t DEFAULT_DRIFT_IDLE_TURNS = 420;
    static const int32_t DEFAULT_EVENT_TRAINING_TOGETHER = 3;
    static const int32_t DEFAULT_EVENT_DEFEATED_ENEMIES_TOGETHER = 8;
    static const int32_t DEFAULT_EVENT_ARENA_LOSS = -10;
    static const int32_t DEFAULT_EVENT_CHICKEN_SNATCHED = -12;
    //! Training together counts once per pair in this many turns (one training cycle)
    static const int64_t DEFAULT_TRAINING_TOGETHER_COOLDOWN_TURNS = 40;
    //! Creatures that hit the same enemy within this many turns took part in defeating it
    static const int64_t DEFAULT_FIGHT_PARTICIPANT_TURNS = 100;
    //! Side by side defense: radius in tiles, bonus for friends and best friends, penalty for a nemesis nearby
    static const double DEFAULT_COMBAT_RADIUS_TILES = 3.0;
    static const double DEFAULT_COMBAT_BONUS_FRIENDS = 0.75;
    static const double DEFAULT_COMBAT_BONUS_BEST_FRIENDS = 1.5;
    static const double DEFAULT_COMBAT_PENALTY_NEMESIS = 0.75;
    //! Mood points lost per hated or nemesis creature, and how many of them count
    static const int32_t DEFAULT_MOOD_PENALTY_HATED = 250;
    static const int32_t DEFAULT_MOOD_PENALTY_NEMESIS = 400;
    static const int32_t DEFAULT_MOOD_MAX_PAIRS = 2;
    //! Limits per creature of each tier
    static const int32_t DEFAULT_MAX_FRIENDS = 3;
    static const int32_t DEFAULT_MAX_PARTNERS = 1;
    static const int32_t DEFAULT_MAX_NEMESES = 2;
    //! Nemesis brawls: check interval, chance per check (percent), notice distance in tiles, stop health
    //! (percent), longest duration (turns) and the change of the value afterwards
    static const int64_t DEFAULT_BRAWL_CHECK_INTERVAL_TURNS = 20;
    static const int32_t DEFAULT_BRAWL_CHANCE_PERCENT = 6;
    static const int32_t DEFAULT_BRAWL_MAX_DISTANCE_TILES = 6;
    static const int32_t DEFAULT_BRAWL_STOP_HEALTH_PERCENT = 25;
    static const int64_t DEFAULT_BRAWL_MAX_TURNS = 150;
    static const int32_t DEFAULT_BRAWL_VALUE_CHANGE = -6;
    //! Default levels a friend must be higher to mentor a training creature, and the extra experience in percent
    static const int32_t DEFAULT_MENTOR_MIN_LEVEL_DIFF = 2;
    static const int32_t DEFAULT_MENTOR_XP_BONUS_PERCENT = 50;
    //! Default cap of the temporary relationship mood and the points it fades each turn
    static const int32_t DEFAULT_TEMP_MOOD_MAX = 1000;
    static const int32_t DEFAULT_TEMP_MOOD_DECAY_PER_TURN = 3;
    //! Default mood points lost for a dead friend, turns of rage afterwards and the extra damage in percent
    static const int32_t DEFAULT_GRIEF_MOOD_PENALTY = 400;
    static const int64_t DEFAULT_GRIEF_RAGE_TURNS = 300;
    static const int32_t DEFAULT_GRIEF_RAGE_BONUS_PERCENT = 30;
    //! Default points the friends of a creature lose towards a newcomer it befriends
    static const int32_t DEFAULT_JEALOUSY_VALUE_LOSS = 3;
    //! Default chance in percent that the best friend of an unhappy leaver leaves with it
    static const int32_t DEFAULT_LEAVE_TOGETHER_CHANCE_PERCENT = 30;
    //! Default mood points for sleeping next to a friend, and the distance in tiles that counts as next to it
    static const int32_t DEFAULT_SLEEP_NEXT_TO_FRIEND_MOOD = 150;
    static const int32_t DEFAULT_NEIGHBOUR_BED_TILES = 3;
    //! Default mood points for eating together with a friend, and the distance in tiles that counts as together
    static const int32_t DEFAULT_EAT_TOGETHER_MOOD = 100;
    static const int32_t DEFAULT_EAT_TOGETHER_TILES = 6;
    //! Default mood points the friends that see a slap lose
    static const int32_t DEFAULT_SLAP_FRIENDS_MOOD_PENALTY = 150;
    //! Default points a partner loses towards the newcomer a creature befriends, and the value below which lovers break up
    static const int32_t DEFAULT_LOVERS_JEALOUSY_VALUE_LOSS = 8;
    static const int32_t DEFAULT_LOVERS_BREAK_VALUE = 70;
    //! Default defense of a creature that fights next to its partner
    static const double DEFAULT_COMBAT_BONUS_LOVERS = 2.5;
    //! Default mood bonus while the partner is alive, mood malus when it dies, and the chance (percent) it leaves with a leaver
    static const int32_t DEFAULT_MOOD_BONUS_LOVERS = 300;
    static const int32_t DEFAULT_PARTNER_GRIEF_MOOD_PENALTY = 800;
    static const int32_t DEFAULT_LEAVE_PARTNER_CHANCE_PERCENT = 70;
    //! Largest absolute value of a decimal config setting; larger values are cut to it
    static const double DECIMAL_SETTING_LIMIT = 1000.0;

    //! Largest absolute value a config setting may have; larger values are cut to it
    static const int64_t SETTING_LIMIT = 1000000;
    //! Number of tab separated fields of a saved pair line: creatureA, creatureB, value, age in turns
    static const size_t NB_SAVE_FIELDS = 4;
    //! Number of fields of a saved pair that also has the lovers flag
    static const size_t NB_SAVE_FIELDS_WITH_LOVERS = 5;

    //! \brief Reads a whole text as an integer. Returns false and leaves value untouched if the text
    //! is not exactly one integer.
    bool parseInt(const std::string& text, int64_t& value)
    {
        std::istringstream ss(text);
        int64_t tmp;
        if(!(ss >> tmp))
            return false;

        std::string rest;
        if(ss >> rest)
            return false;

        value = tmp;
        return true;
    }

    //! \brief Reads a whole text as a decimal number. Returns false and leaves value untouched if the text
    //! is not exactly one number.
    bool parseDouble(const std::string& text, double& value)
    {
        std::istringstream ss(text);
        double tmp;
        if(!(ss >> tmp))
            return false;

        std::string rest;
        if(ss >> rest)
            return false;

        value = tmp;
        return true;
    }

    //! Reads key from config. Returns false and leaves value untouched if it is missing or invalid.
    bool readSetting(const std::map<std::string, std::string>& config, const std::string& key, int64_t& value)
    {
        std::map<std::string, std::string>::const_iterator it = config.find(key);
        if(it == config.end())
            return false;

        return parseInt(it->second, value);
    }

    //! Reads key from config as a decimal number. Returns false and leaves value untouched if it is missing or invalid.
    bool readSetting(const std::map<std::string, std::string>& config, const std::string& key, double& value)
    {
        std::map<std::string, std::string>::const_iterator it = config.find(key);
        if(it == config.end())
            return false;

        return parseDouble(it->second, value);
    }
}

RelationshipSettings::RelationshipSettings() :
    mThresholdFriends(DEFAULT_THRESHOLD_FRIENDS),
    mThresholdBestFriends(DEFAULT_THRESHOLD_BEST_FRIENDS),
    mThresholdLovers(DEFAULT_THRESHOLD_LOVERS),
    mThresholdHated(DEFAULT_THRESHOLD_HATED),
    mThresholdNemesis(DEFAULT_THRESHOLD_NEMESIS),
    mDriftAmount(DEFAULT_DRIFT_AMOUNT),
    mDriftIntervalTurns(DEFAULT_DRIFT_INTERVAL_TURNS),
    mDriftIdleTurns(DEFAULT_DRIFT_IDLE_TURNS),
    mEventTrainingTogether(DEFAULT_EVENT_TRAINING_TOGETHER),
    mEventDefeatedEnemiesTogether(DEFAULT_EVENT_DEFEATED_ENEMIES_TOGETHER),
    mEventArenaLoss(DEFAULT_EVENT_ARENA_LOSS),
    mEventChickenSnatched(DEFAULT_EVENT_CHICKEN_SNATCHED),
    mTrainingTogetherCooldownTurns(DEFAULT_TRAINING_TOGETHER_COOLDOWN_TURNS),
    mFightParticipantTurns(DEFAULT_FIGHT_PARTICIPANT_TURNS),
    mCombatRadiusTiles(DEFAULT_COMBAT_RADIUS_TILES),
    mCombatBonusFriends(DEFAULT_COMBAT_BONUS_FRIENDS),
    mCombatBonusBestFriends(DEFAULT_COMBAT_BONUS_BEST_FRIENDS),
    mCombatPenaltyNemesis(DEFAULT_COMBAT_PENALTY_NEMESIS),
    mMoodPenaltyHated(DEFAULT_MOOD_PENALTY_HATED),
    mMoodPenaltyNemesis(DEFAULT_MOOD_PENALTY_NEMESIS),
    mMoodMaxPairs(DEFAULT_MOOD_MAX_PAIRS),
    mMaxFriends(DEFAULT_MAX_FRIENDS),
    mMaxPartners(DEFAULT_MAX_PARTNERS),
    mMaxNemeses(DEFAULT_MAX_NEMESES),
    mBrawlCheckIntervalTurns(DEFAULT_BRAWL_CHECK_INTERVAL_TURNS),
    mBrawlChancePercent(DEFAULT_BRAWL_CHANCE_PERCENT),
    mBrawlMaxDistanceTiles(DEFAULT_BRAWL_MAX_DISTANCE_TILES),
    mBrawlStopHealthPercent(DEFAULT_BRAWL_STOP_HEALTH_PERCENT),
    mBrawlMaxTurns(DEFAULT_BRAWL_MAX_TURNS),
    mBrawlValueChange(DEFAULT_BRAWL_VALUE_CHANGE),
    mMentorMinLevelDiff(DEFAULT_MENTOR_MIN_LEVEL_DIFF),
    mMentorXpBonusPercent(DEFAULT_MENTOR_XP_BONUS_PERCENT),
    mTempMoodMax(DEFAULT_TEMP_MOOD_MAX),
    mTempMoodDecayPerTurn(DEFAULT_TEMP_MOOD_DECAY_PER_TURN),
    mGriefMoodPenalty(DEFAULT_GRIEF_MOOD_PENALTY),
    mGriefRageTurns(DEFAULT_GRIEF_RAGE_TURNS),
    mGriefRageBonusPercent(DEFAULT_GRIEF_RAGE_BONUS_PERCENT),
    mJealousyValueLoss(DEFAULT_JEALOUSY_VALUE_LOSS),
    mLoversJealousyValueLoss(DEFAULT_LOVERS_JEALOUSY_VALUE_LOSS),
    mLoversBreakValue(DEFAULT_LOVERS_BREAK_VALUE),
    mCombatBonusLovers(DEFAULT_COMBAT_BONUS_LOVERS),
    mMoodBonusLovers(DEFAULT_MOOD_BONUS_LOVERS),
    mPartnerGriefMoodPenalty(DEFAULT_PARTNER_GRIEF_MOOD_PENALTY),
    mLeavePartnerChancePercent(DEFAULT_LEAVE_PARTNER_CHANCE_PERCENT),
    mLeaveTogetherChancePercent(DEFAULT_LEAVE_TOGETHER_CHANCE_PERCENT),
    mSleepNextToFriendMood(DEFAULT_SLEEP_NEXT_TO_FRIEND_MOOD),
    mNeighbourBedTiles(DEFAULT_NEIGHBOUR_BED_TILES),
    mEatTogetherMood(DEFAULT_EAT_TOGETHER_MOOD),
    mEatTogetherTiles(DEFAULT_EAT_TOGETHER_TILES),
    mSlapFriendsMoodPenalty(DEFAULT_SLAP_FRIENDS_MOOD_PENALTY)
{
}

int32_t RelationshipSettings::getRacialStart(const std::string& classA, const std::string& classB) const
{
    std::pair<std::string, std::string> key = (classA < classB) ?
        std::pair<std::string, std::string>(classA, classB) : std::pair<std::string, std::string>(classB, classA);
    std::map<std::pair<std::string, std::string>, int32_t>::const_iterator it = mRacialStart.find(key);
    if(it == mRacialStart.end())
        return 0;

    return it->second;
}

RelationshipSettings RelationshipSettings::fromConfig(const std::map<std::string, std::string>& config)
{
    RelationshipSettings settings;

    struct IntEntry
    {
        const char* mKey;
        int32_t* mTarget;
    };
    struct TurnEntry
    {
        const char* mKey;
        int64_t* mTarget;
    };

    IntEntry intEntries[] =
    {
        {"ThresholdFriends", &settings.mThresholdFriends},
        {"ThresholdBestFriends", &settings.mThresholdBestFriends},
        {"ThresholdLovers", &settings.mThresholdLovers},
        {"ThresholdHated", &settings.mThresholdHated},
        {"ThresholdNemesis", &settings.mThresholdNemesis},
        {"DriftAmount", &settings.mDriftAmount},
        {"EventTrainingTogether", &settings.mEventTrainingTogether},
        {"EventDefeatedEnemiesTogether", &settings.mEventDefeatedEnemiesTogether},
        {"EventArenaLoss", &settings.mEventArenaLoss},
        {"EventChickenSnatched", &settings.mEventChickenSnatched},
        {"MoodPenaltyHated", &settings.mMoodPenaltyHated},
        {"MoodPenaltyNemesis", &settings.mMoodPenaltyNemesis},
        {"MoodMaxPairs", &settings.mMoodMaxPairs},
        {"MaxFriends", &settings.mMaxFriends},
        {"MaxPartners", &settings.mMaxPartners},
        {"MaxNemeses", &settings.mMaxNemeses},
        {"BrawlChancePercent", &settings.mBrawlChancePercent},
        {"BrawlMaxDistanceTiles", &settings.mBrawlMaxDistanceTiles},
        {"BrawlStopHealthPercent", &settings.mBrawlStopHealthPercent},
        {"BrawlValueChange", &settings.mBrawlValueChange},
        {"MentorMinLevelDiff", &settings.mMentorMinLevelDiff},
        {"MentorXpBonusPercent", &settings.mMentorXpBonusPercent},
        {"TempMoodMax", &settings.mTempMoodMax},
        {"TempMoodDecayPerTurn", &settings.mTempMoodDecayPerTurn},
        {"GriefMoodPenalty", &settings.mGriefMoodPenalty},
        {"GriefRageBonusPercent", &settings.mGriefRageBonusPercent},
        {"JealousyValueLoss", &settings.mJealousyValueLoss},
        {"LoversJealousyValueLoss", &settings.mLoversJealousyValueLoss},
        {"LoversBreakValue", &settings.mLoversBreakValue},
        {"MoodBonusLovers", &settings.mMoodBonusLovers},
        {"PartnerGriefMoodPenalty", &settings.mPartnerGriefMoodPenalty},
        {"LeavePartnerChancePercent", &settings.mLeavePartnerChancePercent},
        {"LeaveTogetherChancePercent", &settings.mLeaveTogetherChancePercent},
        {"SleepNextToFriendMood", &settings.mSleepNextToFriendMood},
        {"NeighbourBedTiles", &settings.mNeighbourBedTiles},
        {"EatTogetherMood", &settings.mEatTogetherMood},
        {"EatTogetherTiles", &settings.mEatTogetherTiles},
        {"SlapFriendsMoodPenalty", &settings.mSlapFriendsMoodPenalty}
    };
    struct DoubleEntry
    {
        const char* mKey;
        double* mTarget;
    };
    DoubleEntry doubleEntries[] =
    {
        {"CombatRadiusTiles", &settings.mCombatRadiusTiles},
        {"CombatBonusFriends", &settings.mCombatBonusFriends},
        {"CombatBonusBestFriends", &settings.mCombatBonusBestFriends},
        {"CombatBonusLovers", &settings.mCombatBonusLovers},
        {"CombatPenaltyNemesis", &settings.mCombatPenaltyNemesis}
    };
    TurnEntry turnEntries[] =
    {
        {"DriftIntervalTurns", &settings.mDriftIntervalTurns},
        {"DriftIdleTurns", &settings.mDriftIdleTurns},
        {"TrainingTogetherCooldownTurns", &settings.mTrainingTogetherCooldownTurns},
        {"FightParticipantTurns", &settings.mFightParticipantTurns},
        {"BrawlCheckIntervalTurns", &settings.mBrawlCheckIntervalTurns},
        {"BrawlMaxTurns", &settings.mBrawlMaxTurns},
        {"GriefRageTurns", &settings.mGriefRageTurns}
    };

    std::string missing;
    for(IntEntry& entry : intEntries)
    {
        int64_t value;
        if(!readSetting(config, entry.mKey, value))
        {
            missing += std::string(" ") + entry.mKey;
            continue;
        }
        *entry.mTarget = static_cast<int32_t>(std::max<int64_t>(-SETTING_LIMIT, std::min<int64_t>(SETTING_LIMIT, value)));
    }
    for(DoubleEntry& entry : doubleEntries)
    {
        double value;
        if(!readSetting(config, entry.mKey, value))
        {
            missing += std::string(" ") + entry.mKey;
            continue;
        }
        *entry.mTarget = std::max(0.0, std::min(DECIMAL_SETTING_LIMIT, value));
    }
    for(TurnEntry& entry : turnEntries)
    {
        int64_t value;
        if(!readSetting(config, entry.mKey, value) || (value < 1))
        {
            missing += std::string(" ") + entry.mKey;
            continue;
        }
        *entry.mTarget = value;
    }

    // Racial start values: "Racial_<ClassA>_<ClassB>  <value>"
    static const std::string racialPrefix = "Racial_";
    for(std::map<std::string, std::string>::const_iterator it = config.begin(); it != config.end(); ++it)
    {
        if(it->first.compare(0, racialPrefix.size(), racialPrefix) != 0)
            continue;

        std::string classes = it->first.substr(racialPrefix.size());
        std::string::size_type pos = classes.find('_');
        int64_t value;
        if((pos == std::string::npos) || (pos == 0) || (pos + 1 >= classes.size())
            || !parseInt(it->second, value))
        {
            missing += " " + it->first;
            continue;
        }

        std::string classA = classes.substr(0, pos);
        std::string classB = classes.substr(pos + 1);
        if(classB < classA)
            std::swap(classA, classB);
        value = std::max<int64_t>(RelationshipSettings::VALUE_MIN, std::min<int64_t>(RelationshipSettings::VALUE_MAX, value));
        settings.mRacialStart[std::pair<std::string, std::string>(classA, classB)] = static_cast<int32_t>(value);
    }

    // Logged once per process, not once per game
    static bool warned = false;
    if(!missing.empty() && !warned)
    {
        warned = true;
        OD_LOG_WRN("relationships.cfg: missing or invalid entries, using defaults for:" + missing);
    }

    return settings;
}

CreatureRelationships::CreatureRelationships() :
    mLastDriftTurn(0)
{
}

CreatureRelationships::CreatureRelationships(const RelationshipSettings& settings) :
    mSettings(settings),
    mLastDriftTurn(0)
{
}

CreatureRelationships::Pair CreatureRelationships::makePair(const std::string& creatureA, const std::string& creatureB)
{
    if(creatureA < creatureB)
        return Pair(creatureA, creatureB);

    return Pair(creatureB, creatureA);
}

void CreatureRelationships::onRelationshipEvent(RelationshipEvent event, const std::string& creatureA,
    const std::string& creatureB, int64_t turn, const std::string& classA, const std::string& classB)
{
    // A creature has no relationship with itself
    if(creatureA == creatureB)
        return;

    Pair pair = makePair(creatureA, creatureB);
    if(event == RelationshipEvent::trainingTogether)
    {
        // Counts once per training cycle, not once per blow
        std::map<Pair, int64_t>::iterator itTraining = mLastTrainingTurn.find(pair);
        if((itTraining != mLastTrainingTurn.end()) &&
           ((turn - itTraining->second) < mSettings.mTrainingTogetherCooldownTurns))
        {
            return;
        }
        mLastTrainingTurn[pair] = turn;
    }

    // The racial start value is applied when the pair first gets a value
    if(!classA.empty() && !classB.empty() && (mPairs.find(pair) == mPairs.end()))
    {
        int32_t start = mSettings.getRacialStart(classA, classB);
        if(start != 0)
            changeValue(creatureA, creatureB, start, turn);
    }

    int32_t amount = 0;
    switch(event)
    {
        case RelationshipEvent::trainingTogether:
            amount = mSettings.mEventTrainingTogether;
            break;
        case RelationshipEvent::defeatedEnemiesTogether:
            amount = mSettings.mEventDefeatedEnemiesTogether;
            break;
        case RelationshipEvent::arenaLoss:
            amount = mSettings.mEventArenaLoss;
            break;
        case RelationshipEvent::chickenSnatched:
            amount = mSettings.mEventChickenSnatched;
            break;
    }

    changeValue(creatureA, creatureB, amount, turn);
}

void CreatureRelationships::changeValue(const std::string& creatureA, const std::string& creatureB,
    int32_t amount, int64_t turn)
{
    // A creature has no relationship with itself
    if(creatureA == creatureB)
        return;

    Pair pair = makePair(creatureA, creatureB);
    int32_t value = amount;
    int32_t oldValue = 0;
    std::map<Pair, PairData>::const_iterator it = mPairs.find(pair);
    if(it != mPairs.end())
    {
        oldValue = it->second.mValue;
        value += oldValue;
    }

    setValue(pair, value, turn, true);
    updateLovers(pair);
    enforceLimits(creatureA, turn);
    enforceLimits(creatureB, turn);

    // Becoming friends makes the friends of both creatures jealous
    if(((mSettings.mJealousyValueLoss > 0) || (mSettings.mLoversJealousyValueLoss > 0))
       && (oldValue < mSettings.mThresholdFriends) && isFriend(creatureA, creatureB))
    {
        applyJealousy(creatureA, creatureB, turn);
        applyJealousy(creatureB, creatureA, turn);
    }
}

void CreatureRelationships::applyJealousy(const std::string& creature, const std::string& newFriend, int64_t turn)
{
    std::vector<std::string> friends;
    getFriends(creature, friends);
    for(size_t i = 0; i < friends.size(); ++i)
    {
        if(friends[i] == newFriend)
            continue;

        // A partner is hurt more than a friend
        int32_t loss = isLovers(creature, friends[i]) ? mSettings.mLoversJealousyValueLoss : mSettings.mJealousyValueLoss;
        if(loss > 0)
            changeValue(friends[i], newFriend, -loss, turn);
    }
}

void CreatureRelationships::enforceLimits(const std::string& creature, int64_t turn)
{
    std::vector<std::pair<std::string, int32_t> > partners;
    getPartners(creature, partners);
    int32_t nbFriends = 0;
    int32_t nbNemeses = 0;
    int32_t nbLovers = 0;
    for(size_t i = 0; i < partners.size(); ++i)
    {
        if(isLovers(creature, partners[i].first))
            ++nbLovers;
        else if(partners[i].second >= mSettings.mThresholdFriends)
            ++nbFriends;
        else if(partners[i].second <= mSettings.mThresholdNemesis)
            ++nbNemeses;
    }

    while(nbLovers > mSettings.mMaxPartners)
    {
        // The weakest couple falls back to best friends
        size_t weakest = partners.size();
        for(size_t i = 0; i < partners.size(); ++i)
        {
            if(!isLovers(creature, partners[i].first))
                continue;
            if((weakest == partners.size()) || (partners[i].second < partners[weakest].second))
                weakest = i;
        }
        if(weakest == partners.size())
            break;

        Pair pair = makePair(creature, partners[weakest].first);
        std::map<Pair, PairData>::iterator it = mPairs.find(pair);
        it->second.mValue = std::min(it->second.mValue, mSettings.mThresholdLovers - 1);
        clearLovers(pair, it->second);
        partners[weakest].second = it->second.mValue;
        --nbLovers;
    }

    while(nbFriends > mSettings.mMaxFriends)
    {
        // The weakest friend falls back to a good acquaintance
        size_t weakest = partners.size();
        for(size_t i = 0; i < partners.size(); ++i)
        {
            if((partners[i].second < mSettings.mThresholdFriends) || isLovers(creature, partners[i].first))
                continue;
            if((weakest == partners.size()) || (partners[i].second < partners[weakest].second))
                weakest = i;
        }
        if(weakest == partners.size())
            break;

        setValue(makePair(creature, partners[weakest].first), mSettings.mThresholdFriends - 1, turn, false);
        partners[weakest].second = mSettings.mThresholdFriends - 1;
        --nbFriends;
    }

    while(nbNemeses > mSettings.mMaxNemeses)
    {
        // The weakest nemesis falls back to a hated creature
        size_t weakest = partners.size();
        for(size_t i = 0; i < partners.size(); ++i)
        {
            if(partners[i].second > mSettings.mThresholdNemesis)
                continue;
            if((weakest == partners.size()) || (partners[i].second > partners[weakest].second))
                weakest = i;
        }
        if(weakest == partners.size())
            break;

        setValue(makePair(creature, partners[weakest].first), mSettings.mThresholdNemesis + 1, turn, false);
        partners[weakest].second = mSettings.mThresholdNemesis + 1;
        --nbNemeses;
    }
}

void CreatureRelationships::setValue(const Pair& pair, int32_t value, int64_t turn, bool isEvent)
{
    value = std::max(RelationshipSettings::VALUE_MIN, std::min(RelationshipSettings::VALUE_MAX, value));

    RelationshipTier oldTier = RelationshipTier::neutral;
    std::map<Pair, PairData>::iterator it = mPairs.find(pair);
    if(it != mPairs.end())
        oldTier = tierOfData(*this, it->second);

    if(value == 0)
    {
        if(it != mPairs.end())
            mPairs.erase(it);
    }
    else if(it == mPairs.end())
    {
        PairData data;
        data.mValue = value;
        data.mLastEventTurn = turn;
        data.mLovers = false;
        mPairs[pair] = data;
    }
    else
    {
        it->second.mValue = value;
        if(isEvent)
            it->second.mLastEventTurn = turn;
        // Lovers break up below the break value
        if(it->second.mLovers && (value < mSettings.mLoversBreakValue))
            it->second.mLovers = false;
    }

    RelationshipTier newTier = RelationshipTier::neutral;
    it = mPairs.find(pair);
    if(it != mPairs.end())
        newTier = tierOfData(*this, it->second);
    recordTierChange(pair, oldTier, newTier);
}

RelationshipTier CreatureRelationships::tierOfData(const CreatureRelationships& self, const PairData& data)
{
    if(data.mLovers)
        return RelationshipTier::lovers;

    return self.tierOfValue(data.mValue, false);
}

bool CreatureRelationships::haveLoversGenders(const Pair& pair) const
{
    if(!mGenderLookup)
        return false;

    std::string genderA = mGenderLookup(pair.first);
    std::string genderB = mGenderLookup(pair.second);
    return ((genderA == "Male") && (genderB == "Female")) || ((genderA == "Female") && (genderB == "Male"));
}

bool CreatureRelationships::canTakeLover(const std::string& creature, int32_t value) const
{
    if(mSettings.mMaxPartners <= 0)
        return false;

    int32_t nbLovers = 0;
    int32_t weakest = RelationshipSettings::VALUE_MAX;
    for(std::map<Pair, PairData>::const_iterator it = mPairs.begin(); it != mPairs.end(); ++it)
    {
        if(!it->second.mLovers || ((it->first.first != creature) && (it->first.second != creature)))
            continue;

        ++nbLovers;
        weakest = std::min(weakest, it->second.mValue);
    }

    // A weaker partner is replaced by the stronger new couple
    return (nbLovers < mSettings.mMaxPartners) || (value > weakest);
}

void CreatureRelationships::updateLovers(const Pair& pair)
{
    std::map<Pair, PairData>::iterator it = mPairs.find(pair);
    if((it == mPairs.end()) || it->second.mLovers || (it->second.mValue < mSettings.mThresholdLovers))
        return;

    if(!haveLoversGenders(pair) || !canTakeLover(pair.first, it->second.mValue)
       || !canTakeLover(pair.second, it->second.mValue))
    {
        return;
    }

    RelationshipTier oldTier = tierOfData(*this, it->second);
    it->second.mLovers = true;
    recordTierChange(pair, oldTier, RelationshipTier::lovers);
}

void CreatureRelationships::clearLovers(const Pair& pair, PairData& data)
{
    data.mLovers = false;
    recordTierChange(pair, RelationshipTier::lovers, tierOfValue(data.mValue, false));
}

void CreatureRelationships::recordTierChange(const Pair& pair, RelationshipTier oldTier, RelationshipTier newTier)
{
    if(oldTier == newTier)
        return;

    RelationshipTierChange change;
    change.mCreatureA = pair.first;
    change.mCreatureB = pair.second;
    change.mOldTier = oldTier;
    change.mNewTier = newTier;
    mTierChanges.push_back(change);
}

int32_t CreatureRelationships::getValue(const std::string& creatureA, const std::string& creatureB) const
{
    std::map<Pair, PairData>::const_iterator it = mPairs.find(makePair(creatureA, creatureB));
    if(it == mPairs.end())
        return 0;

    return it->second.mValue;
}

RelationshipTier CreatureRelationships::tierOfValue(int32_t value, bool loversAllowed) const
{
    if(loversAllowed && (value >= mSettings.mThresholdLovers))
        return RelationshipTier::lovers;
    if(value >= mSettings.mThresholdBestFriends)
        return RelationshipTier::bestFriends;
    if(value >= mSettings.mThresholdFriends)
        return RelationshipTier::friends;
    if(value <= mSettings.mThresholdNemesis)
        return RelationshipTier::nemesis;
    if(value <= mSettings.mThresholdHated)
        return RelationshipTier::hated;

    return RelationshipTier::neutral;
}

RelationshipTier CreatureRelationships::tierOf(const std::string& creatureA, const std::string& creatureB,
    bool loversAllowed) const
{
    std::map<Pair, PairData>::const_iterator it = mPairs.find(makePair(creatureA, creatureB));
    if(it == mPairs.end())
        return RelationshipTier::neutral;

    if(loversAllowed && it->second.mLovers)
        return RelationshipTier::lovers;

    return tierOfValue(it->second.mValue, false);
}

bool CreatureRelationships::isLovers(const std::string& creatureA, const std::string& creatureB) const
{
    std::map<Pair, PairData>::const_iterator it = mPairs.find(makePair(creatureA, creatureB));
    return (it != mPairs.end()) && it->second.mLovers;
}

std::string CreatureRelationships::getPartner(const std::string& creature) const
{
    for(std::map<Pair, PairData>::const_iterator it = mPairs.begin(); it != mPairs.end(); ++it)
    {
        if(!it->second.mLovers)
            continue;
        if(it->first.first == creature)
            return it->first.second;
        if(it->first.second == creature)
            return it->first.first;
    }

    return std::string();
}

bool CreatureRelationships::isFriend(const std::string& creatureA, const std::string& creatureB) const
{
    return getValue(creatureA, creatureB) >= mSettings.mThresholdFriends;
}

bool CreatureRelationships::isNemesis(const std::string& creatureA, const std::string& creatureB) const
{
    return getValue(creatureA, creatureB) <= mSettings.mThresholdNemesis;
}

bool CreatureRelationships::isHated(const std::string& creatureA, const std::string& creatureB) const
{
    return getValue(creatureA, creatureB) <= mSettings.mThresholdHated;
}

double CreatureRelationships::combatModifier(const std::string& creature,
    const std::vector<std::string>& nearbyFighters) const
{
    double bonus = 0.0;
    bool nemesisNearby = false;
    for(size_t i = 0; i < nearbyFighters.size(); ++i)
    {
        if(nearbyFighters[i] == creature)
            continue;

        int32_t value = getValue(creature, nearbyFighters[i]);
        if(isLovers(creature, nearbyFighters[i]))
            bonus = std::max(bonus, mSettings.mCombatBonusLovers);
        else if(value >= mSettings.mThresholdBestFriends)
            bonus = std::max(bonus, mSettings.mCombatBonusBestFriends);
        else if(value >= mSettings.mThresholdFriends)
            bonus = std::max(bonus, mSettings.mCombatBonusFriends);
        else if(value <= mSettings.mThresholdNemesis)
            nemesisNearby = true;
    }

    if(nemesisNearby)
        bonus -= mSettings.mCombatPenaltyNemesis;

    return bonus;
}

double CreatureRelationships::mentoringFactor(const std::string& creature, uint32_t level,
    const std::vector<std::pair<std::string, uint32_t> >& trainees) const
{
    for(size_t i = 0; i < trainees.size(); ++i)
    {
        if(trainees[i].first == creature)
            continue;

        if((trainees[i].second >= level + static_cast<uint32_t>(std::max<int32_t>(0, mSettings.mMentorMinLevelDiff)))
           && isFriend(creature, trainees[i].first))
        {
            return 1.0 + static_cast<double>(std::max<int32_t>(0, mSettings.mMentorXpBonusPercent)) / 100.0;
        }
    }

    return 1.0;
}

int32_t CreatureRelationships::moodModifier(const std::string& creature) const
{
    std::vector<std::pair<std::string, int32_t> > partners;
    getPartners(creature, partners);
    int32_t nbHated = 0;
    int32_t modifier = 0;
    for(size_t i = 0; i < partners.size(); ++i)
    {
        // The partner is a pure bonus and does not count against the hated limit
        if(isLovers(creature, partners[i].first))
        {
            modifier += mSettings.mMoodBonusLovers;
            continue;
        }

        if(nbHated >= mSettings.mMoodMaxPairs)
            continue;

        if(partners[i].second <= mSettings.mThresholdNemesis)
            modifier -= mSettings.mMoodPenaltyNemesis;
        else if(partners[i].second <= mSettings.mThresholdHated)
            modifier -= mSettings.mMoodPenaltyHated;
        else
            continue;

        ++nbHated;
    }

    return modifier;
}

std::string CreatureRelationships::getBestFriend(const std::string& creature) const
{
    std::vector<std::pair<std::string, int32_t> > partners;
    getPartners(creature, partners);
    std::string best;
    int32_t bestValue = mSettings.mThresholdBestFriends - 1;
    for(size_t i = 0; i < partners.size(); ++i)
    {
        if(partners[i].second > bestValue)
        {
            best = partners[i].first;
            bestValue = partners[i].second;
        }
    }

    return best;
}

void CreatureRelationships::getFriends(const std::string& creature, std::vector<std::string>& friends) const
{
    friends.clear();
    std::vector<std::pair<std::string, int32_t> > partners;
    getPartners(creature, partners);
    for(size_t i = 0; i < partners.size(); ++i)
    {
        if(partners[i].second >= mSettings.mThresholdFriends)
            friends.push_back(partners[i].first);
    }
}

void CreatureRelationships::getPartners(const std::string& creature,
    std::vector<std::pair<std::string, int32_t> >& partners) const
{
    partners.clear();
    for(std::map<Pair, PairData>::const_iterator it = mPairs.begin(); it != mPairs.end(); ++it)
    {
        if(it->first.first == creature)
            partners.push_back(std::pair<std::string, int32_t>(it->first.second, it->second.mValue));
        else if(it->first.second == creature)
            partners.push_back(std::pair<std::string, int32_t>(it->first.first, it->second.mValue));
    }
}

void CreatureRelationships::getNemesisPairs(std::vector<Pair>& pairs) const
{
    pairs.clear();
    for(std::map<Pair, PairData>::const_iterator it = mPairs.begin(); it != mPairs.end(); ++it)
    {
        if(it->second.mValue <= mSettings.mThresholdNemesis)
            pairs.push_back(it->first);
    }
}

void CreatureRelationships::removeCreature(const std::string& creature)
{
    std::map<Pair, int64_t>::iterator itTraining = mLastTrainingTurn.begin();
    while(itTraining != mLastTrainingTurn.end())
    {
        if((itTraining->first.first == creature) || (itTraining->first.second == creature))
            itTraining = mLastTrainingTurn.erase(itTraining);
        else
            ++itTraining;
    }

    std::map<Pair, PairData>::iterator it = mPairs.begin();
    while(it != mPairs.end())
    {
        if((it->first.first == creature) || (it->first.second == creature))
            it = mPairs.erase(it);
        else
            ++it;
    }
}

void CreatureRelationships::doTurn(int64_t turn)
{
    if((turn - mLastDriftTurn) < mSettings.mDriftIntervalTurns)
        return;

    mLastDriftTurn = turn;
    std::map<Pair, int64_t>::iterator itTraining = mLastTrainingTurn.begin();
    while(itTraining != mLastTrainingTurn.end())
    {
        if((turn - itTraining->second) >= mSettings.mTrainingTogetherCooldownTurns)
            itTraining = mLastTrainingTurn.erase(itTraining);
        else
            ++itTraining;
    }

    std::map<Pair, PairData>::iterator it = mPairs.begin();
    while(it != mPairs.end())
    {
        if((turn - it->second.mLastEventTurn) < mSettings.mDriftIdleTurns)
        {
            ++it;
            continue;
        }

        int32_t oldValue = it->second.mValue;
        RelationshipTier oldTier = tierOfData(*this, it->second);
        int32_t newValue = oldValue;
        if(oldValue > 0)
            newValue = std::max<int32_t>(0, oldValue - mSettings.mDriftAmount);
        else
            newValue = std::min<int32_t>(0, oldValue + mSettings.mDriftAmount);

        if(newValue == 0)
        {
            recordTierChange(it->first, oldTier, RelationshipTier::neutral);
            it = mPairs.erase(it);
            continue;
        }

        it->second.mValue = newValue;
        if(it->second.mLovers && (newValue < mSettings.mLoversBreakValue))
            it->second.mLovers = false;
        recordTierChange(it->first, oldTier, tierOfData(*this, it->second));
        ++it;
    }
}

void CreatureRelationships::takeTierChanges(std::vector<RelationshipTierChange>& changes)
{
    changes.clear();
    changes.swap(mTierChanges);
}

int32_t CreatureRelationships::representativeValue(RelationshipTier tier) const
{
    switch(tier)
    {
        case RelationshipTier::nemesis:
            return mSettings.mThresholdNemesis;
        case RelationshipTier::hated:
            return mSettings.mThresholdHated;
        case RelationshipTier::friends:
            return mSettings.mThresholdFriends;
        case RelationshipTier::bestFriends:
            return mSettings.mThresholdBestFriends;
        case RelationshipTier::lovers:
            return mSettings.mThresholdLovers;
        case RelationshipTier::neutral:
            break;
    }
    return 0;
}

void CreatureRelationships::setTier(const std::string& creatureA, const std::string& creatureB, RelationshipTier tier)
{
    if(creatureA == creatureB)
        return;

    Pair pair = makePair(creatureA, creatureB);
    int32_t value = representativeValue(tier);
    if(value == 0)
    {
        mPairs.erase(pair);
        return;
    }

    PairData data;
    data.mValue = value;
    data.mLastEventTurn = 0;
    data.mLovers = (tier == RelationshipTier::lovers);
    mPairs[pair] = data;
}

void CreatureRelationships::getTiers(std::vector<RelationshipTierChange>& tiers) const
{
    tiers.clear();
    for(std::map<Pair, PairData>::const_iterator it = mPairs.begin(); it != mPairs.end(); ++it)
    {
        RelationshipTier tier = tierOfData(*this, it->second);
        if(tier == RelationshipTier::neutral)
            continue;

        RelationshipTierChange entry;
        entry.mCreatureA = it->first.first;
        entry.mCreatureB = it->first.second;
        entry.mOldTier = RelationshipTier::neutral;
        entry.mNewTier = tier;
        tiers.push_back(entry);
    }
}

void CreatureRelationships::writeToStream(std::ostream& os, int64_t turn) const
{
    for(std::map<Pair, PairData>::const_iterator it = mPairs.begin(); it != mPairs.end(); ++it)
    {
        int64_t age = std::max<int64_t>(0, turn - it->second.mLastEventTurn);
        os << it->first.first << "\t" << it->first.second << "\t" << it->second.mValue << "\t" << age << "\t"
           << (it->second.mLovers ? 1 : 0) << "\n";
    }
}

bool CreatureRelationships::readFromStream(std::istream& is, int64_t turn)
{
    mPairs.clear();
    mTierChanges.clear();
    std::set<Pair> legacy;

    std::string line;
    while(true)
    {
        if(!is.good())
            return false;

        std::getline(is, line);
        if(!line.empty() && (line[line.size() - 1] == '\r'))
            line.erase(line.size() - 1);

        if(line == "[/Relationships]")
        {
            // Pairs of old saves have no lovers flag: they are lovers when they qualify now
            for(std::map<Pair, PairData>::iterator it = mPairs.begin(); it != mPairs.end(); ++it)
            {
                if(legacy.count(it->first) != 0)
                    updateLovers(it->first);
            }
            // The limits are applied once per creature that has a partner
            std::vector<std::string> names;
            for(std::map<Pair, PairData>::const_iterator it = mPairs.begin(); it != mPairs.end(); ++it)
            {
                if(it->second.mLovers)
                {
                    names.push_back(it->first.first);
                    names.push_back(it->first.second);
                }
            }
            for(size_t i = 0; i < names.size(); ++i)
                enforceLimits(names[i], turn);

            // Nothing of this is news for the clients
            mTierChanges.clear();
            return true;
        }

        if(line.empty() || (line[0] == '#'))
            continue;

        std::vector<std::string> fields;
        std::string::size_type start = 0;
        while(true)
        {
            std::string::size_type pos = line.find('\t', start);
            if(pos == std::string::npos)
            {
                fields.push_back(line.substr(start));
                break;
            }
            fields.push_back(line.substr(start, pos - start));
            start = pos + 1;
        }

        int64_t value;
        int64_t age;
        int64_t lovers = 0;
        bool hasFlag = (fields.size() == NB_SAVE_FIELDS_WITH_LOVERS);
        if(((fields.size() != NB_SAVE_FIELDS) && !hasFlag) || fields[0].empty() || fields[1].empty() || (fields[0] == fields[1])
            || !parseInt(fields[2], value) || !parseInt(fields[3], age) || (hasFlag && !parseInt(fields[4], lovers)))
        {
            return false;
        }

        value = std::max<int64_t>(RelationshipSettings::VALUE_MIN,
            std::min<int64_t>(RelationshipSettings::VALUE_MAX, value));
        if(value == 0)
            continue;

        PairData data;
        data.mValue = static_cast<int32_t>(value);
        data.mLastEventTurn = turn - std::max<int64_t>(0, age);
        data.mLovers = hasFlag && (lovers != 0) && (data.mValue >= mSettings.mLoversBreakValue);
        Pair pair = makePair(fields[0], fields[1]);
        mPairs[pair] = data;
        if(!hasFlag)
            legacy.insert(pair);
    }
}

void writeRelationshipCreatureStates(std::ostream& os, const std::vector<RelationshipCreatureState>& states)
{
    for(size_t i = 0; i < states.size(); ++i)
    {
        const RelationshipCreatureState& state = states[i];
        if(state.isEmpty())
            continue;

        os << state.mName << "\t" << state.mGriefMood << "\t" << std::max<int64_t>(0, state.mRageTurnsLeft) << "\t"
           << state.mRageSeatId << "\t" << state.mBrawlOpponent << "\t" << std::max<int64_t>(0, state.mBrawlTurnsLeft)
           << "\n";
    }
}

bool readRelationshipCreatureStates(std::istream& is, std::vector<RelationshipCreatureState>& states)
{
    states.clear();
    std::string line;
    while(true)
    {
        if(!is.good())
            return false;

        std::getline(is, line);
        if(!line.empty() && (line[line.size() - 1] == '\r'))
            line.erase(line.size() - 1);

        if(line == "[/RelationshipState]")
            return true;

        if(line.empty() || (line[0] == '#'))
            continue;

        std::vector<std::string> fields;
        std::string::size_type start = 0;
        while(true)
        {
            std::string::size_type pos = line.find('\t', start);
            if(pos == std::string::npos)
            {
                fields.push_back(line.substr(start));
                break;
            }
            fields.push_back(line.substr(start, pos - start));
            start = pos + 1;
        }

        int64_t grief = 0;
        int64_t rageTurns = 0;
        int64_t rageSeat = 0;
        int64_t brawlTurns = 0;
        if((fields.size() < STATE_FIELD_COUNT) || fields[0].empty() || !parseInt(fields[STATE_FIELD_GRIEF], grief)
           || !parseInt(fields[STATE_FIELD_RAGE_TURNS], rageTurns) || !parseInt(fields[STATE_FIELD_RAGE_SEAT], rageSeat)
           || !parseInt(fields[STATE_FIELD_BRAWL_TURNS], brawlTurns))
        {
            return false;
        }

        RelationshipCreatureState state;
        state.mName = fields[0];
        state.mGriefMood = static_cast<int32_t>(std::max<int64_t>(-STATE_GRIEF_LIMIT,
            std::min<int64_t>(STATE_GRIEF_LIMIT, grief)));
        state.mRageTurnsLeft = std::max<int64_t>(0, rageTurns);
        state.mRageSeatId = static_cast<int32_t>(rageSeat);
        state.mBrawlOpponent = fields[STATE_FIELD_BRAWL_OPPONENT];
        state.mBrawlTurnsLeft = std::max<int64_t>(0, brawlTurns);
        states.push_back(state);
    }
}

bool relationshipsAllowed(bool optionOnServerMap, bool hasSeat, bool rogueSeat, bool heroFaction,
    bool worker, bool inPrison)
{
    if(!optionOnServerMap)
        return false;

    if(!hasSeat || rogueSeat || heroFaction)
        return false;

    return !worker && !inPrison;
}

bool relationshipEventAllowed(bool firstAllowed, bool secondAllowed, bool sameSeat)
{
    return firstAllowed && secondAllowed && sameSeat;
}

bool anyHatedCoworker(const CreatureRelationships& relationships, bool creatureAllowed,
    const std::string& creature, const std::vector<std::string>& coworkers)
{
    if(!creatureAllowed)
        return false;

    for(size_t i = 0; i < coworkers.size(); ++i)
    {
        if(relationships.isHated(creature, coworkers[i]))
            return true;
    }

    return false;
}

bool useDislikedRoomAsFallback(bool haveDislikedRoom, size_t nbOtherSuitableRooms)
{
    return haveDislikedRoom && (nbOtherSuitableRooms == 0);
}

bool canStartBrawl(const BrawlCandidateState& state, const RelationshipSettings& settings)
{
    if(!state.mAllowed || !state.mOnMap || !state.mAlive || state.mKo || state.mBrawling)
        return false;

    if(!state.mHasTile)
        return false;

    // Not while fighting, in the arena or the casino, and not when badly hurt
    if(state.mFighting || state.mInArenaOrCasino)
        return false;

    return (state.mHp * BRAWL_PERCENT_FACTOR) >
        (state.mMaxHp * static_cast<double>(settings.mBrawlStopHealthPercent + BRAWL_START_HEALTH_MARGIN_PERCENT));
}

bool shouldStopBrawl(const BrawlFighterState& first, const BrawlFighterState& second, int64_t turnsSinceStart,
    bool stillFighting, const RelationshipSettings& settings)
{
    double stopRatio = static_cast<double>(settings.mBrawlStopHealthPercent) / BRAWL_PERCENT_FACTOR;
    return !first.mAlive || !second.mAlive || first.mKo || second.mKo
        || (first.mHp <= (first.mMaxHp * stopRatio)) || (second.mHp <= (second.mMaxHp * stopRatio))
        || (turnsSinceStart >= settings.mBrawlMaxTurns)
        || !stillFighting;
}

bool isBrawlCheckTurn(int64_t turn, const RelationshipSettings& settings)
{
    return (turn % settings.mBrawlCheckIntervalTurns) == 0;
}

bool isWithinBrawlDistance(double dx, double dy, const RelationshipSettings& settings)
{
    double maxDistance = static_cast<double>(settings.mBrawlMaxDistanceTiles);
    return (dx * dx + dy * dy) <= (maxDistance * maxDistance);
}

bool brawlChanceHit(int32_t roll, const RelationshipSettings& settings)
{
    return roll < settings.mBrawlChancePercent;
}

int32_t getGriefMoodPenalty(bool partner, const RelationshipSettings& settings)
{
    return partner ? settings.mPartnerGriefMoodPenalty : settings.mGriefMoodPenalty;
}

int32_t getLeaveTogetherChancePercent(bool partner, const RelationshipSettings& settings)
{
    return partner ? settings.mLeavePartnerChancePercent : settings.mLeaveTogetherChancePercent;
}
