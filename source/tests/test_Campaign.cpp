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

#define BOOST_TEST_MODULE Campaign
#include "BoostTestTargetConfig.h"

#include "game/Campaign.h"
#include "game/CampaignWorld.h"

#include <sstream>
#include <string>
#include <vector>

static const std::string sample =
    "# comment\n"
    "[Level]\n"
    "File=campaign/One.level\n"
    "Title=First\n"
    "Briefing=Line one\\nLine two\n"
    "Debriefing=Well done\n"
    "[Level]\n"
    "File=campaign/Two.level\n"
    "Title=Second\n"
    "[Level]\n"
    "Title=No file, ignored\n";

BOOST_AUTO_TEST_CASE(test_sequence_and_progress)
{
    Campaign& campaign = Campaign::getSingleton();
    std::istringstream is(sample);
    BOOST_REQUIRE(campaign.importDefinition(is));
    BOOST_REQUIRE_EQUAL(campaign.getNumLevels(), 2u);
    BOOST_CHECK_EQUAL(campaign.getLevel(0).mFile, "campaign/One.level");
    BOOST_CHECK_EQUAL(campaign.getLevel(0).mBriefing, "Line one\nLine two");
    BOOST_CHECK_EQUAL(campaign.getLevel(1).mTitle, "Second");

    // New campaign: only level 1 is unlocked.
    BOOST_CHECK_EQUAL(campaign.getCurrentLevel(), 0u);
    BOOST_CHECK(campaign.isUnlocked(0));
    BOOST_CHECK(!campaign.isUnlocked(1));
    BOOST_CHECK(!campaign.hasProgress());

    // Win level 1: level 2 becomes the current one.
    BOOST_CHECK(!campaign.onLevelWon());
    campaign.startLevel(0);
    BOOST_CHECK(campaign.onLevelWon());
    BOOST_CHECK(campaign.getPlayedLevelWon());
    BOOST_CHECK(campaign.isCompleted(0));
    BOOST_CHECK_EQUAL(campaign.getCurrentLevel(), 1u);
    BOOST_CHECK(campaign.isUnlocked(1));

    // Save and load into a fresh state: continue lands on level 2.
    std::ostringstream os;
    campaign.exportProgress(os);
    std::istringstream is2(sample);
    BOOST_REQUIRE(campaign.importDefinition(is2));
    BOOST_CHECK_EQUAL(campaign.getCurrentLevel(), 0u);
    std::istringstream progress(os.str());
    BOOST_REQUIRE(campaign.importProgress(progress));
    BOOST_CHECK_EQUAL(campaign.getCurrentLevel(), 1u);
    BOOST_CHECK(campaign.hasProgress());
    BOOST_CHECK(!campaign.isFinished());

    // A won level can be replayed.
    BOOST_CHECK(campaign.isUnlocked(0));
}

static const std::string sampleBonus =
    "[Level]\nFile=campaign/One.level\nTitle=First\n"
    "[Level]\nFile=campaign/Bonus.level\nTitle=Hidden\nBonus=1\n"
    "[Level]\nFile=campaign/Two.level\nTitle=Second\n";

static const std::string sampleBonusOwnReward =
    "[Level]\nFile=campaign/One.level\nTitle=First\n"
    "[Level]\nFile=campaign/Bonus.level\nTitle=Hidden\nBonus=1\n"
    "[Level]\nFile=campaign/Own.level\nTitle=Own reward\nBonus=1\nHeartstone=0\n";

BOOST_AUTO_TEST_CASE(test_bonus_level_without_Heartstone_piece)
{
    Campaign& campaign = Campaign::getSingleton();
    std::istringstream is(sampleBonusOwnReward);
    BOOST_REQUIRE(campaign.importDefinition(is));
    BOOST_CHECK(campaign.getLevel(1).mHeartstone);
    BOOST_CHECK(campaign.getLevel(2).mBonus);
    BOOST_CHECK(!campaign.getLevel(2).mHeartstone);
    BOOST_CHECK_EQUAL(campaign.getHeartstoneTotal(), 1u);

    // Finding the level with its own reward gives no piece.
    BOOST_CHECK(campaign.discoverBonusLevel("campaign/Own.level"));
    BOOST_CHECK_EQUAL(campaign.getHeartstonePieces(), 0u);
    BOOST_CHECK(!campaign.isHeartstoneComplete());
    BOOST_CHECK(campaign.discoverBonusLevel("campaign/Bonus.level"));
    BOOST_CHECK_EQUAL(campaign.getHeartstonePieces(), 1u);
    BOOST_CHECK(campaign.isHeartstoneComplete());
}

