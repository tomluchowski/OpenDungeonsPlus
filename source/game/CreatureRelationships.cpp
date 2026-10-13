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
#include <sstream>

const int32_t RelationshipSettings::VALUE_MIN = -100;
const int32_t RelationshipSettings::VALUE_MAX = 100;

namespace
{
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

    //! Largest absolute value a config setting may have; larger values are cut to it
    static const int64_t SETTING_LIMIT = 1000000;
    //! Number of tab separated fields of a saved pair line: creatureA, creatureB, value, age in turns
    static const size_t NB_SAVE_FIELDS = 4;

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

    //! Reads key from config. Returns false and leaves value untouched if it is missing or invalid.
    bool readSetting(const std::map<std::string, std::string>& config, const std::string& key, int64_t& value)
    {
        std::map<std::string, std::string>::const_iterator it = config.find(key);
        if(it == config.end())
            return false;

        return parseInt(it->second, value);
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
    mFightParticipantTurns(DEFAULT_FIGHT_PARTICIPANT_TURNS)
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
        {"EventChickenSnatched", &settings.mEventChickenSnatched}
    };
    TurnEntry turnEntries[] =
    {
        {"DriftIntervalTurns", &settings.mDriftIntervalTurns},
        {"DriftIdleTurns", &settings.mDriftIdleTurns},
        {"TrainingTogetherCooldownTurns", &settings.mTrainingTogetherCooldownTurns},
        {"FightParticipantTurns", &settings.mFightParticipantTurns}
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
    std::map<Pair, PairData>::const_iterator it = mPairs.find(pair);
    if(it != mPairs.end())
        value += it->second.mValue;

    setValue(pair, value, turn, true);
}

void CreatureRelationships::setValue(const Pair& pair, int32_t value, int64_t turn, bool isEvent)
{
    value = std::max(RelationshipSettings::VALUE_MIN, std::min(RelationshipSettings::VALUE_MAX, value));

    int32_t oldValue = 0;
    std::map<Pair, PairData>::iterator it = mPairs.find(pair);
    if(it != mPairs.end())
        oldValue = it->second.mValue;

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
        mPairs[pair] = data;
    }
    else
    {
        it->second.mValue = value;
        if(isEvent)
            it->second.mLastEventTurn = turn;
    }

    recordTierChange(pair, oldValue, value);
}

void CreatureRelationships::recordTierChange(const Pair& pair, int32_t oldValue, int32_t newValue)
{
    RelationshipTier oldTier = tierOfValue(oldValue, false);
    RelationshipTier newTier = tierOfValue(newValue, false);
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
    return tierOfValue(getValue(creatureA, creatureB), loversAllowed);
}

bool CreatureRelationships::isFriend(const std::string& creatureA, const std::string& creatureB) const
{
    return getValue(creatureA, creatureB) >= mSettings.mThresholdFriends;
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
        int32_t newValue = oldValue;
        if(oldValue > 0)
            newValue = std::max<int32_t>(0, oldValue - mSettings.mDriftAmount);
        else
            newValue = std::min<int32_t>(0, oldValue + mSettings.mDriftAmount);

        recordTierChange(it->first, oldValue, newValue);
        if(newValue == 0)
        {
            it = mPairs.erase(it);
            continue;
        }

        it->second.mValue = newValue;
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
    mPairs[pair] = data;
}

void CreatureRelationships::getTiers(std::vector<RelationshipTierChange>& tiers) const
{
    tiers.clear();
    for(std::map<Pair, PairData>::const_iterator it = mPairs.begin(); it != mPairs.end(); ++it)
    {
        RelationshipTier tier = tierOfValue(it->second.mValue, false);
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
        os << it->first.first << "\t" << it->first.second << "\t" << it->second.mValue << "\t" << age << "\n";
    }
}

bool CreatureRelationships::readFromStream(std::istream& is, int64_t turn)
{
    mPairs.clear();
    mTierChanges.clear();

    std::string line;
    while(true)
    {
        if(!is.good())
            return false;

        std::getline(is, line);
        if(!line.empty() && (line[line.size() - 1] == '\r'))
            line.erase(line.size() - 1);

        if(line == "[/Relationships]")
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

        int64_t value;
        int64_t age;
        if((fields.size() != NB_SAVE_FIELDS) || fields[0].empty() || fields[1].empty() || (fields[0] == fields[1])
            || !parseInt(fields[2], value) || !parseInt(fields[3], age))
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
        mPairs[makePair(fields[0], fields[1])] = data;
    }
}
