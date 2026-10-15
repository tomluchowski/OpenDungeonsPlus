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

#ifndef SOCIALWINDOW_H
#define SOCIALWINDOW_H

#include <CEGUI/Event.h>
#include <CEGUI/EventArgs.h>

#include <stdint.h>
#include <string>
#include <vector>

class GameMap;

namespace social
{
struct Post;
}

namespace CEGUI
{
class Window;
}

//! \brief The Dungeonbook: a window with the list of the creatures of the local player on the left and,
//! on the right, either the feed of their posts (see social/PostLog.h) or the profile of the selected
//! creature (switched by two tab buttons, selecting a creature shows its profile). The window itself comes
//! from gui/WindowSocial.layout, the event handlers are connected by the game mode. The window only
//! redraws while it is visible and only when the post log or the roster changed.
class SocialWindow
{
public:
    SocialWindow(CEGUI::Window* rootWindow, GameMap& gameMap);
    ~SocialWindow();

    //! \brief Shows the window on top of the other ones and draws list and feed.
    void show();
    //! \brief Same, with the given creature selected in the list (all creatures are listed then).
    void showCreature(const std::string& creatureName);
    void hide();
    bool isVisible() const;

    //! \brief Called every frame by the game mode, only does something while the window is visible.
    void update(float elapsed);

    bool onCloseClicked(const CEGUI::EventArgs& e);
    bool onFilterClicked(const CEGUI::EventArgs& e);
    bool onFeedModeClicked(const CEGUI::EventArgs& e);
    bool onSelectionChanged(const CEGUI::EventArgs& e);
    bool onFeedTabClicked(const CEGUI::EventArgs& e);
    bool onProfileTabClicked(const CEGUI::EventArgs& e);
    //! \brief A friend or foe name in the profile: selects that creature.
    bool onLinkClicked(const CEGUI::EventArgs& e);

    //! \brief Marks the tab button as the active one (gold, in brackets); the button stays enabled.
    static void setTabState(CEGUI::Window* tab, const std::string& label, bool active);

    //! \brief "Latest: <text> (2 min ago)" for the newest post of the creature, empty if it has none.
    static std::string describeLatestPost(const std::string& creatureName, int64_t turnNow);

    //! \brief The text of a post, used by the post log to keep neighbouring posts different.
    static std::string renderPostTextForLog(const social::Post& post);

private:
    enum class CreatureFilter
    {
        All,
        Workers,
        Fighters,
        Unhappy
    };

    void rebuildCreatureList();
    void rebuildFeed();
    //! \brief Shows the feed (false) or the profile (true) in the right part of the window.
    void showTab(bool profile);
    //! \brief Selects the creature in the list (resetting the filter if it hides the creature) and shows its profile.
    void selectCreature(const std::string& creatureName);
    //! \brief Fills the profile pane with the profile of the selected creature.
    void refreshProfile();

    GameMap& mGameMap;
    CEGUI::Window* mWindow;
    CreatureFilter mFilter;
    bool mSelectedOnly;
    //! True while the list is rebuilt, selection events are ignored then
    bool mRebuildingList;
    std::string mSelectedCreature;
    //! Creature names of the list items, indexed by the item id
    std::vector<std::string> mListedCreatures;
    //! True while the profile (and not the feed) is shown
    bool mProfileTab;
    //! The profile page inside the profile pane, created when it is needed the first time
    CEGUI::Window* mProfilePage;
    //! Click subscriptions of the friend and foe names of the profile page
    std::vector<CEGUI::Event::Connection> mLinkConnections;
    uint32_t mShownRosterVersion;
    uint32_t mShownPostVersion;
    float mSinceRefreshCheck;
    float mSinceFeedRebuild;
    uint32_t mShownProfilePostVersion;
    float mSinceProfileRefresh;
};

#endif // SOCIALWINDOW_H
