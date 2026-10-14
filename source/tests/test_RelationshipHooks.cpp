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

// Tests of the decision logic behind the relationship hooks. The hooks themselves live in
// Creature, GameMap, Room and CreatureActionSearchJob (they need a game map); they only collect
// the state of the creatures and call the free functions of CreatureRelationships.h tested here.
// The packet layout is shared by server and client through network/RelationshipPacket.h.

#define BOOST_TEST_MODULE RelationshipHooks
#include "BoostTestTargetConfig.h"

#include "game/CreatureRelationships.h"
#include "network/ODPacket.h"
#include "network/RelationshipPacket.h"
#include "network/ServerNotification.h"
#include "utils/LogManager.h"
#include "utils/LogSinkConsole.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{
    struct LogFixture
    {
        LogFixture()
        {
            mLogMgr.addSink(std::unique_ptr<LogSink>(new LogSinkConsole()));
        }

        LogManager mLogMgr;
    };

    std::vector<std::string> names(const std::string& first, const std::string& second = std::string(),
        const std::string& third = std::string())
    {
        std::vector<std::string> result;
        result.push_back(first);
        if(!second.empty())
            result.push_back(second);
        if(!third.empty())
            result.push_back(third);
        return result;
    }

    //! A creature that is idle, healthy and free to start a brawl
    BrawlCandidateState readyCandidate(const RelationshipSettings& settings)
    {
        BrawlCandidateState state;
        state.mAllowed = true;
        state.mOnMap = true;
        state.mAlive = true;
        state.mKo = false;
        state.mBrawling = false;
        state.mHasTile = true;
        state.mFighting = false;
        state.mInArenaOrCasino = false;
        state.mMaxHp = 100.0;
        state.mHp = state.mMaxHp;
        return state;
    }

    BrawlFighterState healthyFighter()
    {
        BrawlFighterState state;
        state.mAlive = true;
        state.mKo = false;
        state.mHp = 100.0;
        state.mMaxHp = 100.0;
        return state;
    }

    std::map<std::string, std::string> genders;

    std::string testGender(const std::string& name)
    {
        std::map<std::string, std::string>::const_iterator it = genders.find(name);
        return (it == genders.end()) ? std::string() : it->second;
    }

    //! Writes the first part of the startGameMode message like ODServer does
    void writeStartGameMode(ODPacket& packet)
    {
        packet << static_cast<int32_t>(ServerNotificationType::startGameMode) << static_cast<int32_t>(3)
            << static_cast<int32_t>(1);
        packet << true << true << true << true;
    }

    //! Reads the part of the startGameMode message before the relationships flag like ODClient does
    void readStartGameModeHead(ODPacket& packet)
    {
        int32_t type = 0;
        int32_t seatId = 0;
        int32_t mode = 0;
        packet >> type >> seatId >> mode;
        BOOST_CHECK_EQUAL(type, static_cast<int32_t>(ServerNotificationType::startGameMode));
        BOOST_CHECK_EQUAL(seatId, 3);
        BOOST_CHECK_EQUAL(mode, 1);
        bool flag = false;
        for(int i = 0; i < 4; ++i)
        {
            BOOST_REQUIRE(!packet.endOfPacket());
            packet >> flag;
            BOOST_CHECK(flag);
        }
    }
}

BOOST_FIXTURE_TEST_SUITE(RelationshipHooksSuite, LogFixture)

// ---------------------------------------------------------------------------------------------
// Creature::canHaveRelationships and Creature::reportRelationshipEvent (filter)
// ---------------------------------------------------------------------------------------------

//! Covers: a normal keeper creature may have relationships; a missing seat, a rogue seat, the hero
//! faction, a worker and a prisoner each exclude the creature on their own.
BOOST_AUTO_TEST_CASE(test_CanHaveRelationshipsFilter)
{
    BOOST_CHECK(relationshipsAllowed(true, true, false, false, false, false));
    BOOST_CHECK(!relationshipsAllowed(true, false, false, false, false, false));
    BOOST_CHECK(!relationshipsAllowed(true, true, true, false, false, false));
    BOOST_CHECK(!relationshipsAllowed(true, true, false, true, false, false));
    BOOST_CHECK(!relationshipsAllowed(true, true, false, false, true, false));
    BOOST_CHECK(!relationshipsAllowed(true, true, false, false, false, true));
}

//! Covers: option off (or not on the server map) means no creature can have relationships,
//! whatever the other properties are. This is the gate of every hook.
BOOST_AUTO_TEST_CASE(test_OptionOffAllowsNobody)
{
    for(int mask = 0; mask < 32; ++mask)
    {
        bool hasSeat = ((mask & 1) != 0);
        bool rogue = ((mask & 2) != 0);
        bool hero = ((mask & 4) != 0);
        bool worker = ((mask & 8) != 0);
        bool prison = ((mask & 16) != 0);
        BOOST_CHECK(!relationshipsAllowed(false, hasSeat, rogue, hero, worker, prison));
    }
}

