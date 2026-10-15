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

#ifndef MENUMODECAMPAIGN_H
#define MENUMODECAMPAIGN_H

#include "AbstractApplicationMode.h"

#include "game/CampaignWorld.h"

#include <CEGUI/Vector.h>

#include <cstdint>
#include <string>
#include <vector>

namespace CEGUI
{
class Image;
}

//! \brief The campaign menu: the world map of the campaign. Each province is
//! one level. A province that can be played lifts off the map under the mouse
//! and opens its briefing on a click. A tooltip names the province under the
//! mouse and a panel shows the progress of the campaign.
class MenuModeCampaign: public AbstractApplicationMode
{
public:
    //! \brief Connects the buttons and the map of the campaign sheet to this mode
    MenuModeCampaign(ModeManager*);

    //! \brief Called when the mode is activated
    //! Used to call the corresponding Gui Sheet.
    void activate() final override;

    //! \brief Fades the lift layer of the province under the mouse
    void onFrameStarted(const Ogre::FrameEvent& evt) override;

    //! \brief Reads the campaign definition and the saved progress from disk.
    //! Returns false if there is no campaign definition.
    static bool loadCampaign();

private:
    enum class State
    {
        locked,
        available,
        conquered
    };

    //! \brief Debriefing text of the level that was played last (empty if none)
    std::string mResultText;
    //! The world map layout and its id map
    CampaignWorld mWorld;
    bool mWorldLoaded;
    //! Windows on the map (layers, markers, texts), destroyed with clearMap()
    std::vector<CEGUI::Window*> mWorldWindows;
    CEGUI::Window* mLiftWindow;
    CEGUI::Window* mTooltip;
    //! Province under the mouse, or the number of provinces if none
    size_t mHoveredProvince;
    //! Province the lift layer shows, or the number of provinces if none
    size_t mLiftProvince;
    //! Fade of the lift layer from 0 (invisible) to 1
    float mLiftFade;
    //! Level of the open briefing, or the number of levels if none
    size_t mBriefingLevel;

    //! \brief Reads the world description and the id map once. Returns false if one of them cannot be read.
    bool loadWorld();
    //! \brief The image of the world art file (relative to gui/campaign), loaded on first use.
    //! Returns NULL if it cannot be read.
    CEGUI::Image* loadWorldImage(const std::string& file);
    //! \brief Draws the province layers, the bonus site markers and the progress panel on the map
    void fillMap();
    //! \brief Removes everything fillMap drew
    void clearMap();
    //! \brief Places a window on the map. The values are map pixels, the window gets the share of the map they cover.
    void setMapArea(CEGUI::Window* window, float x, float y, float width, float height) const;
    CEGUI::Window* createMapWindow(const std::string& type, const std::string& name, float x, float y,
        float width, float height);
    //! \brief State of a province; level is its level, or the number of levels if it has none.
    State getProvinceState(size_t province, size_t& level) const;
    //! \brief Found bonus site at the map pixel, or the number of sites if none.
    size_t findSiteAt(float x, float y) const;
    //! \brief Moves the lift layer towards the province under the mouse. \param elapsed Seconds since the last frame
    void updateLift(float elapsed);
    //! \brief Shows the tooltip of the province or bonus site under the mouse, or hides it when there is none
    void updateTooltip(const CEGUI::Vector2f& position, size_t province, size_t site);
    //! \brief Shows the level briefing and its preview when available.
    void showBriefing(size_t level);
    //! \brief Shows the last level result without a preview.
    void showDebriefing();
    //! \brief Shows or hides the preview and selects the matching panel and text areas.
    void setBriefingPreviewVisible(bool visible);
    //! \brief Closes the briefing or debriefing panel
    void hideBriefing();
    //! \brief Starts the server and the client for a campaign level
    void startLevel(size_t index);

    //! \brief The mouse moves over the map: the province or site below it is highlighted
    bool mapMoved(const CEGUI::EventArgs& e);
    //! \brief The mouse leaves the map: the highlight and the tooltip are removed
    bool mapLeft(const CEGUI::EventArgs& e);
    //! \brief A click on a province or found bonus site opens its briefing
    bool mapClicked(const CEGUI::EventArgs& e);
    //! \brief Starts the level of the open briefing
    bool briefingStartPressed(const CEGUI::EventArgs&);
    //! \brief Closes the briefing without starting the level
    bool briefingClosePressed(const CEGUI::EventArgs&);
    //! \brief Chooses the next difficulty while no level is completed
    bool difficultyButtonPressed(const CEGUI::EventArgs&);
    //! \brief Shows the difficulty of the campaign on its button
    void updateDifficultyButton();
    //! \brief Leaves the campaign and shows the main menu
    bool backButtonPressed(const CEGUI::EventArgs&);
};

#endif // MENUMODECAMPAIGN_H
