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

#define BOOST_TEST_MODULE CreatureRelationships
#include "BoostTestTargetConfig.h"

#include "game/CreatureRelationships.h"
#include "utils/LogManager.h"
#include "utils/LogSinkConsole.h"

#include <map>
#include <sstream>
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
}

BOOST_FIXTURE_TEST_SUITE(CreatureRelationshipsSuite, LogFixture)

BOOST_AUTO_TEST_CASE(test_ClampingAndSparseStorage)
{
    CreatureRelationships relationships;
    BOOST_CHECK_EQUAL(relationships.getNbPairs(), 0u);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), 0);

    relationships.changeValue("Orc1", "Troll1", 70, 0);
    relationships.changeValue("Troll1", "Orc1", 70, 0);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), 100);
    BOOST_CHECK_EQUAL(relationships.getValue("Troll1", "Orc1"), 100);
    BOOST_CHECK_EQUAL(relationships.getNbPairs(), 1u);

    relationships.changeValue("Orc1", "Troll1", -500, 0);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), -100);

    // A value of 0 does not stay in the table
    relationships.changeValue("Orc1", "Troll1", 100, 0);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), 0);
    BOOST_CHECK_EQUAL(relationships.getNbPairs(), 0u);

    // A creature has no relationship with itself
    relationships.changeValue("Orc1", "Orc1", 50, 0);
    BOOST_CHECK_EQUAL(relationships.getNbPairs(), 0u);
}

BOOST_AUTO_TEST_CASE(test_Tiers)
{
    CreatureRelationships relationships;
    BOOST_CHECK(relationships.tierOfValue(0, false) == RelationshipTier::neutral);
    BOOST_CHECK(relationships.tierOfValue(49, false) == RelationshipTier::neutral);
    BOOST_CHECK(relationships.tierOfValue(50, false) == RelationshipTier::friends);
    BOOST_CHECK(relationships.tierOfValue(79, false) == RelationshipTier::friends);
    BOOST_CHECK(relationships.tierOfValue(80, false) == RelationshipTier::bestFriends);
    BOOST_CHECK(relationships.tierOfValue(100, false) == RelationshipTier::bestFriends);
    BOOST_CHECK(relationships.tierOfValue(89, true) == RelationshipTier::bestFriends);
    BOOST_CHECK(relationships.tierOfValue(90, true) == RelationshipTier::lovers);
    BOOST_CHECK(relationships.tierOfValue(-49, false) == RelationshipTier::neutral);
    BOOST_CHECK(relationships.tierOfValue(-50, false) == RelationshipTier::hated);
    BOOST_CHECK(relationships.tierOfValue(-79, false) == RelationshipTier::hated);
    BOOST_CHECK(relationships.tierOfValue(-80, false) == RelationshipTier::nemesis);

    relationships.changeValue("Orc1", "Troll1", 50, 0);
    BOOST_CHECK(relationships.isFriend("Troll1", "Orc1"));
    BOOST_CHECK(relationships.tierOf("Orc1", "Troll1") == RelationshipTier::friends);
    BOOST_CHECK(!relationships.isFriend("Orc1", "Troll2"));
}

BOOST_AUTO_TEST_CASE(test_EventAmounts)
{
    CreatureRelationships relationships;
    relationships.onRelationshipEvent(RelationshipEvent::trainingTogether, "Orc1", "Troll1", 0);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), relationships.getSettings().mEventTrainingTogether);

    relationships.onRelationshipEvent(RelationshipEvent::arenaLoss, "Orc1", "Troll2", 0);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll2"), relationships.getSettings().mEventArenaLoss);
}

BOOST_AUTO_TEST_CASE(test_TierChangesAreRecordedOnce)
{
    CreatureRelationships relationships;
    std::vector<RelationshipTierChange> changes;

    relationships.changeValue("Orc1", "Troll1", 30, 0);
    relationships.takeTierChanges(changes);
    BOOST_CHECK(changes.empty());

    relationships.changeValue("Orc1", "Troll1", 25, 1);
    relationships.takeTierChanges(changes);
    BOOST_REQUIRE_EQUAL(changes.size(), 1u);
    BOOST_CHECK_EQUAL(changes[0].mCreatureA, "Orc1");
    BOOST_CHECK_EQUAL(changes[0].mCreatureB, "Troll1");
    BOOST_CHECK(changes[0].mOldTier == RelationshipTier::neutral);
    BOOST_CHECK(changes[0].mNewTier == RelationshipTier::friends);

    relationships.takeTierChanges(changes);
    BOOST_CHECK(changes.empty());
}

BOOST_AUTO_TEST_CASE(test_Drift)
{
    RelationshipSettings settings;
    settings.mDriftAmount = 2;
    settings.mDriftIntervalTurns = 10;
    settings.mDriftIdleTurns = 30;
    CreatureRelationships relationships(settings);
    std::vector<RelationshipTierChange> changes;

    relationships.changeValue("Orc1", "Troll1", 51, 0);
    relationships.changeValue("Orc2", "Troll2", -51, 0);
    relationships.takeTierChanges(changes);

    // No drift while the pair had an event recently
    for(int64_t turn = 1; turn < 30; ++turn)
        relationships.doTurn(turn);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), 51);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc2", "Troll2"), -51);

    // The first drift step after the idle time moves towards 0, one step per interval
    relationships.doTurn(30);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), 49);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc2", "Troll2"), -49);
    relationships.doTurn(31);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), 49);
    relationships.doTurn(40);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), 47);

    // The tier changes were recorded when the values left the tier
    relationships.takeTierChanges(changes);
    BOOST_CHECK_EQUAL(changes.size(), 2u);

    // A pair reaching 0 is removed and never overshoots
    int64_t turn = 40;
    while(relationships.getNbPairs() > 0)
    {
        turn += 10;
        BOOST_REQUIRE(turn < 1000);
        relationships.doTurn(turn);
    }
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), 0);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc2", "Troll2"), 0);
}

