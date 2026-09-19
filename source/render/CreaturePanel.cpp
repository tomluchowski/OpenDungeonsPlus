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
    return higher == lower ? 0 : (higher ? 1 : -1);
}
}

CreaturePanel::CreaturePanel(GameMap& gameMap, Gui& gui, CEGUI::Window* parent) :
    mGameMap(gameMap), mGui(gui)
{
    mWindow = createWindow(parent, "DefaultWindow", "PopulationPanel", 0, 0, 1, 112);
    mWindow->setArea(CEGUI::URect(CEGUI::UDim(0, 0), CEGUI::UDim(0, 4),
        CEGUI::UDim(1, -48), CEGUI::UDim(0, 116)));
    mWindow->setUserString("AllowEdgeScrolling", "true");
    CEGUI::Window* workerBackground = createWindow(mWindow, "OD/StaticImage", "WorkerBackground",
        0, 0, 74, 108);
    workerBackground->setProperty("FrameEnabled", "False");
    workerBackground->setProperty("BackgroundEnabled", "False");
    workerBackground->setProperty("Image", "OpenDungeonsSkin/SelectionBrush");
    workerBackground->setProperty("ImageColours", "tl:FF101010 tr:FF101010 bl:FF101010 br:FF101010");
    workerBackground->setMousePassThroughEnabled(true);
    const char* workerIcons[] = {"WorkerButton", "HourglassIcon", "HammerAnvilIcon", "TrainingHallButton"};
    for(size_t i = 0; i < mWorkerCounts.size(); ++i)
    {
        CEGUI::Window* icon = createWindow(mWindow, "OD/StaticImage", "WorkerIcon" + std::to_string(i),
            2, static_cast<float>(i * 27 + 3), 20, 20);
        icon->setProperty("FrameEnabled", "False");
        icon->setProperty("BackgroundEnabled", "False");
        icon->setProperty("Image", "OpenDungeonsIcons/" + std::string(workerIcons[i]));
        icon->setMousePassThroughEnabled(true);
        CEGUI::Window* count = createWindow(mWindow, "OD/StaticText", "WorkerCount" + std::to_string(i),
            24, static_cast<float>(i * 27), 48, 26);
        prepareCount(count);
        count->setTooltipText("Workers");
        count->setUserString("ContextHelp", std::string("Workers: ") + CRITERION_NAMES[static_cast<size_t>(WORKER_CRITERIA[i])]);
        mWorkerCounts[i] = count;
        mConnections.emplace_back(count->subscribeEvent(CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber([this, i](const CEGUI::EventArgs& args)
            {
                if(static_cast<const CEGUI::MouseEventArgs&>(args).button == CEGUI::LeftButton)
                    pickUp("", WORKER_CRITERIA[i], true);
                return true;
            })));
    }

    const char* viewNames[] = {"Total", "Jobs", "Fighting", "Moods"};
    const char* viewIcons[] = {"CreaturesIcon", "HammerAnvilIcon", "FighterButton", "SeatIcon"};
    for(size_t i = 0; i < mViewButtons.size(); ++i)
    {
        CEGUI::Window* button = createWindow(mWindow, "OD/GameTabButton", "View" + std::to_string(i),
            76, static_cast<float>(i * 27), 26, 26);
        button->setProperty("NormalImage", "OpenDungeonsIcons/" + std::string(viewIcons[i]));
        button->setTooltipText(viewNames[i]);
        mViewButtons[i] = button;
        mConnections.emplace_back(button->subscribeEvent(CEGUI::PushButton::EventClicked,
            CEGUI::Event::Subscriber([this, i](const CEGUI::EventArgs&) { selectView(i); return true; })));
    }
    mPrevious = createWindow(mWindow, "OD/Button", "PreviousTypes", 106, 27, 24, 26);
    mPrevious->setText("<");
    mPrevious->setTooltipText("Previous creature types");
    mNext = createWindow(mWindow, "OD/Button", "NextTypes", 106, 56, 24, 26);
    mNext->setText(">");
    mNext->setTooltipText("Next creature types");
    mConnections.emplace_back(mPrevious->subscribeEvent(CEGUI::PushButton::EventClicked,
        CEGUI::Event::Subscriber([this](const CEGUI::EventArgs&) { scroll(-1); return true; })));
    mConnections.emplace_back(mNext->subscribeEvent(CEGUI::PushButton::EventClicked,
        CEGUI::Event::Subscriber([this](const CEGUI::EventArgs&) { scroll(1); return true; })));
    mStrip = createWindow(mWindow, "DefaultWindow", "Types", 134, 0, 1, 108);
    mStrip->setArea(CEGUI::URect(CEGUI::UDim(0, 134), CEGUI::UDim(0, 0),
        CEGUI::UDim(1, 0), CEGUI::UDim(0, 108)));
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
        static_cast<float>(index * 58), 0, 54, 108);
    slot.portrait = createWindow(slot.window, "OD/StaticImage", "Portrait", 0, 0, 54, 108);
    // Keep the original model proportions; the surrounding frame fills the column.
    slot.portrait->setProperty("HorzFormatting", "Stretched");
    slot.portrait->setProperty("VertFormatting", "Stretched");
    mConnections.emplace_back(slot.portrait->subscribeEvent(CEGUI::Window::EventMouseClick,
        CEGUI::Event::Subscriber([this, index](const CEGUI::EventArgs& args)
        {
            const auto button = static_cast<const CEGUI::MouseEventArgs&>(args).button;
            if(button == CEGUI::RightButton)
                focus(mSlots[index].type);
            else if(button == CEGUI::LeftButton && selectedLevelOrder() != 0)
                pickUp(mSlots[index].type, Criterion::Total, false, selectedLevelOrder());
            return true;
        })));
    for(size_t row = 0; row < slot.counts.size(); ++row)
    {
        CEGUI::Window* count = createWindow(slot.window, "OD/StaticText", "Count" + std::to_string(row),
            0, static_cast<float>(row * 26), 54, 26);
        prepareCount(count);
        slot.counts[row] = count;
        mConnections.emplace_back(count->subscribeEvent(CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber([this, index, row](const CEGUI::EventArgs& args)
            {
                const auto button = static_cast<const CEGUI::MouseEventArgs&>(args).button;
                if(button == CEGUI::LeftButton && row < VIEW_CRITERIA[mView].size())
                    pickUp(mSlots[index].type, VIEW_CRITERIA[mView][row], false, selectedLevelOrder());
                else if(button == CEGUI::RightButton)
                    focus(mSlots[index].type);
                return true;
            })));
    }
    mSlots.push_back(slot);
    mGui.registerWindowHierarchy(slot.window);
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
    const float scale = size.d_height / 108.0f;
    mVisibleSlots = scale > 0 ? static_cast<size_t>(std::max(0.0f, size.d_width) / (58.0f * scale)) : 0;

    std::vector<std::string> types;
    CreaturePanelCounts workers{};
    // Preserve the map's existing creature-definition order, including custom classes.
    for(unsigned int i = 0; i < mGameMap.numClassDescriptions(); ++i)
    {
        const CreatureDefinition* definition = mGameMap.getClassDescription(i);
        const auto found = mData.find(definition->getClassName());
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
        slot.portrait->setTooltipText(slot.type);
        slot.portrait->setUserString("ContextHelp", slot.type + ": right-click to locate");
        const auto& criteria = VIEW_CRITERIA[mView];
        for(size_t row = 0; row < slot.counts.size(); ++row)
        {
            CEGUI::Window* countWindow = slot.counts[row];
            countWindow->setVisible(row < criteria.size());
            countWindow->setVerticalAlignment(criteria.size() == 1 ? CEGUI::VA_CENTRE : CEGUI::VA_TOP);
            if(row >= criteria.size())
                continue;
            const auto criterion = criteria[row];
            const uint32_t count = mData.at(slot.type)[static_cast<size_t>(criterion)];
            countWindow->setText(Helper::toString(count));
            countWindow->setEnabled(count > 0);
            countWindow->setTooltipText(slot.type);
            countWindow->setUserString("ContextHelp", slot.type + ": " + CRITERION_NAMES[static_cast<size_t>(criterion)] +
                "\nLeft-click to pick up; right-click to locate");
        }
    }
}

void CreaturePanel::pickUp(const std::string& type, CreaturePanelCriterion criterion, bool workersOnly, int levelOrder)
{
    if(!ODClient::getSingleton().isConnected())
        return;
    Seat* seat = mGameMap.getLocalPlayer()->getSeat();
    Creature* selected = nullptr;
    for(Creature* creature : mGameMap.getCreaturesBySeat(seat))
    {
        const CreatureDefinition* definition = creature->getDefinition();
        if((workersOnly ? !definition->isWorker() : definition->getClassName() != type) ||
            mPendingPickups.count(creature->getName()) != 0 || !creature->tryPickup(seat) ||
            !matchesCreaturePanelCriterion(criterion, creature->getActivity(), creature->getMoodValue(), definition->isWorker()))
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
    for(Creature* creature : mGameMap.getCreaturesBySeat(mGameMap.getLocalPlayer()->getSeat()))
    {
        if(creature->getDefinition()->getClassName() != type || !creature->getIsOnMap())
            continue;
        const Ogre::Vector3& position = creature->getPosition();
        ODFrameListener::getSingleton().getCameraManager()->onMiniMapClick(Ogre::Vector2(position.x, position.y));
        break;
    }
}
