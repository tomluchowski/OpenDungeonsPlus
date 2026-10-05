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

#include "render/WallTorchView.h"

#include "render/RenderManager.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <OgreAxisAlignedBox.h>
#include <OgreCamera.h>
#include <OgreColourValue.h>
#include <OgreLight.h>
#include <OgreParticleSystem.h>
#include <OgreParticleSystemManager.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>

#include <algorithm>
#include <cmath>

namespace
{

const double TWO_PI = 6.283185307179586;
//! Seconds between two checks of what is near the camera
const double REFRESH_INTERVAL = 0.25;
//! Value that no config file holds, to find out whether a key is missing
const double MISSING_VALUE = -987654.5;
//! Distance factor of the mode "reduced"
const double REDUCED_DISTANCE_FACTOR = 0.7;
//! Distance from the camera within which a torch is shown at all
const double SHOW_DISTANCE = 16.0;
//! Torches this close to the camera are shown even when they are just out of view
const double ALWAYS_SHOWN_DISTANCE = 3.0;
//! Torches must be this close to the look point of the camera to get a real light
const double LIGHT_DISTANCE = 30.0;
//! Distance from the middle of the wall tile to the surface of the wall
const double WALL_SURFACE_OFFSET = 0.55;
//! How far in front of the wall the light hangs and how high
const double LIGHT_FRONT_OFFSET = 0.3;
const double LIGHT_HEIGHT = 1.6;

//! The parts of a torch: bracket, flame, glow, smoke
const uint32_t NB_PARTS = 4;
const char* const PART_SYSTEMS[NB_PARTS] = {"RoomAmbTorchBracket", "RoomAmbTorchFlame", "RoomAmbTorchGlow", "RoomAmbTorchSmoke"};
const double PART_HEIGHTS[NB_PARTS] = {1.25, 1.4, 1.4, 1.4};
const double PART_DISTANCES[NB_PARTS] = {16.0, 16.0, 14.0, 12.0};
//! The mode "reduced" shows only the bracket and the flame
const uint32_t NB_REDUCED_PARTS = 2;
//! Flicker of the flame and the glow (the glow flickers a little more and slower)
const double PART_FLICKER_FACTORS[NB_PARTS] = {0.0, 1.0, 1.2, 0.0};
const double PART_SPEED_FACTORS[NB_PARTS] = {0.0, 1.0, 0.74, 0.0};

uint64_t makeKey(int32_t x, int32_t y)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(y));
}

std::string keyToString(uint64_t key)
{
    return Helper::toString(key >> 32) + "_" + Helper::toString(key & 0xFFFFFFFFu);
}

Ogre::Vector3 sideToDirection(WallTorchSide side)
{
    switch(side)
    {
        case WallTorchSide::north:
            return Ogre::Vector3(0.0f, 1.0f, 0.0f);
        case WallTorchSide::south:
            return Ogre::Vector3(0.0f, -1.0f, 0.0f);
        case WallTorchSide::east:
            return Ogre::Vector3(1.0f, 0.0f, 0.0f);
        default:
            return Ogre::Vector3(-1.0f, 0.0f, 0.0f);
    }
}

//! Sorts indices by the distance stored for them
struct NearerTorch
{
    NearerTorch(const std::vector<double>& distances) :
        mDistances(distances)
    {}

    bool operator()(uint32_t a, uint32_t b) const
    { return mDistances[a] < mDistances[b]; }

    const std::vector<double>& mDistances;
};

} // namespace

WallTorchView::WallTorchView() :
    mSettingsLoaded(false),
    mClock(0.0),
    mRefreshTimer(REFRESH_INTERVAL),
    mUniqueNumber(0),
    mNbLights(0),
    mLastMode(Mode::full),
    mRandom(54321)
{
}

double WallTorchView::readValue(const std::string& name, double defaultValue)
{
    double value = ConfigManager::getSingleton().getRoomConfigDoubleOrDefault(name, MISSING_VALUE);
    if(value != MISSING_VALUE)
        return value;

    if(std::find(mReportedKeys.begin(), mReportedKeys.end(), name) == mReportedKeys.end())
    {
        mReportedKeys.push_back(name);
        OD_LOG_WRN("Wall torches: " + name + " is missing in the rooms configuration, using " + Helper::toString(defaultValue));
    }

    return defaultValue;
}