BOOST_AUTO_TEST_CASE(test_Removal)
{
    CreatureRelationships relationships;
    relationships.changeValue("Orc1", "Troll1", 10, 0);
    relationships.changeValue("Orc1", "Troll2", 10, 0);
    relationships.changeValue("Orc2", "Troll1", 10, 0);
    BOOST_CHECK_EQUAL(relationships.getNbPairs(), 3u);

    relationships.removeCreature("Orc1");
    BOOST_CHECK_EQUAL(relationships.getNbPairs(), 1u);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), 0);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc2", "Troll1"), 10);

    relationships.removeCreature("Unknown");
    BOOST_CHECK_EQUAL(relationships.getNbPairs(), 1u);
}

BOOST_AUTO_TEST_CASE(test_SaveLoadRoundTrip)
{
    CreatureRelationships relationships;
    relationships.changeValue("Orc1", "Troll1", 85, 100);
    relationships.changeValue("Orc2", "Troll1", -60, 120);

    std::stringstream saved;
    saved << "[Relationships]\n";
    relationships.writeToStream(saved, 200);
    saved << "[/Relationships]\n";

    std::string header;
    saved >> header;
    BOOST_CHECK_EQUAL(header, "[Relationships]");

    CreatureRelationships loaded;
    loaded.changeValue("Old1", "Old2", 5, 0);
    BOOST_REQUIRE(loaded.readFromStream(saved, 0));
    BOOST_CHECK_EQUAL(loaded.getNbPairs(), 2u);
    BOOST_CHECK_EQUAL(loaded.getValue("Orc1", "Troll1"), 85);
    BOOST_CHECK_EQUAL(loaded.getValue("Troll1", "Orc2"), -60);
    BOOST_CHECK_EQUAL(loaded.getValue("Old1", "Old2"), 0);

    // Loading does not report tier changes
    std::vector<RelationshipTierChange> changes;
    loaded.takeTierChanges(changes);
    BOOST_CHECK(changes.empty());

    // The idle time of a pair survives the round trip: this pair had its last event 100
    // turns before saving, so it drifts as soon as the idle time of the settings is reached.
    loaded.doTurn(loaded.getSettings().mDriftIdleTurns - 100);
    BOOST_CHECK_EQUAL(loaded.getValue("Orc1", "Troll1"), 84);
}

BOOST_AUTO_TEST_CASE(test_SaveLoadWithoutPairs)
{
    // An empty table (option on, nothing happened yet) round trips as an empty section
    CreatureRelationships empty;
    std::stringstream saved;
    empty.writeToStream(saved, 0);
    saved << "[/Relationships]\n";

    CreatureRelationships loaded;
    loaded.changeValue("Orc1", "Troll1", 5, 0);
    BOOST_REQUIRE(loaded.readFromStream(saved, 0));
    BOOST_CHECK_EQUAL(loaded.getNbPairs(), 0u);

    // A save without the section simply never calls readFromStream: the table stays empty
    CreatureRelationships untouched;
    BOOST_CHECK_EQUAL(untouched.getNbPairs(), 0u);
}

BOOST_AUTO_TEST_CASE(test_LoadInvalidSection)
{
    CreatureRelationships relationships;

    std::stringstream wrongFields("Orc1\tTroll1\t5\n[/Relationships]\n");
    BOOST_CHECK(!relationships.readFromStream(wrongFields, 0));

    std::stringstream notANumber("Orc1\tTroll1\tabc\t4\n[/Relationships]\n");
    BOOST_CHECK(!relationships.readFromStream(notANumber, 0));

    std::stringstream samePair("Orc1\tOrc1\t5\t4\n[/Relationships]\n");
    BOOST_CHECK(!relationships.readFromStream(samePair, 0));

    std::stringstream noEnd("Orc1\tTroll1\t5\t4\n");
    BOOST_CHECK(!relationships.readFromStream(noEnd, 0));

    // Values outside the range are clamped, zero values are dropped
    std::stringstream outOfRange("Orc1\tTroll1\t500\t4\nOrc2\tTroll2\t0\t4\n[/Relationships]\n");
    BOOST_REQUIRE(relationships.readFromStream(outOfRange, 0));
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), 100);
    BOOST_CHECK_EQUAL(relationships.getNbPairs(), 1u);
}

BOOST_AUTO_TEST_CASE(test_SettingsFromConfig)
{
    RelationshipSettings defaults;

    // Missing and invalid entries keep their defaults
    std::map<std::string, std::string> config;
    config["ThresholdFriends"] = "40";
    config["DriftAmount"] = "abc";
    config["DriftIntervalTurns"] = "0";
    config["EventArenaLoss"] = "-7";
    RelationshipSettings settings = RelationshipSettings::fromConfig(config);
    BOOST_CHECK_EQUAL(settings.mThresholdFriends, 40);
    BOOST_CHECK_EQUAL(settings.mDriftAmount, defaults.mDriftAmount);
    BOOST_CHECK_EQUAL(settings.mDriftIntervalTurns, defaults.mDriftIntervalTurns);
    BOOST_CHECK_EQUAL(settings.mEventArenaLoss, -7);
    BOOST_CHECK_EQUAL(settings.mThresholdNemesis, defaults.mThresholdNemesis);

    // An empty configuration (missing file) gives the defaults
    RelationshipSettings fromEmpty = RelationshipSettings::fromConfig(std::map<std::string, std::string>());
    BOOST_CHECK_EQUAL(fromEmpty.mThresholdBestFriends, defaults.mThresholdBestFriends);
    BOOST_CHECK_EQUAL(fromEmpty.mEventTrainingTogether, defaults.mEventTrainingTogether);
}

