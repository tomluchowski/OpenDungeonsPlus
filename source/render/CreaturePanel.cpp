/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/CreaturePanel.h"
#include "render/CreaturePortrait.h"
#include "render/Gui.h"
#include "render/ODFrameListener.h"
#include "camera/CameraManager.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/GameEntityType.h"
#include "game/Player.h"
#include "modes/InputManager.h"
#include "gamemap/GameMap.h"
#include "network/ODClient.h"
#include "network/ClientNotification.h"
#include "utils/Helper.h"

#include <CEGUI/InputEvent.h>
#include <CEGUI/Image.h>
#include <CEGUI/Window.h>
#include <CEGUI/WindowManager.h>
#include <CEGUI/widgets/PushButton.h>
#include <algorithm>

namespace
{
using Criterion = CreaturePanelCriterion;
const std::array<std::vector<Criterion>, 4> VIEW_CRITERIA = {{
    {Criterion::Total},
    {Criterion::Idle, Criterion::Manufacturing, Criterion::Training, Criterion::OtherJobs},
    {Criterion::Fighting, Criterion::Guarding, Criterion::OtherFighting},
    {Criterion::Happy, Criterion::Unhappy, Criterion::Angry}
}};
const std::array<Criterion, 4> WORKER_CRITERIA = {{
    Criterion::Total, Criterion::Idle, Criterion::Working, Criterion::Fighting
}};
const std::array<const char*, 12> CRITERION_NAMES = {{
    "Total", "Idle", "Working", "Fighting", "Manufacturing", "Training", "Other jobs",
    "Guarding", "Other activities", "Happy", "Unhappy", "Angry"
}};

// Layout of the population panel in unscaled pixels.
const float PANEL_TOP = 4.0f;
const float PANEL_BOTTOM = 116.0f;
const float PANEL_RIGHT_MARGIN = -48.0f;
const float STRIP_HEIGHT = 108.0f;
const float STRIP_X = 134.0f;
const float WORKER_BACKGROUND_WIDTH = 74.0f;
const float WORKER_ROW_PITCH = 27.0f;
const float WORKER_ICON_X = 2.0f;
const float WORKER_ICON_Y = 3.0f;
const float WORKER_ICON_SIZE = 20.0f;
const float WORKER_COUNT_X = 24.0f;
const float WORKER_COUNT_WIDTH = 48.0f;
const float COUNT_HEIGHT = 26.0f;
const float VIEW_BUTTON_X = 76.0f;
const float VIEW_BUTTON_SIZE = 26.0f;
const float SCROLL_BUTTON_X = 106.0f;
const float SCROLL_BUTTON_WIDTH = 24.0f;
const float SCROLL_BUTTON_HEIGHT = 26.0f;
const float PREVIOUS_BUTTON_Y = 27.0f;
const float NEXT_BUTTON_Y = 56.0f;
const float SLOT_PITCH = 58.0f;
const float SLOT_WIDTH = 54.0f;

//! Window that raised the event.
CEGUI::Window* getEventWindow(const CEGUI::EventArgs& args)
{
    return static_cast<const CEGUI::WindowEventArgs&>(args).window;
}

CEGUI::MouseButton getClickedButton(const CEGUI::EventArgs& args)
{
    return static_cast<const CEGUI::MouseEventArgs&>(args).button;
}

CEGUI::Window* createWindow(CEGUI::Window* parent, const std::string& type,
    const std::string& name, float x, float y, float width, float height)
{
    CEGUI::Window* window = CEGUI::WindowManager::getSingleton().createWindow(type, name);
    parent->addChild(window);
    window->setArea(CEGUI::URect(CEGUI::UDim(0, x), CEGUI::UDim(0, y),
        CEGUI::UDim(0, x + width), CEGUI::UDim(0, y + height)));
    window->setRiseOnClickEnabled(false);
    return window;
}

void prepareCount(CEGUI::Window* window)
{
    window->setProperty("FrameEnabled", "False");
    window->setProperty("BackgroundEnabled", "False");
    window->setProperty("HorzFormatting", "CentreAligned");
    window->setProperty("VertFormatting", "CentreAligned");
    window->setText("0");
}

int selectedLevelOrder()
{
    Keyboard& keyboard = *InputManager::getSingleton().mKeyboard;
    if(!keyboard.isModifierDown(OIS::Keyboard::Ctrl))
        return 0;
    const bool higher = keyboard.isKeyDown(OIS::KC_PERIOD);
    const bool lower = keyboard.isKeyDown(OIS::KC_COMMA);
    if(higher == lower)
        return 0;
    if(higher)
        return 1;
    return -1;
}
}

