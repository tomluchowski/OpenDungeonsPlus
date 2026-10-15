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

#include "modes/MenuModeCampaign.h"

#include "ai/KeeperAIType.h"
#include "game/Campaign.h"
#include "gamemap/GameMap.h"
#include "modes/ModeManager.h"
#include "network/ODClient.h"
#include "network/ODServer.h"
#include "network/ServerMode.h"
#include "render/Gui.h"
#include "render/ODFrameListener.h"
#include "sound/MusicPlayer.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/ResourceManager.h"

#include <CEGUI/CEGUI.h>
#include <CEGUI/BasicImage.h>
#include <CEGUI/ImageManager.h>
#include <CEGUI/RendererModules/Ogre/Renderer.h>
#include <CEGUI/widgets/ProgressBar.h>
#include "boost/filesystem.hpp"

#include <Ogre.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>

namespace
{
const std::string CAMPAIGN_DEFINITION_FILE = "levels/campaign/Campaign.cfg";
const std::string CAMPAIGN_PROGRESS_FILE = "campaign.progress";

const std::string CMP_FRAME = "CampaignWindowFrame";
const std::string CMP_TEXT_LOADING = "CampaignWindowFrame/LoadingText";
const std::string CMP_BUTTON_DIFFICULTY = "CampaignWindowFrame/DifficultyButton";
const std::string CMP_BUTTON_BACK = "CampaignWindowFrame/BackButton";
const std::string CMP_MAP = "CampaignWindowFrame/CampaignMap";
const std::string CMP_BRIEFING = "CampaignWindowFrame/BriefingPanel";
const std::string CMP_BRIEFING_TEXT = "CampaignWindowFrame/BriefingPanel/BriefingText";
static const std::string CMP_BRIEFING_IMAGE = "CampaignWindowFrame/BriefingPanel/BriefingImage";
static const std::string BRIEFING_PREVIEW_PANEL_AREA = "{{0.5,-336},{0.5,-340},{0.5,336},{0.5,340}}";
static const std::string BRIEFING_TEXT_PANEL_AREA = "{{0.5,-300},{0.5,-210},{0.5,300},{0.5,210}}";
static const std::string BRIEFING_PREVIEW_TEXT_AREA = "{{0,16},{0,392},{1,-16},{1,-72}}";
static const std::string BRIEFING_TEXT_AREA = "{{0,16},{0,16},{1,-16},{1,-72}}";
const std::string CMP_BRIEFING_START = "CampaignWindowFrame/BriefingPanel/BriefingStartButton";
const std::string CMP_BRIEFING_CLOSE = "CampaignWindowFrame/BriefingPanel/BriefingCloseButton";

const std::string WORLD_DIRECTORY = "gui/campaign/";
const std::string WORLD_DEFINITION_FILE = "campaign-world.json";
const std::string WORLD_BASE_FILE = "world_base.png";
const std::string WORLD_IDMAP_FILE = "world_idmap.png";
const std::string WORLD_IMAGE_PREFIX = "CampaignWorld/";
const std::string CAMPAIGN_TITLE = "The Fall of Hollowmark";

//! Time the lift layer needs to fade in or out, in seconds
const float LIFT_FADE_TIME = 0.12f;
//! Scale of the lift layer when it is fully faded in
const float LIFT_SCALE = 1.04f;
//! Distance in map pixels from the centre of a bonus site marker that still hits it
const float SITE_HIT_RADIUS = 40.0f;
//! Size of the tooltip window and its distance from the mouse, in screen pixels
const float TOOLTIP_WIDTH = 400.0f;
const float TOOLTIP_HEIGHT = 140.0f;
const float TOOLTIP_OFFSET = 96.0f;
//! Aspect ratio (width to height) of the campaign map art
const float MAP_ASPECT_RATIO = 1.6f;
//! Percent value of a completely finished campaign and its factor to a bar fraction
const int PERCENT_FULL = 100;
const float PERCENT_FULL_FLOAT = 100.0f;
//! Layout of the progress texts and bar inside the progress panel, as fractions of its width and height
const float PROGRESS_LEFT = 0.04f;
const float PROGRESS_TEXT_WIDTH = 0.92f;
const float PROGRESS_TEXT_HEIGHT = 0.70f;
const float PROGRESS_BAR_TOP = 0.74f;
const float PROGRESS_BAR_WIDTH = 0.74f;
const float PROGRESS_BAR_HEIGHT = 0.20f;
const float PROGRESS_PERCENT_LEFT = 0.80f;
const float PROGRESS_PERCENT_TOP = 0.68f;
const float PROGRESS_PERCENT_WIDTH = 0.16f;
const float PROGRESS_PERCENT_HEIGHT = 0.32f;

//! Reads and decodes a PNG file into RGBA bytes. Returns false if it cannot be read.
bool decodeImage(const std::string& path, int& width, int& height, std::vector<uint8_t>& rgba)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    if(!file.is_open())
        return false;

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if(data.empty())
        return false;