BOOST_AUTO_TEST_CASE(test_ClientTiers)
{
    CreatureRelationships server;
    server.changeValue("Orc1", "Troll1", 85, 0);
    server.changeValue("Orc1", "Troll2", -90, 0);
    server.changeValue("Orc2", "Troll2", 10, 0);

    std::vector<RelationshipTierChange> tiers;
    server.getTiers(tiers);
    BOOST_REQUIRE_EQUAL(tiers.size(), 2u);

    CreatureRelationships client;
    for(const RelationshipTierChange& tier : tiers)
        client.setTier(tier.mCreatureA, tier.mCreatureB, tier.mNewTier);

    BOOST_CHECK(client.tierOf("Orc1", "Troll1") == RelationshipTier::bestFriends);
    BOOST_CHECK(client.tierOf("Orc1", "Troll2") == RelationshipTier::nemesis);
    BOOST_CHECK(client.tierOf("Orc2", "Troll2") == RelationshipTier::neutral);

    client.setTier("Orc1", "Troll1", RelationshipTier::neutral);
    BOOST_CHECK(client.tierOf("Orc1", "Troll1") == RelationshipTier::neutral);
    BOOST_CHECK_EQUAL(client.getNbPairs(), 1u);
}

// Training next to each other raises the value of the pair only once per cooldown, whichever of the two reports it
BOOST_AUTO_TEST_CASE(test_TrainingTogetherCountsOncePerCycle)
{
    CreatureRelationships relationships;
    int32_t amount = relationships.getSettings().mEventTrainingTogether;
    int64_t cooldown = relationships.getSettings().mTrainingTogetherCooldownTurns;

    relationships.onRelationshipEvent(RelationshipEvent::trainingTogether, "Orc1", "Orc2", 100);
    relationships.onRelationshipEvent(RelationshipEvent::trainingTogether, "Orc2", "Orc1", 101);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Orc2"), amount);

    relationships.onRelationshipEvent(RelationshipEvent::trainingTogether, "Orc1", "Orc2", 100 + cooldown);
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Orc2"), 2 * amount);
}

// The start value of a pair comes from the table of the races (in both orders), is applied once when the pair
// first gets a value and never applies to a creature with itself
BOOST_AUTO_TEST_CASE(test_RacialStartValue)
{
    std::map<std::string, std::string> config;
    config["Racial_Elf_Orc"] = "-15";
    config["Racial_Bad"] = "5";
    RelationshipSettings settings = RelationshipSettings::fromConfig(config);
    BOOST_CHECK_EQUAL(settings.getRacialStart("Orc", "Elf"), -15);
    BOOST_CHECK_EQUAL(settings.getRacialStart("Elf", "Orc"), -15);
    BOOST_CHECK_EQUAL(settings.getRacialStart("Orc", "Troll"), 0);

    CreatureRelationships withTable(settings);
    int32_t amount = withTable.getSettings().mEventTrainingTogether;

    // Applied once, when the pair first gets a value
    withTable.onRelationshipEvent(RelationshipEvent::trainingTogether, "Orc1", "Elf1", 0, "Orc", "Elf");
    BOOST_CHECK_EQUAL(withTable.getValue("Orc1", "Elf1"), -15 + amount);
    withTable.onRelationshipEvent(RelationshipEvent::arenaLoss, "Orc1", "Elf1", 1000, "Orc", "Elf");
    BOOST_CHECK_EQUAL(withTable.getValue("Orc1", "Elf1"), -15 + amount + withTable.getSettings().mEventArenaLoss);

    // A creature has no relationship with itself
    withTable.onRelationshipEvent(RelationshipEvent::trainingTogether, "Orc1", "Orc1", 0, "Orc", "Orc");
    BOOST_CHECK_EQUAL(withTable.getValue("Orc1", "Orc1"), 0);
}

// The combat bonus of a creature follows its best friend nearby (bonuses are not added up) and a nemesis
// nearby takes part of it away
BOOST_AUTO_TEST_CASE(test_CombatModifier)
{
    CreatureRelationships relationships;
    const RelationshipSettings& settings = relationships.getSettings();
    relationships.changeValue("A", "Friend", 60, 0);
    relationships.changeValue("A", "Best", 85, 0);
    relationships.changeValue("A", "Foe", -90, 0);
    relationships.changeValue("A", "Stranger", 10, 0);

    std::vector<std::string> nearby;
    BOOST_CHECK_EQUAL(relationships.combatModifier("A", nearby), 0.0);

    nearby.push_back("Stranger");
    BOOST_CHECK_EQUAL(relationships.combatModifier("A", nearby), 0.0);

    nearby.push_back("Friend");
    BOOST_CHECK_EQUAL(relationships.combatModifier("A", nearby), settings.mCombatBonusFriends);

    // Only the best bonus counts, it is not added up
    nearby.push_back("Best");
    BOOST_CHECK_EQUAL(relationships.combatModifier("A", nearby), settings.mCombatBonusBestFriends);

    // A nemesis next to the creature takes the bonus (partly) away
    nearby.push_back("Foe");
    BOOST_CHECK_EQUAL(relationships.combatModifier("A", nearby),
        settings.mCombatBonusBestFriends - settings.mCombatPenaltyNemesis);

    std::vector<std::string> onlyFoe(1, "Foe");
    BOOST_CHECK_EQUAL(relationships.combatModifier("A", onlyFoe), -settings.mCombatPenaltyNemesis);
}

