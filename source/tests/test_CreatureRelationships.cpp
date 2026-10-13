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
    // turns before saving, so it drifts right at the first drift step after loading.
    loaded.doTurn(10);
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

BOOST_AUTO_TEST_SUITE_END()