//! Covers: an event needs two creatures that may have relationships and that share the seat.
BOOST_AUTO_TEST_CASE(test_EventFilter)
{
    BOOST_CHECK(relationshipEventAllowed(true, true, true));
    BOOST_CHECK(!relationshipEventAllowed(false, true, true));
    BOOST_CHECK(!relationshipEventAllowed(true, false, true));
    BOOST_CHECK(!relationshipEventAllowed(true, true, false));
    BOOST_CHECK(!relationshipEventAllowed(false, false, false));
}

//! Covers: the hook (filter, then onRelationshipEvent) changes the table for a valid pair and
//! leaves it untouched when the filter says no (other seat, hero, option off).
BOOST_AUTO_TEST_CASE(test_FilteredEventDoesNotTouchTable)
{
    CreatureRelationships relationships;

    // Same keeper, both allowed
    if(relationshipEventAllowed(true, true, true))
        relationships.onRelationshipEvent(RelationshipEvent::trainingTogether, "Orc1", "Orc2", 10);
    BOOST_CHECK_EQUAL(relationships.getNbPairs(), 1u);
    BOOST_CHECK(relationships.getValue("Orc1", "Orc2") > 0);

    // Different seats, one of them a hero, option off: the hook returns before the table is used
    if(relationshipEventAllowed(true, true, false))
        relationships.onRelationshipEvent(RelationshipEvent::trainingTogether, "Orc1", "Troll1", 10);
    if(relationshipEventAllowed(relationshipsAllowed(true, true, false, true, false, false), true, true))
        relationships.onRelationshipEvent(RelationshipEvent::trainingTogether, "Orc1", "Hero1", 10);
    if(relationshipEventAllowed(relationshipsAllowed(false, true, false, false, false, false),
        relationshipsAllowed(false, true, false, false, false, false), true))
    {
        relationships.onRelationshipEvent(RelationshipEvent::trainingTogether, "Orc3", "Orc4", 10);
    }
    BOOST_CHECK_EQUAL(relationships.getNbPairs(), 1u);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), 0);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Hero1"), 0);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc3", "Orc4"), 0);
}

//! Covers: option-off identity at the level of the query functions. With the option off the game
//! map has no relationship table (or an empty one); every effect query then returns the neutral
//! value and a turn changes nothing, so combat, mood, training and room choice stay classic.
BOOST_AUTO_TEST_CASE(test_OptionOffIdentityOfQueries)
{
    CreatureRelationships relationships;
    std::vector<std::string> others = names("Orc2", "Troll1", "Kob1");

    BOOST_CHECK_EQUAL(relationships.combatModifier("Orc1", others), 0.0);
    BOOST_CHECK_EQUAL(relationships.moodModifier("Orc1"), 0);
    std::vector<std::pair<std::string, uint32_t> > trainees;
    trainees.push_back(std::pair<std::string, uint32_t>("Orc2", 9));
    BOOST_CHECK_EQUAL(relationships.mentoringFactor("Orc1", 1, trainees), 1.0);
    BOOST_CHECK(relationships.getBestFriend("Orc1").empty());
    BOOST_CHECK(relationships.getPartner("Orc1").empty());
    BOOST_CHECK(!relationships.isFriend("Orc1", "Orc2"));
    BOOST_CHECK(!relationships.isHated("Orc1", "Orc2"));
    BOOST_CHECK(!relationships.isNemesis("Orc1", "Orc2"));
    BOOST_CHECK(relationships.tierOf("Orc1", "Orc2", true) == RelationshipTier::neutral);
    std::vector<std::string> friends;
    relationships.getFriends("Orc1", friends);
    BOOST_CHECK(friends.empty());
    std::vector<CreatureRelationships::Pair> pairs;
    relationships.getNemesisPairs(pairs);
    BOOST_CHECK(pairs.empty());

    // The hooks that read the table: no hated coworker, no brawl partner
    BOOST_CHECK(!anyHatedCoworker(relationships, true, "Orc1", others));

    for(int64_t turn = 0; turn < 500; turn += 7)
        relationships.doTurn(turn);
    std::vector<RelationshipTierChange> changes;
    relationships.takeTierChanges(changes);
    BOOST_CHECK(changes.empty());
    relationships.getTiers(changes);
    BOOST_CHECK(changes.empty());
    BOOST_CHECK_EQUAL(relationships.getNbPairs(), 0u);
}