// The mood penalty counts hated creatures and nemeses (at most the configured number of hated ones) and
// the helpers isHated and isNemesis tell the two apart
BOOST_AUTO_TEST_CASE(test_MoodModifier)
{
    CreatureRelationships relationships;
    const RelationshipSettings& settings = relationships.getSettings();
    BOOST_CHECK_EQUAL(relationships.moodModifier("A"), 0);

    relationships.changeValue("A", "Friend", 70, 0);
    relationships.changeValue("A", "Annoying", -40, 0);
    BOOST_CHECK_EQUAL(relationships.moodModifier("A"), 0);

    relationships.changeValue("A", "Hated", -60, 0);
    BOOST_CHECK_EQUAL(relationships.moodModifier("A"), -settings.mMoodPenaltyHated);
    BOOST_CHECK_EQUAL(relationships.moodModifier("Hated"), -settings.mMoodPenaltyHated);

    relationships.changeValue("A", "Foe", -90, 0);
    BOOST_CHECK_EQUAL(relationships.moodModifier("A"), -settings.mMoodPenaltyHated - settings.mMoodPenaltyNemesis);

    // At most MoodMaxPairs hated creatures count
    relationships.changeValue("A", "Hated2", -60, 0);
    BOOST_CHECK_EQUAL(relationships.moodModifier("A"), -settings.mMoodPenaltyHated - settings.mMoodPenaltyNemesis);

    BOOST_CHECK(relationships.isHated("A", "Hated"));
    BOOST_CHECK(relationships.isHated("A", "Foe"));
    BOOST_CHECK(!relationships.isHated("A", "Annoying"));
    BOOST_CHECK(relationships.isNemesis("A", "Foe"));
    BOOST_CHECK(!relationships.isNemesis("A", "Hated"));
}

// A creature has a limited number of friends and nemeses: a stronger new one pushes the weakest one back
// below the threshold, a weaker one does not get in, and the limit counts for both creatures of a pair
BOOST_AUTO_TEST_CASE(test_LimitsPerCreature)
{
    CreatureRelationships relationships;
    const RelationshipSettings& settings = relationships.getSettings();
    BOOST_REQUIRE_EQUAL(settings.mMaxFriends, 3);
    BOOST_REQUIRE_EQUAL(settings.mMaxNemeses, 2);

    relationships.changeValue("A", "F1", 55, 0);
    relationships.changeValue("A", "F2", 65, 0);
    relationships.changeValue("A", "F3", 75, 0);
    BOOST_CHECK(relationships.isFriend("A", "F1"));

    // A fourth friend: the weakest friend falls back, the new one stays
    relationships.changeValue("A", "F4", 85, 0);
    BOOST_CHECK(!relationships.isFriend("A", "F1"));
    BOOST_CHECK_EQUAL(relationships.getValue("A", "F1"), settings.mThresholdFriends - 1);
    BOOST_CHECK(relationships.isFriend("A", "F2"));
    BOOST_CHECK(relationships.isFriend("A", "F3"));
    BOOST_CHECK(relationships.isFriend("A", "F4"));

    // A new friend that is weaker than all others cannot get in
    relationships.changeValue("A", "F5", 50, 0);
    BOOST_CHECK(!relationships.isFriend("A", "F5"));
    BOOST_CHECK(relationships.isFriend("A", "F2"));

    // The limit counts for the other creature, too
    relationships.changeValue("B", "F2", 90, 0);
    relationships.changeValue("C", "F2", 91, 0);
    relationships.changeValue("D", "F2", 92, 0);
    relationships.changeValue("E", "F2", 93, 0);
    BOOST_CHECK(!relationships.isFriend("B", "F2"));
    BOOST_CHECK(!relationships.isFriend("A", "F2"));

    // Nemeses
    relationships.changeValue("X", "N1", -81, 0);
    relationships.changeValue("X", "N2", -95, 0);
    relationships.changeValue("X", "N3", -100, 0);
    BOOST_CHECK(!relationships.isNemesis("X", "N1"));
    BOOST_CHECK_EQUAL(relationships.getValue("X", "N1"), settings.mThresholdNemesis + 1);
    BOOST_CHECK(relationships.isHated("X", "N1"));
    BOOST_CHECK(relationships.isNemesis("X", "N2"));
    BOOST_CHECK(relationships.isNemesis("X", "N3"));

    std::vector<CreatureRelationships::Pair> pairs;
    relationships.getNemesisPairs(pairs);
    BOOST_CHECK_EQUAL(pairs.size(), 2u);
}

BOOST_AUTO_TEST_CASE(test_Mentoring)
{
    CreatureRelationships relationships;
    std::vector<std::pair<std::string, uint32_t> > trainees;
    trainees.push_back(std::pair<std::string, uint32_t>("Pupil", 2));
    trainees.push_back(std::pair<std::string, uint32_t>("Master", 5));
    BOOST_CHECK_EQUAL(relationships.mentoringFactor("Pupil", 2, trainees), 1.0);

    // Only a friend teaches
    relationships.changeValue("Pupil", "Master", 60, 0);
    BOOST_CHECK_EQUAL(relationships.mentoringFactor("Pupil", 2, trainees), 1.5);
    // The master does not learn from the pupil
    BOOST_CHECK_EQUAL(relationships.mentoringFactor("Master", 5, trainees), 1.0);
    // The level difference must be large enough
    BOOST_CHECK_EQUAL(relationships.mentoringFactor("Pupil", 4, trainees), 1.0);
}

BOOST_AUTO_TEST_CASE(test_Friends)
{
    CreatureRelationships relationships;
    relationships.changeValue("A", "Friend", 60, 0);
    relationships.changeValue("A", "Acquaintance", 20, 0);
    relationships.changeValue("A", "Foe", -90, 0);
    std::vector<std::string> friends;
    relationships.getFriends("A", friends);
    BOOST_REQUIRE_EQUAL(friends.size(), 1u);
    BOOST_CHECK_EQUAL(friends[0], "Friend");
    relationships.getFriends("Friend", friends);
    BOOST_REQUIRE_EQUAL(friends.size(), 1u);
    BOOST_CHECK_EQUAL(friends[0], "A");
    relationships.getFriends("Nobody", friends);
    BOOST_CHECK(friends.empty());
}