    try
    {
        Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream(&data[0], data.size(), false, true));
        Ogre::Image image;
        image.load(stream, "png");
        width = static_cast<int>(image.getWidth());
        height = static_cast<int>(image.getHeight());
        rgba.assign(static_cast<size_t>(width) * height * 4, 0);
        Ogre::PixelBox target(width, height, 1, Ogre::PF_BYTE_RGBA, &rgba[0]);
        Ogre::PixelUtil::bulkPixelConversion(image.getPixelBox(), target);
    }
    catch(const Ogre::Exception&)
    {
        return false;
    }
    return true;
}
} // namespace

MenuModeCampaign::MenuModeCampaign(ModeManager* modeManager):
    AbstractApplicationMode(modeManager, ModeManager::MENU_CAMPAIGN),
    mWorldLoaded(false),
    mLiftWindow(nullptr),
    mTooltip(nullptr),
    mHoveredProvince(0),
    mLiftProvince(0),
    mLiftFade(0.0f),
    mBriefingLevel(0)
{
    CEGUI::Window* window = modeManager->getGui().getGuiSheet(Gui::guiSheet::campaignMenu);

    addEventConnection(
        window->getChild(CMP_BUTTON_DIFFICULTY)->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeCampaign::difficultyButtonPressed, this)
        )
    );
    addEventConnection(
        window->getChild(CMP_BUTTON_BACK)->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeCampaign::backButtonPressed, this)
        )
    );
    addEventConnection(
        window->getChild(CMP_FRAME)->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&MenuModeCampaign::backButtonPressed, this)
        )
    );
    addEventConnection(
        window->getChild(CMP_BRIEFING_START)->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeCampaign::briefingStartPressed, this)
        )
    );
    addEventConnection(
        window->getChild(CMP_BRIEFING_CLOSE)->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&MenuModeCampaign::briefingClosePressed, this)
        )
    );

    // The map keeps the aspect ratio of the art and is centred on the screen
    CEGUI::Window* map = window->getChild(CMP_MAP);
    map->setHorizontalAlignment(CEGUI::HA_CENTRE);
    map->setVerticalAlignment(CEGUI::VA_CENTRE);
    map->setAspectMode(CEGUI::AM_SHRINK);
    map->setAspectRatio(MAP_ASPECT_RATIO);
    addEventConnection(
        map->subscribeEvent(
            CEGUI::Window::EventMouseMove,
            CEGUI::Event::Subscriber(&MenuModeCampaign::mapMoved, this)
        )
    );
    addEventConnection(
        map->subscribeEvent(
            CEGUI::Window::EventMouseLeavesArea,
            CEGUI::Event::Subscriber(&MenuModeCampaign::mapLeft, this)
        )
    );
    addEventConnection(
        map->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&MenuModeCampaign::mapClicked, this)
        )
    );
}

bool MenuModeCampaign::loadCampaign()
{
    ResourceManager& resources = ResourceManager::getSingleton();
    Campaign& campaign = Campaign::getSingleton();

    std::ifstream definition((resources.getGameDataPath() + CAMPAIGN_DEFINITION_FILE).c_str());
    if(!definition.is_open() || !campaign.importDefinition(definition))
    {
        OD_LOG_ERR("Could not read the campaign definition " + CAMPAIGN_DEFINITION_FILE);
        return false;
    }

    std::string progressPath = resources.getUserDataPath() + CAMPAIGN_PROGRESS_FILE;
    campaign.setProgressPath(progressPath);
    std::ifstream progress(progressPath.c_str());
    if(progress.is_open())
        campaign.importProgress(progress);

    return true;
}