BOOST_AUTO_TEST_CASE(test_bonus_levels_and_Heartstone)
{
    Campaign& campaign = Campaign::getSingleton();
    std::istringstream is(sampleBonus);
    BOOST_REQUIRE(campaign.importDefinition(is));
    BOOST_REQUIRE_EQUAL(campaign.getNumLevels(), 3u);
    BOOST_CHECK(campaign.getLevel(1).mBonus);
    BOOST_CHECK_EQUAL(campaign.getHeartstoneTotal(), 1u);
    BOOST_CHECK_EQUAL(campaign.getHeartstonePieces(), 0u);

    // The bonus level stays hidden when level 1 is completed.
    BOOST_CHECK(!campaign.isUnlocked(1));
    campaign.startLevel(0);
    BOOST_CHECK(campaign.onLevelWon());
    BOOST_CHECK(!campaign.isUnlocked(1));
    // It does not block level 2.
    BOOST_CHECK(campaign.isUnlocked(2));
    BOOST_CHECK_EQUAL(campaign.getCurrentLevel(), 2u);

    // A level script finds it (only for a bonus level of the campaign).
    BOOST_CHECK(!campaign.discoverBonusLevel("campaign/Two.level"));
    BOOST_CHECK(!campaign.discoverBonusLevel("campaign/Unknown.level"));
    BOOST_CHECK(campaign.discoverBonusLevel("campaign/Bonus.level"));
    BOOST_CHECK(!campaign.discoverBonusLevel("campaign/Bonus.level"));
    BOOST_CHECK(campaign.isUnlocked(1));
    BOOST_CHECK_EQUAL(campaign.getHeartstonePieces(), 1u);
    BOOST_CHECK(campaign.isHeartstoneComplete());

    // The discovery is saved.
    std::ostringstream os;
    campaign.exportProgress(os);
    std::istringstream is2(sampleBonus);
    BOOST_REQUIRE(campaign.importDefinition(is2));
    BOOST_CHECK(!campaign.isDiscovered(1));
    std::istringstream progress(os.str());
    BOOST_REQUIRE(campaign.importProgress(progress));
    BOOST_CHECK(campaign.isDiscovered(1));

    // The campaign can be finished without completing the bonus level.
    campaign.startLevel(2);
    campaign.onLevelWon();
    BOOST_CHECK(campaign.isFinished());
}

BOOST_AUTO_TEST_CASE(test_difficulty_is_saved)
{
    Campaign& campaign = Campaign::getSingleton();
    std::istringstream is(sample);
    BOOST_REQUIRE(campaign.importDefinition(is));
    campaign.resetProgress();
    BOOST_CHECK_EQUAL(campaign.getDifficulty(), Campaign::getDefaultDifficulty());

    campaign.setDifficulty(0);
    BOOST_CHECK_EQUAL(campaign.getDifficulty(), 0u);
    // Values beyond the highest AI level are ignored
    campaign.setDifficulty(99);
    BOOST_CHECK_EQUAL(campaign.getDifficulty(), 0u);

    std::ostringstream os;
    campaign.exportProgress(os);
    campaign.setDifficulty(Campaign::getDefaultDifficulty());
    std::istringstream progress(os.str());
    BOOST_REQUIRE(campaign.importProgress(progress));
    BOOST_CHECK_EQUAL(campaign.getDifficulty(), 0u);

    // A new campaign returns to the default
    campaign.resetProgress();
    BOOST_CHECK_EQUAL(campaign.getDifficulty(), Campaign::getDefaultDifficulty());
}

BOOST_AUTO_TEST_CASE(test_province_keys)
{
    Campaign& campaign = Campaign::getSingleton();
    std::istringstream is(
        "[Level]\n"
        "File=a.level\n"
        "Province=T01\n"
        "Warden=Reeve Test\n"
        "Difficulty=9\n"
        "[Level]\n"
        "File=b.level\n"
        "Province=B01\n"
        "[Level]\n"
        "File=c.level\n");
    BOOST_REQUIRE(campaign.importDefinition(is));

    BOOST_CHECK_EQUAL(campaign.getLevel(0).mProvince, "T01");
    BOOST_CHECK_EQUAL(campaign.getLevel(0).mWarden, "Reeve Test");
    // The difficulty is limited to 1..5, without the setting it is 1
    BOOST_CHECK_EQUAL(campaign.getLevel(0).mDifficulty, 5u);
    BOOST_CHECK_EQUAL(campaign.getLevel(1).mDifficulty, 1u);
    BOOST_CHECK_EQUAL(campaign.findLevelByProvince("B01"), 1u);
    // Levels without a province are not on the map
    BOOST_CHECK_EQUAL(campaign.findLevelByProvince("T09"), campaign.getNumLevels());
    BOOST_CHECK_EQUAL(campaign.findLevelByProvince(""), campaign.getNumLevels());
}

static const std::string sampleBranch =
    "[Level]\nFile=p.level\nProvince=T08\n"
    "[Level]\nFile=a.level\nProvince=T09A\nBranch=T09B\n"
    "[Level]\nFile=b.level\nProvince=T09B\nBranch=T09A\n"
    "[Level]\nFile=n.level\nProvince=T10\n";

