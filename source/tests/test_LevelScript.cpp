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

#define BOOST_TEST_MODULE LevelScript
#include "BoostTestTargetConfig.h"

#include "gamemap/LevelScript.h"

#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

//! \brief A [Triggers] section (without the opening tag) that uses every kind of line of the model
static const std::string sample =
    "# comment\n"
    "Flag\tgateOpen\t1\n"
    "TimeLimit\t300\n"
    "Countdown\t900\n"
    "WaveCountdown\t240\n"
    "Region\tGate\t30\t12\t28\t10\n"
    "[Trigger]\n"
    "Name\tambush\n"
    "Mode\trepeat\t30\n"
    "Cond\ttime\t10\n"
    "Cond\tregion\t1\t20\t21\t25\t26\n"
    "Cond\tcreatures\t1\t>=\t3\n"
    "Cond\troom\t1\tTreasury\t1\n"
    "Cond\tgoal\t1\tClaimNTiles\n"
    "Cond\tflag\tgateOpen\t1\n"
    "Action\tmessage\t-1\tThey are coming, keeper.\n"
    "Action\tobjective\t1\tDefend the heart\n"
    "Action\tspawn\t0\t30\t30\t1\tKnight:2\tArcher:1\n"
    "Action\tgold\t1\t500\n"
    "Action\tsetflag\tambushDone\t1\n"
    "Action\twin\t1\n"
    "Action\tlose\t2\n"
    "State\t2\t120\n"
    "[/Trigger]\n"
    "[Trigger]\n"
    "Name\tsecond\n"
    "Mode\tonce\n"
    "Cond\tflag\tambushDone\t1\n"
    "Action\tmessage\t1\tDone\n"
    "[/Trigger]\n"
    "[Trigger]\n"
    "Name\tgateWatch\n"
    "Mode\tonce\n"
    "Cond\tregion\t1\tGate\n"
    "Cond\tgold\t1\t>=\t500\n"
    "Cond\tmana\t1\t<=\t100\n"
    "Cond\tkills\t1\t>=\t7\n"
    "Cond\tmined\t1\t>=\t900\n"
    "Cond\tclaimed\t1\tGate\t4\n"
    "Cond\tclaimed\t2\tGate\tall\n"
    "Action\treveal\t1\tGate\n"
    "Action\taddflag\tvisits\t-2\n"
    "[/Trigger]\n"
    "[Trigger]\n"
    "Name\tunlock\n"
    "Mode\tonce\n"
    "Cond\tflag\tvisits\t-2\n"
    "Action\tmake\t1\troomHatchery\n"
    "Action\ttimelimit\t120\n"
    "Action\tcountdown\t60\n"
    "Action\twavecountdown\t30\n"
    "[/Trigger]\n"
    "[Trigger]\n"
    "Name\tcreatureEvents\n"
    "Mode\tonce\n"
    "Cond\thappy\t1\t>=\t2\n"
    "Cond\tangry\t1\t<=\t0\n"
    "Cond\tatlevel\t1\t3\t>=\t2\n"
    "Cond\tlost\t1\t>=\t1\n"
    "Cond\tpickedup\t1\t>=\t4\n"
    "Cond\tdropped\t1\t>=\t3\n"
    "Cond\tslapped\t1\t<=\t5\n"
    "Action\tmessage\t1\tEvents\n"
    "[/Trigger]\n"
    "[/Triggers]\n";