BOOST_AUTO_TEST_CASE(test_Jealousy)
{
    CreatureRelationships relationships;
    const RelationshipSettings& settings = relationships.getSettings();
    relationships.changeValue("A", "B", 60, 0);
    BOOST_CHECK_EQUAL(relationships.getValue("A", "C"), 0);

    // B becomes friends with C: A, the friend of B, gets jealous of C
    relationships.changeValue("B", "C", 55, 0);
    BOOST_CHECK_EQUAL(relationships.getValue("A", "C"), -settings.mJealousyValueLoss);
    BOOST_CHECK_EQUAL(relationships.getValue("B", "C"), 55);
    BOOST_CHECK_EQUAL(relationships.getValue("A", "B"), 60);

    // Strengthening an existing friendship does not count again
    relationships.changeValue("B", "C", 10, 0);
    BOOST_CHECK_EQUAL(relationships.getValue("A", "C"), -settings.mJealousyValueLoss);

    // Without friends nobody is jealous
    relationships.changeValue("X", "Y", 60, 0);
    BOOST_CHECK_EQUAL(relationships.getNbPairs(), 4u);
}

BOOST_AUTO_TEST_CASE(test_BestFriend)
{
    CreatureRelationships relationships;
    relationships.changeValue("A", "Friend", 60, 0);
    BOOST_CHECK(relationships.getBestFriend("A").empty());

    relationships.changeValue("A", "Buddy", 85, 0);
    BOOST_CHECK_EQUAL(relationships.getBestFriend("A"), "Buddy");
    BOOST_CHECK_EQUAL(relationships.getBestFriend("Buddy"), "A");

    relationships.changeValue("A", "Soulmate", 95, 0);
    BOOST_CHECK_EQUAL(relationships.getBestFriend("A"), "Soulmate");
    BOOST_CHECK(relationships.getBestFriend("Nobody").empty());
}

// Settings read from the config keep the default when a key is missing or not a number
BOOST_AUTO_TEST_CASE(test_ConfigValues)
{
    std::map<std::string, std::string> config;
    config["CombatRadiusTiles"] = "4.5";
    config["CombatBonusFriends"] = "abc";
    config["MaxFriends"] = "5";
    config["BrawlStopHealthPercent"] = "30";
    RelationshipSettings settings = RelationshipSettings::fromConfig(config);
    BOOST_CHECK_EQUAL(settings.mCombatRadiusTiles, 4.5);
    BOOST_CHECK_EQUAL(settings.mCombatBonusFriends, 0.75);
    BOOST_CHECK_EQUAL(settings.mMaxFriends, 5);
    BOOST_CHECK_EQUAL(settings.mBrawlStopHealthPercent, 30);
    BOOST_CHECK_EQUAL(settings.mMaxNemeses, 2);
}

namespace
{
    std::map<std::string, std::string> genders;

    std::string testGender(const std::string& name)
    {
        std::map<std::string, std::string>::const_iterator it = genders.find(name);
        return (it == genders.end()) ? std::string() : it->second;
    }

    void setupGenders(CreatureRelationships& relationships)
    {
        genders.clear();
        genders["Adam"] = "Male";
        genders["Bob"] = "Male";
        genders["Eve"] = "Female";
        genders["Fay"] = "Female";
        genders["Rock"] = "";
        relationships.setGenderLookup(testGender);
    }
}

BOOST_AUTO_TEST_CASE(test_LoversNeedOneMaleAndOneFemale)
{
    CreatureRelationships relationships;
    setupGenders(relationships);

    // Below the threshold nobody is lovers
    relationships.changeValue("Adam", "Eve", 89, 0);
    BOOST_CHECK(relationships.tierOf("Adam", "Eve", true) == RelationshipTier::bestFriends);
    BOOST_CHECK(!relationships.isLovers("Adam", "Eve"));

    // Male and female at the threshold
    std::vector<RelationshipTierChange> changes;
    relationships.takeTierChanges(changes);
    relationships.changeValue("Adam", "Eve", 1, 1);
    BOOST_CHECK(relationships.tierOf("Adam", "Eve", true) == RelationshipTier::lovers);
    BOOST_CHECK(relationships.isLovers("Eve", "Adam"));
    BOOST_CHECK_EQUAL(relationships.getPartner("Adam"), "Eve");
    BOOST_CHECK_EQUAL(relationships.getPartner("Eve"), "Adam");
    // The caller can still ask for the pure value tier
    BOOST_CHECK(relationships.tierOf("Adam", "Eve", false) == RelationshipTier::bestFriends);
    relationships.takeTierChanges(changes);
    BOOST_REQUIRE_EQUAL(changes.size(), 1u);
    BOOST_CHECK(changes[0].mOldTier == RelationshipTier::bestFriends);
    BOOST_CHECK(changes[0].mNewTier == RelationshipTier::lovers);

    // Same gender stays at best friends, however high the value is
    relationships.changeValue("Adam", "Bob", 100, 2);
    BOOST_CHECK(relationships.tierOf("Adam", "Bob", true) == RelationshipTier::bestFriends);
    relationships.changeValue("Eve", "Fay", 100, 2);
    BOOST_CHECK(relationships.tierOf("Eve", "Fay", true) == RelationshipTier::bestFriends);

    // No gender: skipped
    relationships.changeValue("Rock", "Fay", 100, 2);
    relationships.changeValue("Rock", "Unknown", 100, 2);
    BOOST_CHECK(relationships.tierOf("Rock", "Fay", true) == RelationshipTier::bestFriends);
    BOOST_CHECK(relationships.tierOf("Rock", "Unknown", true) == RelationshipTier::bestFriends);

    // Without a lookup nobody is lovers
    CreatureRelationships noLookup;
    noLookup.changeValue("Adam", "Eve", 100, 0);
    BOOST_CHECK(noLookup.tierOf("Adam", "Eve", true) == RelationshipTier::bestFriends);
}