void WallTorchView::loadSettings()
{
    mSettingsLoaded = true;
    Settings defaults;
    double lights = std::max(0.0, readValue("WallTorchActiveLights", defaults.mActiveLights));
    double lightsReduced = std::max(0.0, readValue("WallTorchActiveLightsReduced", defaults.mActiveLightsReduced));
    mSettings.mActiveLights = static_cast<uint32_t>(lights);
    mSettings.mActiveLightsReduced = static_cast<uint32_t>(lightsReduced);
    mSettings.mRange = std::max(0.5, readValue("WallTorchLightRadius", defaults.mRange));
    mSettings.mIntensity = std::max(0.0, readValue("WallTorchLightIntensity", defaults.mIntensity));
    mSettings.mColourR = std::min(1.0, std::max(0.0, readValue("WallTorchLightColorR", defaults.mColourR)));
    mSettings.mColourG = std::min(1.0, std::max(0.0, readValue("WallTorchLightColorG", defaults.mColourG)));
    mSettings.mColourB = std::min(1.0, std::max(0.0, readValue("WallTorchLightColorB", defaults.mColourB)));
    mSettings.mFlickerStrength = std::min(1.0, std::max(0.0, readValue("WallTorchFlickerStrength", defaults.mFlickerStrength)));
    mSettings.mFlickerSpeed = std::max(0.0, readValue("WallTorchFlickerSpeed", defaults.mFlickerSpeed));
}

void WallTorchView::destroyTorch(Torch& torch)
{
    destroyLight(torch);
    for(uint32_t i = 0; i < NB_PARTS; ++i)
        destroyPart(torch.mParts[i]);
    torch.mShown = false;
}

void WallTorchView::setSpots(const std::vector<WallTorchSpot>& spots)
{
    // A torch that stays keeps its particle systems and light, a torch that is gone is removed
    std::map<uint64_t, Torch> kept;
    for(const WallTorchSpot& spot : spots)
    {
        uint64_t key = makeKey(spot.mX, spot.mY);
        Ogre::Vector3 direction = sideToDirection(spot.mSide);
        Ogre::Vector3 wall(static_cast<Ogre::Real>(spot.mX), static_cast<Ogre::Real>(spot.mY), 0.0f);
        Ogre::Vector3 position = wall + direction * static_cast<Ogre::Real>(WALL_SURFACE_OFFSET);

        std::map<uint64_t, Torch>::iterator it = mTorches.find(key);
        if(it != mTorches.end())
        {
            if((it->second.mPosition - position).length() < 0.01f)
            {
                kept.insert(std::make_pair(key, it->second));
                mTorches.erase(it);
                continue;
            }

            // The torch hangs on another side of the wall now: build it again
            destroyTorch(it->second);
            mTorches.erase(it);
        }

        Torch torch;
        torch.mPosition = position;
        torch.mLightPosition = wall + direction * static_cast<Ogre::Real>(WALL_SURFACE_OFFSET + LIGHT_FRONT_OFFSET);
        torch.mLightPosition.z = static_cast<Ogre::Real>(LIGHT_HEIGHT);
        std::uniform_real_distribution<double> phase(0.0, TWO_PI);
        torch.mPhase = phase(mRandom);
        kept.insert(std::make_pair(key, torch));
    }

    // What is left in the old map is gone from the list
    for(std::map<uint64_t, Torch>::iterator it = mTorches.begin(); it != mTorches.end(); ++it)
        destroyTorch(it->second);

    mTorches.swap(kept);
    mRefreshTimer = REFRESH_INTERVAL;
}

void WallTorchView::stopAll()
{
    for(std::map<uint64_t, Torch>::iterator it = mTorches.begin(); it != mTorches.end(); ++it)
        destroyTorch(it->second);

    mNbLights = 0;
    mRefreshTimer = REFRESH_INTERVAL;
}

void WallTorchView::destroyPart(Part& part)
{
    if(RenderManager::getSingletonPtr() != nullptr)
    {
        Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
        if(part.mNode != nullptr)
            part.mNode->detachAllObjects();
        if(part.mSystem != nullptr)
            sceneManager->destroyParticleSystem(part.mSystem);
        if(part.mNode != nullptr)
            sceneManager->destroySceneNode(part.mNode);
    }

    part.mNode = nullptr;
    part.mSystem = nullptr;
}