CreaturePanel::CreaturePanel(GameMap& gameMap, Gui& gui, CEGUI::Window* parent) :
    mGameMap(gameMap), mGui(gui)
{
    mWindow = createWindow(parent, "DefaultWindow", "PopulationPanel", 0, 0, 1, PANEL_BOTTOM - PANEL_TOP);
    mWindow->setArea(CEGUI::URect(CEGUI::UDim(0, 0), CEGUI::UDim(0, PANEL_TOP),
        CEGUI::UDim(1, PANEL_RIGHT_MARGIN), CEGUI::UDim(0, PANEL_BOTTOM)));
    mWindow->setUserString("AllowEdgeScrolling", "true");
    CEGUI::Window* workerBackground = createWindow(mWindow, "OD/StaticImage", "WorkerBackground",
        0, 0, WORKER_BACKGROUND_WIDTH, STRIP_HEIGHT);
    workerBackground->setProperty("FrameEnabled", "False");
    workerBackground->setProperty("BackgroundEnabled", "False");
    workerBackground->setProperty("Image", "OpenDungeonsSkin/SelectionBrush");
    workerBackground->setProperty("ImageColours", "tl:FF101010 tr:FF101010 bl:FF101010 br:FF101010");
    workerBackground->setMousePassThroughEnabled(true);
    const char* workerIcons[] = {"WorkerButton", "HourglassIcon", "HammerAnvilIcon", "TrainingHallButton"};
    for(size_t i = 0; i < mWorkerCounts.size(); ++i)
    {
        CEGUI::Window* icon = createWindow(mWindow, "OD/StaticImage", "WorkerIcon" + std::to_string(i),
            WORKER_ICON_X, static_cast<float>(i) * WORKER_ROW_PITCH + WORKER_ICON_Y, WORKER_ICON_SIZE, WORKER_ICON_SIZE);
        icon->setProperty("FrameEnabled", "False");
        icon->setProperty("BackgroundEnabled", "False");
        icon->setProperty("Image", "OpenDungeonsIcons/" + std::string(workerIcons[i]));
        icon->setMousePassThroughEnabled(true);
        CEGUI::Window* count = createWindow(mWindow, "OD/StaticText", "WorkerCount" + std::to_string(i),
            WORKER_COUNT_X, static_cast<float>(i) * WORKER_ROW_PITCH, WORKER_COUNT_WIDTH, COUNT_HEIGHT);
        prepareCount(count);
        count->setTooltipText(std::string("Workers: ") + CRITERION_NAMES[static_cast<size_t>(WORKER_CRITERIA[i])]);
        count->setID(static_cast<CEGUI::uint>(i));
        mWorkerCounts[i] = count;
        mConnections.emplace_back(count->subscribeEvent(CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&CreaturePanel::onWorkerCountClicked, this)));
    }

    const char* viewNames[] = {"Total", "Jobs", "Fighting", "Moods"};
    const char* viewIcons[] = {"CreaturesIcon", "HammerAnvilIcon", "FighterButton", "SeatIcon"};
    for(size_t i = 0; i < mViewButtons.size(); ++i)
    {
        CEGUI::Window* button = createWindow(mWindow, "OD/GameTabButton", "View" + std::to_string(i),
            VIEW_BUTTON_X, static_cast<float>(i) * WORKER_ROW_PITCH, VIEW_BUTTON_SIZE, VIEW_BUTTON_SIZE);
        button->setProperty("NormalImage", "OpenDungeonsIcons/" + std::string(viewIcons[i]));
        button->setTooltipText(viewNames[i]);
        button->setID(static_cast<CEGUI::uint>(i));
        mViewButtons[i] = button;
        mConnections.emplace_back(button->subscribeEvent(CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber(&CreaturePanel::onViewButtonClicked, this)));
    }
    mPrevious = createWindow(mWindow, "OD/Button", "PreviousTypes", SCROLL_BUTTON_X, PREVIOUS_BUTTON_Y,
        SCROLL_BUTTON_WIDTH, SCROLL_BUTTON_HEIGHT);
    mPrevious->setText("<");
    mPrevious->setTooltipText("Previous creature types");
    mNext = createWindow(mWindow, "OD/Button", "NextTypes", SCROLL_BUTTON_X, NEXT_BUTTON_Y,
        SCROLL_BUTTON_WIDTH, SCROLL_BUTTON_HEIGHT);
    mNext->setText(">");
    mNext->setTooltipText("Next creature types");
    mConnections.emplace_back(mPrevious->subscribeEvent(CEGUI::PushButton::EventClicked,
        CEGUI::Event::Subscriber(&CreaturePanel::onPreviousClicked, this)));
    mConnections.emplace_back(mNext->subscribeEvent(CEGUI::PushButton::EventClicked,
        CEGUI::Event::Subscriber(&CreaturePanel::onNextClicked, this)));
    mStrip = createWindow(mWindow, "DefaultWindow", "Types", STRIP_X, 0, 1, STRIP_HEIGHT);
    mStrip->setArea(CEGUI::URect(CEGUI::UDim(0, STRIP_X), CEGUI::UDim(0, 0),
        CEGUI::UDim(1, 0), CEGUI::UDim(0, STRIP_HEIGHT)));
    mGui.registerWindowHierarchy(mWindow);
    mWindow->hide();
}