void MenuModeCampaign::activate()
{
    // Loads the corresponding Gui sheet.
    getModeManager().getGui().loadGuiSheet(Gui::guiSheet::campaignMenu);

    giveFocus();

    // Play the main menu music
    MusicPlayer::getSingleton().play(ConfigManager::getSingleton().getMainMenuMusic());

    // We may come from a finished game: show the menu scene and clean the map
    GameMap* gameMap = ODFrameListener::getSingleton().getClientGameMap();
    gameMap->clearAll();
    gameMap->setGamePaused(true);
    ODFrameListener::getSingleton().stopGameRenderer();
    ODFrameListener::getSingleton().createMainMenuScene();

    Campaign& campaign = Campaign::getSingleton();
    CEGUI::Window* window = getModeManager().getGui().getGuiSheet(Gui::guiSheet::campaignMenu);
    window->getChild(CMP_TEXT_LOADING)->setText("");

    // Debriefing of the level that was played last
    mResultText.clear();
    size_t playedLevel = campaign.getPlayedLevel();
    if(playedLevel < campaign.getNumLevels())
    {
        const CampaignLevel& level = campaign.getLevel(playedLevel);
        if(campaign.getPlayedLevelWon())
        {
            mResultText = "Debriefing - " + level.mTitle + "\n\n" + level.mDebriefing + "\n\n";
            if(level.mBonus)
            {
                mResultText += "You have cleared the hidden land.";
            }
            else if(campaign.isFinished())
                mResultText += "You have completed the campaign.";
            else
                mResultText += "The next level is unlocked.";

            // The numbers of the level, as sent by the server when it was won
            std::string summary = campaign.getLevelSummary();
            if(!summary.empty())
                mResultText += "\n\n" + summary;
        }
        else
        {
            mResultText = "You did not complete this level. You can try again.";
        }
        campaign.clearPlayedLevel();
    }

    clearMap();
    hideBriefing();
    if(campaign.getNumLevels() == 0)
        return;

    if(!loadWorld())
    {
        window->getChild(CMP_TEXT_LOADING)->setText("ERROR: The campaign world map could not be loaded.");
        return;
    }

    mHoveredProvince = mWorld.getProvinces().size();
    mLiftProvince = mWorld.getProvinces().size();
    mLiftFade = 0.0f;
    fillMap();
    if(!mResultText.empty())
        showDebriefing();
    updateDifficultyButton();
}

void MenuModeCampaign::updateDifficultyButton()
{
    Campaign& campaign = Campaign::getSingleton();
    CEGUI::Window* window = getModeManager().getGui().getGuiSheet(Gui::guiSheet::campaignMenu);
    CEGUI::Window* button = window->getChild(CMP_BUTTON_DIFFICULTY);
    KeeperAIType type = static_cast<KeeperAIType>(campaign.getDifficulty());
    std::string name = KeeperAITypes::toString(type);
    name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    button->setText("Difficulty: " + name);
    // The difficulty is chosen when a new campaign is begun and then stays
    button->setEnabled(!campaign.hasProgress());
}

bool MenuModeCampaign::difficultyButtonPressed(const CEGUI::EventArgs&)
{
    Campaign& campaign = Campaign::getSingleton();
    if(campaign.hasProgress())
        return true;

    uint32_t next = (campaign.getDifficulty() + 1) % static_cast<uint32_t>(KeeperAIType::nbAI);
    campaign.setDifficulty(next);
    updateDifficultyButton();
    return true;
}

bool MenuModeCampaign::loadWorld()
{
    if(mWorldLoaded)
        return true;

    std::string directory = ResourceManager::getSingleton().getGameDataPath() + WORLD_DIRECTORY;
    std::ifstream definition((directory + WORLD_DEFINITION_FILE).c_str());
    if(!definition.is_open() || !mWorld.importDefinition(definition))
    {
        OD_LOG_ERR("Could not read the campaign world " + directory + WORLD_DEFINITION_FILE);
        return false;
    }

    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
    if(!decodeImage(directory + WORLD_IDMAP_FILE, width, height, rgba)
        || (width != mWorld.getWidth()) || (height != mWorld.getHeight()))
    {
        OD_LOG_ERR("Could not read the campaign world id map " + directory + WORLD_IDMAP_FILE);
        return false;
    }

    std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3);
    for(size_t i = 0; i < static_cast<size_t>(width) * height; ++i)
    {
        rgb[i * 3] = rgba[i * 4];
        rgb[i * 3 + 1] = rgba[i * 4 + 1];
        rgb[i * 3 + 2] = rgba[i * 4 + 2];
    }
    mWorld.setIdMap(width, height, rgb);
    mWorldLoaded = true;
    return true;
}

CEGUI::Image* MenuModeCampaign::loadWorldImage(const std::string& file)
{
    std::string name = WORLD_IMAGE_PREFIX + file;
    CEGUI::ImageManager& images = CEGUI::ImageManager::getSingleton();
    if(images.isDefined(name))
        return &images.get(name);

    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
    std::string path = ResourceManager::getSingleton().getGameDataPath() + WORLD_DIRECTORY + file;
    if(!decodeImage(path, width, height, rgba))
    {
        OD_LOG_ERR("Could not read the campaign world image " + path);
        return nullptr;
    }

    CEGUI::Texture& texture = CEGUI::System::getSingleton().getRenderer()->createTexture(name);
    texture.loadFromMemory(&rgba[0], CEGUI::Sizef(static_cast<float>(width), static_cast<float>(height)),
        CEGUI::Texture::PF_RGBA);
    CEGUI::BasicImage& image = static_cast<CEGUI::BasicImage&>(images.create("BasicImage", name));
    image.setTexture(&texture);
    image.setArea(CEGUI::Rectf(0, 0, static_cast<float>(width), static_cast<float>(height)));
    return &image;
}

