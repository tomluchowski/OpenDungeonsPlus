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

#include "camera/CameraManager.h"
#include "render/RenderManager.h"
#include "renderscene/RenderScene.h"
#include "renderscene/RenderSceneGroup.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <OgreMaterialManager.h>
#include <OgreRectangle2D.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreTextureManager.h>
#include <OgreViewport.h>

#include <CEGUI/System.h>
#include <CEGUI/GUIContext.h>
#include <CEGUI/MouseCursor.h>
#include <CEGUI/widgets/ButtonBase.h>
#include <CEGUI/widgets/FrameWindow.h>

#include <algorithm>
#include <cmath>

RenderSceneMenu::RenderSceneMenu()
{
}

RenderSceneMenu::~RenderSceneMenu()
{
    clearAtmosphere();
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
    Ogre::Rectangle2D* background = static_cast<Ogre::Rectangle2D*>(
        renderManager.getSceneManager()->getSceneNode("Background")->getAttachedObject(0));
    Ogre::TextureManager::getSingleton().load("MainMenuBackground.png", "Graphics");
    background->setMaterial(Ogre::MaterialManager::getSingleton().getByName("MainMenuBackground", "Graphics"));
    createAtmosphere(renderManager);
    updateMenu(cameraManager, renderManager, 0.0f);
}

void RenderSceneMenu::freeMenu(CameraManager& cameraManager, RenderManager& renderManager)
{
    renderManager.rrSetHandPose(false, false);
    clearAtmosphere();
    Ogre::Rectangle2D* background = static_cast<Ogre::Rectangle2D*>(
        renderManager.getSceneManager()->getSceneNode("Background")->getAttachedObject(0));
    background->setMaterial(Ogre::MaterialManager::getSingleton().getByName("Background", "Graphics"));
    background->setCorners(-1.0f, 1.0f, 1.0f, -1.0f);
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

    Ogre::Viewport* viewport = cameraManager.getViewport();
    if(viewport->getActualWidth() == 0 || viewport->getActualHeight() == 0)
        return;
    const Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().getByName("MainMenuBackground.png", "Graphics");
    const Ogre::Real imageAspect = static_cast<Ogre::Real>(texture->getWidth()) / texture->getHeight();
    const Ogre::Real viewportAspect = static_cast<Ogre::Real>(viewport->getActualWidth()) / viewport->getActualHeight();
    const Ogre::Real halfWidth = std::min(1.0f, imageAspect / viewportAspect);
    const Ogre::Real halfHeight = std::min(1.0f, viewportAspect / imageAspect);
    Ogre::Rectangle2D* background = static_cast<Ogre::Rectangle2D*>(
        renderManager.getSceneManager()->getSceneNode("Background")->getAttachedObject(0));
    background->setCorners(-halfWidth, halfHeight, halfWidth, -halfHeight);

    mAtmosphereTime += std::min(timeSinceLastFrame, 0.1f);
    for(AtmosphereEffect& effect : mAtmosphereEffects)
    {
        Ogre::Real alpha = 0.0f;
        Ogre::Real scaleX = 1.0f;
        Ogre::Real scaleY = 1.0f;
        Ogre::Real driftX = 0.0f;
        Ogre::Real driftY = 0.0f;
        const Ogre::Real time = mAtmosphereTime + effect.mPhase;
        switch(effect.mType)
        {
            case AtmosphereEffectType::fog:
            {
                alpha = 0.19f + 0.035f * Ogre::Math::Sin(time * 0.24f);
                driftX = 0.009f * Ogre::Math::Sin(time * 0.11f);
                driftY = 0.003f * Ogre::Math::Sin(time * 0.16f);
                break;
            }
            case AtmosphereEffectType::fire:
            {
                const Ogre::Real flicker = 0.78f +
                    0.15f * Ogre::Math::Sin(time * 8.0f) +
                    0.07f * Ogre::Math::Sin(time * 13.0f);
                alpha = 0.48f * flicker;
                scaleX = 0.94f + 0.06f * flicker;
                scaleY = 0.90f + 0.14f * flicker;
                driftY = 0.003f * Ogre::Math::Sin(time * 11.0f);
                break;
            }
            case AtmosphereEffectType::acid:
                alpha = 0.11f + 0.04f * Ogre::Math::Sin(time * 1.4f);
                scaleX = 1.0f + 0.05f * Ogre::Math::Sin(time * 0.9f);
                scaleY = 1.0f + 0.04f * Ogre::Math::Sin(time * 1.1f);
                break;
            case AtmosphereEffectType::lightning:
            {
                const Ogre::Real cycle = std::fmod(time, 6.5f);
                Ogre::Real flash = 0.0f;
                if(cycle < 0.12f)
                    flash = 1.0f - cycle / 0.12f;
                else if(cycle >= 0.22f && cycle < 0.30f)
                    flash = 0.7f * (1.0f - (cycle - 0.22f) / 0.08f);
                alpha = 0.025f + 0.70f * flash;
                scaleX = 0.9f + 0.15f * flash;
                scaleY = 1.0f + 0.08f * flash;
                break;
            }
            case AtmosphereEffectType::ember:
            {
                const Ogre::Real lifetime = 2.4f + 0.35f * effect.mPhase;
                const Ogre::Real progress = std::fmod(time, lifetime) / lifetime;
                alpha = 0.85f * Ogre::Math::Sin(Ogre::Math::PI * progress);
                driftX = 0.011f * Ogre::Math::Sin(time * 1.4f + effect.mPhase)
                    * progress;
                driftY = -0.09f * progress;
                scaleX = 1.0f - 0.5f * progress;
                scaleY = scaleX;
                break;
            }
        }

        const Ogre::Real centerX = -halfWidth +
            2.0f * halfWidth * (effect.mCenter.x + driftX);
        const Ogre::Real centerY = halfHeight -
            2.0f * halfHeight * (effect.mCenter.y + driftY);
        const Ogre::Real effectHalfWidth = halfWidth * effect.mSize.x * scaleX;
        const Ogre::Real effectHalfHeight = halfHeight * effect.mSize.y * scaleY;
        effect.mRectangle->setCorners(centerX - effectHalfWidth,
            centerY + effectHalfHeight, centerX + effectHalfWidth,
            centerY - effectHalfHeight);

        Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().getByName(
            effect.mMaterialName, "Graphics");
        Ogre::ColourValue colour = effect.mColour;
        colour.a = alpha;
        Ogre::GpuProgramParametersSharedPtr parameters =
            material->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
        parameters->setNamedConstant("tint", colour);
        parameters->setNamedConstant("time", time);
    }
}