// ---------------------------------------------------------------------------------------------
// Room::hasHatedCoworker and CreatureActionSearchJob (room choice)
// ---------------------------------------------------------------------------------------------

//! Covers: hated and nemesis coworkers count, friends, neutral and unknown creatures do not, the
//! threshold is inclusive, an empty room has none, and a creature without relationships
//! (option off, hero, worker, prisoner) has none even when the table has a hated pair.
BOOST_AUTO_TEST_CASE(test_HasHatedCoworker)
{
    CreatureRelationships relationships;
    const RelationshipSettings& settings = relationships.getSettings();
    relationships.changeValue("Orc1", "Troll1", settings.mThresholdHated, 0);
    relationships.changeValue("Orc1", "Troll2", settings.mThresholdNemesis, 0);
    relationships.changeValue("Orc1", "Kob1", settings.mThresholdHated + 1, 0);
    relationships.changeValue("Orc1", "Orc2", settings.mThresholdFriends, 0);

    BOOST_CHECK(anyHatedCoworker(relationships, true, "Orc1", names("Troll1")));
    BOOST_CHECK(anyHatedCoworker(relationships, true, "Orc1", names("Troll2")));
    BOOST_CHECK(anyHatedCoworker(relationships, true, "Orc1", names("Orc2", "Kob1", "Troll1")));
    // The relationship is symmetric, the hated creature sees it the same way
    BOOST_CHECK(anyHatedCoworker(relationships, true, "Troll1", names("Orc1")));

    BOOST_CHECK(!anyHatedCoworker(relationships, true, "Orc1", names("Kob1")));
    BOOST_CHECK(!anyHatedCoworker(relationships, true, "Orc1", names("Orc2")));
    BOOST_CHECK(!anyHatedCoworker(relationships, true, "Orc1", names("Unknown1")));
    BOOST_CHECK(!anyHatedCoworker(relationships, true, "Orc1", std::vector<std::string>()));
    BOOST_CHECK(!anyHatedCoworker(relationships, true, "Orc9", names("Troll1")));

    BOOST_CHECK(!anyHatedCoworker(relationships, false, "Orc1", names("Troll1", "Troll2")));
}

//! Covers: the fallback of the job. A creature stays in a room it does not like only when it
//! found no other suitable room; it never stays without such a room, and a missing disliked
//! room does not create one.
BOOST_AUTO_TEST_CASE(test_DislikedRoomFallback)
{
    BOOST_CHECK(useDislikedRoomAsFallback(true, 0));
    BOOST_CHECK(!useDislikedRoomAsFallback(true, 1));
    BOOST_CHECK(!useDislikedRoomAsFallback(true, 5));
    BOOST_CHECK(!useDislikedRoomAsFallback(false, 0));
    BOOST_CHECK(!useDislikedRoomAsFallback(false, 3));
}

//! Covers: preference and fallback together as the job uses them. A hated coworker makes the
//! current room the disliked room; another room without hated coworkers wins; if every other
//! room has a hated coworker or is not reachable, the creature stays; once the hated creature is
//! gone (removeCreature) the room is a normal choice again.
BOOST_AUTO_TEST_CASE(test_RoomPreferenceAndFallback)
{
    CreatureRelationships relationships;
    relationships.changeValue("Orc1", "Troll1", relationships.getSettings().mThresholdNemesis, 0);

    bool currentRoomDisliked = anyHatedCoworker(relationships, true, "Orc1", names("Troll1", "Orc2"));
    BOOST_CHECK(currentRoomDisliked);

    // Candidate rooms are only those without a hated coworker
    size_t nbOtherSuitable = 0;
    if(!anyHatedCoworker(relationships, true, "Orc1", names("Kob1")))
        ++nbOtherSuitable;
    if(!anyHatedCoworker(relationships, true, "Orc1", names("Troll1")))
        ++nbOtherSuitable;
    BOOST_CHECK_EQUAL(nbOtherSuitable, 1u);
    BOOST_CHECK(!useDislikedRoomAsFallback(currentRoomDisliked, nbOtherSuitable));

    // No other room is free of the hated creature: stay
    nbOtherSuitable = 0;
    if(!anyHatedCoworker(relationships, true, "Orc1", names("Troll1", "Kob1")))
        ++nbOtherSuitable;
    BOOST_CHECK(useDislikedRoomAsFallback(currentRoomDisliked, nbOtherSuitable));

    // The hated creature died: the current room is no longer disliked
    relationships.removeCreature("Troll1");
    BOOST_CHECK(!anyHatedCoworker(relationships, true, "Orc1", names("Troll1", "Orc2")));
}