void MenuModeCampaign::clearMap()
{
    CEGUI::WindowManager& windowManager = CEGUI::WindowManager::getSingleton();
    for(std::vector<CEGUI::Window*>::iterator it = mWorldWindows.begin(); it != mWorldWindows.end(); ++it)
        windowManager.destroyWindow(*it);
    mWorldWindows.clear();
    mLiftWindow = nullptr;
    mTooltip = nullptr;
}

void MenuModeCampaign::setMapArea(CEGUI::Window* window, float x, float y, float width, float height) const
{
    float mapWidth = static_cast<float>(mWorld.getWidth());
    float mapHeight = static_cast<float>(mWorld.getHeight());
    window->setArea(CEGUI::UDim(x / mapWidth, 0), CEGUI::UDim(y / mapHeight, 0),
        CEGUI::UDim(width / mapWidth, 0), CEGUI::UDim(height / mapHeight, 0));
}

CEGUI::Window* MenuModeCampaign::createMapWindow(const std::string& type, const std::string& name, float x, float y,
    float width, float height)
{
    CEGUI::Window* window = CEGUI::WindowManager::getSingleton().createWindow(type, "CampaignWorld_" + name);
    CEGUI::Window* map = getModeManager().getGui().getGuiSheet(Gui::guiSheet::campaignMenu)->getChild(CMP_MAP);
    setMapArea(window, x, y, width, height);
    // The map handles the mouse itself, with the id map
    window->setMousePassThroughEnabled(true);
    map->addChild(window);
    mWorldWindows.push_back(window);
    return window;
}

MenuModeCampaign::State MenuModeCampaign::getProvinceState(size_t province, size_t& level) const
{
    Campaign& campaign = Campaign::getSingleton();
    level = campaign.findLevelByProvince(mWorld.getProvinces()[province].mId);
    if(level >= campaign.getNumLevels())
        return State::locked;

    if(campaign.isCompleted(level))
        return State::conquered;

    if(campaign.isUnlocked(level))
        return State::available;

    return State::locked;
}

size_t MenuModeCampaign::findSiteAt(float x, float y) const
{
    Campaign& campaign = Campaign::getSingleton();
    const std::vector<CampaignWorldSite>& sites = mWorld.getSites();
    for(size_t i = 0; i < sites.size(); ++i)
    {
        // A site that was not found yet is not on the map
        size_t level = campaign.findLevelByProvince(sites[i].mId);
        if((level >= campaign.getNumLevels()) || !campaign.isUnlocked(level))
            continue;

        float dx = x - static_cast<float>(sites[i].mX);
        float dy = y - static_cast<float>(sites[i].mY);
        if(dx * dx + dy * dy <= SITE_HIT_RADIUS * SITE_HIT_RADIUS)
            return i;
    }
    return sites.size();
}

