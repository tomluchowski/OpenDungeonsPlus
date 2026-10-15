/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CREATUREPANEL_H
#define CREATUREPANEL_H

#include "game/CreaturePanelData.h"
#include <CEGUI/Event.h>
#include <CEGUI/EventArgs.h>
#include <CEGUI/Size.h>
#include <array>
#include <cstddef>
#include <set>
#include <string>
#include <vector>

class GameMap;
class Gui;
namespace CEGUI { class Window; }

//! Population panel of the game mode: creature portraits with live counts per view.
class CreaturePanel
{
public:
    CreaturePanel(GameMap& gameMap, Gui& gui, CEGUI::Window* parent);
    ~CreaturePanel();
    //! Replaces the counts shown with the latest snapshot sent by the server.
    void setData(const CreaturePanelData& data);
    //! Rebuilds the visible slots when the data, the view or the size changed.
    void update();

private:
    //! Number of count rows per slot; also the stride of the count window ids.
    static const size_t SLOT_COUNT_ROWS = 4;

    //! One creature type column of the strip.
    struct Slot
    {
        //! Container of the column.
        CEGUI::Window* window = nullptr;
        //! Portrait image, also used for locating the creature type.
        CEGUI::Window* portrait = nullptr;
        //! Count labels, one per criterion of the selected view.
        std::array<CEGUI::Window*, SLOT_COUNT_ROWS> counts = {{nullptr, nullptr, nullptr, nullptr}};
        //! Creature class name shown in this column.
        std::string type;
    };

    void addSlot();
    void selectView(size_t view);
    void scroll(int direction);
    void pickUp(const std::string& type, CreaturePanelCriterion criterion, bool workersOnly, int levelOrder = 0);
    void focus(const std::string& type);

    //! Event handlers; the window id identifies the worker row, view, slot or count row.
    bool onWorkerCountClicked(const CEGUI::EventArgs& args);
    bool onViewButtonClicked(const CEGUI::EventArgs& args);
    bool onPreviousClicked(const CEGUI::EventArgs& args);
    bool onNextClicked(const CEGUI::EventArgs& args);
    bool onPortraitClicked(const CEGUI::EventArgs& args);
    bool onCountClicked(const CEGUI::EventArgs& args);

    //! Game map the panel reads creatures from.
    GameMap& mGameMap;
    Gui& mGui;
    //! Root window of the panel.
    CEGUI::Window* mWindow;
    //! Container of the creature type slots.
    CEGUI::Window* mStrip;
    //! Count labels of the worker column.
    std::array<CEGUI::Window*, 4> mWorkerCounts;
    //! Buttons selecting the total, jobs, fighting and mood views.
    std::array<CEGUI::Window*, 4> mViewButtons;
    CEGUI::Window* mPrevious;
    CEGUI::Window* mNext;
    std::vector<Slot> mSlots;
    //! Event connections, disconnected on destruction.
    std::vector<CEGUI::Event::Connection> mConnections;
    //! Latest server snapshot.
    CreaturePanelData mData;
    //! Creatures with an outstanding pick-up request, cleared by the next snapshot.
    std::set<std::string> mPendingPickups;
    //! Strip size at the last rebuild.
    CEGUI::Sizef mLastSize;
    //! Index of the selected view.
    size_t mView = 0;
    //! Index of the first visible creature type.
    size_t mFirstType = 0;
    size_t mVisibleSlots = 0;
    //! Number of non-worker creature types in the snapshot.
    size_t mTypeCount = 0;
    bool mHasData = false;
    //! Whether the slots need to be rebuilt on the next update.
    bool mDirty = true;
};

#endif