// ---------------------------------------------------------------------------------------------
// Brawls: GameMap::checkRelationshipBrawls, Creature::canStartBrawl, updateBrawl, endBrawl
// ---------------------------------------------------------------------------------------------

//! Covers: brawls are only looked for on turns that are a multiple of the check interval.
BOOST_AUTO_TEST_CASE(test_BrawlCheckInterval)
{
    RelationshipSettings settings;
    BOOST_CHECK(isBrawlCheckTurn(0, settings));
    BOOST_CHECK(isBrawlCheckTurn(settings.mBrawlCheckIntervalTurns, settings));
    BOOST_CHECK(isBrawlCheckTurn(settings.mBrawlCheckIntervalTurns * 7, settings));
    BOOST_CHECK(!isBrawlCheckTurn(1, settings));
    BOOST_CHECK(!isBrawlCheckTurn(settings.mBrawlCheckIntervalTurns - 1, settings));
    BOOST_CHECK(!isBrawlCheckTurn(settings.mBrawlCheckIntervalTurns + 1, settings));
}

//! Covers: only nemesis pairs (value at or below the nemesis threshold) are brawl candidates,
//! each pair once; hated or friendly pairs are not listed, and a pair that is no longer nemesis
//! or whose creature is gone drops out.
BOOST_AUTO_TEST_CASE(test_BrawlPairSelection)
{
    CreatureRelationships relationships;
    const RelationshipSettings& settings = relationships.getSettings();
    relationships.changeValue("Orc1", "Troll1", settings.mThresholdNemesis, 0);
    relationships.changeValue("Orc2", "Troll2", settings.mThresholdNemesis + 1, 0);
    relationships.changeValue("Kob1", "Kob2", settings.mThresholdFriends, 0);
    relationships.changeValue("Troll3", "Orc3", -100, 0);

    std::vector<CreatureRelationships::Pair> pairs;
    relationships.getNemesisPairs(pairs);
    BOOST_REQUIRE_EQUAL(pairs.size(), 2u);
    bool foundFirst = false;
    bool foundThird = false;
    for(size_t i = 0; i < pairs.size(); ++i)
    {
        if(((pairs[i].first == "Orc1") && (pairs[i].second == "Troll1"))
           || ((pairs[i].first == "Troll1") && (pairs[i].second == "Orc1")))
            foundFirst = true;
        if(((pairs[i].first == "Orc3") && (pairs[i].second == "Troll3"))
           || ((pairs[i].first == "Troll3") && (pairs[i].second == "Orc3")))
            foundThird = true;
    }
    BOOST_CHECK(foundFirst);
    BOOST_CHECK(foundThird);

    relationships.removeCreature("Troll3");
    relationships.getNemesisPairs(pairs);
    BOOST_CHECK_EQUAL(pairs.size(), 1u);
}

//! Covers: the distance rule. Pairs within the configured number of tiles (circle, not square)
//! notice each other, the border is inclusive, dx and dy may be negative.
BOOST_AUTO_TEST_CASE(test_BrawlDistance)
{
    RelationshipSettings settings;
    double d = static_cast<double>(settings.mBrawlMaxDistanceTiles);
    BOOST_CHECK(isWithinBrawlDistance(0.0, 0.0, settings));
    BOOST_CHECK(isWithinBrawlDistance(d, 0.0, settings));
    BOOST_CHECK(isWithinBrawlDistance(0.0, -d, settings));
    BOOST_CHECK(!isWithinBrawlDistance(d + 1.0, 0.0, settings));
    BOOST_CHECK(!isWithinBrawlDistance(d, 1.0, settings));
    // Diagonal: inside the circle although each axis is below the limit
    BOOST_CHECK(isWithinBrawlDistance(d * 0.7, d * 0.7, settings));
    BOOST_CHECK(!isWithinBrawlDistance(d * 0.8, d * 0.8, settings));
}

//! Covers: the chance. A roll below the percentage starts the brawl, 0 percent never and 100
//! percent always.
BOOST_AUTO_TEST_CASE(test_BrawlChance)
{
    RelationshipSettings settings;
    BOOST_CHECK(brawlChanceHit(0, settings));
    BOOST_CHECK(brawlChanceHit(settings.mBrawlChancePercent - 1, settings));
    BOOST_CHECK(!brawlChanceHit(settings.mBrawlChancePercent, settings));
    BOOST_CHECK(!brawlChanceHit(99, settings));

    settings.mBrawlChancePercent = 0;
    for(int32_t roll = 0; roll < 100; ++roll)
        BOOST_CHECK(!brawlChanceHit(roll, settings));

    settings.mBrawlChancePercent = 100;
    for(int32_t roll = 0; roll < 100; ++roll)
        BOOST_CHECK(brawlChanceHit(roll, settings));
}