void MenuModeCampaign::fillMap()
{
    Campaign& campaign = Campaign::getSingleton();
    CEGUI::Window* map = getModeManager().getGui().getGuiSheet(Gui::guiSheet::campaignMenu)->getChild(CMP_MAP);

    if(loadWorldImage(WORLD_BASE_FILE) != nullptr)
        map->setProperty("Image", WORLD_IMAGE_PREFIX + WORLD_BASE_FILE);

    // The layer of the state of each province
    const std::vector<CampaignWorldProvince>& provinces = mWorld.getProvinces();
    for(size_t i = 0; i < provinces.size(); ++i)
    {
        const CampaignWorldProvince& province = provinces[i];
        size_t level = 0;
        State state = getProvinceState(i, level);
        const std::string* file = &province.mLayerLocked;
        if(state == State::available)
            file = &province.mLayerAvailable;
        else if(state == State::conquered)
            file = &province.mLayerConquered;

        CEGUI::Image* image = loadWorldImage(*file);
        if(image == nullptr)
            continue;

        CEGUI::Sizef size = image->getRenderedSize();
        CEGUI::Window* layer = createMapWindow("OD/StaticImage", "Layer_" + province.mId,
            static_cast<float>(province.mLayerX), static_cast<float>(province.mLayerY), size.d_width, size.d_height);
        layer->setProperty("FrameEnabled", "False");
        layer->setProperty("BackgroundEnabled", "False");
        layer->setProperty("Image", WORLD_IMAGE_PREFIX + *file);
    }

    // Bonus sites appear once they are found
    const std::vector<CampaignWorldSite>& sites = mWorld.getSites();
    for(size_t i = 0; i < sites.size(); ++i)
    {
        const CampaignWorldSite& site = sites[i];
        size_t level = campaign.findLevelByProvince(site.mId);
        if((level >= campaign.getNumLevels()) || !campaign.isUnlocked(level))
            continue;

        const std::string& file = campaign.isCompleted(level) ? site.mIconDone : site.mIconFound;
        CEGUI::Image* image = loadWorldImage(file);
        if(image == nullptr)
            continue;

        CEGUI::Sizef size = image->getRenderedSize();
        CEGUI::Window* marker = createMapWindow("OD/StaticImage", "Site_" + site.mId,
            static_cast<float>(site.mX) - size.d_width / 2.0f, static_cast<float>(site.mY) - size.d_height / 2.0f,
            size.d_width, size.d_height);
        marker->setProperty("FrameEnabled", "False");
        marker->setProperty("BackgroundEnabled", "False");
        marker->setProperty("Image", WORLD_IMAGE_PREFIX + file);
    }

    // The province under the mouse lifts off the map
    mLiftWindow = createMapWindow("OD/StaticImage", "Lift", 0, 0, 1, 1);
    mLiftWindow->setProperty("FrameEnabled", "False");
    mLiftWindow->setProperty("BackgroundEnabled", "False");
    mLiftWindow->setVisible(false);

    // The campaign title in the cartouche
    const CampaignWorldRect& cartouche = mWorld.getTitleCartouche();
    CEGUI::Window* title = createMapWindow("OD/StaticText", "Title", static_cast<float>(cartouche.mX),
        static_cast<float>(cartouche.mY), static_cast<float>(cartouche.mWidth), static_cast<float>(cartouche.mHeight));
    title->setProperty("FrameEnabled", "False");
    title->setProperty("BackgroundEnabled", "False");
    title->setProperty("Font", "MedievalSharp-10");
    title->setProperty("HorzFormatting", "CentreAligned");
    title->setProperty("VertFormatting", "CentreAligned");
    title->setProperty("TextColours", "FF2A1A0C");
    title->setText(CAMPAIGN_TITLE);

    // Progress: provinces conquered, hidden sites found and a bar in percent
    int conquered = 0;
    for(size_t i = 0; i < campaign.getNumLevels(); ++i)
    {
        if(!campaign.getLevel(i).mBonus && campaign.isCompleted(i))
            ++conquered;
    }
    int provinceTotal = static_cast<int>(provinces.size());
    int siteTotal = static_cast<int>(sites.size());
    int percent = (provinceTotal > 0) ? (conquered * PERCENT_FULL / provinceTotal) : 0;

    const CampaignWorldRect& panel = mWorld.getProgressPanel();
    float panelX = static_cast<float>(panel.mX);
    float panelY = static_cast<float>(panel.mY);
    float panelWidth = static_cast<float>(panel.mWidth);
    float panelHeight = static_cast<float>(panel.mHeight);
    CEGUI::Window* progress = createMapWindow("OD/StaticText", "ProgressText", panelX + panelWidth * PROGRESS_LEFT,
        panelY, panelWidth * PROGRESS_TEXT_WIDTH, panelHeight * PROGRESS_TEXT_HEIGHT);
    progress->setProperty("FrameEnabled", "False");
    progress->setProperty("BackgroundEnabled", "False");
    progress->setProperty("Font", "MedievalSharp-6");
    progress->setProperty("HorzFormatting", "LeftAligned");
    progress->setProperty("VertFormatting", "CentreAligned");
    progress->setProperty("TextColours", "FF2A1A0C");
    progress->setText(Helper::toString(conquered) + " / " + Helper::toString(provinceTotal)
        + " provinces conquered\n" + Helper::toString(static_cast<int>(campaign.getHeartstonePieces())) + " / "
        + Helper::toString(siteTotal) + " hidden sites");

    CEGUI::Window* bar = createMapWindow("OD/ProgressBar", "ProgressBar", panelX + panelWidth * PROGRESS_LEFT,
        panelY + panelHeight * PROGRESS_BAR_TOP, panelWidth * PROGRESS_BAR_WIDTH, panelHeight * PROGRESS_BAR_HEIGHT);
    static_cast<CEGUI::ProgressBar*>(bar)->setProgress(static_cast<float>(percent) / PERCENT_FULL_FLOAT);

    CEGUI::Window* percentText = createMapWindow("OD/StaticText", "ProgressPercent",
        panelX + panelWidth * PROGRESS_PERCENT_LEFT, panelY + panelHeight * PROGRESS_PERCENT_TOP,
        panelWidth * PROGRESS_PERCENT_WIDTH, panelHeight * PROGRESS_PERCENT_HEIGHT);
    percentText->setProperty("FrameEnabled", "False");
    percentText->setProperty("BackgroundEnabled", "False");
    percentText->setProperty("Font", "MedievalSharp-6");
    percentText->setProperty("HorzFormatting", "RightAligned");
    percentText->setProperty("VertFormatting", "CentreAligned");
    percentText->setProperty("TextColours", "FF2A1A0C");
    percentText->setText(Helper::toString(percent) + "%");

    // The tooltip is the topmost window of the map
    mTooltip = createMapWindow("OD/StaticText", "Tooltip", 0, 0, 1, 1);
    mTooltip->setProperty("Font", "MedievalSharp-8");
    mTooltip->setProperty("HorzFormatting", "LeftAligned");
    mTooltip->setProperty("VertFormatting", "CentreAligned");
    mTooltip->setVisible(false);
}

