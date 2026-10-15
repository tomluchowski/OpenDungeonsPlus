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
#include "rooms/WallTorchConfig.h"

#include <OgrePrerequisites.h>
#include <OgreVector3.h>

#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

/*! \brief Shows the wall torches the server sent, client side only.
 *
 * Every torch has a bracket (the model WallTorch.mesh, a billboard if it cannot be loaded), a flickering flame and a thread of smoke (the particle systems
 * of the room ambience). Only the nearest few torches to the camera also get a warm, flickering point
 * light (no shadows); the others show flame without a light source. How many depends on the
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

    //! The parts of a torch: bracket, flame, smoke
    static const uint32_t NB_TORCH_PARTS = 3;

    //! \brief Creates the view without any torch; the settings are read when the first torches arrive
    WallTorchView();

    //! \brief Replaces the shown torches by the given list
    void setSpots(const std::vector<WallTorchSpot>& spots);

    //! \brief Called every frame. camera may be nullptr (nothing is changed then)
    void update(double timeSinceLastFrame, Mode mode, Ogre::Camera* camera);

    //! \brief Removes every particle system and light. The spots are kept
    void stopAll();

    //! \brief Number of torches that currently cast a real light
    inline uint32_t getNbLights() const
    { return mNbLights; }

private:
    struct Part
    {
        //! \brief Creates a part that has no scene node, particle system or model yet
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
        //! \brief Creates a torch that is not shown and has no parts, light or sound yet
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
        //! Bracket, flame, smoke (an unused part has no node)
        Part mParts[NB_TORCH_PARTS];
        Ogre::SceneNode* mLightNode;
        Ogre::Light* mLight;
        //! Handle of the running crackling loop, 0 if none
        uint32_t mSoundHandle;
    };

    struct Settings
    {
        //! \brief Creates the settings with the default values of the wall torch configuration
        Settings() :
            mActiveLights(DEFAULT_TORCH_ACTIVE_LIGHTS), mActiveLightsReduced(DEFAULT_TORCH_ACTIVE_LIGHTS_REDUCED),
            mSoundLoops(DEFAULT_TORCH_SOUND_LOOPS), mRange(DEFAULT_TORCH_LIGHT_RADIUS),
            mIntensity(DEFAULT_TORCH_LIGHT_INTENSITY), mColourR(DEFAULT_TORCH_LIGHT_COLOR_R),
            mColourG(DEFAULT_TORCH_LIGHT_COLOR_G), mColourB(DEFAULT_TORCH_LIGHT_COLOR_B),
            mFlickerStrength(DEFAULT_TORCH_FLICKER_STRENGTH), mFlickerSpeed(DEFAULT_TORCH_FLICKER_SPEED)
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

    //! \brief Reads the torch settings once (WallTorchConfig)
    void loadSettings();
    //! \brief Chooses which torches are shown and which of them get a light, from their distance to the camera
    void refresh(Mode mode, Ogre::Camera* camera);
    //! \brief Creates particle system number index of the torch (flame, smoke) beside the holder
    void createPart(Torch& torch, uint32_t index, const std::string& name);
    //! Creates the bracket model; false if the mesh cannot be loaded
    bool createModel(Torch& torch, const std::string& name);
    //! \brief Removes one particle system of a torch
    void destroyPart(Part& part);
    //! \brief Gives the torch a real flickering light
    void createLight(Torch& torch, const std::string& name);
    //! \brief Removes the light of the torch
    void destroyLight(Torch& torch);
    //! \brief Starts the crackling loop at the torch
    void startSound(Torch& torch);
    //! \brief Stops the crackling loop at the torch
    void stopSound(Torch& torch);
    //! \brief Lets the lights flicker
    void animate();
    //! \brief Removes everything that belongs to the torch: parts, light and sound
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