void WallTorchView::createPart(Torch& torch, uint32_t index, const std::string& name)
{
    if(Ogre::ParticleSystemManager::getSingleton().getTemplate(PART_SYSTEMS[index]) == nullptr)
    {
        if(std::find(mReportedKeys.begin(), mReportedKeys.end(), PART_SYSTEMS[index]) == mReportedKeys.end())
        {
            mReportedKeys.push_back(PART_SYSTEMS[index]);
            OD_LOG_WRN(std::string("Wall torches: unknown particle system ") + PART_SYSTEMS[index]);
        }
        return;
    }

    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    Part& part = torch.mParts[index];
    std::string partName = name + "_" + Helper::toString(index);
    Ogre::Vector3 position = torch.mPosition;
    position.z = static_cast<Ogre::Real>(PART_HEIGHTS[index]);
    part.mNode = sceneManager->getRootSceneNode()->createChildSceneNode(partName + "_node", position);
    part.mSystem = sceneManager->createParticleSystem(partName, PART_SYSTEMS[index]);
    part.mNode->attachObject(part.mSystem);
    part.mBaseWidth = part.mSystem->getDefaultWidth();
    part.mBaseHeight = part.mSystem->getDefaultHeight();
}

void WallTorchView::createLight(Torch& torch, const std::string& name)
{
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    torch.mLight = sceneManager->createLight(name);
    torch.mLight->setType(Ogre::Light::LT_POINT);
    // No shadows: a handful of lights must not cost a shadow pass each
    torch.mLight->setCastShadows(false);
    torch.mLight->setSpecularColour(Ogre::ColourValue::Black);
    // Same fall-off shape as the room lights, stretched to the configured range
    double range = mSettings.mRange;
    torch.mLight->setAttenuation(static_cast<Ogre::Real>(range), 1.0f, static_cast<Ogre::Real>(0.54 / range),
        static_cast<Ogre::Real>(1.15 / (range * range)));
    torch.mLight->setDiffuseColour(Ogre::ColourValue(static_cast<float>(mSettings.mColourR * mSettings.mIntensity),
        static_cast<float>(mSettings.mColourG * mSettings.mIntensity), static_cast<float>(mSettings.mColourB * mSettings.mIntensity)));
    torch.mLightNode = RenderManager::getSingleton().getLightSceneNode()->createChildSceneNode(name + "_node", torch.mLightPosition);
    torch.mLightNode->attachObject(torch.mLight);
}

void WallTorchView::destroyLight(Torch& torch)
{
    if((RenderManager::getSingletonPtr() != nullptr) && (torch.mLight != nullptr))
    {
        Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
        if(torch.mLightNode != nullptr)
            torch.mLightNode->detachObject(torch.mLight);
        sceneManager->destroyLight(torch.mLight);
        if(torch.mLightNode != nullptr)
            sceneManager->destroySceneNode(torch.mLightNode);
    }

    torch.mLight = nullptr;
    torch.mLightNode = nullptr;
}

void WallTorchView::update(double timeSinceLastFrame, Mode mode, Ogre::Camera* camera)
{
    if(!mSettingsLoaded)
        loadSettings();

    if(mode != mLastMode)
    {
        // Everything is rebuilt by the next check with the limits of the new mode
        mLastMode = mode;
        stopAll();
    }

    if(mode == Mode::off)
    {
        stopAll();
        return;
    }

    if((camera == nullptr) || mTorches.empty())
        return;

    mClock += timeSinceLastFrame;
    mRefreshTimer += timeSinceLastFrame;
    if(mRefreshTimer >= REFRESH_INTERVAL)
    {
        mRefreshTimer = 0.0;
        refresh(mode, camera);
    }

    animate();
}

