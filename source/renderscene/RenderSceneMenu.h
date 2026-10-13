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

#ifndef RENDERSCENEMENU_H
#define RENDERSCENEMENU_H

class CameraManager;
class RenderManager;
class RenderSceneGroup;
namespace Ogre
{
class Rectangle2D;
}

#include "renderscene/RenderScene.h"

#include <OgrePrerequisites.h>

#include <string>

//! \brief This class is basically used to store the needed scenes to display a menu
class RenderSceneMenu : public RenderSceneListener
{
public:
    // Constructors
    RenderSceneMenu();
    virtual ~RenderSceneMenu();

    void dispatchSyncPost(const std::string& event) override;

    void resetMenu(CameraManager& cameraManager, RenderManager& renderManager);
    void freeMenu(CameraManager& cameraManager, RenderManager& renderManager);
    void updateMenu(CameraManager& cameraManager, RenderManager& renderManager,
        Ogre::Real timeSinceLastFrame);
    void readSceneMenu(const std::string& fileName);

private:
    //! \brief The kinds of animated overlay drawn on top of the menu artwork. Each kind has its own animation.
    enum class AtmosphereEffectType
    {
        fog,
        fire,
        acid,
        lightning,
        ember
    };

    //! \brief One animated overlay rectangle together with the ogre objects that have to be destroyed with it.
    struct AtmosphereEffect
    {
        //! Selects the animation applied in updateAtmosphereEffect()
        AtmosphereEffectType mType;
        //! The rectangle that is drawn; owned by this effect
        Ogre::Rectangle2D* mRectangle;
        //! The scene node mRectangle is attached to
        Ogre::SceneNode* mNode;
        //! Name of the material cloned for this effect, so that its shader parameters are not shared
        std::string mMaterialName;
        //! Centre in artwork coordinates, from (0, 0) at the top left to (1, 1) at the bottom right
        Ogre::Vector2 mCenter;
        //! Width and height as a fraction of the width and height of the artwork
        Ogre::Vector2 mSize;
        //! Colour of the effect; the alpha is replaced by the animation every frame
        Ogre::ColourValue mColour;
        //! Offset in seconds added to the animation clock, so that equal effects do not move in step
        Ogre::Real mPhase;
    };

    //! \brief Creates all overlay effects for the current menu scene. Replaces effects created before.
    void createAtmosphere(RenderManager& renderManager);

    //! \brief Creates one overlay effect from a clone of baseMaterial and adds it to mAtmosphereEffects.
    void addAtmosphereEffect(AtmosphereEffectType type, const std::string& baseMaterial,
        const Ogre::Vector2& center, const Ogre::Vector2& size, const Ogre::ColourValue& colour,
        Ogre::Real phase);

    //! \brief Destroys all overlay effects and their cloned materials.
    void clearAtmosphere();

    //! \brief Advances the animation of one effect and places it on the artwork,
    //! whose visible half extent in screen space is halfWidth x halfHeight.
    void updateAtmosphereEffect(AtmosphereEffect& effect, Ogre::Real halfWidth, Ogre::Real halfHeight);

    std::vector<RenderSceneGroup*> mSceneGroups;
    RenderSceneListener* mRenderSceneListener;
    //! The overlay effects currently shown; empty outside of the menu
    std::vector<AtmosphereEffect> mAtmosphereEffects;
    //! The scene manager the effects were created in, or nullptr while there are none
    Ogre::SceneManager* mAtmosphereSceneManager = nullptr;
    //! Animation clock in seconds, advanced in updateMenu()
    Ogre::Real mAtmosphereTime = 0.0f;
};

#endif // RENDERSCENEMENU_H