void MenuModeCampaign::onFrameStarted(const Ogre::FrameEvent& evt)
{
    updateLift(evt.timeSinceLastFrame);
}

void MenuModeCampaign::updateLift(float elapsed)
{
    if(mLiftWindow == nullptr)
        return;

    size_t count = mWorld.getProvinces().size();
    if((mHoveredProvince != count) && (mHoveredProvince != mLiftProvince))
    {
        const CampaignWorldProvince& province = mWorld.getProvinces()[mHoveredProvince];
        if(loadWorldImage(province.mLayerLift) == nullptr)
        {
            mHoveredProvince = count;
        }
        else
        {
            mLiftProvince = mHoveredProvince;
            mLiftFade = 0.0f;
            mLiftWindow->setProperty("Image", WORLD_IMAGE_PREFIX + province.mLayerLift);
        }
    }

    float step = elapsed / LIFT_FADE_TIME;
    if(mHoveredProvince != count)
        mLiftFade = std::min(1.0f, mLiftFade + step);
    else
        mLiftFade = std::max(0.0f, mLiftFade - step);

    if((mLiftProvince == count) || ((mHoveredProvince == count) && (mLiftFade <= 0.0f)))
    {
        mLiftProvince = count;
        mLiftWindow->setVisible(false);
        return;
    }

    const CampaignWorldProvince& province = mWorld.getProvinces()[mLiftProvince];
    CEGUI::Image* image = loadWorldImage(province.mLayerLift);
    if(image == nullptr)
        return;

    // The layer grows around its centre while it fades in
    CEGUI::Sizef size = image->getRenderedSize();
    float scale = 1.0f + (LIFT_SCALE - 1.0f) * mLiftFade;
    float width = size.d_width * scale;
    float height = size.d_height * scale;
    float centreX = static_cast<float>(province.mLiftX) + size.d_width / 2.0f;
    float centreY = static_cast<float>(province.mLiftY) + size.d_height / 2.0f;
    setMapArea(mLiftWindow, centreX - width / 2.0f, centreY - height / 2.0f, width, height);
    mLiftWindow->setAlpha(mLiftFade);
    mLiftWindow->setVisible(true);
}

void MenuModeCampaign::updateTooltip(const CEGUI::Vector2f& position, size_t province, size_t site)
{
    if(mTooltip == nullptr)
        return;

    Campaign& campaign = Campaign::getSingleton();
    std::string text;
    if(site < mWorld.getSites().size())
    {
        const CampaignWorldSite& found = mWorld.getSites()[site];
        size_t level = campaign.findLevelByProvince(found.mId);
        text = found.mName + "\nBonus site\nState: "
            + ((level < campaign.getNumLevels() && campaign.isCompleted(level)) ? "Cleared" : "Found");
    }
    else if(province < mWorld.getProvinces().size())
    {
        size_t level = 0;
        State state = getProvinceState(province, level);
        text = mWorld.getProvinces()[province].mName;
        if(level < campaign.getNumLevels())
        {
            const CampaignLevel& campaignLevel = campaign.getLevel(level);
            if(!campaignLevel.mWarden.empty())
                text += "\nWarden: " + campaignLevel.mWarden;
        }
        if(state == State::locked)
            text += "\nState: Locked";
        else if(state == State::available)
            text += "\nState: Available";
        else
            text += "\nState: Conquered";
        if(level < campaign.getNumLevels())
            text += "\nDifficulty: " + Helper::toString(static_cast<int>(campaign.getLevel(level).mDifficulty)) + " / 5";
    }
    else
    {
        mTooltip->setVisible(false);
        return;
    }

    const CEGUI::Rectf& mapRect = mTooltip->getParent()->getUnclippedOuterRect().get();
    float x = position.d_x - mapRect.left() + TOOLTIP_OFFSET;
    float y = position.d_y - mapRect.top() + TOOLTIP_OFFSET;
    // Keep it inside of the map
    if(x + TOOLTIP_WIDTH > mapRect.getWidth())
        x = position.d_x - mapRect.left() - TOOLTIP_OFFSET - TOOLTIP_WIDTH;
    if(y + TOOLTIP_HEIGHT > mapRect.getHeight())
        y = position.d_y - mapRect.top() - TOOLTIP_OFFSET - TOOLTIP_HEIGHT;
    mTooltip->setArea(CEGUI::UDim(0, x), CEGUI::UDim(0, y), CEGUI::UDim(0, TOOLTIP_WIDTH), CEGUI::UDim(0, TOOLTIP_HEIGHT));
    mTooltip->setText(reinterpret_cast<const CEGUI::utf8*>(text.c_str()));
    mTooltip->setVisible(true);
}