BOOST_AUTO_TEST_CASE(test_LoversLimitOnePartner)
{
    CreatureRelationships relationships;
    setupGenders(relationships);
    relationships.changeValue("Adam", "Eve", 95, 0);
    BOOST_REQUIRE(relationships.isLovers("Adam", "Eve"));

    // A weaker candidate does not take the place of the partner
    relationships.changeValue("Adam", "Fay", 92, 1);
    BOOST_CHECK(relationships.isLovers("Adam", "Eve"));
    BOOST_CHECK(!relationships.isLovers("Adam", "Fay"));
    BOOST_CHECK(relationships.tierOf("Adam", "Fay", true) == RelationshipTier::bestFriends);
    BOOST_CHECK_EQUAL(relationships.getValue("Adam", "Fay"), 92);

    // A stronger candidate does: the old couple falls back below the threshold
    relationships.changeValue("Adam", "Fay", 6, 2);
    BOOST_CHECK(relationships.isLovers("Adam", "Fay"));
    BOOST_CHECK(!relationships.isLovers("Adam", "Eve"));
    BOOST_CHECK_EQUAL(relationships.getValue("Adam", "Eve"), 89);
    BOOST_CHECK_EQUAL(relationships.getPartner("Adam"), "Fay");
    BOOST_CHECK_EQUAL(relationships.getPartner("Eve"), "");
}

BOOST_AUTO_TEST_CASE(test_LoversBreakUpHysteresis)
{
    CreatureRelationships relationships;
    setupGenders(relationships);
    relationships.changeValue("Adam", "Eve", 95, 0);
    BOOST_REQUIRE(relationships.isLovers("Adam", "Eve"));

    // Between the break value and the threshold they stay lovers
    relationships.changeValue("Adam", "Eve", -25, 1);
    BOOST_CHECK_EQUAL(relationships.getValue("Adam", "Eve"), 70);
    BOOST_CHECK(relationships.tierOf("Adam", "Eve", true) == RelationshipTier::lovers);

    // Below 70 they break up and drop to the lower tier
    std::vector<RelationshipTierChange> changes;
    relationships.takeTierChanges(changes);
    relationships.changeValue("Adam", "Eve", -1, 2);
    BOOST_CHECK(!relationships.isLovers("Adam", "Eve"));
    BOOST_CHECK(relationships.tierOf("Adam", "Eve", true) == RelationshipTier::friends);
    BOOST_CHECK_EQUAL(relationships.getPartner("Adam"), "");
    relationships.takeTierChanges(changes);
    BOOST_REQUIRE_EQUAL(changes.size(), 1u);
    BOOST_CHECK(changes[0].mOldTier == RelationshipTier::lovers);
    BOOST_CHECK(changes[0].mNewTier == RelationshipTier::friends);

    // Back at 89 they are not lovers again, only at 90
    relationships.changeValue("Adam", "Eve", 20, 3);
    BOOST_CHECK(!relationships.isLovers("Adam", "Eve"));
    relationships.changeValue("Adam", "Eve", 1, 4);
    BOOST_CHECK(relationships.isLovers("Adam", "Eve"));

    // A single large drop ends the relationship in one tier change
    relationships.takeTierChanges(changes);
    relationships.changeValue("Adam", "Eve", -200, 5);
    BOOST_CHECK(!relationships.isLovers("Adam", "Eve"));
    relationships.takeTierChanges(changes);
    BOOST_REQUIRE_EQUAL(changes.size(), 1u);
    BOOST_CHECK(changes[0].mOldTier == RelationshipTier::lovers);
    BOOST_CHECK(changes[0].mNewTier == RelationshipTier::nemesis);
}

BOOST_AUTO_TEST_CASE(test_LoversBreakUpByDrift)
{
    CreatureRelationships relationships;
    setupGenders(relationships);
    relationships.changeValue("Adam", "Eve", 71, 0);
    BOOST_CHECK(!relationships.isLovers("Adam", "Eve"));
    relationships.changeValue("Adam", "Eve", 24, 0);
    BOOST_REQUIRE(relationships.isLovers("Adam", "Eve"));
    relationships.changeValue("Adam", "Eve", -24, 0);
    BOOST_CHECK_EQUAL(relationships.getValue("Adam", "Eve"), 71);
    BOOST_CHECK(relationships.isLovers("Adam", "Eve"));

    // Drift moves 1 point per step: 71 -> 70 keeps them, 70 -> 69 ends it
    int64_t turn = relationships.getSettings().mDriftIdleTurns + relationships.getSettings().mDriftIntervalTurns;
    relationships.doTurn(turn);
    BOOST_CHECK_EQUAL(relationships.getValue("Adam", "Eve"), 70);
    BOOST_CHECK(relationships.isLovers("Adam", "Eve"));
    std::vector<RelationshipTierChange> changes;
    relationships.takeTierChanges(changes);
    relationships.doTurn(turn + relationships.getSettings().mDriftIntervalTurns);
    BOOST_CHECK_EQUAL(relationships.getValue("Adam", "Eve"), 69);
    BOOST_CHECK(!relationships.isLovers("Adam", "Eve"));
    relationships.takeTierChanges(changes);
    BOOST_REQUIRE_EQUAL(changes.size(), 1u);
    BOOST_CHECK(changes[0].mOldTier == RelationshipTier::lovers);
    BOOST_CHECK(changes[0].mNewTier == RelationshipTier::friends);
}

