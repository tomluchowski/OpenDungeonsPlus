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

#include "renderscene/RenderSceneMenu.h"

#include "renderscene/RenderScene.h"
#include "renderscene/RenderSceneGroup.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "render/RenderManager.h"

#include <CEGUI/System.h>
#include <CEGUI/GUIContext.h>
#include <CEGUI/MouseCursor.h>
#include <CEGUI/widgets/ButtonBase.h>
#include <CEGUI/widgets/FrameWindow.h>

RenderSceneMenu::RenderSceneMenu()
{
}

RenderSceneMenu::~RenderSceneMenu()
{
    for(RenderSceneGroup* sceneGroup : mSceneGroups)
        delete sceneGroup;

    mSceneGroups.clear();
}

void RenderSceneMenu::dispatchSyncPost(const std::string& event)
{
    for(RenderSceneGroup* sceneGroup : mSceneGroups)
        sceneGroup->notifySyncPost(event);
}

void RenderSceneMenu::resetMenu(CameraManager& cameraManager, RenderManager& renderManager)
{
    for(RenderSceneGroup* sceneGroup : mSceneGroups)
        sceneGroup->reset(cameraManager, renderManager);
}

void RenderSceneMenu::freeMenu(CameraManager& cameraManager, RenderManager& renderManager)
{
    renderManager.rrSetHandPose(false, false);
    for(RenderSceneGroup* sceneGroup : mSceneGroups)
        sceneGroup->freeGroup(cameraManager, renderManager);
}

void RenderSceneMenu::updateMenu(CameraManager& cameraManager, RenderManager& renderManager,
        Ogre::Real timeSinceLastFrame)
{
    bool pointing = false;
    for(CEGUI::Window* window = CEGUI::System::getSingleton().getDefaultGUIContext().getWindowContainingMouse();
        window != nullptr; window = window->getParent())
    {
        if(window->isDisabled())
            break;
        if(CEGUI::ButtonBase* button = dynamic_cast<CEGUI::ButtonBase*>(window))
        {
            pointing = button->isHovering();
            break;
        }
        if(dynamic_cast<CEGUI::FrameWindow*>(window) != nullptr)
        {
            pointing = window->isHit(window->getGUIContext().getMouseCursor().getPosition());
            break;
        }
    }
    renderManager.rrSetHandPose(pointing, false);

    for(RenderSceneGroup* sceneGroup : mSceneGroups)
        sceneGroup->update(cameraManager, renderManager, timeSinceLastFrame);
}

void RenderSceneMenu::readSceneMenu(const std::string& fileName)
{
    if(!mSceneGroups.empty())
    {
        OD_LOG_ERR("Scene already loaded. Cannot read " + fileName);
        return;
    }

    std::stringstream defFile;
    if(!Helper::readFile(fileName, defFile,true))
    {
        OD_LOG_ERR("Couldn't read " + fileName);
        return;
    }

    std::string nextParam;
    while(true)
    {
        if(!defFile.good())
            break;

        RenderSceneGroup* group = RenderSceneGroup::load(defFile);
        if(group == nullptr)
            return;

        group->setRenderSceneListener(this);
        mSceneGroups.emplace_back(group);
    }
}
