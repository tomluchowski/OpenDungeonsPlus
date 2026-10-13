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

#include "social/PostLog.h"

#include <sstream>

#define BOOST_TEST_MODULE PostLog
#include "BoostTestTargetConfig.h"

using social::PostCategory;
using social::PostLog;

namespace
{
//! One turn per second keeps the arithmetic of the limits easy to read
const double TURNS_PER_SECOND = 1.0;

bool add(PostLog& log, int64_t turn, const std::string& creature, PostCategory category)
{
    return log.addPost(turn, creature, "Orc", false, category, 0);
}
}

namespace
{
std::string threeTexts(const social::Post& post)
{
    std::ostringstream stream;
    stream << "text " << (post.mVariant % 3);
    return stream.str();
}
}

BOOST_AUTO_TEST_CASE(test_neighbouring_posts_differ)
{
    PostLog log;
    log.start(0, TURNS_PER_SECOND);
    log.setTextFunction(&threeTexts);
    BOOST_CHECK(add(log, 10, "Orc1", PostCategory::Eat));
    BOOST_CHECK(add(log, 10, "Orc2", PostCategory::Eat));
    BOOST_CHECK(add(log, 10, "Orc3", PostCategory::Eat));
    BOOST_REQUIRE_EQUAL(static_cast<int>(log.getPosts().size()), 3);
    BOOST_CHECK(threeTexts(log.getPosts()[0]) != threeTexts(log.getPosts()[1]));
    BOOST_CHECK(threeTexts(log.getPosts()[0]) != threeTexts(log.getPosts()[2]));
    BOOST_CHECK(threeTexts(log.getPosts()[1]) != threeTexts(log.getPosts()[2]));
}

BOOST_AUTO_TEST_CASE(test_inactive_log_ignores_posts)
{
    PostLog log;
    BOOST_CHECK(!log.isActive());
    BOOST_CHECK(!add(log, 100, "Orc1", PostCategory::Eat));
    BOOST_CHECK_EQUAL(static_cast<int>(log.getPosts().size()), 0);
}

BOOST_AUTO_TEST_CASE(test_burst_suppression)
{
    PostLog log;
    log.start(10, TURNS_PER_SECOND);
    BOOST_CHECK(!add(log, 10, "Orc1", PostCategory::Eat));
    BOOST_CHECK(!add(log, 12, "Orc1", PostCategory::Eat));
    BOOST_CHECK(add(log, 13, "Orc1", PostCategory::Eat));
    BOOST_CHECK_EQUAL(static_cast<int>(log.getPosts().size()), 1);
}

BOOST_AUTO_TEST_CASE(test_cooldown_per_creature_and_category)
{
    PostLog log;
    log.start(0, TURNS_PER_SECOND);
    BOOST_CHECK(add(log, 10, "Orc1", PostCategory::Eat));
    BOOST_CHECK(!add(log, 50, "Orc1", PostCategory::Eat));
    // Another category and another creature are not affected
    BOOST_CHECK(add(log, 50, "Orc1", PostCategory::Sleep));
    BOOST_CHECK(add(log, 50, "Orc2", PostCategory::Eat));
    // The cooldown is 120 seconds, at one turn per second
    BOOST_CHECK(!add(log, 129, "Orc1", PostCategory::Eat));
    BOOST_CHECK(add(log, 130, "Orc1", PostCategory::Eat));
}

BOOST_AUTO_TEST_CASE(test_limit_per_creature)
{
    PostLog log;
    log.start(0, TURNS_PER_SECOND);
    const PostCategory categories[] = {PostCategory::Eat, PostCategory::Sleep, PostCategory::Train,
        PostCategory::Work, PostCategory::Fight, PostCategory::Hurt, PostCategory::LevelUp};
    int accepted = 0;
    for(int i = 0; i < 7; ++i)
    {
        if(add(log, 10 + i * 2, "Orc1", categories[i]))
            ++accepted;
    }
    BOOST_CHECK_EQUAL(accepted, 6);
    // The window is 600 seconds long
    BOOST_CHECK(add(log, 10 + 600, "Orc1", PostCategory::Payday));
}