void RenderSceneMenu::createAtmosphere(RenderManager& renderManager)
{
    clearAtmosphere();
    mAtmosphereSceneManager = renderManager.getSceneManager();
    mAtmosphereTime = 0.0f;

    const auto addEffect = [&](AtmosphereEffectType type,
        const std::string& baseMaterial, const Ogre::Vector2& center,
        const Ogre::Vector2& size, const Ogre::ColourValue& colour,
        Ogre::Real phase)
    {
        const std::string suffix = Helper::toString(mAtmosphereEffects.size());
        const std::string objectName = "MainMenuAtmosphere_" + suffix;
        const std::string materialName = objectName + "_material";
        Ogre::MaterialPtr base = Ogre::MaterialManager::getSingleton().getByName(
            baseMaterial, "Graphics");
        Ogre::MaterialPtr material = base->clone(materialName, true, "Graphics");
        Ogre::Rectangle2D* rectangle = new Ogre::Rectangle2D(objectName, true);
        rectangle->setMaterial(material);
        rectangle->setRenderQueueGroupAndPriority(
            Ogre::RENDER_QUEUE_SKIES_EARLY, 101);
        rectangle->setBoundingBox(Ogre::AxisAlignedBox::BOX_INFINITE);
        Ogre::SceneNode* node = mAtmosphereSceneManager->getRootSceneNode()
            ->createChildSceneNode(objectName + "_node");
        node->attachObject(rectangle);
        mAtmosphereEffects.push_back({type, rectangle, node, materialName,
            center, size, colour, phase});
    };

    addEffect(AtmosphereEffectType::fire, "MainMenuAtmosphereGlow",
        Ogre::Vector2(0.082f, 0.485f), Ogre::Vector2(0.105f, 0.16f),
        Ogre::ColourValue(1.0f, 0.24f, 0.025f), 0.3f);
    addEffect(AtmosphereEffectType::fire, "MainMenuAtmosphereGlow",
        Ogre::Vector2(0.918f, 0.485f), Ogre::Vector2(0.105f, 0.16f),
        Ogre::ColourValue(1.0f, 0.24f, 0.025f), 1.7f);
    addEffect(AtmosphereEffectType::fire, "MainMenuAtmosphereGlow",
        Ogre::Vector2(0.302f, 0.145f), Ogre::Vector2(0.065f, 0.095f),
        Ogre::ColourValue(1.0f, 0.32f, 0.035f), 2.4f);
    addEffect(AtmosphereEffectType::fire, "MainMenuAtmosphereGlow",
        Ogre::Vector2(0.698f, 0.145f), Ogre::Vector2(0.065f, 0.095f),
        Ogre::ColourValue(1.0f, 0.32f, 0.035f), 3.6f);
    addEffect(AtmosphereEffectType::acid, "MainMenuAtmosphereGlow",
        Ogre::Vector2(0.505f, 0.385f), Ogre::Vector2(0.15f, 0.14f),
        Ogre::ColourValue(0.08f, 0.95f, 0.22f), 1.2f);
    addEffect(AtmosphereEffectType::acid, "MainMenuAtmosphereGlow",
        Ogre::Vector2(0.835f, 0.845f), Ogre::Vector2(0.16f, 0.08f),
        Ogre::ColourValue(0.04f, 0.8f, 0.16f), 3.1f);
    addEffect(AtmosphereEffectType::lightning, "MainMenuAtmosphereGlow",
        Ogre::Vector2(0.879f, 0.105f), Ogre::Vector2(0.075f, 0.19f),
        Ogre::ColourValue(0.16f, 1.0f, 0.32f), 0.0f);
    addEffect(AtmosphereEffectType::fog, "MainMenuAtmosphereFog",
        Ogre::Vector2(0.21f, 0.73f), Ogre::Vector2(0.39f, 0.23f),
        Ogre::ColourValue(0.38f, 0.40f, 0.42f), 0.4f);
    addEffect(AtmosphereEffectType::fog, "MainMenuAtmosphereFog",
        Ogre::Vector2(0.79f, 0.73f), Ogre::Vector2(0.39f, 0.23f),
        Ogre::ColourValue(0.32f, 0.40f, 0.37f), 12.6f);
    addEffect(AtmosphereEffectType::fog, "MainMenuAtmosphereFog",
        Ogre::Vector2(0.50f, 0.91f), Ogre::Vector2(0.82f, 0.14f),
        Ogre::ColourValue(0.31f, 0.35f, 0.36f), 24.8f);

    for(const Ogre::Vector2& fire : {Ogre::Vector2(0.082f, 0.485f),
        Ogre::Vector2(0.918f, 0.485f), Ogre::Vector2(0.302f, 0.145f),
        Ogre::Vector2(0.698f, 0.145f)})
    {
        for(unsigned int ember = 0; ember < 6; ++ember)
        {
            addEffect(AtmosphereEffectType::ember, "MainMenuAtmosphereGlow",
                fire, Ogre::Vector2(0.0025f, 0.006f),
                Ogre::ColourValue(1.0f, 0.38f, 0.055f),
                ember * 0.67f + fire.x * 3.0f);
        }
    }
}

void RenderSceneMenu::clearAtmosphere()
{
    if(mAtmosphereSceneManager == nullptr)
        return;

    for(AtmosphereEffect& effect : mAtmosphereEffects)
    {
        effect.mNode->detachObject(effect.mRectangle);
        delete effect.mRectangle;
        mAtmosphereSceneManager->destroySceneNode(effect.mNode);
        Ogre::MaterialManager::getSingleton().remove(
            effect.mMaterialName, "Graphics");
    }
    mAtmosphereEffects.clear();
    mAtmosphereSceneManager = nullptr;
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