BOOST_AUTO_TEST_CASE(test_LoversSaveLoadWithFlag)
{
    CreatureRelationships relationships;
    setupGenders(relationships);
    // A couple that has dropped to 75 is still lovers, a same-gender pair at 95 is not
    relationships.changeValue("Adam", "Eve", 95, 0);
    relationships.changeValue("Adam", "Eve", -20, 0);
    relationships.changeValue("Bob", "Fay", 60, 0);
    relationships.changeValue("Adam", "Bob", 95, 0);
    BOOST_REQUIRE(relationships.isLovers("Adam", "Eve"));

    std::stringstream saved;
    saved << "[Relationships]\n";
    relationships.writeToStream(saved, 10);
    saved << "[/Relationships]\n";
    std::string header;
    saved >> header;

    // The flag is read back as stored, even without a gender lookup
    CreatureRelationships loaded;
    BOOST_REQUIRE(loaded.readFromStream(saved, 10));
    BOOST_CHECK(loaded.isLovers("Adam", "Eve"));
    BOOST_CHECK_EQUAL(loaded.getValue("Adam", "Eve"), 75);
    BOOST_CHECK(!loaded.isLovers("Adam", "Bob"));
    BOOST_CHECK(!loaded.isLovers("Bob", "Fay"));
    BOOST_CHECK(loaded.tierOf("Adam", "Eve", true) == RelationshipTier::lovers);
    std::vector<RelationshipTierChange> changes;
    loaded.takeTierChanges(changes);
    BOOST_CHECK(changes.empty());
}

BOOST_AUTO_TEST_CASE(test_LoversLoadOldSaveWithoutFlag)
{
    // Old saves have four fields per line: the flag is derived from the value and the genders
    CreatureRelationships relationships;
    setupGenders(relationships);
    std::stringstream saved;
    saved << "Adam\tEve\t92\t5\n";
    saved << "Adam\tBob\t95\t5\n";
    saved << "Bob\tFay\t75\t5\n";
    saved << "[/Relationships]\n";
    BOOST_REQUIRE(relationships.readFromStream(saved, 10));
    BOOST_CHECK(relationships.isLovers("Adam", "Eve"));
    BOOST_CHECK(!relationships.isLovers("Adam", "Bob"));
    // Below the threshold an old pair is not lovers
    BOOST_CHECK(!relationships.isLovers("Bob", "Fay"));

    // Old save with two possible partners for one creature: only the stronger pair stays lovers
    CreatureRelationships crowded;
    setupGenders(crowded);
    std::stringstream saved2;
    saved2 << "Adam\tEve\t92\t5\n";
    saved2 << "Adam\tFay\t97\t5\n";
    saved2 << "[/Relationships]\n";
    BOOST_REQUIRE(crowded.readFromStream(saved2, 10));
    BOOST_CHECK(crowded.isLovers("Adam", "Fay"));
    BOOST_CHECK(!crowded.isLovers("Adam", "Eve"));

    // A line with a fifth field that is not a number is invalid
    CreatureRelationships broken;
    std::stringstream saved3;
    saved3 << "Adam\tEve\t92\t5\tx\n";
    saved3 << "[/Relationships]\n";
    BOOST_CHECK(!broken.readFromStream(saved3, 10));
}

BOOST_AUTO_TEST_CASE(test_LoversClientTiers)
{
    CreatureRelationships server;
    setupGenders(server);
    server.changeValue("Adam", "Eve", 95, 0);
    server.changeValue("Bob", "Fay", 85, 0);

    std::vector<RelationshipTierChange> tiers;
    server.getTiers(tiers);
    BOOST_REQUIRE_EQUAL(tiers.size(), 2u);
    CreatureRelationships client;
    for(const RelationshipTierChange& tier : tiers)
        client.setTier(tier.mCreatureA, tier.mCreatureB, tier.mNewTier);

    BOOST_CHECK(client.tierOf("Adam", "Eve", true) == RelationshipTier::lovers);
    BOOST_CHECK(client.tierOf("Bob", "Fay", true) == RelationshipTier::bestFriends);
    client.setTier("Adam", "Eve", RelationshipTier::bestFriends);
    BOOST_CHECK(client.tierOf("Adam", "Eve", true) == RelationshipTier::bestFriends);
}

BOOST_AUTO_TEST_CASE(test_LoversEffects)
{
    CreatureRelationships relationships;
    setupGenders(relationships);
    const RelationshipSettings& settings = relationships.getSettings();

    relationships.changeValue("Adam", "Eve", 95, 0);
    relationships.changeValue("Adam", "Bob", 85, 0);
    BOOST_REQUIRE(relationships.isLovers("Adam", "Eve"));

    // The strongest combat bonus: the partner beats a best friend, the best bonus counts once
    std::vector<std::string> nearby;
    nearby.push_back("Bob");
    BOOST_CHECK_EQUAL(relationships.combatModifier("Adam", nearby), settings.mCombatBonusBestFriends);
    nearby.push_back("Eve");
    BOOST_CHECK_EQUAL(relationships.combatModifier("Adam", nearby), settings.mCombatBonusLovers);
    BOOST_CHECK(settings.mCombatBonusLovers > settings.mCombatBonusBestFriends);

    // The mood boost is there while the pair exists; removing the creature (death, leaving) ends it
    BOOST_CHECK_EQUAL(relationships.moodModifier("Adam"), settings.mMoodBonusLovers);
    BOOST_CHECK_EQUAL(relationships.moodModifier("Eve"), settings.mMoodBonusLovers);
    BOOST_CHECK_EQUAL(relationships.moodModifier("Bob"), 0);
    relationships.removeCreature("Eve");
    BOOST_CHECK_EQUAL(relationships.moodModifier("Adam"), 0);
    BOOST_CHECK_EQUAL(relationships.getPartner("Adam"), "");
}