static void completeLevel(Campaign& campaign, size_t index)
{
    campaign.startLevel(index);
    BOOST_REQUIRE(campaign.onLevelWon());
}

BOOST_AUTO_TEST_CASE(test_branch_levels)
{
    Campaign& campaign = Campaign::getSingleton();
    std::istringstream is(sampleBranch);
    BOOST_REQUIRE(campaign.importDefinition(is));
    BOOST_CHECK_EQUAL(campaign.getLevel(1).mBranch, "T09B");

    // Neither sister completed: the level after them is locked, both are closed
    BOOST_CHECK(!campaign.isUnlocked(1));
    BOOST_CHECK(!campaign.isUnlocked(2));
    BOOST_CHECK(!campaign.isUnlocked(3));
    completeLevel(campaign, 0);
    // Both sisters open once the level before is done
    BOOST_CHECK(campaign.isUnlocked(1));
    BOOST_CHECK(campaign.isUnlocked(2));
    BOOST_CHECK(!campaign.isUnlocked(3));

    // First sister done: the next level opens, the other stays playable
    completeLevel(campaign, 1);
    BOOST_CHECK(campaign.isUnlocked(3));
    BOOST_CHECK(campaign.isUnlocked(2));
    BOOST_CHECK_EQUAL(campaign.getCurrentLevel(), 3u);
    completeLevel(campaign, 3);
    BOOST_CHECK(campaign.isFinished());

    // The second sister first gives the same result
    std::istringstream is2(sampleBranch);
    BOOST_REQUIRE(campaign.importDefinition(is2));
    completeLevel(campaign, 0);
    completeLevel(campaign, 2);
    BOOST_CHECK(campaign.isUnlocked(3));
    BOOST_CHECK(campaign.isUnlocked(1));
    BOOST_CHECK_EQUAL(campaign.getCurrentLevel(), 3u);
}

BOOST_AUTO_TEST_CASE(test_world_map)
{
    CampaignWorld world;
    std::istringstream is(
        "{\"version\":1,\"size\":[4,2],\"provinces\":["
        "{\"id\":\"T01\",\"name\":\"One\",\"mask_rgb\":[10,20,30],\"layer_origin\":[1,2],\"lift_origin\":[0,1],"
        "\"layers\":{\"locked\":\"l.png\",\"available\":\"a.png\",\"conquered\":\"c.png\",\"lift\":\"f.png\"}},"
        "{\"id\":\"T02\",\"name\":\"Two\",\"mask_rgb\":[40,50,60],\"layer_origin\":[3,4],\"lift_origin\":[2,3],"
        "\"layers\":{\"locked\":\"l.png\",\"available\":\"a.png\",\"conquered\":\"c.png\",\"lift\":\"f.png\"}}],"
        "\"sites\":[{\"id\":\"B01\",\"name\":\"Cave\",\"host\":\"T01\",\"pos\":[5,6],"
        "\"icons\":{\"hidden\":\"h.png\",\"found\":\"f.png\",\"done\":\"d.png\"}}],"
        "\"progress_panel\":[1,2,3,4],\"title_cartouche\":[5,6,7,8]}");
    BOOST_REQUIRE(world.importDefinition(is));
    BOOST_CHECK_EQUAL(world.getProvinces().size(), 2u);
    BOOST_CHECK_EQUAL(world.getWidth(), 4);
    BOOST_CHECK_EQUAL(world.getProvinces()[1].mLayerY, 4);
    BOOST_CHECK_EQUAL(world.getSites()[0].mX, 5);
    BOOST_CHECK_EQUAL(world.getProgressPanel().mHeight, 4);
    BOOST_CHECK_EQUAL(world.findProvince("T02"), 1u);
    BOOST_CHECK_EQUAL(world.findProvince("T99"), 2u);

    // Id map 4x2: pixel 0 is province one, pixel 5 province two, the rest is outside of all provinces
    std::vector<uint8_t> rgb(4 * 2 * 3, 0);
    rgb[0] = 10; rgb[1] = 20; rgb[2] = 30;
    rgb[15] = 40; rgb[16] = 50; rgb[17] = 60;
    world.setIdMap(4, 2, rgb);
    BOOST_CHECK_EQUAL(world.getProvinceAt(0, 0), 0u);
    BOOST_CHECK_EQUAL(world.getProvinceAt(1, 1), 1u);
    BOOST_CHECK_EQUAL(world.getProvinceAt(2, 0), 2u);
    BOOST_CHECK_EQUAL(world.getProvinceAt(-1, 0), 2u);
    BOOST_CHECK_EQUAL(world.getProvinceAt(4, 0), 2u);

    std::istringstream broken("{\"size\":[4,2]}");
    BOOST_CHECK(!world.importDefinition(broken));
}