CreaturePanel::~CreaturePanel()
{
    for(CEGUI::Event::Connection& connection : mConnections)
        connection->disconnect();
    mConnections.clear();
    CEGUI::WindowManager::getSingleton().destroyWindow(mWindow);
}

void CreaturePanel::setData(const CreaturePanelData& data)
{
    mPendingPickups.clear();
    if(!mHasData || data != mData)
    {
        mData = data;
        mDirty = true;
    }
    mHasData = true;
}

void CreaturePanel::selectView(size_t view)
{
    mView = view;
    mDirty = true;
}

void CreaturePanel::scroll(int direction)
{
    if(direction < 0 && mFirstType > 0)
        --mFirstType;
    if(direction > 0 && mFirstType + mVisibleSlots < mTypeCount)
        ++mFirstType;
    mDirty = true;
}

void CreaturePanel::addSlot()
{
    const size_t index = mSlots.size();
    Slot slot;
    slot.window = createWindow(mStrip, "DefaultWindow", "Type" + std::to_string(index),
        static_cast<float>(index) * SLOT_PITCH, 0, SLOT_WIDTH, STRIP_HEIGHT);
    slot.portrait = createWindow(slot.window, "OD/StaticImage", "Portrait", 0, 0, SLOT_WIDTH, STRIP_HEIGHT);
    // Keep the original model proportions; the surrounding frame fills the column.
    slot.portrait->setProperty("HorzFormatting", "Stretched");
    slot.portrait->setProperty("VertFormatting", "Stretched");
    slot.portrait->setID(static_cast<CEGUI::uint>(index));
    mConnections.emplace_back(slot.portrait->subscribeEvent(CEGUI::Window::EventMouseClick,
        CEGUI::Event::Subscriber(&CreaturePanel::onPortraitClicked, this)));
    for(size_t row = 0; row < slot.counts.size(); ++row)
    {
        CEGUI::Window* count = createWindow(slot.window, "OD/StaticText", "Count" + std::to_string(row),
            0, static_cast<float>(row) * COUNT_HEIGHT, SLOT_WIDTH, COUNT_HEIGHT);
        prepareCount(count);
        count->setID(static_cast<CEGUI::uint>(index * SLOT_COUNT_ROWS + row));
        slot.counts[row] = count;
        mConnections.emplace_back(count->subscribeEvent(CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&CreaturePanel::onCountClicked, this)));
    }
    mSlots.push_back(slot);
    mGui.registerWindowHierarchy(slot.window);
}