//! \brief The triggers, conditions and actions of the sample are read with the values written there
BOOST_AUTO_TEST_CASE(test_parse)
{
    LevelScript script;
    std::istringstream is(sample);
    BOOST_REQUIRE(script.importFromStream(is));
    BOOST_REQUIRE_EQUAL(script.getTriggers().size(), 5u);
    BOOST_CHECK_EQUAL(script.getFlag("gateOpen"), 1);
    BOOST_CHECK_EQUAL(script.getFlag("unknown"), 0);

    const LevelScriptTrigger& t = script.getTriggers()[0];
    BOOST_CHECK_EQUAL(t.mName, "ambush");
    BOOST_CHECK(t.mRepeat);
    BOOST_CHECK_EQUAL(t.mCooldownSeconds, 30);
    BOOST_REQUIRE_EQUAL(t.mConditions.size(), 6u);
    BOOST_REQUIRE_EQUAL(t.mActions.size(), 7u);
    BOOST_CHECK_EQUAL(t.mTimesFired, 2u);
    BOOST_CHECK_EQUAL(t.mLastFiredTurn, 120);

    BOOST_CHECK(t.mConditions[1].mType == LevelScriptConditionType::region);
    BOOST_CHECK_EQUAL(t.mConditions[1].mX1, 20);
    BOOST_CHECK_EQUAL(t.mConditions[1].mY2, 26);
    BOOST_CHECK(t.mConditions[2].mAtLeast);
    BOOST_CHECK_EQUAL(t.mConditions[3].mName, "Treasury");

    BOOST_CHECK_EQUAL(t.mActions[0].mText, "They are coming, keeper.");
    BOOST_CHECK_EQUAL(t.mActions[0].mSeatId, -1);
    BOOST_REQUIRE_EQUAL(t.mActions[2].mCreatures.size(), 2u);
    BOOST_CHECK_EQUAL(t.mActions[2].mCreatures[0].first, "Knight");
    BOOST_CHECK_EQUAL(t.mActions[2].mCreatures[0].second, 2u);
    BOOST_CHECK_EQUAL(t.mActions[2].mTargetSeatId, 1);
    BOOST_CHECK_EQUAL(script.getTriggers()[1].mTimesFired, 0u);

    BOOST_REQUIRE_EQUAL(script.getRegions().size(), 1u);
    const LevelScriptTrigger& watch = script.getTriggers()[2];
    BOOST_CHECK_EQUAL(watch.mConditions[0].mName, "Gate");
    BOOST_CHECK(watch.mActions[0].mType == LevelScriptActionType::reveal);
    BOOST_CHECK_EQUAL(watch.mActions[0].mText, "Gate");

    BOOST_REQUIRE_EQUAL(watch.mConditions.size(), 7u);
    BOOST_CHECK(watch.mConditions[1].mType == LevelScriptConditionType::gold);
    BOOST_CHECK(watch.mConditions[1].mAtLeast);
    BOOST_CHECK_EQUAL(watch.mConditions[1].mNumber, 500);
    BOOST_CHECK(watch.mConditions[2].mType == LevelScriptConditionType::mana);
    BOOST_CHECK(!watch.mConditions[2].mAtLeast);
    BOOST_CHECK(watch.mConditions[3].mType == LevelScriptConditionType::kills);
    BOOST_CHECK_EQUAL(watch.mConditions[3].mNumber, 7);
    BOOST_CHECK(watch.mConditions[4].mType == LevelScriptConditionType::goldMined);
    BOOST_CHECK_EQUAL(watch.mConditions[5].mNumber, 4);
    BOOST_CHECK_EQUAL(watch.mConditions[6].mNumber, -1);
    BOOST_REQUIRE_EQUAL(watch.mActions.size(), 2u);
    BOOST_CHECK(watch.mActions[1].mType == LevelScriptActionType::addFlag);
    BOOST_CHECK_EQUAL(watch.mActions[1].mNumber, -2);

    const LevelScriptTrigger& unlock = script.getTriggers()[3];
    BOOST_REQUIRE_EQUAL(unlock.mActions.size(), 4u);
    BOOST_CHECK(unlock.mActions[0].mType == LevelScriptActionType::make);
    BOOST_CHECK_EQUAL(unlock.mActions[0].mSeatId, 1);
    BOOST_CHECK_EQUAL(unlock.mActions[0].mText, "roomHatchery");
    BOOST_CHECK(unlock.mActions[1].mType == LevelScriptActionType::timeLimit);
    BOOST_CHECK_EQUAL(unlock.mActions[1].mNumber, 120);
    BOOST_CHECK(unlock.mActions[2].mType == LevelScriptActionType::countdown);
    BOOST_CHECK_EQUAL(unlock.mActions[2].mNumber, 60);
    BOOST_CHECK(unlock.mActions[3].mType == LevelScriptActionType::waveCountdown);
    BOOST_CHECK_EQUAL(unlock.mActions[3].mNumber, 30);

    const LevelScriptTrigger& events = script.getTriggers()[4];
    BOOST_REQUIRE_EQUAL(events.mConditions.size(), 7u);
    BOOST_CHECK(events.mConditions[0].mType == LevelScriptConditionType::happyCreatures);
    BOOST_CHECK(events.mConditions[0].mAtLeast);
    BOOST_CHECK_EQUAL(events.mConditions[0].mNumber, 2);
    BOOST_CHECK(events.mConditions[1].mType == LevelScriptConditionType::angryCreatures);
    BOOST_CHECK(!events.mConditions[1].mAtLeast);
    BOOST_CHECK(events.mConditions[2].mType == LevelScriptConditionType::creaturesAtLevel);
    BOOST_CHECK_EQUAL(events.mConditions[2].mSeatId, 1);
    BOOST_CHECK_EQUAL(events.mConditions[2].mX1, 3);
    BOOST_CHECK(events.mConditions[2].mAtLeast);
    BOOST_CHECK_EQUAL(events.mConditions[2].mNumber, 2);
    BOOST_CHECK(events.mConditions[3].mType == LevelScriptConditionType::creaturesLost);
    BOOST_CHECK(events.mConditions[4].mType == LevelScriptConditionType::creaturesPickedUp);
    BOOST_CHECK_EQUAL(events.mConditions[4].mNumber, 4);
    BOOST_CHECK(events.mConditions[5].mType == LevelScriptConditionType::creaturesDropped);
    BOOST_CHECK(events.mConditions[6].mType == LevelScriptConditionType::creaturesSlapped);
    BOOST_CHECK(!events.mConditions[6].mAtLeast);
}