//! Covers: a healthy idle creature may start; each blocking condition (no relationships, off the
//! map, dead, KO, already brawling, no tile, fighting, in arena or casino) stops it.
BOOST_AUTO_TEST_CASE(test_CanStartBrawlConditions)
{
    RelationshipSettings settings;
    BrawlCandidateState state = readyCandidate(settings);
    BOOST_CHECK(canStartBrawl(state, settings));

    state = readyCandidate(settings);
    state.mAllowed = false;
    BOOST_CHECK(!canStartBrawl(state, settings));
    state = readyCandidate(settings);
    state.mOnMap = false;
    BOOST_CHECK(!canStartBrawl(state, settings));
    state = readyCandidate(settings);
    state.mAlive = false;
    BOOST_CHECK(!canStartBrawl(state, settings));
    state = readyCandidate(settings);
    state.mKo = true;
    BOOST_CHECK(!canStartBrawl(state, settings));
    state = readyCandidate(settings);
    state.mBrawling = true;
    BOOST_CHECK(!canStartBrawl(state, settings));
    state = readyCandidate(settings);
    state.mHasTile = false;
    BOOST_CHECK(!canStartBrawl(state, settings));
    state = readyCandidate(settings);
    state.mFighting = true;
    BOOST_CHECK(!canStartBrawl(state, settings));
    state = readyCandidate(settings);
    state.mInArenaOrCasino = true;
    BOOST_CHECK(!canStartBrawl(state, settings));
}

//! Covers: the health rule for starting. The creature needs more than the stop percentage plus
//! 25 points of its maximum health (50 percent with the defaults), exactly that is not enough.
BOOST_AUTO_TEST_CASE(test_CanStartBrawlHealth)
{
    RelationshipSettings settings;
    BrawlCandidateState state = readyCandidate(settings);
    state.mMaxHp = 200.0;
    double limit = state.mMaxHp * static_cast<double>(settings.mBrawlStopHealthPercent + 25) / 100.0;

    state.mHp = limit;
    BOOST_CHECK(!canStartBrawl(state, settings));
    state.mHp = limit + 0.5;
    BOOST_CHECK(canStartBrawl(state, settings));
    state.mHp = limit - 0.5;
    BOOST_CHECK(!canStartBrawl(state, settings));
    state.mHp = state.mMaxHp;
    BOOST_CHECK(canStartBrawl(state, settings));
    state.mHp = 0.0;
    BOOST_CHECK(!canStartBrawl(state, settings));

    // A different stop percentage moves the limit
    settings.mBrawlStopHealthPercent = 40;
    state.mHp = state.mMaxHp * 0.64;
    BOOST_CHECK(!canStartBrawl(state, settings));
    state.mHp = state.mMaxHp * 0.66;
    BOOST_CHECK(canStartBrawl(state, settings));
}

//! Covers: a healthy brawl keeps running for turns below the limit.
BOOST_AUTO_TEST_CASE(test_BrawlContinues)
{
    RelationshipSettings settings;
    BrawlFighterState first = healthyFighter();
    BrawlFighterState second = healthyFighter();
    BOOST_CHECK(!shouldStopBrawl(first, second, 0, true, settings));
    BOOST_CHECK(!shouldStopBrawl(first, second, settings.mBrawlMaxTurns - 1, true, settings));
}

//! Covers: the brawl ends when either fighter reaches the stop percentage of its own maximum
//! health (25 percent with the defaults, inclusive), independent of the other one.
BOOST_AUTO_TEST_CASE(test_BrawlStopsAtLowHealth)
{
    RelationshipSettings settings;
    BrawlFighterState first = healthyFighter();
    BrawlFighterState second = healthyFighter();
    second.mMaxHp = 400.0;
    second.mHp = 400.0;
    double stopFraction = static_cast<double>(settings.mBrawlStopHealthPercent) / 100.0;

    first.mHp = first.mMaxHp * stopFraction + 1.0;
    BOOST_CHECK(!shouldStopBrawl(first, second, 10, true, settings));
    first.mHp = first.mMaxHp * stopFraction;
    BOOST_CHECK(shouldStopBrawl(first, second, 10, true, settings));
    first.mHp = 5.0;
    BOOST_CHECK(shouldStopBrawl(first, second, 10, true, settings));

    first = healthyFighter();
    second.mHp = second.mMaxHp * stopFraction + 1.0;
    BOOST_CHECK(!shouldStopBrawl(first, second, 10, true, settings));
    second.mHp = second.mMaxHp * stopFraction;
    BOOST_CHECK(shouldStopBrawl(first, second, 10, true, settings));
}