BOOST_AUTO_TEST_CASE(test_global_rate_limit)
{
    PostLog log;
    log.start(0, TURNS_PER_SECOND);
    int accepted = 0;
    for(int i = 0; i < 20; ++i)
    {
        std::ostringstream name;
        name << "Orc" << i;
        if(add(log, 10, name.str(), PostCategory::Eat))
            ++accepted;
    }
    BOOST_CHECK_EQUAL(accepted, 4);
    // The next second accepts posts again
    BOOST_CHECK(add(log, 11, "Orc30", PostCategory::Eat));
}

BOOST_AUTO_TEST_CASE(test_size_limit_keeps_the_newest)
{
    PostLog log;
    log.start(0, TURNS_PER_SECOND);
    int64_t turn = 10;
    for(int i = 0; i < 300; ++i)
    {
        std::ostringstream name;
        name << "Orc" << i;
        // Two posts per turn stay below the global rate limit, every creature is new
        if(i % 2 == 0)
            ++turn;
        BOOST_CHECK(add(log, turn, name.str(), PostCategory::Eat));
    }
    BOOST_CHECK_EQUAL(static_cast<int>(log.getPosts().size()), static_cast<int>(PostLog::MAX_POSTS));
    BOOST_CHECK_EQUAL(log.getPosts().back().mCreature, std::string("Orc299"));
    BOOST_CHECK_EQUAL(log.getPosts().front().mCreature, std::string("Orc100"));
}

BOOST_AUTO_TEST_CASE(test_versions_and_stop)
{
    PostLog log;
    log.start(0, TURNS_PER_SECOND);
    uint32_t version = log.getVersion();
    uint32_t roster = log.getRosterVersion();
    BOOST_CHECK(add(log, 10, "Orc1", PostCategory::Eat));
    BOOST_CHECK(log.getVersion() != version);
    BOOST_CHECK(!add(log, 11, "Orc1", PostCategory::Eat));

    version = log.getVersion();
    log.rosterChanged();
    BOOST_CHECK(log.getRosterVersion() != roster);
    BOOST_CHECK_EQUAL(log.getVersion(), version);

    BOOST_CHECK(log.findLatestPost("Orc1") != nullptr);
    BOOST_CHECK(log.findLatestPost("Orc2") == nullptr);

    roster = log.getRosterVersion();
    log.stop();
    BOOST_CHECK(!log.isActive());
    BOOST_CHECK_EQUAL(static_cast<int>(log.getPosts().size()), 0);
    BOOST_CHECK(log.getRosterVersion() != roster);
    BOOST_CHECK(!add(log, 500, "Orc1", PostCategory::Eat));

    // A new game starts empty, the cooldowns of the old one are gone
    log.start(1000, TURNS_PER_SECOND);
    BOOST_CHECK(add(log, 1010, "Orc1", PostCategory::Eat));
}

BOOST_AUTO_TEST_CASE(test_variant_is_reproducible)
{
    PostLog first;
    PostLog second;
    first.start(0, TURNS_PER_SECOND);
    second.start(0, TURNS_PER_SECOND);
    BOOST_CHECK(add(first, 10, "Orc1", PostCategory::Eat));
    BOOST_CHECK(add(second, 10, "Orc1", PostCategory::Eat));
    BOOST_CHECK_EQUAL(first.getPosts().back().mVariant, second.getPosts().back().mVariant);
}

BOOST_AUTO_TEST_CASE(test_relationship_post_keeps_other_name)
{
    PostLog log;
    log.start(0, TURNS_PER_SECOND);
    BOOST_CHECK(log.addPost(10, "Orc1", "Orc", false, PostCategory::Friendship, 0, "Gruk Stonefist"));
    BOOST_CHECK_EQUAL(log.getPosts().back().mOther, "Gruk Stonefist");
    BOOST_CHECK_EQUAL(social::getPostCategoryName(PostCategory::Friendship), "friendship");
    BOOST_CHECK_EQUAL(social::getPostCategoryName(PostCategory::Breakup), "breakup");
}