bool MenuModeCampaign::mapMoved(const CEGUI::EventArgs& e)
{
    const CEGUI::MouseEventArgs& args = static_cast<const CEGUI::MouseEventArgs&>(e);
    if(!mWorldLoaded)
        return true;

    const CEGUI::Rectf& mapRect = args.window->getUnclippedOuterRect().get();
    if(mapRect.getWidth() <= 0.0f)
        return true;

    float x = (args.position.d_x - mapRect.left()) * static_cast<float>(mWorld.getWidth()) / mapRect.getWidth();
    float y = (args.position.d_y - mapRect.top()) * static_cast<float>(mWorld.getHeight()) / mapRect.getHeight();
    size_t site = findSiteAt(x, y);
    size_t province = mWorld.getProvinceAt(static_cast<int>(x), static_cast<int>(y));
    size_t count = mWorld.getProvinces().size();

    // Only provinces that are available or conquered lift off the map
    mHoveredProvince = count;
    if((site >= mWorld.getSites().size()) && (province < count))
    {
        size_t level = 0;
        if(getProvinceState(province, level) != State::locked)
            mHoveredProvince = province;
    }
    updateTooltip(args.position, province, site);
    return true;
}

bool MenuModeCampaign::mapLeft(const CEGUI::EventArgs&)
{
    mHoveredProvince = mWorld.getProvinces().size();
    if(mTooltip != nullptr)
        mTooltip->setVisible(false);
    return true;
}

bool MenuModeCampaign::mapClicked(const CEGUI::EventArgs& e)
{
    const CEGUI::MouseEventArgs& args = static_cast<const CEGUI::MouseEventArgs&>(e);
    if(!mWorldLoaded || (args.button != CEGUI::LeftButton))
        return true;

    Campaign& campaign = Campaign::getSingleton();
    CEGUI::Window* sheet = getModeManager().getGui().getGuiSheet(Gui::guiSheet::campaignMenu);
    if(sheet->getChild(CMP_BRIEFING)->isVisible())
        return true;

    const CEGUI::Rectf& mapRect = args.window->getUnclippedOuterRect().get();
    if(mapRect.getWidth() <= 0.0f)
        return true;

    float x = (args.position.d_x - mapRect.left()) * static_cast<float>(mWorld.getWidth()) / mapRect.getWidth();
    float y = (args.position.d_y - mapRect.top()) * static_cast<float>(mWorld.getHeight()) / mapRect.getHeight();
    size_t level = campaign.getNumLevels();
    size_t site = findSiteAt(x, y);
    size_t province = mWorld.getProvinceAt(static_cast<int>(x), static_cast<int>(y));
    CEGUI::Window* statusText = sheet->getChild(CMP_TEXT_LOADING);
    statusText->setText("");
    if(site < mWorld.getSites().size())
    {
        level = campaign.findLevelByProvince(mWorld.getSites()[site].mId);
    }
    else if(province < mWorld.getProvinces().size())
    {
        State state = getProvinceState(province, level);
        if(state == State::locked)
        {
            if(level < campaign.getNumLevels())
                statusText->setText("This province is locked. Conquer the previous provinces first.");
            else
                statusText->setText("This province cannot be played yet.");
            return true;
        }
    }

    if(level < campaign.getNumLevels())
        showBriefing(level);
    return true;
}

//! \brief Shows a level briefing with its optional preview, falling back to the text-only layout.
void MenuModeCampaign::showBriefing(size_t level)
{
    Campaign& campaign = Campaign::getSingleton();
    CEGUI::Window* sheet = getModeManager().getGui().getGuiSheet(Gui::guiSheet::campaignMenu);
    const CampaignLevel& campaignLevel = campaign.getLevel(level);
    CEGUI::Image* image = nullptr;
    if(!campaignLevel.mPreview.empty())
        image = loadWorldImage(campaignLevel.mPreview);
    setBriefingPreviewVisible(image != nullptr);
    if(image != nullptr)
        sheet->getChild(CMP_BRIEFING_IMAGE)->setProperty("Image", WORLD_IMAGE_PREFIX + campaignLevel.mPreview);
    std::string text = "Briefing - " + campaignLevel.mTitle + "\n\n" + campaignLevel.mBriefing;
    sheet->getChild(CMP_BRIEFING_TEXT)->setText(reinterpret_cast<const CEGUI::utf8*>(text.c_str()));
    sheet->getChild(CMP_BRIEFING_START)->setVisible(true);
    sheet->getChild(CMP_BRIEFING)->setVisible(true);
    mBriefingLevel = level;
}