//! \brief The time limit and the two countdowns are read, moved to a new level start, written and cleared
BOOST_AUTO_TEST_CASE(test_time_limit)
{
    LevelScript script;
    BOOST_CHECK_EQUAL(script.getTimeLimitSeconds(), LevelScript::TIME_LIMIT_NOT_SET);
    BOOST_CHECK_EQUAL(script.getCountdownSeconds(), LevelScript::COUNTDOWN_NOT_SET);
    BOOST_CHECK_EQUAL(script.getWaveCountdownSeconds(), LevelScript::COUNTDOWN_NOT_SET);
    BOOST_CHECK(script.isEmpty());

    std::istringstream is(sample);
    BOOST_REQUIRE(script.importFromStream(is));
    BOOST_CHECK_EQUAL(script.getTimeLimitSeconds(), 300);
    BOOST_CHECK_EQUAL(script.getCountdownSeconds(), 900);
    BOOST_CHECK_EQUAL(script.getWaveCountdownSeconds(), 240);

    // A saved game counts the limit from its new start, which is later than the old one
    script.rebaseTimeLimit(100);
    BOOST_CHECK_EQUAL(script.getTimeLimitSeconds(), 200);
    BOOST_CHECK_EQUAL(script.getCountdownSeconds(), 800);
    BOOST_CHECK_EQUAL(script.getWaveCountdownSeconds(), 140);
    script.rebaseTimeLimit(500);
    BOOST_CHECK_EQUAL(script.getTimeLimitSeconds(), 0);
    BOOST_CHECK_EQUAL(script.getCountdownSeconds(), 300);
    BOOST_CHECK_EQUAL(script.getWaveCountdownSeconds(), 0);

    // No limit and a removed limit stay what they are
    script.setTimeLimitSeconds(LevelScript::TIME_LIMIT_REMOVED);
    script.rebaseTimeLimit(10);
    BOOST_CHECK_EQUAL(script.getTimeLimitSeconds(), LevelScript::TIME_LIMIT_REMOVED);
    script.setTimeLimitSeconds(LevelScript::TIME_LIMIT_NOT_SET);
    script.rebaseTimeLimit(10);
    BOOST_CHECK_EQUAL(script.getTimeLimitSeconds(), LevelScript::TIME_LIMIT_NOT_SET);

    // The limit alone is a script worth writing, and it survives the round trip
    LevelScript onlyLimit;
    onlyLimit.setTimeLimitSeconds(42);
    BOOST_CHECK(!onlyLimit.isEmpty());
    std::ostringstream os;
    onlyLimit.exportToStream(os);
    std::string written = os.str();
    BOOST_REQUIRE_EQUAL(written.compare(0, 11, "[Triggers]\n"), 0);
    LevelScript again;
    std::istringstream is2(written.substr(11));
    BOOST_REQUIRE(again.importFromStream(is2));
    BOOST_CHECK_EQUAL(again.getTimeLimitSeconds(), 42);

    again.clear();
    BOOST_CHECK_EQUAL(again.getTimeLimitSeconds(), LevelScript::TIME_LIMIT_NOT_SET);
}