bool CreaturePanel::onWorkerCountClicked(const CEGUI::EventArgs& args)
{
    if(getClickedButton(args) == CEGUI::LeftButton)
        pickUp("", WORKER_CRITERIA[getEventWindow(args)->getID()], true);
    return true;
}

bool CreaturePanel::onViewButtonClicked(const CEGUI::EventArgs& args)
{
    selectView(getEventWindow(args)->getID());
    return true;
}

bool CreaturePanel::onPreviousClicked(const CEGUI::EventArgs&)
{
    scroll(-1);
    return true;
}

bool CreaturePanel::onNextClicked(const CEGUI::EventArgs&)
{
    scroll(1);
    return true;
}

bool CreaturePanel::onPortraitClicked(const CEGUI::EventArgs& args)
{
    const size_t index = getEventWindow(args)->getID();
    const CEGUI::MouseButton button = getClickedButton(args);
    if(button == CEGUI::RightButton)
        focus(mSlots[index].type);
    else if(button == CEGUI::LeftButton && selectedLevelOrder() != 0)
        pickUp(mSlots[index].type, Criterion::Total, false, selectedLevelOrder());
    return true;
}

bool CreaturePanel::onCountClicked(const CEGUI::EventArgs& args)
{
    const size_t id = getEventWindow(args)->getID();
    const size_t index = id / SLOT_COUNT_ROWS;
    const size_t row = id % SLOT_COUNT_ROWS;
    const CEGUI::MouseButton button = getClickedButton(args);
    if(button == CEGUI::LeftButton && row < VIEW_CRITERIA[mView].size())
        pickUp(mSlots[index].type, VIEW_CRITERIA[mView][row], false, selectedLevelOrder());
    else if(button == CEGUI::RightButton)
        focus(mSlots[index].type);
    return true;
}

