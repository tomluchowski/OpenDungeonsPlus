/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CREATUREPANEL_H
#define CREATUREPANEL_H

#include "game/CreaturePanelData.h"
#include <CEGUI/Event.h>
#include <CEGUI/Size.h>
#include <set>
#include <vector>

class GameMap;
class Gui;
namespace CEGUI { class Window; }

//! \brief The population panel of the creatures tab: counts of the player's creatures per type
//! and criterion, with buttons to pick creatures up and to locate them.
class CreaturePanel
{
public:
    CreaturePanel(GameMap& gameMap, Gui& gui, CEGUI::Window* parent);
    ~CreaturePanel();
    //! \brief Stores the counts received from the server and schedules a refresh of the labels
    void setData(const CreaturePanelData& data);
    //! \brief Called every frame: shows the panel once data was received and refreshes the labels
    //! when the data, the selected view, the scroll position or the panel size changed
    void update();

private:
    //! \brief One column of the panel, showing a single creature type.
    struct Slot
    {
        //! \brief Container of the column
        CEGUI::Window* window;
        //! \brief Portrait image of the creature type
        CEGUI::Window* portrait;
        //! \brief Count labels below each other, one per criterion of the selected view
        std::array<CEGUI::Window*, 4> counts;
        //! \brief Creature class name currently shown in this column
        std::string type;
    };

    //! \brief Creates the windows of one more type column
    void addSlot();
    //! \brief Shows the given view (index into the view buttons) in the type columns
    void selectView(size_t view);
    //! \brief Moves the visible type columns by one type, towards the first (-1) or the last (1) type
    void scroll(int direction);
    //! \brief Asks the server to pick up one of the player's creatures that matches the criterion.
    //! \param type Creature class name; ignored if workersOnly is set
    //! \param workersOnly Picks among all workers instead of one creature type
    //! \param levelOrder 1 prefers the highest level, -1 the lowest, 0 takes the first match
    void pickUp(const std::string& type, CreaturePanelCriterion criterion, bool workersOnly, int levelOrder = 0);
    //! \brief Moves the camera to the first creature of the given type that is on the map
    void focus(const std::string& type);

    GameMap& mGameMap;
    Gui& mGui;
    //! \brief Root window of the panel
    CEGUI::Window* mWindow;
    //! \brief Container of the type columns, right of the scroll buttons
    CEGUI::Window* mStrip;
    //! \brief Count labels of the worker block, one per WORKER_CRITERIA entry
    std::array<CEGUI::Window*, 4> mWorkerCounts;
    //! \brief Buttons selecting the view: total, jobs, fighting, moods
    std::array<CEGUI::Window*, 4> mViewButtons;
    //! \brief Button scrolling to the previous creature types
    CEGUI::Window* mPrevious;
    //! \brief Button scrolling to the next creature types
    CEGUI::Window* mNext;
    //! \brief The type columns created so far; only the first mVisibleSlots are shown
    std::vector<Slot> mSlots;
    //! \brief Event subscriptions to disconnect in the destructor
    std::vector<CEGUI::Event::Connection> mConnections;
    //! \brief Latest counts received from the server
    CreaturePanelData mData;
    //! \brief Names of the creatures a pickup was requested for and that are not yet confirmed by a new
    //! snapshot; avoids requesting the same creature twice
    std::set<std::string> mPendingPickups;
    //! \brief Panel size at the last refresh, to detect resizes
    CEGUI::Sizef mLastSize;
    //! \brief Index of the selected view
    size_t mView = 0;
    //! \brief Index of the first creature type shown, counted over the types that exist in mData
    size_t mFirstType = 0;
    //! \brief Number of type columns that fit into the panel and are filled
    size_t mVisibleSlots = 0;
    //! \brief Number of non-worker creature types in mData
    size_t mTypeCount = 0;
    //! \brief Whether a snapshot was received; the panel stays hidden before
    bool mHasData = false;
    //! \brief Whether the labels have to be refreshed in the next update()
    bool mDirty = true;
};

#endif