//! \brief Regions: the corners may come in any order, names are unique and the editor helpers work
BOOST_AUTO_TEST_CASE(test_regions)
{
    LevelScript script;
    std::istringstream is(sample);
    BOOST_REQUIRE(script.importFromStream(is));

    // The corners may be given in any order
    const LevelScriptRegion* gate = script.getRegion("Gate");
    BOOST_REQUIRE(gate != nullptr);
    BOOST_CHECK(gate->contains(29, 11));
    BOOST_CHECK(gate->contains(28, 10));
    BOOST_CHECK(gate->contains(30, 12));
    BOOST_CHECK(!gate->contains(31, 11));
    BOOST_CHECK_EQUAL(script.getRegionNameAt(29, 11), "Gate");
    BOOST_CHECK_EQUAL(script.getRegionNameAt(0, 0), "");
    BOOST_CHECK(script.getRegion("Missing") == nullptr);

    BOOST_CHECK_EQUAL(script.getFreeRegionName(), "Region1");
    script.setRegion(LevelScriptRegion("Region1", 1, 2, 3, 4));
    BOOST_CHECK_EQUAL(script.getFreeRegionName(), "Region2");

    // Same name: the region is moved, not duplicated
    script.setRegion(LevelScriptRegion("Gate", 5, 5, 6, 6));
    BOOST_CHECK_EQUAL(script.getRegions().size(), 2u);
    BOOST_CHECK(!script.getRegion("Gate")->contains(29, 11));

    BOOST_CHECK(script.removeRegion("Region1"));
    BOOST_CHECK(!script.removeRegion("Region1"));
    BOOST_CHECK_EQUAL(script.getRegions().size(), 1u);

    // A level with markers and no triggers must still be written
    LevelScript onlyRegion;
    onlyRegion.setRegion(LevelScriptRegion("A", 0, 0, 1, 1));
    BOOST_CHECK(!onlyRegion.isEmpty());
}

//! \brief Writing a script and reading it again gives the same text
BOOST_AUTO_TEST_CASE(test_roundtrip)
{
    LevelScript script;
    std::istringstream is(sample);
    BOOST_REQUIRE(script.importFromStream(is));

    std::ostringstream os;
    script.exportToStream(os);

    // The tag has to be consumed by the level reader before the import
    std::string written = os.str();
    BOOST_REQUIRE_EQUAL(written.compare(0, 11, "[Triggers]\n"), 0);

    LevelScript script2;
    std::istringstream is2(written.substr(11));
    BOOST_REQUIRE(script2.importFromStream(is2));

    std::ostringstream os2;
    script2.exportToStream(os2);
    BOOST_CHECK_EQUAL(written, os2.str());
}

//! \brief Malformed sections are refused
BOOST_AUTO_TEST_CASE(test_invalid)
{
    const char* invalid[] = {
        // Unknown condition
        "[Trigger]\nName\tx\nCond\tmoon\t1\nAction\twin\t1\n[/Trigger]\n[/Triggers]\n",
        // No action
        "[Trigger]\nName\tx\nCond\ttime\t1\n[/Trigger]\n[/Triggers]\n",
        // Missing argument
        "[Trigger]\nName\tx\nCond\tregion\t1\t2\t3\nAction\twin\t1\n[/Trigger]\n[/Triggers]\n",
        // Bad creature entry
        "[Trigger]\nName\tx\nCond\ttime\t1\nAction\tspawn\t0\t1\t1\t-1\tKnight\n[/Trigger]\n[/Triggers]\n",
        // Missing end tag
        "[Trigger]\nName\tx\nCond\ttime\t1\nAction\twin\t1\n[/Trigger]\n",
        // Not a number
        "[Trigger]\nName\tx\nCond\ttime\tsoon\nAction\twin\t1\n[/Trigger]\n[/Triggers]\n",
        // Nothing to claim
        "[Trigger]\nName\tx\nCond\tclaimed\t1\tGate\t0\nAction\twin\t1\n[/Trigger]\n[/Triggers]\n",
        // Wrong comparison
        "[Trigger]\nName\tx\nCond\tgold\t1\t=\t5\nAction\twin\t1\n[/Trigger]\n[/Triggers]\n",
        // Region with a missing corner value
        "Region\tA\t1\t2\t3\n[/Triggers]\n",
        // Region inside a trigger
        "[Trigger]\nName\tx\nRegion\tA\t1\t2\t3\t4\nCond\ttime\t1\nAction\twin\t1\n[/Trigger]\n[/Triggers]\n",
        // reveal without a region name
        "[Trigger]\nName\tx\nCond\ttime\t1\nAction\treveal\t1\n[/Trigger]\n[/Triggers]\n",
        // atlevel without a level
        "[Trigger]\nName\tx\nCond\tatlevel\t1\t>=\t2\nAction\twin\t1\n[/Trigger]\n[/Triggers]\n",
        // atlevel with level 0
        "[Trigger]\nName\tx\nCond\tatlevel\t1\t0\t>=\t2\nAction\twin\t1\n[/Trigger]\n[/Triggers]\n",
        // A creature event with a wrong comparison
        "[Trigger]\nName\tx\nCond\tslapped\t1\t=\t2\nAction\twin\t1\n[/Trigger]\n[/Triggers]\n",
        // A negative time limit
        "[Trigger]\nName\tx\nCond\ttime\t1\nAction\ttimelimit\t-5\n[/Trigger]\n[/Triggers]\n",
        // TimeLimit inside a trigger
        "[Trigger]\nName\tx\nTimeLimit\t5\nCond\ttime\t1\nAction\twin\t1\n[/Trigger]\n[/Triggers]\n",
        // make without a skill name
        "[Trigger]\nName\tx\nCond\ttime\t1\nAction\tmake\t1\n[/Trigger]\n[/Triggers]\n"
    };

    for(const char* text : invalid)
    {
        LevelScript script;
        std::istringstream is(text);
        BOOST_CHECK_MESSAGE(!script.importFromStream(is), text);
    }
}