void WallTorchView::refresh(Mode mode, Ogre::Camera* camera)
{
    Ogre::Vector3 cameraPosition = camera->getDerivedPosition();
    Ogre::Vector3 direction = camera->getDerivedDirection();
    Ogre::Vector3 lookPoint = cameraPosition;
    if(direction.z < -0.05f)
        lookPoint = cameraPosition + direction * (cameraPosition.z / -direction.z);

    double distanceFactor = 1.0;
    uint32_t nbParts = NB_PARTS;
    uint32_t nbLights = mSettings.mActiveLights;
    if(mode == Mode::reduced)
    {
        distanceFactor = REDUCED_DISTANCE_FACTOR;
        nbParts = NB_REDUCED_PARTS;
        nbLights = mSettings.mActiveLightsReduced;
    }

    // Flame and glow for what is close to the camera and in view
    std::vector<std::map<uint64_t, Torch>::iterator> shown;
    std::vector<double> lookDistances;
    for(std::map<uint64_t, Torch>::iterator it = mTorches.begin(); it != mTorches.end(); ++it)
    {
        Torch& torch = it->second;
        torch.mDistance = (torch.mPosition - cameraPosition).length();
        torch.mShown = false;
        if(torch.mDistance <= SHOW_DISTANCE * distanceFactor)
        {
            Ogre::Vector3 extent(1.5f, 1.5f, 1.5f);
            Ogre::Vector3 center = torch.mPosition;
            center.z = 1.4f;
            torch.mShown = (torch.mDistance <= ALWAYS_SHOWN_DISTANCE) ||
                camera->isVisible(Ogre::AxisAlignedBox(center - extent, center + extent));
        }

        for(uint32_t i = 0; i < NB_PARTS; ++i)
        {
            bool wanted = torch.mShown && (i < nbParts) && (torch.mDistance <= PART_DISTANCES[i] * distanceFactor);
            if(wanted && (torch.mParts[i].mNode == nullptr))
                createPart(torch, i, "WallTorch_" + keyToString(it->first) + "_" + Helper::toString(++mUniqueNumber));
            else if(!wanted && (torch.mParts[i].mNode != nullptr))
                destroyPart(torch.mParts[i]);
        }

        if(torch.mShown)
        {
            shown.push_back(it);
            lookDistances.push_back((torch.mPosition - lookPoint).length());
        }
        else
            destroyLight(torch);
    }

    // Real light only for the nearest few of them (to the point the camera looks at)
    std::vector<uint32_t> order;
    for(uint32_t i = 0; i < shown.size(); ++i)
    {
        if(lookDistances[i] <= LIGHT_DISTANCE)
            order.push_back(i);
    }
    std::sort(order.begin(), order.end(), NearerTorch(lookDistances));

    std::vector<bool> lit(shown.size(), false);
    for(uint32_t i = 0; (i < order.size()) && (i < nbLights); ++i)
        lit[order[i]] = true;

    mNbLights = 0;
    for(uint32_t i = 0; i < shown.size(); ++i)
    {
        Torch& torch = shown[i]->second;
        if(lit[i])
        {
            if(torch.mLight == nullptr)
                createLight(torch, "WallTorchLight_" + keyToString(shown[i]->first));
            ++mNbLights;
        }
        else
            destroyLight(torch);
    }
}

void WallTorchView::animate()
{
    for(std::map<uint64_t, Torch>::iterator it = mTorches.begin(); it != mTorches.end(); ++it)
    {
        Torch& torch = it->second;
        for(uint32_t i = 0; i < NB_PARTS; ++i)
        {
            Part& part = torch.mParts[i];
            if((part.mSystem == nullptr) || (PART_FLICKER_FACTORS[i] <= 0.0))
                continue;

            double rate = TWO_PI * mSettings.mFlickerSpeed * PART_SPEED_FACTORS[i] * mClock;
            double noise = 0.5 * (std::sin(rate + torch.mPhase) + std::sin(1.7 * rate + 2.0 * torch.mPhase));
            double factor = 1.0 + mSettings.mFlickerStrength * PART_FLICKER_FACTORS[i] * noise;
            part.mSystem->setDefaultDimensions(static_cast<Ogre::Real>(part.mBaseWidth * factor),
                static_cast<Ogre::Real>(part.mBaseHeight * factor));
        }

        if(torch.mLight != nullptr)
        {
            double rate = TWO_PI * mSettings.mFlickerSpeed * mClock;
            double noise = 0.5 * (std::sin(rate + torch.mPhase) + std::sin(1.7 * rate + 2.0 * torch.mPhase));
            double factor = mSettings.mIntensity * std::max(0.0, 1.0 + mSettings.mFlickerStrength * noise);
            torch.mLight->setDiffuseColour(Ogre::ColourValue(static_cast<float>(mSettings.mColourR * factor),
                static_cast<float>(mSettings.mColourG * factor), static_cast<float>(mSettings.mColourB * factor)));
        }
    }
}
