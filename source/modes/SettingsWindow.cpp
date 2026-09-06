/*!
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

#include "modes/SettingsWindow.h"

#include "gamemap/MiniMap.h"
#include "network/ODClient.h"
#include "camera/CameraManager.h"
#include "render/Gui.h"
#include "render/ODFrameListener.h"
#include "render/RenderManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"
#include "utils/Helper.h"

#include <CEGUI/CEGUI.h>
#include <CEGUI/widgets/ToggleButton.h>
#include <CEGUI/widgets/Combobox.h>
#include <CEGUI/widgets/ToggleButton.h>
#include <CEGUI/widgets/PushButton.h>
#include <CEGUI/WindowManager.h>

#include <OgreRoot.h>
#include <OgreCamera.h>
#include <OgreRenderWindow.h>
#include <OgreSceneManager.h>

#include <SFML/Audio/Listener.hpp>

#include <algorithm>
#include <exception>
#include <map>
#include <sstream>

SettingsWindow::SettingsWindow(CEGUI::Window* rootWindow, Gui& gui):
    mSettingsWindow(nullptr),
    mApplyWindow(nullptr),
    mRootWindow(rootWindow),
    mGui(gui)
{
    if (rootWindow == nullptr)
    {
        OD_LOG_ERR("Settings Window loaded without any main CEGUI window!!");
        return;
    }
    CEGUI::WindowManager* wmgr = CEGUI::WindowManager::getSingletonPtr();

    mSettingsWindow = wmgr->loadLayoutFromFile("WindowSettings.layout");
    if (mSettingsWindow == nullptr)
    {
        OD_LOG_ERR("Couldn't load the Settings Window layout!!");
        return;
    }
    rootWindow->addChild(mSettingsWindow);
    mSettingsWindow->hide();

    mApplyWindow = wmgr->loadLayoutFromFile("WindowApplyChanges.layout");
    if (mApplyWindow == nullptr)
    {
        OD_LOG_ERR("Couldn't load the apply changes popup Window layout!!");
        return;
    }
    rootWindow->addChild(mApplyWindow);
    mApplyWindow->hide();

    // Events connections
    // Settings window
    addEventConnection(
        mSettingsWindow->getChild("CancelButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&SettingsWindow::onCancelSettings, this)
        )
    );
    addEventConnection(
        mSettingsWindow->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&SettingsWindow::onCancelSettings, this)
        )
    );
    addEventConnection(
        mSettingsWindow->getChild("ApplyButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&SettingsWindow::onApplySettings, this)
        )
    );

    // Music volume slider
    CEGUI::Slider* volumeSlider = static_cast<CEGUI::Slider*>(
        mSettingsWindow->getChild("MainTabControl/Audio/AudioSP/MusicSlider"));
    addEventConnection(
        volumeSlider->subscribeEvent(
            CEGUI::Slider::EventValueChanged,
            CEGUI::Event::Subscriber(&SettingsWindow::onMusicVolumeChanged, this)
        )
    );

    // Light factor slider
    CEGUI::Slider* lightFactorSlider = static_cast<CEGUI::Slider*>(
        mSettingsWindow->getChild("MainTabControl/Game/GameSP/LightSlider"));
    addEventConnection(
        lightFactorSlider->subscribeEvent(
            CEGUI::Slider::EventValueChanged,
            CEGUI::Event::Subscriber(&SettingsWindow::onLightFactorChanged, this)
        )
    );

    // Camera pan speed slider
    CEGUI::Slider* panSpeedSlider = static_cast<CEGUI::Slider*>(
        mSettingsWindow->getChild("MainTabControl/Input/InputSP/PanSpeedSlider"));
    addEventConnection(
        panSpeedSlider->subscribeEvent(
            CEGUI::Slider::EventValueChanged,
            CEGUI::Event::Subscriber(&SettingsWindow::onPanSpeedChanged, this)
        )
    );

    // Apply Pop-up
    addEventConnection(
        mApplyWindow->getChild("CancelButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&SettingsWindow::onPopupCancelApplySettings, this)
        )
    );
    addEventConnection(
        mApplyWindow->subscribeEvent(
            CEGUI::FrameWindow::EventCloseClicked,
            CEGUI::Event::Subscriber(&SettingsWindow::onPopupCancelApplySettings, this)
        )
    );
    addEventConnection(
        mApplyWindow->getChild("ApplyButton")->subscribeEvent(
            CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&SettingsWindow::onPopupApplySettings, this)
        )
    );

    addEventConnection(
        mSettingsWindow->getChild("MainTabControl/Video/VideoSP/UIScaleCombobox")->subscribeEvent(
            CEGUI::Combobox::EventListSelectionAccepted,
            CEGUI::Event::Subscriber(&SettingsWindow::onUiScaleChanged, this)
        )
    );

    initConfig();
    mGui.registerWindowHierarchy(mApplyWindow);
}

SettingsWindow::~SettingsWindow()
{
    // Disconnects all event connections.
    for(CEGUI::Event::Connection& c : mEventConnections)
        c->disconnect();

    if (mSettingsWindow)
    {
        mSettingsWindow->hide();
        mSettingsWindow->setModalState(false);
        // Will be handled by the window manager. Don't do it.
        //mRootWindow->removeChild(mSettingsWindow->getID());
        CEGUI::WindowManager* wmgr = CEGUI::WindowManager::getSingletonPtr();
        wmgr->destroyWindow(mSettingsWindow);
    }

    if (mApplyWindow)
    {
        mApplyWindow->hide();
        mApplyWindow->setModalState(false);
        // Will be handled by the window manager. Don't do it.
        //mRootWindow->removeChild(mApplyWindow->getID());
        CEGUI::WindowManager* wmgr = CEGUI::WindowManager::getSingletonPtr();
        wmgr->destroyWindow(mApplyWindow);
    }
}

void SettingsWindow::initConfig()
{
    ConfigManager& config = ConfigManager::getSingleton();

    const CEGUI::Image* selImg = &CEGUI::ImageManager::getSingleton().get("OpenDungeonsSkin/SelectionBrush");

    // Game
    CEGUI::Editbox* nicknameEb = static_cast<CEGUI::Editbox*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Game/GameSP/NicknameEdit"));
    std::string nickname = config.getGameValue(Config::NICKNAME, std::string(), false);
    if (!nickname.empty())
        nicknameEb->setText(reinterpret_cast<const CEGUI::utf8*>(nickname.c_str()));

    CEGUI::Combobox* keeperVoiceCb = static_cast<CEGUI::Combobox*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Game/GameSP/KeeperVoice"));
    keeperVoiceCb->resetList();
    std::string keeperVoice = config.getGameValue(Config::KEEPERVOICE, ConfigManager::DEFAULT_KEEPER_VOICE, false);
    uint32_t cptVoice = 0;
    for(const std::string& keeperVoiceAvailable : config.getKeeperVoices())
    {
        CEGUI::ListboxTextItem* item = new CEGUI::ListboxTextItem(keeperVoiceAvailable, cptVoice);
        item->setSelectionBrushImage(selImg);
        keeperVoiceCb->addItem(item);
        if(keeperVoiceAvailable == keeperVoice)
        {
            keeperVoiceCb->setText(item->getText());
            keeperVoiceCb->setItemSelectState(item, true);
        }
        ++cptVoice;
    }

    CEGUI::Combobox* minimapTypes = static_cast<CEGUI::Combobox*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Game/GameSP/MiniMapType"));
    minimapTypes->resetList();
    std::string minimapTypeCurrent = config.getGameValue(Config::MINIMAP_TYPE, MiniMap::DEFAULT_MINIMAP, false);
    uint32_t cptMiniMapType = 0;
    for(const std::string& minimapType : MiniMap::getMiniMapTypes())
    {
        CEGUI::ListboxTextItem* item = new CEGUI::ListboxTextItem(minimapType, cptMiniMapType);
        item->setSelectionBrushImage(selImg);
        minimapTypes->addItem(item);
        if(minimapType == minimapTypeCurrent)
        {
            minimapTypes->setText(item->getText());
            minimapTypes->setItemSelectState(item, true);
        }
        ++cptMiniMapType;
    }

    std::string lightStr = config.getGameValue(Config::LIGHT_FACTOR, std::string(), false);
    float lightFactor = lightStr.empty() ? 0.0f : Helper::toFloat(lightStr);
    setLightFactorValue(lightFactor);

    std::string panSpeedStr = config.getInputValue(Config::PAN_SPEED, "100", false);
    float panSpeedPercent = panSpeedStr.empty() ? 100.0f : Helper::toFloat(panSpeedStr);
    setPanSpeedValue(panSpeedPercent);

    // Input
    CEGUI::ToggleButton* keyboardGrabCheckbox = static_cast<CEGUI::ToggleButton*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Input/InputSP/KeyboardGrabCheckbox"));
    keyboardGrabCheckbox->setSelected(config.getInputValue(Config::KEYBOARD_GRAB, "No", false) == "Yes");

    CEGUI::ToggleButton* mouseGrabCheckbox = static_cast<CEGUI::ToggleButton*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Input/InputSP/MouseGrabCheckbox"));
    mouseGrabCheckbox->setSelected(config.getInputValue(Config::MOUSE_GRAB, "No", false) == "Yes");

    CEGUI::ToggleButton* autoscrollCheckbox = static_cast<CEGUI::ToggleButton*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Input/InputSP/AutoscrollCheckbox"));
    autoscrollCheckbox->setSelected(config.getInputValue(Config::AUTOSCROLL, "No", false) == "Yes");
    
    // Audio
    std::string volumeStr = config.getAudioValue(Config::MUSIC_VOLUME, std::string(), false);
    float volume = volumeStr.empty() ? sf::Listener::getGlobalVolume() : Helper::toFloat(volumeStr);
    setMusicVolumeValue(volume);

    // Video
    Ogre::Root* ogreRoot = Ogre::Root::getSingletonPtr();
    Ogre::RenderSystem* renderer = ogreRoot->getRenderSystem();
    const Ogre::ConfigOptionMap& options = renderer->getConfigOptions();


    
    // Get the video settings.

    // Available renderers
    const Ogre::RenderSystemList& rdrList = ogreRoot->getAvailableRenderers();
    Ogre::RenderSystem* renderSystem = ogreRoot->getRenderSystem();

    CEGUI::Combobox* rdrCb = static_cast<CEGUI::Combobox*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/RendererCombobox"));
    rdrCb->setReadOnly(true);
    rdrCb->setSortingEnabled(true);
    rdrCb->resetList();
    uint32_t i = 0;
    for (Ogre::RenderSystem* rdr : rdrList)
    {
        CEGUI::ListboxTextItem* item = new CEGUI::ListboxTextItem(rdr->getName(), i);
        item->setSelectionBrushImage(selImg);
        rdrCb->addItem(item);

        if (rdr == renderSystem)
        {
            rdrCb->setItemSelectState(item, true);
            rdrCb->setText(item->getText());
        }
        ++i;
    }

    // Resolution

    CEGUI::ToggleButton* dynamicShadowsCheckBox = static_cast<CEGUI::ToggleButton*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/DynamicShadowsCheckbox"));
    dynamicShadowsCheckBox->setSelected( config.getUserValue(Config::Ctg::AUDIO,Config::SHADOWS,"",false) == "Yes");
    Ogre::ConfigOptionMap::const_iterator it = options.find(Config::VIDEO_MODE);
    if (it != options.end())
    {
        const Ogre::ConfigOption& video = it->second;
        CEGUI::Combobox* resCb = static_cast<CEGUI::Combobox*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/ResolutionCombobox"));
        resCb->setReadOnly(true);
        resCb->setSortingEnabled(true);
        resCb->resetList();
        uint32_t i = 0;
        for (std::string res : video.possibleValues)
        {
            CEGUI::ListboxTextItem* item = new CEGUI::ListboxTextItem(res, i);
            item->setSelectionBrushImage(selImg);
            resCb->addItem(item);

            if (res == video.currentValue)
            {
                resCb->setItemSelectState(item, true);
                resCb->setText(item->getText());
            }
            ++i;
        }
    }

    it = options.find(Config::FULL_SCREEN);
    if (it != options.end())
    {
        const Ogre::ConfigOption& fullscreen = it->second;
        CEGUI::ToggleButton* fsCheckBox = static_cast<CEGUI::ToggleButton*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/FullscreenCheckbox"));
        fsCheckBox->setSelected((fullscreen.currentValue == "Yes"));
    }
    
    it = options.find(Config::VSYNC);
    if (it != options.end())
    {
        const Ogre::ConfigOption& vsync = it->second;
        CEGUI::ToggleButton* vsCheckBox = static_cast<CEGUI::ToggleButton*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/VSyncCheckbox"));
        vsCheckBox->setSelected((vsync.currentValue == "Yes"));
    }

    CEGUI::Combobox* uiScaleCb = static_cast<CEGUI::Combobox*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/UIScaleCombobox"));
    uiScaleCb->setReadOnly(true);
    uiScaleCb->resetList();
    int configuredUiScale = 100;
    std::istringstream uiScaleParser(config.getGameValue(Config::UI_SCALE, "100", false));
    if(!(uiScaleParser >> configuredUiScale))
        configuredUiScale = 100;
    configuredUiScale = std::max(static_cast<int>(Gui::MIN_UI_SCALE_PERCENT),
        std::min(static_cast<int>(Gui::MAX_UI_SCALE_PERCENT), configuredUiScale));
    configuredUiScale = (configuredUiScale + 5) / 10 * 10;
    for(uint32_t uiScale = Gui::MIN_UI_SCALE_PERCENT;
        uiScale <= Gui::MAX_UI_SCALE_PERCENT; uiScale += 10)
    {
        CEGUI::ListboxTextItem* item = new CEGUI::ListboxTextItem(
            Helper::toString(uiScale) + "%", uiScale);
        item->setSelectionBrushImage(selImg);
        uiScaleCb->addItem(item);
        if(static_cast<int>(uiScale) == configuredUiScale)
        {
            uiScaleCb->setItemSelectState(item, true);
            uiScaleCb->setText(item->getText());
        }
    }
    mGui.setUserScalePercent(static_cast<float>(configuredUiScale));

    //! First of all, clear up the previously created windows.
    CEGUI::WindowManager& winMgr = CEGUI::WindowManager::getSingleton();
    CEGUI::Window* parentWindow = mSettingsWindow->getChild("MainTabControl/Video/VideoSP/");
    for (CEGUI::Window* win : mCustomVideoComboBoxes)
    {
        parentWindow->removeChild(win);
        winMgr.destroyWindow(win);
    }
    mCustomVideoComboBoxes.clear();
    for (CEGUI::Window* win : mCustomVideoTexts)
    {
        parentWindow->removeChild(win);
        winMgr.destroyWindow(win);
    }
    mCustomVideoTexts.clear();

    // Find every other config options and add them to the config
    CEGUI::Window* videoTab = mSettingsWindow->getChild("MainTabControl/Video/VideoSP/");
    uint32_t offset = 0;
    for (std::pair<Ogre::String, Ogre::ConfigOption> option : options)
    {
        std::string optionName = option.first;
        // Skip the main options already set.
        if (optionName == Config::VSYNC || optionName == Config::FULL_SCREEN
            || optionName == Config::VIDEO_MODE)
            continue;

        Ogre::ConfigOption& config = option.second;
        // If the option is immutable, we can't change it and shouldn't see it. (at least for now)
        if (config.immutable || config.possibleValues.empty())
            continue;

        // OGRE may repeat colour depths for different fullscreen refresh rates.
        std::sort(config.possibleValues.begin(), config.possibleValues.end());
        config.possibleValues.erase(std::unique(config.possibleValues.begin(), config.possibleValues.end()),
                                   config.possibleValues.end());

        // The text next to the combobox
        CEGUI::DefaultWindow* videoCbText = static_cast<CEGUI::DefaultWindow*>(videoTab->createChild("OD/StaticText", optionName + "_Text"));
        videoCbText->setArea(CEGUI::UDim(0, 20), CEGUI::UDim(0, 238 + offset), CEGUI::UDim(0.4, 0), CEGUI::UDim(0, 34));
        videoCbText->setText(optionName + ": ");
        videoCbText->setProperty("FrameEnabled", "False");
        videoCbText->setProperty("BackgroundEnabled", "False");

        CEGUI::Combobox* videoCb = static_cast<CEGUI::Combobox*>(videoTab->createChild("OD/Combobox", optionName));
        videoCb->setArea(CEGUI::UDim(0.5, 0), CEGUI::UDim(0, 238 + offset), CEGUI::UDim(0.5, -20),
                         CEGUI::UDim(0, config.possibleValues.size() * 17 + 30));
        videoCb->setReadOnly(true);
        videoCb->setSortingEnabled(true);

        // Register the widgets for potential later deletion.
        mCustomVideoTexts.push_back(videoCbText);
        mCustomVideoComboBoxes.push_back(videoCb);

        // Fill the combobox with possible values.
        uint32_t cbIndex = 0;
        for(const std::string& value : config.possibleValues)
        {
            CEGUI::ListboxTextItem* item = new CEGUI::ListboxTextItem(value, cbIndex);
            item->setSelectionBrushImage(selImg);
            videoCb->addItem(item);
            // Set the combobox to the current value
            if(value == config.currentValue)
            {
                videoCb->setItemSelectState(item, true);
                videoCb->setText(item->getText());
            }
            ++cbIndex;
        }
        offset += 40;
    }

    mGui.registerWindowHierarchy(mSettingsWindow);
}

bool SettingsWindow::saveConfig()
{
    Ogre::Root* ogreRoot = Ogre::Root::getSingletonPtr();
    ConfigManager& config = ConfigManager::getSingleton();

    // Save config
    // Game
    CEGUI::Editbox* usernameEb = static_cast<CEGUI::Editbox*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Game/GameSP/NicknameEdit"));
    config.setGameValue(Config::NICKNAME, usernameEb->getText().c_str());
    ODClient::getSingleton().requestNicknameChange(usernameEb->getText().c_str());

    CEGUI::Combobox* keeperVoiceCb = static_cast<CEGUI::Combobox*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Game/GameSP/KeeperVoice"));
    CEGUI::ListboxItem* keeperVoiceItem = keeperVoiceCb->getSelectedItem();
    if(keeperVoiceItem != nullptr)
        config.setGameValue(Config::KEEPERVOICE, keeperVoiceItem->getText().c_str());
    else
        config.setGameValue(Config::KEEPERVOICE, "");

    CEGUI::Combobox* minimapTypes = static_cast<CEGUI::Combobox*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Game/GameSP/MiniMapType"));
    CEGUI::ListboxItem* minimapTypesItem = minimapTypes->getSelectedItem();
    if(minimapTypesItem != nullptr)
        config.setGameValue(Config::MINIMAP_TYPE, minimapTypesItem->getText().c_str());
    else
        config.setGameValue(Config::MINIMAP_TYPE, "");

    CEGUI::Slider* lightSlider = static_cast<CEGUI::Slider*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Game/GameSP/LightSlider"));
    config.setGameValue(Config::LIGHT_FACTOR, Helper::toString(lightSlider->getCurrentValue()));

    CEGUI::Slider* panSpeedSlider = static_cast<CEGUI::Slider*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Input/InputSP/PanSpeedSlider"));
    config.setInputValue(Config::PAN_SPEED,
        Helper::toString(static_cast<int32_t>(10.0f + panSpeedSlider->getCurrentValue())));

    // Input
    CEGUI::ToggleButton* keyboardGrabCheckbox = static_cast<CEGUI::ToggleButton*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Input/InputSP/KeyboardGrabCheckbox"));
    config.setInputValue(Config::KEYBOARD_GRAB, keyboardGrabCheckbox->isSelected() ? "Yes" : "No");

    CEGUI::ToggleButton* mouseGrabCheckbox = static_cast<CEGUI::ToggleButton*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Input/InputSP/MouseGrabCheckbox"));
    config.setInputValue(Config::MOUSE_GRAB, mouseGrabCheckbox->isSelected() ? "Yes" : "No");

    CEGUI::ToggleButton* autoscrollCheckbox = static_cast<CEGUI::ToggleButton*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Input/InputSP/AutoscrollCheckbox"));
    config.setInputValue(Config::AUTOSCROLL, autoscrollCheckbox->isSelected() ? "Yes" : "No");
    
    // Audio
    CEGUI::Slider* volumeSlider = static_cast<CEGUI::Slider*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Audio/AudioSP/MusicSlider"));
    config.setAudioValue(Config::MUSIC_VOLUME, Helper::toString(volumeSlider->getCurrentValue()));

    CEGUI::Combobox* uiScaleCb = static_cast<CEGUI::Combobox*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/UIScaleCombobox"));
    CEGUI::ListboxItem* uiScaleItem = uiScaleCb->getSelectedItem();
    if(uiScaleItem != nullptr)
        config.setGameValue(Config::UI_SCALE, Helper::toString(uiScaleItem->getID()));

    CEGUI::ToggleButton* dynamicShadowsCheckBox = static_cast<CEGUI::ToggleButton*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/DynamicShadowsCheckbox"));
    config.setAudioValue(Config::SHADOWS, dynamicShadowsCheckBox->isSelected() ? "Yes" : "No");
    
    // Video
    Ogre::RenderSystem* renderer = ogreRoot->getRenderSystem();

    CEGUI::Combobox* rdrCb = static_cast<CEGUI::Combobox*>(
    mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/RendererCombobox"));
    std::string rendererName = rdrCb->getSelectedItem()->getText().c_str();
    if (rendererName != renderer->getName())
    {
        OD_LOG_ERR("Cannot replace the active renderer while the game is running: " + rendererName);
        return false;
    }

    config.setVideoValue(Config::RENDERER, renderer->getName());

    CEGUI::ToggleButton* fsCheckBox = static_cast<CEGUI::ToggleButton*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/FullscreenCheckbox"));
    CEGUI::Combobox* resCb = static_cast<CEGUI::Combobox*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/ResolutionCombobox"));
    CEGUI::ToggleButton* vsCheckBox = static_cast<CEGUI::ToggleButton*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/VSyncCheckbox"));

    std::map<std::string, std::string> selectedVideoOptions;
    selectedVideoOptions[Config::FULL_SCREEN] = fsCheckBox->isSelected() ? "Yes" : "No";
    selectedVideoOptions[Config::VIDEO_MODE] = resCb->getSelectedItem()->getText().c_str();
    selectedVideoOptions[Config::VSYNC] = vsCheckBox->isSelected() ? "Yes" : "No";
    for (CEGUI::Window* combo : mCustomVideoComboBoxes)
        selectedVideoOptions[combo->getName().c_str()] = combo->getText().c_str();

    const Ogre::ConfigOptionMap initialRendererOptions = renderer->getConfigOptions();
    std::map<std::string, std::string> previousRendererOptions;
    std::map<std::string, std::string> previousVideoConfig;
    bool resizeRenderWindow = false;
    bool recreateRenderWindow = false;
    for(const std::pair<const std::string, std::string>& selected : selectedVideoOptions)
    {
        Ogre::ConfigOptionMap::const_iterator previous = initialRendererOptions.find(selected.first);
        if(previous == initialRendererOptions.end())
            continue;
        previousRendererOptions[selected.first] = previous->second.currentValue;
        previousVideoConfig[selected.first] = config.getVideoValue(
            selected.first, previous->second.currentValue, false);
        if(previous->second.currentValue == selected.second)
            continue;
        if(selected.first == Config::FULL_SCREEN || selected.first == Config::VIDEO_MODE)
        {
            resizeRenderWindow = true;
            continue;
        }
        if(selected.first != Config::VSYNC && selected.first != "VSync Interval"
            && selected.first != "Reversed Z-Buffer"
            && selected.first != "Separate Shader Objects" && selected.first != "Debug Layer")
        {
            recreateRenderWindow = true;
        }
    }

    ODFrameListener& frameListener = ODFrameListener::getSingleton();
    try
    {
        renderer->setConfigOption(Config::FULL_SCREEN, selectedVideoOptions[Config::FULL_SCREEN]);
        renderer->setConfigOption(Config::VIDEO_MODE, selectedVideoOptions[Config::VIDEO_MODE]);
        renderer->setConfigOption(Config::VSYNC, selectedVideoOptions[Config::VSYNC]);
        for (CEGUI::Window* combo : mCustomVideoComboBoxes)
            renderer->setConfigOption(combo->getName().c_str(), combo->getText().c_str());

        for(const std::pair<const std::string, std::string>& selected : selectedVideoOptions)
            config.setVideoValue(selected.first, selected.second);
        config.saveUserConfig();

        const Ogre::SceneManager::CameraList& cameras = RenderManager::getSingleton().getSceneManager()->getCameras();
        for(const std::pair<const Ogre::String, Ogre::Camera*>& camera : cameras)
            camera.second->setAspectRatio(camera.second->getAspectRatio());

        if(recreateRenderWindow)
        {
            frameListener.requestRenderWindowRecreation(previousRendererOptions, previousVideoConfig);
        }
        else
        {
            Ogre::RenderWindow* window = frameListener.getRenderWindow();
            if(resizeRenderWindow)
            {
                const std::vector<std::string> resolution = Helper::split(
                    selectedVideoOptions[Config::VIDEO_MODE], 'x');
                if(resolution.size() != 2)
                    throw std::runtime_error("invalid video mode: " + selectedVideoOptions[Config::VIDEO_MODE]);

                const uint32_t width = static_cast<uint32_t>(Helper::toInt(resolution[0]));
                const uint32_t height = static_cast<uint32_t>(Helper::toInt(resolution[1]));
                const bool fullscreen = fsCheckBox->isSelected();
                window->setFullscreen(fullscreen, width, height);
                if(!fullscreen)
                    window->resize(width, height);
                window->windowMovedOrResized();
                frameListener.windowResized(window);
            }
            window->setVSyncInterval(static_cast<unsigned int>(Helper::toInt(
                config.getVideoValue("VSync Interval", "1", false))));
            window->setVSyncEnabled(vsCheckBox->isSelected());
        }
    }
    catch(const std::exception& error)
    {
        OD_LOG_ERR("Could not apply video settings: " + std::string(error.what()));
        std::map<std::string, std::string>::const_iterator fullscreen =
            previousRendererOptions.find(Config::FULL_SCREEN);
        if(fullscreen != previousRendererOptions.end())
            renderer->setConfigOption(fullscreen->first, fullscreen->second);
        std::map<std::string, std::string>::const_iterator videoMode =
            previousRendererOptions.find(Config::VIDEO_MODE);
        if(videoMode != previousRendererOptions.end())
            renderer->setConfigOption(videoMode->first, videoMode->second);
        for(const std::pair<const std::string, std::string>& option : previousRendererOptions)
        {
            if(option.first == Config::FULL_SCREEN || option.first == Config::VIDEO_MODE)
                continue;
            renderer->setConfigOption(option.first, option.second);
        }
        for(const std::pair<const std::string, std::string>& option : previousVideoConfig)
            config.setVideoValue(option.first, option.second);
        config.saveUserConfig();
        initConfig();
        return false;
    }
    RenderManager::getSingleton().setDynamicShadowsEnabled(dynamicShadowsCheckBox->isSelected());
    return true;
}

void SettingsWindow::show()
{
    if (mSettingsWindow)
    {
        initConfig();
        // Input only allowed on this window when visible.
        mSettingsWindow->setModalState(true);
        mSettingsWindow->show();
    }
}

void SettingsWindow::hide()
{
    if (mSettingsWindow)
    {
        mSettingsWindow->setModalState(false);
        mSettingsWindow->hide();
    }
    if (mApplyWindow)
    {
        mApplyWindow->setModalState(false);
        mApplyWindow->hide();
    }
}

bool SettingsWindow::onCancelSettings(const CEGUI::EventArgs&)
{
    initConfig();
    hide();
    return true;
}

bool SettingsWindow::onApplySettings(const CEGUI::EventArgs&)
{
    // Check the renderer change and open the pop-up if needed.
    Ogre::Root* ogreRoot = Ogre::Root::getSingletonPtr();
    Ogre::RenderSystem* currentRenderer = ogreRoot->getRenderSystem();

    // Changing Ogre renderer needs a restart to allow to load shaders and requested stuff
    CEGUI::Combobox* rdrCb = static_cast<CEGUI::Combobox*>(
    mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/RendererCombobox"));
    std::string rendererName = rdrCb->getSelectedItem()->getText().c_str();
    if (rendererName != currentRenderer->getName())
    {
        Ogre::RenderSystem* newRenderer = ogreRoot->getRenderSystemByName(rendererName);
        if (newRenderer == nullptr)
        {
            OD_LOG_ERR("No valid renderer found while searching for: "
                        + std::string(rdrCb->getSelectedItem()->getText().c_str())
                        + ". Restoring the previous value: " + std::string(currentRenderer->getName().c_str()));

            for (size_t i = 0; i < rdrCb->getItemCount(); ++i)
            {
                CEGUI::ListboxTextItem* item = static_cast<CEGUI::ListboxTextItem*>(rdrCb->getListboxItemFromIndex(i));

                if (item->getText() == std::string(currentRenderer->getName().c_str()))
                {
                    rdrCb->setItemSelectState(item, true);
                    rdrCb->setText(item->getText());
                    break;
                }
                ++i;
            }
            return true;
        }

        // If render changed, we need to restart game.
        mApplyWindow->show();
        // Input only allowed on this window when visible.
        mApplyWindow->setModalState(true);
        return true;
    }

    if(saveConfig())
        hide();
    return true;
}

bool SettingsWindow::onPopupCancelApplySettings(const CEGUI::EventArgs&)
{
    mApplyWindow->hide();
    mApplyWindow->setModalState(false);
    // Restore main settings window modal state
    mSettingsWindow->setModalState(true);
    return true;
}

bool SettingsWindow::onPopupApplySettings(const CEGUI::EventArgs&)
{
    hide();
    saveConfig();
    // Should restart right after that.
    return true;
}

bool SettingsWindow::onMusicVolumeChanged(const CEGUI::EventArgs&)
{
    CEGUI::Slider* volumeSlider = static_cast<CEGUI::Slider*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Audio/AudioSP/MusicSlider"));
    setMusicVolumeValue(volumeSlider->getCurrentValue());
    return true;
}

bool SettingsWindow::onUiScaleChanged(const CEGUI::EventArgs&)
{
    CEGUI::Combobox* uiScaleCb = static_cast<CEGUI::Combobox*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Video/VideoSP/UIScaleCombobox"));
    CEGUI::ListboxItem* selectedItem = uiScaleCb->getSelectedItem();
    if(selectedItem != nullptr)
        mGui.setUserScalePercent(static_cast<float>(selectedItem->getID()));
    return true;
}

void SettingsWindow::setMusicVolumeValue(float volume)
{
    sf::Listener::setGlobalVolume(volume);

    // Set the slider position
    CEGUI::Slider* volumeSlider = static_cast<CEGUI::Slider*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Audio/AudioSP/MusicSlider"));
    volumeSlider->setCurrentValue(volume);

    // Set the music volume text
    CEGUI::Window* volumeText = mRootWindow->getChild("SettingsWindow/MainTabControl/Audio/AudioSP/MusicText");
    volumeText->setText("Music: " + Helper::toString(static_cast<int32_t>(volume)) + "%");
}

bool SettingsWindow::onPanSpeedChanged(const CEGUI::EventArgs&)
{
    CEGUI::Slider* panSpeedSlider = static_cast<CEGUI::Slider*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Input/InputSP/PanSpeedSlider"));
    setPanSpeedValue(10.0f + panSpeedSlider->getCurrentValue());
    return true;
}

void SettingsWindow::setPanSpeedValue(float panSpeedPercent)
{
    if(panSpeedPercent < 10.0f)
        panSpeedPercent = 10.0f;
    else if(panSpeedPercent > 300.0f)
        panSpeedPercent = 300.0f;

    // Apply immediately so the effect can be felt while the window is open.
    ODFrameListener::getSingleton().getCameraManager()->setPanSpeedFactor(panSpeedPercent / 100.0f);

    // The slider itself starts at the minimum usable speed rather than zero.
    CEGUI::Slider* panSpeedSlider = static_cast<CEGUI::Slider*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Input/InputSP/PanSpeedSlider"));
    panSpeedSlider->setCurrentValue(panSpeedPercent - 10.0f);

    CEGUI::Window* panSpeedText = mRootWindow->getChild("SettingsWindow/MainTabControl/Input/InputSP/PanSpeedText");
    panSpeedText->setText("Camera pan speed: " + Helper::toString(static_cast<int32_t>(panSpeedPercent)) + "%");
}

bool SettingsWindow::onLightFactorChanged(const CEGUI::EventArgs&)
{
    CEGUI::Slider* lightSlider = static_cast<CEGUI::Slider*>(
        mRootWindow->getChild("SettingsWindow/MainTabControl/Game/GameSP/LightSlider"));
    setLightFactorValue(lightSlider->getCurrentValue());
    return true;
}


void SettingsWindow::setLightFactorValue(float lightFactor)
{
    // Apply setting to ambient light.
    RenderManager::getSingleton().setWorldAmbientLightingFactor(1.0f + (lightFactor / 100.0f));

    // Set the slider position
    CEGUI::Slider* lightSlider = static_cast<CEGUI::Slider*>(
            mRootWindow->getChild("SettingsWindow/MainTabControl/Game/GameSP/LightSlider"));
    lightSlider->setCurrentValue(lightFactor);

    // Set the light factor text
    CEGUI::Window* lightFactorText = mRootWindow->getChild("SettingsWindow/MainTabControl/Game/GameSP/LightText");
    lightFactorText->setText("Ambient Light: +" + Helper::toString(static_cast<int32_t>(lightFactor)) + "%");
}