//! Covers: KO and death of either fighter end the brawl.
BOOST_AUTO_TEST_CASE(test_BrawlStopsOnKoDeath)
{
    RelationshipSettings settings;
    BrawlFighterState healthy = healthyFighter();

    BrawlFighterState state = healthyFighter();
    state.mKo = true;
    BOOST_CHECK(shouldStopBrawl(state, healthy, 10, true, settings));
    BOOST_CHECK(shouldStopBrawl(healthy, state, 10, true, settings));

    state = healthyFighter();
    state.mAlive = false;
    BOOST_CHECK(shouldStopBrawl(state, healthy, 10, true, settings));
    BOOST_CHECK(shouldStopBrawl(healthy, state, 10, true, settings));
}

//! Covers: the brawl ends after the maximum duration (150 turns with the defaults, inclusive),
//! and when the fight action is gone (an enemy appeared and cleared the action queue).
BOOST_AUTO_TEST_CASE(test_BrawlStopsAfterMaxTurnsOrInterruption)
{
    RelationshipSettings settings;
    BrawlFighterState first = healthyFighter();
    BrawlFighterState second = healthyFighter();

    BOOST_CHECK(!shouldStopBrawl(first, second, settings.mBrawlMaxTurns - 1, true, settings));
    BOOST_CHECK(shouldStopBrawl(first, second, settings.mBrawlMaxTurns, true, settings));
    BOOST_CHECK(shouldStopBrawl(first, second, settings.mBrawlMaxTurns + 40, true, settings));

    BOOST_CHECK(shouldStopBrawl(first, second, 5, false, settings));

    settings.mBrawlMaxTurns = 30;
    BOOST_CHECK(!shouldStopBrawl(first, second, 29, true, settings));
    BOOST_CHECK(shouldStopBrawl(first, second, 30, true, settings));
}

//! Covers: the effect of endBrawl on the relationship, which is a plain changeValue with the
//! configured brawl change (-6 with the defaults): a nemesis pair gets worse and stays a nemesis
//! pair, a hated pair can fall to nemesis, the value never leaves -100, and a pair without value
//! gets one.
BOOST_AUTO_TEST_CASE(test_BrawlEndChangesValue)
{
    CreatureRelationships relationships;
    const RelationshipSettings& settings = relationships.getSettings();
    BOOST_CHECK(settings.mBrawlValueChange < 0);

    relationships.changeValue("Orc1", "Troll1", -85, 0);
    relationships.changeValue("Orc1", "Troll1", settings.mBrawlValueChange, 100);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), -85 + settings.mBrawlValueChange);
    BOOST_CHECK(relationships.isNemesis("Orc1", "Troll1"));

    relationships.changeValue("Orc2", "Troll2", settings.mThresholdNemesis + 3, 0);
    BOOST_CHECK(!relationships.isNemesis("Orc2", "Troll2"));
    relationships.changeValue("Orc2", "Troll2", settings.mBrawlValueChange, 100);
    BOOST_CHECK(relationships.isNemesis("Orc2", "Troll2"));

    relationships.changeValue("Orc3", "Troll3", -98, 0);
    relationships.changeValue("Orc3", "Troll3", settings.mBrawlValueChange, 100);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc3", "Troll3"), -100);

    // The name order of the brawl does not matter
    relationships.changeValue("Troll1", "Orc1", settings.mBrawlValueChange, 200);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), -85 + 2 * settings.mBrawlValueChange);
}

// ---------------------------------------------------------------------------------------------
// Network round trip
// ---------------------------------------------------------------------------------------------

//! Covers: the relationshipTier payload (names, tier, replay flag) survives writing and reading
//! for every tier and for both values of the replay flag; the notification type in front of it
//! is read first as the client does.
BOOST_AUTO_TEST_CASE(test_RelationshipTierPacketRoundTrip)
{
    RelationshipTier tiers[6] = {RelationshipTier::nemesis, RelationshipTier::hated, RelationshipTier::neutral,
        RelationshipTier::friends, RelationshipTier::bestFriends, RelationshipTier::lovers};
    for(int i = 0; i < 6; ++i)
    {
        for(int replayValue = 0; replayValue < 2; ++replayValue)
        {
            bool replay = (replayValue != 0);
            ODPacket packet;
            packet << static_cast<int32_t>(ServerNotificationType::relationshipTier);
            writeRelationshipTier(packet, "Orc 1", "Troll_2", tiers[i], replay);

            int32_t type = -1;
            packet >> type;
            BOOST_CHECK_EQUAL(type, static_cast<int32_t>(ServerNotificationType::relationshipTier));
            std::string creatureA;
            std::string creatureB;
            int32_t tier = -1;
            bool outReplay = !replay;
            BOOST_REQUIRE(readRelationshipTier(packet, creatureA, creatureB, tier, outReplay));
            BOOST_CHECK_EQUAL(creatureA, "Orc 1");
            BOOST_CHECK_EQUAL(creatureB, "Troll_2");
            BOOST_CHECK_EQUAL(tier, static_cast<int32_t>(tiers[i]));
            BOOST_CHECK_EQUAL(outReplay, replay);
            BOOST_CHECK(packet.endOfPacket());
        }
    }
}