//! \brief Reads the [Triggers] section of the level file named by the environment variable
//! OD_TEST_LEVEL_FILE (the region test level). Does nothing when the variable is not set.
BOOST_AUTO_TEST_CASE(test_region_level)
{
    const char* path = std::getenv("OD_TEST_LEVEL_FILE");
    if(path == nullptr)
        return;

    std::ifstream file(path);
    BOOST_REQUIRE(file.good());
    std::string line;
    bool found = false;
    while(std::getline(file, line))
    {
        if(line.compare(0, 10, "[Triggers]") == 0)
        {
            found = true;
            break;
        }
    }
    BOOST_REQUIRE(found);

    LevelScript script;
    BOOST_REQUIRE(script.importFromStream(file));
    BOOST_CHECK(script.getRegion("Gate") != nullptr);
    BOOST_CHECK(script.getRegion("Home") != nullptr);
    BOOST_CHECK(script.getRegion("Far") != nullptr);

    // Every condition and action kind that the level is meant to exercise is used
    std::set<int> conditions;
    std::set<int> actions;
    for(const LevelScriptTrigger& trigger : script.getTriggers())
    {
        for(const LevelScriptCondition& cond : trigger.mConditions)
        {
            conditions.insert(static_cast<int>(cond.mType));
            // A named region has to exist
            if(!cond.mName.empty() && ((cond.mType == LevelScriptConditionType::region) ||
               (cond.mType == LevelScriptConditionType::claimed)))
            {
                BOOST_CHECK_MESSAGE(script.getRegion(cond.mName) != nullptr, cond.mName);
            }
        }
        for(const LevelScriptAction& action : trigger.mActions)
        {
            actions.insert(static_cast<int>(action.mType));
            if(action.mType == LevelScriptActionType::reveal)
                BOOST_CHECK_MESSAGE(script.getRegion(action.mText) != nullptr, action.mText);
        }
    }
    BOOST_CHECK(conditions.count(static_cast<int>(LevelScriptConditionType::region)) == 1);
    BOOST_CHECK(conditions.count(static_cast<int>(LevelScriptConditionType::claimed)) == 1);
    BOOST_CHECK(conditions.count(static_cast<int>(LevelScriptConditionType::gold)) == 1);
    BOOST_CHECK(conditions.count(static_cast<int>(LevelScriptConditionType::kills)) == 1);
    BOOST_CHECK(actions.count(static_cast<int>(LevelScriptActionType::reveal)) == 1);
    BOOST_CHECK(actions.count(static_cast<int>(LevelScriptActionType::make)) == 1);
    BOOST_CHECK(actions.count(static_cast<int>(LevelScriptActionType::timeLimit)) == 1);
    BOOST_CHECK(actions.count(static_cast<int>(LevelScriptActionType::win)) == 1);
}

//! \brief A section with only the end tag is valid and empty
BOOST_AUTO_TEST_CASE(test_empty)
{
    LevelScript script;
    std::istringstream is("[/Triggers]\n");
    BOOST_CHECK(script.importFromStream(is));
    BOOST_CHECK(script.isEmpty());
}

