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

#ifndef POSTLOG_H
#define POSTLOG_H

#include <cstddef>
#include <deque>
#include <map>
#include <stdint.h>
#include <string>

namespace social
{

//! \brief What a creature "posts" about. The order is stable, the names are the category
//! names used in config/social-texts.cfg.
enum class PostCategory : uint8_t
{
    Eat,
    Sleep,
    Train,
    Work,
    Fight,
    Hurt,
    LevelUp,
    Payday,
    Unhappy,
    Ko,
    Jail,
    PickedUp,
    Arrived,
    Left,
    Died,
    //! A friendship was formed, a hatred or a nemesis arose, a friendship broke (relationship option)
    Friendship,
    Hatred,
    Nemesis,
    Breakup,
    Nb
};

//! \brief Name of the category in the texts file ("eat", "levelup", ...), empty for Nb.
std::string getPostCategoryName(PostCategory category);

//! \brief One entry of the feed. Only numbers and names are stored, the text is rendered
//! from the templates when the feed is displayed.
struct Post
{
    Post() :
        mTurn(0),
        mCategory(PostCategory::Nb),
        mVariant(0),
        mArgument(0),
        mIsWorker(false)
    {
    }

    int64_t mTurn;
    PostCategory mCategory;
    uint32_t mVariant;
    //! Level for level-up and payday posts, room type for work and train posts
    int32_t mArgument;
    //! Profile name of the second creature of a relationship post, empty for other posts
    std::string mOther;
    std::string mCreature;
    std::string mClassName;
    bool mIsWorker;
};

//! \brief In-memory log of the creature posts of the local game. It is never saved and never
//! sent over the network. All limits are applied when a post is added, nothing is polled.
class PostLog
{
public:
    //! Oldest posts are dropped above this size
    static const std::size_t MAX_POSTS = 200;
    //! A creature posts at most once per category in this many seconds of game time
    static const uint32_t COOLDOWN_SECONDS = 120;
    //! A creature posts at most MAX_POSTS_PER_CREATURE posts in this many seconds of game time
    static const uint32_t CREATURE_WINDOW_SECONDS = 600;
    static const uint32_t MAX_POSTS_PER_CREATURE = 6;
    //! At most MAX_POSTS_PER_SECOND posts are accepted per second of game time
    static const uint32_t MAX_POSTS_PER_SECOND = 4;
    //! Events of the first turns after start() only update state and never post
    static const int64_t BURST_TURNS = 3;
    //! A new post does not repeat the text of one of this many posts before it
    static const std::size_t RECENT_TEXTS = 8;

    //! Renders the text of a post (empty if it has none), used to keep neighbouring posts different
    typedef std::string (*PostTextFunction)(const Post& post);

    PostLog();

    //! \brief Sets the function that renders the text of a post. A post whose text equals the text of one of
    //! the last RECENT_TEXTS posts gets another variant, so two creatures never post the same sentence in a row.
    inline void setTextFunction(PostTextFunction function)
    { mTextFunction = function; }

    static PostLog& getSingleton();

    //! \brief Starts a game: clears the log and suppresses the posts of the first turns.
    void start(int64_t turnNow, double turnsPerSecond);

    //! \brief Leaves the game: clears the log, later posts are ignored.
    void stop();

    inline bool isActive() const
    { return mActive; }

    //! \brief Adds a post unless a limit applies. Returns true if it was added.
    bool addPost(int64_t turn, const std::string& creature, const std::string& className,
        bool isWorker, PostCategory category, int32_t argument, const std::string& other = std::string());

    inline const std::deque<Post>& getPosts() const
    { return mPosts; }

    //! \brief The newest post of the creature, nullptr if the log has none.
    const Post* findLatestPost(const std::string& creature) const;

    //! \brief Incremented whenever a post is added, the window redraws the feed when it changes.
    inline uint32_t getVersion() const
    { return mVersion; }

    //! \brief Incremented whenever a creature is added, removed or changes seat. Never reset,
    //! so values cached with an older version are always recognised as outdated.
    inline uint32_t getRosterVersion() const
    { return mRosterVersion; }

    inline void rosterChanged()
    { ++mRosterVersion; }

private:
    int64_t secondsToTurns(uint32_t seconds) const;

    bool mActive;
    int64_t mEnabledFromTurn;
    double mTurnsPerSecond;
    uint32_t mVersion;
    uint32_t mRosterVersion;
    std::deque<Post> mPosts;
    //! Turn of the last post per creature and category
    std::map<std::string, int64_t> mLastPostTurns;
    //! Turns of the recent posts per creature
    std::map<std::string, std::deque<int64_t> > mCreaturePostTurns;
    //! Turns of the recent posts of all creatures
    std::deque<int64_t> mGlobalPostTurns;
    PostTextFunction mTextFunction;
    //! Texts of the last posts
    std::deque<std::string> mRecentTexts;
};

}

#endif // POSTLOG_H