void CreaturePanel::update()
{
    if(!mHasData || !mWindow->getParent()->isVisible())
        return;
    mWindow->show();
    const CEGUI::Sizef size = mStrip->getPixelSize();
    if(!mDirty && size == mLastSize)
        return;
    mLastSize = size;
    mDirty = false;
    const float scale = size.d_height / STRIP_HEIGHT;
    mVisibleSlots = scale > 0 ? static_cast<size_t>(std::max(0.0f, size.d_width) / (SLOT_PITCH * scale)) : 0;

    std::vector<std::string> types;
    CreaturePanelCounts workers{};
    // Preserve the map's existing creature-definition order, including custom classes.
    for(unsigned int i = 0; i < mGameMap.numClassDescriptions(); ++i)
    {
        const CreatureDefinition* definition = mGameMap.getClassDescription(i);
        const CreaturePanelData::iterator found = mData.find(definition->getClassName());
        if(found == mData.end())
            continue;
        if(definition->isWorker())
        {
            for(size_t c = 0; c < workers.size(); ++c)
                workers[c] += found->second[c];
        }
        else
            types.push_back(found->first);
    }
    mTypeCount = types.size();
    mVisibleSlots = std::min(mVisibleSlots, mTypeCount);
    mFirstType = std::min(mFirstType, mTypeCount - mVisibleSlots);
    while(mSlots.size() < mVisibleSlots)
        addSlot();
    for(size_t i = 0; i < mWorkerCounts.size(); ++i)
    {
        const uint32_t count = workers[static_cast<size_t>(WORKER_CRITERIA[i])];
        mWorkerCounts[i]->setText(Helper::toString(count));
        mWorkerCounts[i]->setEnabled(count > 0);
    }
    for(size_t i = 0; i < mViewButtons.size(); ++i)
        mViewButtons[i]->setProperty("SelectionColour", i == mView ? "80FFD060" : "00FFFFFF");
    mPrevious->setEnabled(mFirstType > 0 && mVisibleSlots > 0);
    mNext->setEnabled(mFirstType + mVisibleSlots < mTypeCount && mVisibleSlots > 0);
    for(size_t i = 0; i < mSlots.size(); ++i)
    {
        Slot& slot = mSlots[i];
        slot.window->setVisible(i < mVisibleSlots);
        if(i >= mVisibleSlots)
            continue;
        slot.type = types[mFirstType + i];
        const CreatureDefinition* definition = mGameMap.getClassDescription(slot.type);
        slot.portrait->setProperty("Image", getCreaturePanelPortraitImage(definition->getMeshName()).getName());
        slot.portrait->setTooltipText(slot.type + ": right-click to locate");
        const std::vector<Criterion>& criteria = VIEW_CRITERIA[mView];
        for(size_t row = 0; row < slot.counts.size(); ++row)
        {
            CEGUI::Window* countWindow = slot.counts[row];
            countWindow->setVisible(row < criteria.size());
            countWindow->setVerticalAlignment(criteria.size() == 1 ? CEGUI::VA_CENTRE : CEGUI::VA_TOP);
            if(row >= criteria.size())
                continue;
            const Criterion criterion = criteria[row];
            const uint32_t count = mData.at(slot.type)[static_cast<size_t>(criterion)];
            countWindow->setText(Helper::toString(count));
            countWindow->setEnabled(count > 0);
            countWindow->setTooltipText(slot.type + ": " + CRITERION_NAMES[static_cast<size_t>(criterion)] +
                "\nLeft-click to pick up; right-click to locate");
        }
    }
}

void CreaturePanel::pickUp(const std::string& type, CreaturePanelCriterion criterion, bool workersOnly, int levelOrder)
{
    if(!ODClient::getSingleton().isConnected() || mGameMap.getLocalPlayer() == nullptr)
        return;
    Seat* seat = mGameMap.getLocalPlayer()->getSeat();
    Creature* selected = nullptr;
    for(Creature* creature : mGameMap.getCreaturesBySeat(seat))
    {
        const CreatureDefinition* definition = creature->getDefinition();
        const bool isWantedType = workersOnly ? definition->isWorker() : definition->getClassName() == type;
        if(!isWantedType || mPendingPickups.count(creature->getName()) != 0 || !creature->tryPickup(seat))
            continue;
        if(!matchesCreaturePanelCriterion(criterion, creature->getActivity(), creature->getMoodValue(),
            definition->isWorker()))
            continue;
        if(selected == nullptr || (levelOrder > 0 && creature->getLevel() > selected->getLevel()) ||
            (levelOrder < 0 && creature->getLevel() < selected->getLevel()))
            selected = creature;
        if(levelOrder == 0)
            break;
    }
    if(selected == nullptr)
        return;
    mPendingPickups.insert(selected->getName());
    ODClient::getSingleton().queueClientNotification(ClientNotificationType::askEntityPickUp,
        selected->getObjectType(), selected->getName());
}

void CreaturePanel::focus(const std::string& type)
{
    if(mGameMap.getLocalPlayer() == nullptr)
        return;
    for(Creature* creature : mGameMap.getCreaturesBySeat(mGameMap.getLocalPlayer()->getSeat()))
    {
        if(creature->getDefinition()->getClassName() != type || !creature->getIsOnMap())
            continue;
        const Ogre::Vector3& position = creature->getPosition();
        ODFrameListener::getSingleton().getCameraManager()->onMiniMapClick(Ogre::Vector2(position.x, position.y));
        break;
    }
}