BOOST_AUTO_TEST_CASE(test_LoversJealousy)
{
    CreatureRelationships relationships;
    setupGenders(relationships);
    const RelationshipSettings& settings = relationships.getSettings();

    relationships.changeValue("Adam", "Eve", 95, 0);
    BOOST_REQUIRE(relationships.isLovers("Adam", "Eve"));
    relationships.changeValue("Adam", "Bob", 10, 0);
    relationships.changeValue("Eve", "Bob", 10, 0);

    // Adam becomes friends with Bob: Eve (the partner) loses more towards Bob than a friend would
    relationships.changeValue("Adam", "Bob", 45, 1);
    BOOST_CHECK(relationships.isFriend("Adam", "Bob"));
    BOOST_CHECK_EQUAL(relationships.getValue("Eve", "Bob"), 10 - settings.mLoversJealousyValueLoss);
    BOOST_CHECK(settings.mLoversJealousyValueLoss > settings.mJealousyValueLoss);
}

BOOST_AUTO_TEST_CASE(test_LoversConfigValues)
{
    RelationshipSettings defaults;
    BOOST_CHECK_EQUAL(defaults.mThresholdLovers, 90);
    BOOST_CHECK_EQUAL(defaults.mLoversBreakValue, 70);
    BOOST_CHECK_EQUAL(defaults.mMaxPartners, 1);
    BOOST_CHECK(defaults.mPartnerGriefMoodPenalty > defaults.mGriefMoodPenalty);

    std::map<std::string, std::string> config;
    config["LoversBreakValue"] = "60";
    config["CombatBonusLovers"] = "3";
    config["MoodBonusLovers"] = "123";
    config["PartnerGriefMoodPenalty"] = "900";
    config["LeavePartnerChancePercent"] = "55";
    config["LoversJealousyValueLoss"] = "9";
    RelationshipSettings settings = RelationshipSettings::fromConfig(config);
    BOOST_CHECK_EQUAL(settings.mLoversBreakValue, 60);
    BOOST_CHECK_EQUAL(settings.mCombatBonusLovers, 3.0);
    BOOST_CHECK_EQUAL(settings.mMoodBonusLovers, 123);
    BOOST_CHECK_EQUAL(settings.mPartnerGriefMoodPenalty, 900);
    BOOST_CHECK_EQUAL(settings.mLeavePartnerChancePercent, 55);
    BOOST_CHECK_EQUAL(settings.mLoversJealousyValueLoss, 9);
}

BOOST_AUTO_TEST_CASE(test_CreatureStateSaveLoadRoundTrip)
{
    std::vector<RelationshipCreatureState> states;
    RelationshipCreatureState grieving;
    grieving.mName = "Orc1";
    grieving.mGriefMood = -320;
    grieving.mRageTurnsLeft = 210;
    grieving.mRageSeatId = 3;
    states.push_back(grieving);

    RelationshipCreatureState brawling;
    brawling.mName = "Troll2";
    brawling.mBrawlOpponent = "Orc1";
    brawling.mBrawlTurnsLeft = 75;
    states.push_back(brawling);

    // A creature without anything to save is not written
    RelationshipCreatureState calm;
    calm.mName = "Elf3";
    states.push_back(calm);

    std::ostringstream out;
    writeRelationshipCreatureStates(out, states);
    out << "[/RelationshipState]\n";

    std::istringstream in(out.str());
    std::vector<RelationshipCreatureState> loaded;
    BOOST_REQUIRE(readRelationshipCreatureStates(in, loaded));
    BOOST_REQUIRE_EQUAL(loaded.size(), 2u);

    BOOST_CHECK_EQUAL(loaded[0].mName, "Orc1");
    BOOST_CHECK_EQUAL(loaded[0].mGriefMood, -320);
    BOOST_CHECK_EQUAL(loaded[0].mRageTurnsLeft, 210);
    BOOST_CHECK_EQUAL(loaded[0].mRageSeatId, 3);
    BOOST_CHECK(loaded[0].mBrawlOpponent.empty());
    BOOST_CHECK_EQUAL(loaded[0].mBrawlTurnsLeft, 0);

    BOOST_CHECK_EQUAL(loaded[1].mName, "Troll2");
    BOOST_CHECK_EQUAL(loaded[1].mGriefMood, 0);
    BOOST_CHECK_EQUAL(loaded[1].mRageTurnsLeft, 0);
    BOOST_CHECK_EQUAL(loaded[1].mBrawlOpponent, "Orc1");
    BOOST_CHECK_EQUAL(loaded[1].mBrawlTurnsLeft, 75);
}

BOOST_AUTO_TEST_CASE(test_CreatureStateOldSaveAndEmptyState)
{
    // Nothing to save writes nothing at all
    std::vector<RelationshipCreatureState> states;
    RelationshipCreatureState calm;
    calm.mName = "Orc1";
    states.push_back(calm);
    std::ostringstream out;
    writeRelationshipCreatureStates(out, states);
    BOOST_CHECK(out.str().empty());

    // An old save has no such section: the pair table loads as before and nothing else follows
    CreatureRelationships relationships;
    std::istringstream oldSave("Orc1\tTroll1\t60\t5\t0\n[/Relationships]\n");
    BOOST_REQUIRE(relationships.readFromStream(oldSave, 100));
    BOOST_CHECK_EQUAL(relationships.getValue("Orc1", "Troll1"), 60);
    std::string rest;
    BOOST_CHECK(!(oldSave >> rest));

    // An empty section is valid and gives an empty state
    std::vector<RelationshipCreatureState> loaded;
    loaded.push_back(calm);
    std::istringstream emptySection("[/RelationshipState]\n");
    BOOST_CHECK(readRelationshipCreatureStates(emptySection, loaded));
    BOOST_CHECK(loaded.empty());

    // Broken lines and a missing end marker are refused
    std::istringstream broken("Orc1\tabc\t0\t-1\t\t0\n[/RelationshipState]\n");
    BOOST_CHECK(!readRelationshipCreatureStates(broken, loaded));
    std::istringstream unfinished("Orc1\t-100\t0\t-1\t\t0\n");
    BOOST_CHECK(!readRelationshipCreatureStates(unfinished, loaded));
}

BOOST_AUTO_TEST_SUITE_END()
