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

#ifndef WALLTORCHVIEW_H
#define WALLTORCHVIEW_H

#include "render/WallTorchSpot.h"

#include <OgrePrerequisites.h>
#include <OgreVector3.h>

#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

/*! \brief Shows the wall torches the server sent, client side only.
 *
 * Every torch has a bracket (the model WallTorch.mesh, a billboard if it cannot be loaded),a flickering flame, a glow and a thread of smoke (the particle systems
 * of the room ambience). Only the nearest few torches to the camera also get a warm, flickering point
 * light (no shadows); the others show flame and glow without a light source. How many depends on the
 * room ambience mode (full / reduced / off). Strength, colour, range and flicker of the light come from
 * config/rooms.cfg (keys WallTorch...); a missing key falls back to a default and is logged once.
 */
class WallTorchView
{
public:
    enum class Mode
    {
        full,
        reduced,
        off
    };

    WallTorchView();

    //! \brief Replaces the shown torches by the given list
    void setSpots(const std::vector<WallTorchSpot>& spots);

    //! \brief Called every frame. camera may be nullptr (nothing is changed then)
    void update(double timeSinceLastFrame, Mode mode, Ogre::Camera* camera);

    //! \brief Removes every particle system and light. The spots are kept
    void stopAll();

    inline uint32_t getNbLights() const
    { return mNbLights; }

private:
    struct Part
    {
        Part() :
            mNode(nullptr), mSystem(nullptr), mEntity(nullptr), mBaseWidth(1.0), mBaseHeight(1.0)
        {}

        Ogre::SceneNode* mNode;
        Ogre::ParticleSystem* mSystem;
        //! Set instead of mSystem for the bracket model
        Ogre::Entity* mEntity;
        double mBaseWidth;
        double mBaseHeight;
    };

    struct Torch
    {
        Torch() :
            mPosition(Ogre::Vector3::ZERO), mDirection(Ogre::Vector3::ZERO), mLightPosition(Ogre::Vector3::ZERO), mPhase(0.0), mDistance(0.0), mShown(false),
            mLightNode(nullptr), mLight(nullptr), mSoundHandle(0)
        {}

        Ogre::Vector3 mPosition;
        //! From the wall to the open tile
        Ogre::Vector3 mDirection;
        //! Where the light hangs: a little in front of the wall and higher than the flame
        Ogre::Vector3 mLightPosition;
        double mPhase;
        //! Distance to the camera at the last check
        double mDistance;
        bool mShown;
        //! Bracket, flame, glow, smoke (an unused part has no node)
        Part mParts[4];
        Ogre::SceneNode* mLightNode;
        Ogre::Light* mLight;
        //! Handle of the running crackling loop, 0 if none
        uint32_t mSoundHandle;
    };

    struct Settings
    {
        Settings() :
            mActiveLights(4), mActiveLightsReduced(2), mSoundLoops(4), mRange(6.0), mIntensity(1.0),
            mColourR(1.0), mColourG(0.62), mColourB(0.28), mFlickerStrength(0.25), mFlickerSpeed(2.3)
        {}

        uint32_t mActiveLights;
        uint32_t mActiveLightsReduced;
        uint32_t mSoundLoops;
        double mRange;
        double mIntensity;
        double mColourR;
        double mColourG;
        double mColourB;
        double mFlickerStrength;
        double mFlickerSpeed;
    };

    void loadSettings();
    void refresh(Mode mode, Ogre::Camera* camera);
    void createPart(Torch& torch, uint32_t index, const std::string& name);
    //! Creates the bracket model; false if the mesh cannot be loaded
    bool createModel(Torch& torch, const std::string& name);
    void destroyPart(Part& part);
    void createLight(Torch& torch, const std::string& name);
    void destroyLight(Torch& torch);
    void startSound(Torch& torch);
    void stopSound(Torch& torch);
    void animate();
    void destroyTorch(Torch& torch);

    std::map<uint64_t, Torch> mTorches;
    Settings mSettings;
    bool mSettingsLoaded;
    std::vector<std::string> mReportedKeys;
    double mClock;
    double mRefreshTimer;
    uint32_t mUniqueNumber;
    uint32_t mNbLights;
    Mode mLastMode;
    std::mt19937 mRandom;
};

#endif // WALLTORCHVIEW_H