//! Covers: a packet that is cut off (missing replay flag or tier) is reported as invalid instead
//! of being read as a tier change.
BOOST_AUTO_TEST_CASE(test_RelationshipTierPacketTooShort)
{
    ODPacket packet;
    packet << std::string("Orc1") << std::string("Troll1") << static_cast<int32_t>(RelationshipTier::friends);
    std::string creatureA;
    std::string creatureB;
    int32_t tier = 0;
    bool replay = false;
    BOOST_CHECK(!readRelationshipTier(packet, creatureA, creatureB, tier, replay));

    ODPacket empty;
    BOOST_CHECK(!readRelationshipTier(empty, creatureA, creatureB, tier, replay));
}

//! Covers: several tier messages in a row (as sent for the replay at join or load) are read one
//! after the other, each with its own replay flag.
BOOST_AUTO_TEST_CASE(test_RelationshipTierPacketsInSequence)
{
    ODPacket first;
    writeRelationshipTier(first, "A", "B", RelationshipTier::friends, true);
    ODPacket second;
    writeRelationshipTier(second, "C", "D", RelationshipTier::hated, false);

    std::string a;
    std::string b;
    int32_t tier = 0;
    bool replay = false;
    BOOST_REQUIRE(readRelationshipTier(first, a, b, tier, replay));
    BOOST_CHECK_EQUAL(a, "A");
    BOOST_CHECK_EQUAL(tier, static_cast<int32_t>(RelationshipTier::friends));
    BOOST_CHECK(replay);
    BOOST_REQUIRE(readRelationshipTier(second, a, b, tier, replay));
    BOOST_CHECK_EQUAL(a, "C");
    BOOST_CHECK_EQUAL(tier, static_cast<int32_t>(RelationshipTier::hated));
    BOOST_CHECK(!replay);
}

//! Covers: the whole path of the replay for a joining client. The server table is listed with
//! getTiers, written as replay messages, read and stored with setTier on a client table; the
//! client sees the same tiers (friends, nemesis, lovers) and no pair for neutral creatures.
BOOST_AUTO_TEST_CASE(test_TierReplayRebuildsClientTable)
{
    CreatureRelationships server;
    genders.clear();
    genders["Adam"] = "Male";
    genders["Eve"] = "Female";
    server.setGenderLookup(testGender);
    server.changeValue("Orc1", "Orc2", 60, 0);
    server.changeValue("Orc1", "Troll1", -90, 0);
    server.changeValue("Adam", "Eve", 95, 0);
    server.changeValue("Imp1", "Imp2", 10, 0);
    BOOST_REQUIRE(server.isLovers("Adam", "Eve"));

    std::vector<RelationshipTierChange> tiers;
    server.getTiers(tiers);
    BOOST_CHECK_EQUAL(tiers.size(), 3u);

    CreatureRelationships client;
    for(size_t i = 0; i < tiers.size(); ++i)
    {
        ODPacket packet;
        writeRelationshipTier(packet, tiers[i].mCreatureA, tiers[i].mCreatureB, tiers[i].mNewTier, true);
        std::string a;
        std::string b;
        int32_t tier = 0;
        bool replay = false;
        BOOST_REQUIRE(readRelationshipTier(packet, a, b, tier, replay));
        BOOST_CHECK(replay);
        BOOST_REQUIRE(tier >= static_cast<int32_t>(RelationshipTier::nemesis));
        BOOST_REQUIRE(tier <= static_cast<int32_t>(RelationshipTier::lovers));
        client.setTier(a, b, static_cast<RelationshipTier>(tier));
    }

    BOOST_CHECK(client.tierOf("Orc1", "Orc2", true) == RelationshipTier::friends);
    BOOST_CHECK(client.tierOf("Orc1", "Troll1", true) == RelationshipTier::nemesis);
    BOOST_CHECK(client.tierOf("Adam", "Eve", true) == RelationshipTier::lovers);
    BOOST_CHECK(client.tierOf("Imp1", "Imp2", true) == RelationshipTier::neutral);
    BOOST_CHECK_EQUAL(client.getNbPairs(), 3u);
}