//! \brief Every comparison operator, a flag compared with another flag, and the timers
BOOST_AUTO_TEST_CASE(test_flag_operators_and_timers)
{
    static const std::string text =
        "Timer\tspawnDelay\t1\t45\n"
        "Timer\tstopped\t0\t7\n"
        "[Trigger]\n"
        "Name\tops\n"
        "Mode\tonce\n"
        "Cond\tflag\tF1\t!=\t0\n"
        "Cond\tflag\tF2\t>\t3\n"
        "Cond\tflag\tF3\t<\t4\n"
        "Cond\tflag\tF4\t>=\t@F5\n"
        "Cond\tflag\tF6\t==\t2\n"
        "Cond\tflag\tF7\t1\n"
        "Cond\tgold\t1\t==\t500\n"
        "Cond\tcreatures\t1\t!=\t0\n"
        "Cond\ttimer\tspawnDelay\t>=\t30\n"
        "Action\ttimer\tspawnDelay\t5\n"
        "Action\ttimer\tother\n"
        "[/Trigger]\n"
        "[/Triggers]\n";
    LevelScript script;
    std::istringstream is(text);
    BOOST_REQUIRE(script.importFromStream(is));
    BOOST_REQUIRE_EQUAL(script.getTriggers().size(), 1u);
    const LevelScriptTrigger& trigger = script.getTriggers()[0];
    BOOST_REQUIRE_EQUAL(trigger.mConditions.size(), 9u);
    BOOST_CHECK(trigger.mConditions[0].mCompare == LevelScriptCompare::notEqual);
    BOOST_CHECK(trigger.mConditions[1].mCompare == LevelScriptCompare::greater);
    BOOST_CHECK(trigger.mConditions[2].mCompare == LevelScriptCompare::less);
    BOOST_CHECK(trigger.mConditions[3].mCompare == LevelScriptCompare::atLeast);
    BOOST_CHECK_EQUAL(trigger.mConditions[3].mName2, "F5");
    BOOST_CHECK(trigger.mConditions[4].mCompare == LevelScriptCompare::equal);
    BOOST_CHECK(trigger.mConditions[5].mCompare == LevelScriptCompare::equal);
    BOOST_CHECK_EQUAL(trigger.mConditions[5].mNumber, 1);
    BOOST_CHECK(trigger.mConditions[6].mCompare == LevelScriptCompare::equal);
    BOOST_CHECK(trigger.mConditions[7].mCompare == LevelScriptCompare::notEqual);
    BOOST_CHECK(trigger.mConditions[8].mType == LevelScriptConditionType::timer);
    BOOST_CHECK_EQUAL(trigger.mConditions[8].mNumber, 30);
    BOOST_REQUIRE_EQUAL(trigger.mActions.size(), 2u);
    BOOST_CHECK(trigger.mActions[0].mType == LevelScriptActionType::startTimer);
    BOOST_CHECK_EQUAL(trigger.mActions[0].mNumber, 5);
    BOOST_CHECK_EQUAL(trigger.mActions[1].mNumber, 0);

    BOOST_CHECK(levelScriptCompare(5, LevelScriptCompare::greater, 4));
    BOOST_CHECK(!levelScriptCompare(4, LevelScriptCompare::greater, 4));
    BOOST_CHECK(levelScriptCompare(4, LevelScriptCompare::less, 5));
    BOOST_CHECK(levelScriptCompare(4, LevelScriptCompare::notEqual, 5));
    BOOST_CHECK(levelScriptCompare(4, LevelScriptCompare::equal, 4));

    // Timers are saved with their turns and keep running after a load
    BOOST_CHECK_EQUAL(script.getTimerTurns("spawnDelay"), 45);
    BOOST_CHECK_EQUAL(script.getTimerTurns("never"), 0);
    script.advanceTimers();
    BOOST_CHECK_EQUAL(script.getTimerTurns("spawnDelay"), 46);
    BOOST_CHECK_EQUAL(script.getTimerTurns("stopped"), 7);
    script.startTimer("fresh", 3);
    std::ostringstream os;
    script.exportToStream(os);
    std::string written = os.str();
    LevelScript again;
    std::istringstream is2(written.substr(11));
    BOOST_REQUIRE(again.importFromStream(is2));
    BOOST_CHECK_EQUAL(again.getTimerTurns("spawnDelay"), 46);
    BOOST_CHECK_EQUAL(again.getTimerTurns("fresh"), 3);
    BOOST_CHECK_EQUAL(again.getTimers().size(), 3u);
    std::ostringstream os2;
    again.exportToStream(os2);
    BOOST_CHECK_EQUAL(os2.str(), written);

    // Invalid forms are refused
    static const char* bad[] = {
        "[Trigger]\nName\tx\nMode\tonce\nCond\tflag\tF1\t<>\t2\nAction\tmessage\t1\tx\n[/Trigger]\n[/Triggers]\n",
        "[Trigger]\nName\tx\nMode\tonce\nCond\ttimer\tt\t1\nAction\tmessage\t1\tx\n[/Trigger]\n[/Triggers]\n",
        "[Trigger]\nName\tx\nMode\tonce\nCond\ttime\t1\nAction\ttimer\n[/Trigger]\n[/Triggers]\n"};
    for(const char* entry : bad)
    {
        LevelScript invalid;
        std::istringstream isBad(entry);
        BOOST_CHECK_MESSAGE(!invalid.importFromStream(isBad), entry);
    }
}