//! \brief Shows the last level result using the original text-only panel.
void MenuModeCampaign::showDebriefing()
{
    CEGUI::Window* sheet = getModeManager().getGui().getGuiSheet(Gui::guiSheet::campaignMenu);
    setBriefingPreviewVisible(false);
    sheet->getChild(CMP_BRIEFING_TEXT)->setText(reinterpret_cast<const CEGUI::utf8*>(mResultText.c_str()));
    sheet->getChild(CMP_BRIEFING_START)->setVisible(false);
    sheet->getChild(CMP_BRIEFING)->setVisible(true);
    mBriefingLevel = Campaign::getSingleton().getNumLevels();
}

//! \brief Fits the panel and text around a visible preview, or restores the text-only layout.
void MenuModeCampaign::setBriefingPreviewVisible(bool visible)
{
    CEGUI::Window* sheet = getModeManager().getGui().getGuiSheet(Gui::guiSheet::campaignMenu);
    sheet->getChild(CMP_BRIEFING_IMAGE)->setVisible(visible);
    if(visible)
    {
        sheet->getChild(CMP_BRIEFING)->setProperty("Area", BRIEFING_PREVIEW_PANEL_AREA);
        sheet->getChild(CMP_BRIEFING_TEXT)->setProperty("Area", BRIEFING_PREVIEW_TEXT_AREA);
    }
    else
    {
        sheet->getChild(CMP_BRIEFING)->setProperty("Area", BRIEFING_TEXT_PANEL_AREA);
        sheet->getChild(CMP_BRIEFING_TEXT)->setProperty("Area", BRIEFING_TEXT_AREA);
    }
}

void MenuModeCampaign::hideBriefing()
{
    CEGUI::Window* sheet = getModeManager().getGui().getGuiSheet(Gui::guiSheet::campaignMenu);
    sheet->getChild(CMP_BRIEFING)->setVisible(false);
    mBriefingLevel = Campaign::getSingleton().getNumLevels();
}

bool MenuModeCampaign::briefingStartPressed(const CEGUI::EventArgs&)
{
    size_t level = mBriefingLevel;
    if(level < Campaign::getSingleton().getNumLevels())
        startLevel(level);
    return true;
}

bool MenuModeCampaign::briefingClosePressed(const CEGUI::EventArgs&)
{
    hideBriefing();
    return true;
}

void MenuModeCampaign::startLevel(size_t index)
{
    Campaign& campaign = Campaign::getSingleton();
    CEGUI::Window* mainWin = getModeManager().getGui().getGuiSheet(Gui::guiSheet::campaignMenu);
    CEGUI::Window* statusText = mainWin->getChild(CMP_TEXT_LOADING);

    if(!campaign.isUnlocked(index))
    {
        statusText->setText("This level is locked. Complete the previous levels first.");
        return;
    }

    std::string level = ResourceManager::getSingleton().getGameDataPath() + "levels/"
        + campaign.getLevel(index).mFile;
    if(!boost::filesystem::exists(boost::filesystem::path(level)))
    {
        OD_LOG_ERR("Campaign level not found: " + level);
        statusText->setText("ERROR: Campaign level file not found.");
        return;
    }

    hideBriefing();
    statusText->setText("Loading...");

    // In single player mode, we act as a server
    campaign.startLevel(index);
    const std::string& nickname = ODFrameListener::getSingleton().getClientGameMap()->getLocalPlayerNick();
    if(!ODServer::getSingleton().startServer(nickname, level, ServerMode::ModeGameSinglePlayer, false))
    {
        OD_LOG_ERR("Could not start server for campaign game !!!");
        statusText->setText("ERROR: Could not start server for campaign game !!!");
        campaign.stopCampaign();
        return;
    }

    int port = ODServer::getSingleton().getNetworkPort();
    uint32_t timeout = ConfigManager::getSingleton().getClientConnectionTimeout();
    std::string replayFilename = ResourceManager::getSingleton().getReplayDataPath()
        + ResourceManager::getSingleton().buildReplayFilename();
    if(!ODClient::getSingleton().connect("localhost", port, timeout, replayFilename))
    {
        OD_LOG_ERR("Could not connect to server for campaign game !!!");
        statusText->setText("Error: Couldn't connect to local server!");
        campaign.stopCampaign();
        return;
    }
}

bool MenuModeCampaign::backButtonPressed(const CEGUI::EventArgs&)
{
    Campaign::getSingleton().stopCampaign();
    getModeManager().requestMode(AbstractModeManager::MENU_MAIN);
    return true;
}