//! Covers: a live tier change (replay false) as GameMap::sendRelationshipTierChanges writes it:
//! the change recorded by the table is sent with the new tier and the client can tell it from a
//! replay.
BOOST_AUTO_TEST_CASE(test_LiveTierChangePacket)
{
    CreatureRelationships server;
    server.changeValue("Orc1", "Orc2", 55, 0);
    std::vector<RelationshipTierChange> changes;
    server.takeTierChanges(changes);
    BOOST_REQUIRE_EQUAL(changes.size(), 1u);

    ODPacket packet;
    writeRelationshipTier(packet, changes[0].mCreatureA, changes[0].mCreatureB, changes[0].mNewTier, false);
    std::string a;
    std::string b;
    int32_t tier = 0;
    bool replay = true;
    BOOST_REQUIRE(readRelationshipTier(packet, a, b, tier, replay));
    BOOST_CHECK(!replay);
    BOOST_CHECK_EQUAL(tier, static_cast<int32_t>(RelationshipTier::friends));

    // Nothing is left to send afterwards
    server.takeTierChanges(changes);
    BOOST_CHECK(changes.empty());
}

//! Covers: startGameMode with the trailing relationships flag: true and false are read as sent.
BOOST_AUTO_TEST_CASE(test_StartGameModeFlagPresent)
{
    for(int value = 0; value < 2; ++value)
    {
        bool sent = (value != 0);
        ODPacket packet;
        writeStartGameMode(packet);
        packet << sent;

        readStartGameModeHead(packet);
        BOOST_CHECK(!packet.endOfPacket());
        BOOST_CHECK_EQUAL(readRelationshipsFlag(packet), sent);
        BOOST_CHECK(packet.endOfPacket());
    }
}

//! Covers: startGameMode from a server without the relationships flag (packet ends after the
//! other capability flags): the client reads "off" and does not report a read error.
BOOST_AUTO_TEST_CASE(test_StartGameModeFlagAbsent)
{
    ODPacket packet;
    writeStartGameMode(packet);

    readStartGameModeHead(packet);
    BOOST_CHECK(packet.endOfPacket());
    BOOST_CHECK(!readRelationshipsFlag(packet));
    BOOST_CHECK(static_cast<bool>(packet));

    // An even older server that sends no capability flag at all
    ODPacket oldest;
    oldest << static_cast<int32_t>(ServerNotificationType::startGameMode) << static_cast<int32_t>(3)
        << static_cast<int32_t>(1);
    int32_t type = 0;
    int32_t seatId = 0;
    int32_t mode = 0;
    oldest >> type >> seatId >> mode;
    BOOST_CHECK(!readRelationshipsFlag(oldest));
}

// ---------------------------------------------------------------------------------------------
// Partner values
// ---------------------------------------------------------------------------------------------

//! Covers: the partner grieves with the partner penalty, a friend with the friend penalty, and the
//! partner penalty is the larger one with the defaults.
BOOST_AUTO_TEST_CASE(test_GriefMoodPenaltyForPartnerAndFriend)
{
    RelationshipSettings settings;
    BOOST_CHECK_EQUAL(getGriefMoodPenalty(false, settings), settings.mGriefMoodPenalty);
    BOOST_CHECK_EQUAL(getGriefMoodPenalty(true, settings), settings.mPartnerGriefMoodPenalty);
    BOOST_CHECK(getGriefMoodPenalty(true, settings) > getGriefMoodPenalty(false, settings));

    settings.mPartnerGriefMoodPenalty = 900;
    settings.mGriefMoodPenalty = 100;
    BOOST_CHECK_EQUAL(getGriefMoodPenalty(true, settings), 900);
    BOOST_CHECK_EQUAL(getGriefMoodPenalty(false, settings), 100);
}

//! Covers: a partner follows a creature that leaves with the partner chance, a best friend with the
//! best friend chance.
BOOST_AUTO_TEST_CASE(test_LeaveTogetherChanceForPartnerAndFriend)
{
    RelationshipSettings settings;
    BOOST_CHECK_EQUAL(getLeaveTogetherChancePercent(false, settings), settings.mLeaveTogetherChancePercent);
    BOOST_CHECK_EQUAL(getLeaveTogetherChancePercent(true, settings), settings.mLeavePartnerChancePercent);

    settings.mLeavePartnerChancePercent = 55;
    settings.mLeaveTogetherChancePercent = 15;
    BOOST_CHECK_EQUAL(getLeaveTogetherChancePercent(true, settings), 55);
    BOOST_CHECK_EQUAL(getLeaveTogetherChancePercent(false, settings), 15);
}

BOOST_AUTO_TEST_SUITE_END()