//! The story state conditions (seats, named creatures, spells, traps, rooms, portal state, reaching a place)
//! and the actions portal, available and remove, with the portal and creature class state they keep
BOOST_AUTO_TEST_CASE(test_story_state)
{
    static const std::string text =
        "PortalOff\t3\n"
        "Block\t1\tTroll\n"
        "Region\tExit\t5\t6\t7\t8\n"
        "[Trigger]\n"
        "Name\tmany\n"
        "Mode\tonce\n"
        "Cond\tdefeated\t4\n"
        "Cond\towns\t1\tLordKnight3\n"
        "Cond\thealth\tLordKnight3\t<\t50\n"
        "Cond\tspell\t1\tspellPossess\n"
        "Cond\tbuilt\t1\tCannon\t>=\t2\n"
        "Cond\tbuilt\t2\tany\t==\t0\n"
        "Cond\troomtiles\t1\tTreasury\t>=\t12\n"
        "Cond\troomsize\t1\tHatchery\t>=\t9\n"
        "Cond\tportal\t2\toff\n"
        "Cond\talive\tWarden\n"
        "Cond\talive\tKing\t0\n"
        "Cond\treached\tWarden\tregion\tExit\n"
        "Cond\treached\tCarrier\theart\t1\n"
        "Action\tportal\t2\ton\n"
        "Action\tavailable\t1\tTroll\t0\n"
        "Action\tavailable\t1\tTroll\t1\n"
        "Action\tremove\tKnight_5\n"
        "[/Trigger]\n"
        "[/Triggers]\n";
    LevelScript script;
    std::istringstream is(text);
    BOOST_REQUIRE(script.importFromStream(is));
    BOOST_CHECK(script.isPortalOff(3));
    BOOST_CHECK(!script.isPortalOff(2));
    BOOST_CHECK(script.isCreatureBlocked(1, "Troll"));
    BOOST_CHECK(!script.isCreatureBlocked(2, "Troll"));
    script.setPortalOff(2, true);
    script.setCreatureBlocked(1, "Troll", false);
    BOOST_CHECK(script.isPortalOff(2));
    BOOST_CHECK(!script.isCreatureBlocked(1, "Troll"));
    script.setPortalOff(2, false);
    script.setCreatureBlocked(1, "Troll", true);

    BOOST_REQUIRE_EQUAL(script.getTriggers().size(), 1u);
    const LevelScriptTrigger& trigger = script.getTriggers()[0];
    BOOST_REQUIRE_EQUAL(trigger.mConditions.size(), 12u);
    BOOST_CHECK(trigger.mConditions[0].mType == LevelScriptConditionType::seatDefeated);
    BOOST_CHECK_EQUAL(trigger.mConditions[0].mSeatId, 4);
    BOOST_CHECK(trigger.mConditions[1].mType == LevelScriptConditionType::ownsCreature);
    BOOST_CHECK_EQUAL(trigger.mConditions[1].mName, "LordKnight3");
    BOOST_CHECK(trigger.mConditions[2].mType == LevelScriptConditionType::creatureHealth);
    BOOST_CHECK(trigger.mConditions[2].mCompare == LevelScriptCompare::less);
    BOOST_CHECK_EQUAL(trigger.mConditions[2].mNumber, 50);
    BOOST_CHECK(trigger.mConditions[3].mType == LevelScriptConditionType::spellKnown);
    BOOST_CHECK(trigger.mConditions[4].mType == LevelScriptConditionType::trapsBuilt);
    BOOST_CHECK_EQUAL(trigger.mConditions[4].mName, "Cannon");
    BOOST_CHECK(trigger.mConditions[5].mCompare == LevelScriptCompare::equal);
    BOOST_CHECK(trigger.mConditions[6].mType == LevelScriptConditionType::roomTiles);
    BOOST_CHECK(trigger.mConditions[7].mType == LevelScriptConditionType::largestRoom);
    BOOST_CHECK(trigger.mConditions[8].mType == LevelScriptConditionType::portalActive);
    BOOST_CHECK_EQUAL(trigger.mConditions[8].mSeatId, 2);
    BOOST_CHECK_EQUAL(trigger.mConditions[8].mNumber, 0);
    BOOST_CHECK(trigger.mConditions[9].mType == LevelScriptConditionType::creatureAlive);
    BOOST_CHECK_EQUAL(trigger.mConditions[9].mNumber, 1);
    BOOST_CHECK_EQUAL(trigger.mConditions[10].mNumber, 0);
    BOOST_CHECK(trigger.mConditions[11].mType == LevelScriptConditionType::creatureReached);
    BOOST_CHECK_EQUAL(trigger.mConditions[11].mName, "Warden");
    BOOST_CHECK_EQUAL(trigger.mConditions[11].mName2, "Exit");
    BOOST_REQUIRE_EQUAL(trigger.mActions.size(), 4u);
    BOOST_CHECK(trigger.mActions[0].mType == LevelScriptActionType::portalStatus);
    BOOST_CHECK_EQUAL(trigger.mActions[0].mNumber, 1);
    BOOST_CHECK(trigger.mActions[1].mType == LevelScriptActionType::creatureAvailable);
    BOOST_CHECK_EQUAL(trigger.mActions[1].mNumber, 0);
    BOOST_CHECK_EQUAL(trigger.mActions[2].mNumber, 1);
    BOOST_CHECK(trigger.mActions[3].mType == LevelScriptActionType::removeCreature);
    BOOST_CHECK_EQUAL(trigger.mActions[3].mText, "Knight_5");

    std::ostringstream os;
    script.exportToStream(os);
    std::string written = os.str();
    LevelScript again;
    std::istringstream is2(written.substr(11));
    BOOST_REQUIRE(again.importFromStream(is2));
    std::ostringstream os2;
    again.exportToStream(os2);
    BOOST_CHECK_EQUAL(os2.str(), written);

    static const char* bad[] = {
        "[Trigger]\nName\tx\nMode\tonce\nCond\tdefeated\nAction\tmessage\t1\tx\n[/Trigger]\n[/Triggers]\n",
        "[Trigger]\nName\tx\nMode\tonce\nCond\thealth\tA\t>=\nAction\tmessage\t1\tx\n[/Trigger]\n[/Triggers]\n",
        "[Trigger]\nName\tx\nMode\tonce\nCond\tbuilt\t1\tCannon\t2\nAction\tmessage\t1\tx\n[/Trigger]\n[/Triggers]\n",
        "[Trigger]\nName\tx\nMode\tonce\nCond\tportal\t2\tmaybe\nAction\tgold\t1\t1\n[/Trigger]\n[/Triggers]\n",
        "[Trigger]\nName\tx\nMode\tonce\nCond\talive\tKing\t2\nAction\tgold\t1\t1\n[/Trigger]\n[/Triggers]\n",
        "[Trigger]\nName\tx\nMode\tonce\nCond\treached\tKing\tcorner\tExit\nAction\tgold\t1\t1\n[/Trigger]\n[/Triggers]\n",
        "[Trigger]\nName\tx\nMode\tonce\nCond\ttime\t1\nAction\tportal\t1\tmaybe\n[/Trigger]\n[/Triggers]\n",
        "[Trigger]\nName\tx\nMode\tonce\nCond\ttime\t1\nAction\tavailable\t1\tTroll\t2\n[/Trigger]\n[/Triggers]\n"};
    for(const char* entry : bad)
    {
        LevelScript invalid;
        std::istringstream isBad(entry);
        BOOST_CHECK_MESSAGE(!invalid.importFromStream(isBad), entry);
    }
}
