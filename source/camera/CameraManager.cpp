/*!
 * \file   CameraManager.h
 * \date:  02 July 2011
 * \author StefanP.MUC
 * \brief  Handles the camera movements
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

#include "camera/CameraManager.h"

#include "gamemap/TileContainer.h"
#include "sound/SoundEffectsManager.h"
#include "camera/CullingManager.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "gamemap/GameMap.h"

#include <OgreCamera.h>
#include <OgreMaterialManager.h>
#include <OgrePrerequisites.h>
#include <OgreRectangle2D.h>
#include <OgreRenderWindow.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreViewport.h>

#include <algorithm>
#include <cmath>
#include <sstream>

//! The camera moving speed factor on Z axis.
const Ogre::Real ZOOM_SPEED = 4.0;

//! Camera speed when clicking on the minimap or pushing the home key.
const Ogre::Real FLIGHT_SPEED = 70.0;

//! Camera rotation speed in degrees.
const Ogre::Degree ROTATION_SPEED = Ogre::Degree(90);

//! Default orientation on the X Axis
const Ogre::Real DEFAULT_X_AXIS_VIEW = 25.0;

const Ogre::String BACKGROUND_RECT_NAME = "BackgroundRect";

CameraManager::CameraManager(Ogre::SceneManager* sceneManager, GameMap* gm, Ogre::RenderWindow* renderWindow) :
    mCircleMode(false),
    mCatmullSplineMode(false),
    mFirstIter(true),
    mRadius(0.0),
    mCenterX(0),
    mCenterY(0),
    mAlpha(0.0),
    mActiveCamera(nullptr),
    mActiveCameraNode(nullptr),
    mGameMap(gm),
    mCameraIsFlying(false),
    mCameraFlightDestination(Ogre::Vector3(0.0, 0.0, 0.0)),
    mCameraFlightDistance(Ogre::Math::POS_INFINITY),
    mCameraIsRotating(false),
    mCameraPitchDestination(0.0),
    mCameraRollDestination(0.0),
    mCurrentDefaultViewMode(ViewModes::defaultView),
    mZChange(0.0),
    mMoveSpeed(1.0),
    mMoveSpeedAcceleration(2.0),
    mPanSpeedFactor(1.0),
    mSwivelDegrees(0.0),
    mTranslateVector(Ogre::Vector3(0.0, 0.0, 0.0)),
    mTranslateVectorAccel(Ogre::Vector3(0.0, 0.0, 0.0)),
    mTranslateMaxSpeedFactor(Ogre::Vector2(1.0, 1.0)),
    mRotateLocalVector(Ogre::Vector3(0.0, 0.0, 0.0)),
    mSceneManager(sceneManager),
    mViewport(nullptr)
{
    const std::string panSpeedStr = ConfigManager::getSingleton().getInputValue(Config::PAN_SPEED, "100", false);
    float panSpeedPercent = panSpeedStr.empty() ? 100.0f : Helper::toFloat(panSpeedStr);
    if(panSpeedPercent < 10.0f)
        panSpeedPercent = 10.0f;
    else if(panSpeedPercent > 300.0f)
        panSpeedPercent = 300.0f;
    mPanSpeedFactor = panSpeedPercent / 100.0f;

    createViewport(renderWindow);
    createCamera("RTS", 0.02, 300.0);
    createCameraNode("RTS");

    setActiveCamera("RTS");
    setActiveCameraNode("RTS");



    // Create background rectangle covering the whole screen
    Ogre::Rectangle2D* rect = new Ogre::Rectangle2D(BACKGROUND_RECT_NAME, true);
    rect->setCorners(-1.0, 1.0, 1.0, -1.0);
#if defined(OGRE_VERSION) && OGRE_VERSION < 0x10A00
    // setMaterial(const String&) is replaced by setMaterial(MaterialPtr&) in Ogre 1.10,
    // and eventually dropped in Ogre 1.11.
    rect->setMaterial("Background");
#else
    Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName("Background");
    rect->setMaterial(mat);
#endif
    // Render the background before everything else
    rect->setRenderQueueGroup(Ogre::RenderQueueGroupID::RENDER_QUEUE_SKIES_EARLY);
    // Set the bounding box to something big
    rect->setBoundingBox(Ogre::AxisAlignedBox(-100000.0*Ogre::Vector3::UNIT_SCALE, 100000.0*Ogre::Vector3::UNIT_SCALE));

    // Attach background to the scene
    Ogre::SceneNode* node = mSceneManager->getRootSceneNode()->createChildSceneNode("Background");
    node->attachObject(rect);
    node->showBoundingBox(true);
    OD_LOG_INF("Created camera manager");
}

CameraManager::~CameraManager()
{
    // Delete the rectangle manually as it's not deleted automatically.
    // Surely there should be a better way of handling this?
    Ogre::SceneNode* node = mSceneManager->getSceneNode("Background");
    Ogre::MovableObject* rect = node->detachObject(static_cast<unsigned short>(0));
    OD_ASSERT_TRUE(rect->getName() == BACKGROUND_RECT_NAME);
    delete rect;
}

void CameraManager::createCamera(const Ogre::String& ss, double nearClip, double farClip)
{
    Ogre::Camera* tmpCamera = mSceneManager->createCamera(ss);
    tmpCamera->setNearClipDistance(static_cast<Ogre::Real>(nearClip));
    tmpCamera->setFarClipDistance(static_cast<Ogre::Real>(farClip));

    mRegisteredCameraNames.insert(ss);
    OD_LOG_INF("Creating " + ss + " camera...");
}

void CameraManager::destroyCamera(const Ogre::String& ss)
{
    mRegisteredCameraNames.erase(ss);
    mSceneManager->destroyCamera(ss);
    OD_LOG_INF("Destroying " + ss + "camera ...");
}

void CameraManager::createCameraNode(const std::string& name)
{
    Ogre::SceneNode* node = mSceneManager->getRootSceneNode()->createChildSceneNode(name + "_node");
    OD_ASSERT_TRUE(node);
    // Get camera (throws if camera does not exist).
    Ogre::Camera* tmpCamera = getCamera(name);
    Ogre::SceneNode* node2 = node->createChildSceneNode(name + "_node2");
    OD_ASSERT_TRUE(node2);
    node2->attachObject(tmpCamera);
    Ogre::SceneNode* cameraTargetNode =
            mSceneManager->getRootSceneNode()->createChildSceneNode("CameraTarget_" + name);
    OD_ASSERT_TRUE(cameraTargetNode);
    node2->setAutoTracking(false,
                           cameraTargetNode,
                           Ogre::Vector3(static_cast<Ogre::Real>(mGameMap->getMapSizeX() / 2),
                                         static_cast<Ogre::Real>(mGameMap->getMapSizeY() / 2),
                                         static_cast<Ogre::Real>(0)));

    mRegisteredCameraNodeNames.insert(name);

    OD_LOG_INF("Creating " + name + "_node camera node...");
}

void CameraManager::destroyCameraNode(const std::string& name)
{
    mRegisteredCameraNodeNames.erase(name);
    Ogre::SceneNode* node = static_cast<Ogre::SceneNode*>(mSceneManager->getRootSceneNode()->getChild(name + "_node"));
    mSceneManager->destroySceneNode(name + "_node2");
    mSceneManager->destroySceneNode(name + "_node");
    mSceneManager->destroySceneNode("CameraTarget_" + name);

    OD_LOG_INF("Destroying " + name + "_node camera node...");
}



void CameraManager::setNextDefaultView()
{
    switch(mCurrentDefaultViewMode)
    {
    case ViewModes::defaultView:
        setDefaultIsometricView();
        break;
    case ViewModes::isometricView:
        setDefaultOrthogonalView();
        break;
    case ViewModes::orthogonalView:
    default:
        setDefaultView();
        break;
    }
}

void CameraManager::setDefaultView()
{
    RotateTo(DEFAULT_X_AXIS_VIEW, 0.0);
    mCurrentDefaultViewMode = ViewModes::defaultView;
}

void CameraManager::setDefaultIsometricView()
{
    RotateTo(40.0, 45.0);
    mCurrentDefaultViewMode = ViewModes::isometricView;
}

void CameraManager::setDefaultOrthogonalView()
{
    RotateTo(0.0, 0.0);
    mCurrentDefaultViewMode = ViewModes::orthogonalView;
}

void CameraManager::RotateTo(Ogre::Real pitch, Ogre::Real roll)
{
    move(fullStop);
    const Ogre::Real currentPitch = getActiveCameraNode()->getChild(0)->getOrientation().getPitch().valueDegrees();
    setViewOrientation(getActiveCameraNode()->getOrientation(),
        Ogre::Quaternion(Ogre::Degree(std::max(0.0f, std::min(50.0f, currentPitch))), Ogre::Vector3::UNIT_X));
    mCameraIsRotating = true;
    mCameraPitchDestination = pitch;
    mCameraRollDestination = roll;
}

void CameraManager::createViewport(Ogre::RenderWindow* renderWindow)
{
    mViewport = renderWindow->addViewport(nullptr);
    mViewport->setBackgroundColour(Ogre::ColourValue(0, 0, 0));

// TODO: Update registered camera if needed.
//    for(set<string>::iterator m_itr = mRegisteredCameraNames.begin(); m_itr != mRegisteredCameraNames.end() ; ++m_itr){
//        Ogre::Camera* tmpCamera = getCamera(*m_itr);
//    }
    OD_LOG_INF("Creating viewport...");
}

void CameraManager::setRenderWindow(Ogre::RenderWindow* renderWindow)
{
    Ogre::Viewport* previousViewport = mViewport;
    Ogre::RenderTarget* previousTarget = previousViewport->getTarget();
    Ogre::Viewport* viewport = renderWindow->addViewport(mActiveCamera,
        previousViewport->getZOrder(), previousViewport->getLeft(), previousViewport->getTop(),
        previousViewport->getWidth(), previousViewport->getHeight());
    viewport->setBackgroundColour(previousViewport->getBackgroundColour());
    viewport->setClearEveryFrame(previousViewport->getClearEveryFrame(), previousViewport->getClearBuffers());
    viewport->setMaterialScheme(previousViewport->getMaterialScheme());
    viewport->setOverlaysEnabled(previousViewport->getOverlaysEnabled());
    viewport->setSkiesEnabled(previousViewport->getSkiesEnabled());
    viewport->setShadowsEnabled(previousViewport->getShadowsEnabled());
    viewport->setVisibilityMask(previousViewport->getVisibilityMask());

    mViewport = viewport;
    previousTarget->removeViewport(previousViewport->getZOrder());
    if(viewport->getActualHeight() > 0)
    {
        mActiveCamera->setAspectRatio(static_cast<Ogre::Real>(viewport->getActualWidth())
            / static_cast<Ogre::Real>(viewport->getActualHeight()));
    }
}

const Ogre::Vector3& CameraManager::getActiveCameraPosition() const
{
    return getActiveCameraNode()->getPosition();
}

const Ogre::Quaternion& CameraManager::getActiveCameraOrientation() const
{
    return getActiveCameraNode()->getOrientation();
}

Ogre::SceneNode* CameraManager::setActiveCameraNode(const Ogre::String& ss)
{
    OD_LOG_INF("Setting active camera node to " + ss + "_node ...");

    return mActiveCameraNode = mSceneManager->getSceneNode(ss + "_node");
}

Ogre::Camera* CameraManager::getCamera(const Ogre::String& ss)
{
    return mSceneManager->getCamera(ss);
}

void CameraManager::setActiveCamera(const Ogre::String& ss)
{
    mActiveCamera = mSceneManager->getCamera(ss);
    mViewport->setCamera(mActiveCamera);
    mActiveCamera->setAspectRatio(Ogre::Real(mViewport->getActualWidth()) / Ogre::Real(mViewport->getActualHeight()));

    OD_LOG_INF("Setting Active Camera to " + ss + " ...");
}

void CameraManager::updateCameraFrameTime(const Ogre::Real frameTime)
{
    if (!isCameraMovingAtAll())
        return;

    if(frameTime <= 0.0f)
        return;
    mMoveSpeed = getActiveCameraNode()->getPosition().z / 16.0f * mPanSpeedFactor * mFastPanFactor;
    mMoveSpeedAcceleration = 2.0f * mMoveSpeed;
    
    // Carry out the acceleration/deceleration calculations on the camera translation.
    Ogre::Real speed = mTranslateVector.normalise();
    mTranslateVector *= static_cast<Ogre::Real>(std::max(0.0f, speed - (0.75f + (speed / mMoveSpeed))
                        * mMoveSpeedAcceleration * frameTime));
    mTranslateVector += mTranslateVectorAccel * static_cast<Ogre::Real>(frameTime * 2.0f);

    const Ogre::Real maxMoveSpeedX = mMoveSpeed * mTranslateMaxSpeedFactor.x;
    if(mTranslateVector.x > maxMoveSpeedX)
        mTranslateVector.x = maxMoveSpeedX;
    else if(mTranslateVector.x < -maxMoveSpeedX)
        mTranslateVector.x = -maxMoveSpeedX;

    const Ogre::Real maxMoveSpeedY = mMoveSpeed * mTranslateMaxSpeedFactor.y;
    if(mTranslateVector.y > maxMoveSpeedY)
        mTranslateVector.y = maxMoveSpeedY;
    else if(mTranslateVector.y < -maxMoveSpeedY)
        mTranslateVector.y = -maxMoveSpeedY;

    // If we have sped up to more than the maximum moveSpeed then rescale the
    // vector to that length. We use the squaredLength() in this calculation
    // since squaring the RHS is faster than sqrt'ing the LHS.
    if (mTranslateVector.squaredLength() > mMoveSpeed * mMoveSpeed)
    {
        speed = mTranslateVector.length();
        mTranslateVector *= mMoveSpeed / speed;
    }

    // Get the camera's current position.
    Ogre::Vector3 newPosition =  getActiveCameraNode()->getPosition();

    Ogre::Vector3 viewTarget = getCameraViewTarget();
    // The projected view direction vanishes in a top-down view. The camera's
    // right vector still defines screen-relative pan in every supported view.
    Ogre::Vector3 right = mActiveCamera->getDerivedRight();
    right.z = 0.0f;
    right.normalise();
    Ogre::Vector3 forward(-right.y, right.x, 0.0f);

    // Adjust the newPosition vector to account for the translation due
    // to the movement keys on the keyboard (the arrow keys and/or WASD).
    if (mZChange != 0 || mControlZoom != 0)
    {
        // Zoom towards what is in the middle of the screen, not towards the
        // ground under the camera: the camera looks ahead at an angle, so a
        // plain height change slides the view target while zooming. Keeping
        // the ground point at the screen centre fixed means shifting the
        // camera base by the difference of its ground offsets at the two
        // heights.
        Ogre::Real newZ = newPosition.z + static_cast<Ogre::Real>(mControlZoom * frameTime * ZOOM_SPEED + mZChange);
        if (newZ <= MIN_CAMERA_Z)
            newZ = MIN_CAMERA_Z;
        else if (newZ >= MAX_CAMERA_Z)
            newZ = MAX_CAMERA_Z;
        Ogre::Vector3 offsetBefore = getGroundOffset(newPosition.z);
        Ogre::Vector3 offsetAfter = getGroundOffset(newZ);
        newPosition.x += offsetBefore.x - offsetAfter.x;
        newPosition.y += offsetBefore.y - offsetAfter.y;
        newPosition.z = newZ;
        mZChange = 0.0f;
    }

    // Update the position for the other axices.
    // Retain the nominal 60 Hz speed while making distance elapsed-time based.
    newPosition += (right * mTranslateVector.x + forward * mTranslateVector.y) * (60.0f * frameTime);

    // Prevent camera from moving down into the tiles or too high.
    if (newPosition.z <= MIN_CAMERA_Z)
        newPosition.z = MIN_CAMERA_Z;
    else if (newPosition.z >= MAX_CAMERA_Z)
        newPosition.z = MAX_CAMERA_Z;

    clampToMap(newPosition);

    Ogre::Vector3 groundTarget = newPosition + getGroundOffset(newPosition.z);
    Ogre::Node* tilt = getActiveCameraNode()->getChild(0);
    Ogre::Real pitch = tilt->getOrientation().getPitch().valueDegrees();
    Ogre::Real pitchStep = mRotateLocalVector.x * frameTime;
    if(pitchStep != 0.0f)
        pitchStep = std::max(-pitch, std::min(50.0f - pitch, pitchStep));
    tilt->pitch(Ogre::Degree(pitchStep), Ogre::Node::TS_LOCAL);
    getActiveCameraNode()->rotate(Ogre::Vector3::UNIT_Z,
        Ogre::Degree((mSwivelDegrees.valueDegrees() + mControlSwivel * 117.0f
            + mRotateLocalVector.y) * frameTime), Ogre::Node::TS_WORLD);
    newPosition = groundTarget - getGroundOffset(newPosition.z);
    // groundTarget includes the camera height; getGroundOffset has zero height.
    Ogre::Real radius = 0.0f;

    // If the camera is trying to fly toward a destination, move it in that direction.
    if (mCameraIsFlying)
    {
        // Compute the direction and distance the camera needs to move
        //to get to its intended destination.
        Ogre::Vector3 flightDirection = mCameraFlightDestination - viewTarget;
        radius = flightDirection.normalise();

        // If we are within the stopping distance of the target, then quit flying.
        // Otherwise we move towards the destination.

        // Give up as well when a frame of flight brings the view target no closer than
        // the frame before. The destination is then somewhere the camera cannot look,
        // and flying on only pushes it against the map edge for clampToMap() to pull
        // it back, every frame, forever.
        bool isStalled = (frameTime > 0.0) && (radius >= mCameraFlightDistance);

        if (radius <= 0.25 || isStalled)
        {
            // We are within the stopping distance of the target destination
            // so stop flying towards it.
            mCameraIsFlying = false;
        }
        else
        {
            // Scale the flight direction to move towards at the given speed
            // (the min function prevents overshooting the target) then add
            // this offset vector to the camera position.
            flightDirection *= std::min(FLIGHT_SPEED * frameTime, radius);
            newPosition += flightDirection;
            mCameraFlightDistance = radius;
        }
    }

    if (mCameraIsRotating && frameTime > 0.0)
    {
        Ogre::Quaternion orientationQuat = getActiveCameraNode()->getOrientation();
        Ogre::Quaternion orientationQuat2 = getActiveCameraNode()->getChild(0)->getOrientation();
        // Tilting - X-Axis - Looking up or down
        Ogre::Real pitch = orientationQuat2.getPitch().valueDegrees();
        Ogre::Real pitchUpdate = ROTATION_SPEED.valueDegrees();
        Ogre::Real pitchDiff = std::abs(pitch - mCameraPitchDestination);
        Ogre::Real pitchChange = ((pitchUpdate * frameTime) > pitchDiff) ? pitchDiff / frameTime : pitchUpdate;
        bool pitchDone = false;
        if (pitchDiff <= pitchUpdate * frameTime)
        {
            Ogre::Vector3 anchor = newPosition + getGroundOffset(newPosition.z);
            getActiveCameraNode()->getChild(0)->setOrientation(
                Ogre::Quaternion(Ogre::Degree(mCameraPitchDestination), Ogre::Vector3::UNIT_X));
            newPosition = anchor - getGroundOffset(newPosition.z);
            mRotateLocalVector.x = 0.0;
            pitchDone = true;
        }
        else if (pitch > mCameraPitchDestination)
            mRotateLocalVector.x = -pitchChange;
        else if (pitch < mCameraPitchDestination)
            mRotateLocalVector.x = pitchChange;

        // Roll - Z-Axis - Left or right
        Ogre::Real roll = orientationQuat.getRoll().valueDegrees();
        Ogre::Real rollUpdate = 1.3 * ROTATION_SPEED.valueDegrees();
        Ogre::Real rollDiff = std::abs(roll - mCameraRollDestination);
        Ogre::Real rollChange = ((rollUpdate * frameTime) > rollDiff) ? rollDiff / frameTime : rollUpdate;
        bool rollDone = false;
        if (rollDiff <= rollUpdate * frameTime)
        {
            Ogre::Vector3 anchor = newPosition + getGroundOffset(newPosition.z);
            getActiveCameraNode()->setOrientation(
                Ogre::Quaternion(Ogre::Degree(mCameraRollDestination), Ogre::Vector3::UNIT_Z));
            newPosition = anchor - getGroundOffset(newPosition.z);
            mSwivelDegrees = 0.0;
            rollDone = true;
        }
        else if (roll < mCameraRollDestination)
            mSwivelDegrees = rollChange;
        else if (roll > mCameraRollDestination)
            mSwivelDegrees = -rollChange;

        if (pitchDone && rollDone)
            mCameraIsRotating = false;

        /*
        // Useful to debug... or test.
        std::stringstream ss;
        ss << "Current pitch: " << pitch << ", desired pitch: " << mCameraPitchDestination << std::endl;
        ss << "Pitch update: " << pitchChange * frameTime << std::endl;
        ss << "Current roll: " << roll << ", desired roll: " << mCameraRollDestination << std::endl;
        ss << "Roll update: " << rollChange * frameTime << std::endl;
        OD_LOG_INF(ss.str());
        */
    }

    if(mCircleMode)
    {
        mAlpha += 0.1 * frameTime;

        newPosition.x = static_cast<Ogre::Real>(cos(mAlpha) * mRadius + mCenterX);
        newPosition.y = static_cast<Ogre::Real>(sin(mAlpha) * mRadius + mCenterY);

        if(mAlpha > 2.0 * 3.145)
            mCircleMode = false;
    }
    else if(mCatmullSplineMode)
    {
        mAlpha += 0.1 * frameTime;

        //std::cout << "alpha "<< mAlpha << std::endl;

        double tempX = mXHCS.evaluate(mAlpha);
        //std::cout << "newPosition.x: " << newPosition.x << std::endl;
        double tempY = mYHCS.evaluate(mAlpha);
        //std::cout << "newPosition.y: "<< newPosition.y << std::endl;

        if (tempX < 0.9 * mGameMap->getMapSizeX() &&
                tempX > 10 &&
                tempY < 0.9 * mGameMap->getMapSizeY() &&
                tempY > 10 ) {
            newPosition.x = static_cast<Ogre::Real>(tempX);
            newPosition.y = static_cast<Ogre::Real>(tempY);
        }

        if(mAlpha > mXHCS.getNN())
            mCatmullSplineMode = false;
    }

    // Everything above may have moved the camera again after the first clamp: the
    // flight, the circle and the spline all add to the position directly. Clamp once
    // more so the camera is never placed off the map, not even for the one frame it
    // would take the next clamp to pull it back.
    clampToMap(newPosition);

    // Move the camera to the new location
    getActiveCameraNode()->setPosition(newPosition);
}

Ogre::Vector3 CameraManager::getGroundOffset(Ogre::Real height) const
{
    // Parent rotation does not refresh the attached camera's child transform
    // until the scene graph update; input needs the new direction immediately.
    mActiveCameraNode->_update(true, false);
    // Follow the view direction down to z = 0, the same way getCameraViewTarget()
    // does, but for an arbitrary height rather than the current one.
    Ogre::Vector3 cameraDirection = mActiveCamera->getDerivedDirection();
    if (cameraDirection.z >= 0.0)
        return Ogre::Vector3::ZERO;

    cameraDirection /= fabs(cameraDirection.z);
    Ogre::Vector3 offset = height * cameraDirection;
    offset.z = 0.0;
    return offset;
}

void CameraManager::clampToMap(Ogre::Vector3& position) const
{
    // Keep the point the camera actually looks at inside the map, rather than the
    // camera's own position. The camera is pitched (DEFAULT_X_AXIS_VIEW is 25 degrees
    // off vertical), so its ground target lies z * tan(pitch) ahead of it, which with z
    // between MIN_CAMERA_Z and MAX_CAMERA_Z is several tiles. Clamping the position
    // alone let the view scroll past one edge of the map while stopping short of the
    // opposite one, by exactly twice that offset.
    Ogre::Vector3 groundOffset = getGroundOffset(position.z);

    const Ogre::Real maxX = static_cast<Ogre::Real>(mGameMap->getMapSizeX()) - groundOffset.x;
    const Ogre::Real maxY = static_cast<Ogre::Real>(mGameMap->getMapSizeY()) - groundOffset.y;

    position.x = std::min(std::max(position.x, -groundOffset.x), maxX);
    position.y = std::min(std::max(position.y, -groundOffset.y), maxY);
}

Ogre::Vector3 CameraManager::getCameraViewTarget() const
{
    mActiveCameraNode->_update(true, false);
    Ogre::Vector3 position = mActiveCamera->getRealPosition();
    Ogre::Vector3 target = position + getGroundOffset(position.z);
    target.z = 0.0f;
    return target;
}

void CameraManager::resetCamera(const Ogre::Vector3& position, const Ogre::Vector3& rotation)
{
    // Scripted menu shots must not inherit gameplay movement or view animation.
    move(fullStop);
    Ogre::Node* nodeRotation = getActiveCameraNode()->getChild(0);
    nodeRotation->resetOrientation();

    Ogre::Node* nodeCamera = getActiveCameraNode();
    nodeCamera->resetOrientation();

    nodeCamera->setPosition(position);

    nodeRotation->pitch(Ogre::Degree(rotation.x), Ogre::Node::TS_LOCAL);
    nodeRotation->yaw(Ogre::Degree(rotation.y), Ogre::Node::TS_LOCAL);
    nodeRotation->roll(Ogre::Degree(rotation.z), Ogre::Node::TS_LOCAL);
}

void CameraManager::resetCamera(const Ogre::Vector3& position)
{
    resetCamera(position, Ogre::Vector3(DEFAULT_X_AXIS_VIEW, 0, 0));
}


void CameraManager::flyTo(const Ogre::Vector3& destination)
{
    // clampToMap() keeps the camera's view target on the map, so a destination outside
    // it is one the camera can never arrive at. A click near the border of the minimap
    // names exactly that whenever the minimap shows ground past the map edge, and the
    // camera then flew at that point for the rest of the game, never arriving. Aim at
    // the nearest point on the map instead.
    mCameraFlightDestination.x = std::min(std::max(destination.x, static_cast<Ogre::Real>(0.0)),
        static_cast<Ogre::Real>(mGameMap->getMapSizeX()));
    mCameraFlightDestination.y = std::min(std::max(destination.y, static_cast<Ogre::Real>(0.0)),
        static_cast<Ogre::Real>(mGameMap->getMapSizeY()));
    mCameraFlightDestination.z = destination.z;

    mCameraIsFlying = true;
    mCameraFlightDistance = Ogre::Math::POS_INFINITY;
}

void CameraManager::onMiniMapClick(Ogre::Vector2 cc)
{
    flyTo(Ogre::Vector3(cc.x, cc.y, 0.0));
}

void CameraManager::jumpToViewTarget(const Ogre::Vector2& pos)
{
    move(fullStop);
    const Ogre::Real height = getActiveCameraNode()->getPosition().z;
    Ogre::Vector3 position(pos.x, pos.y, height);
    position -= getGroundOffset(height);
    clampToMap(position);
    getActiveCameraNode()->setPosition(position);
}

void CameraManager::setControls(const Ogre::Vector2& pan, Ogre::Real zoom, Ogre::Real swivel, bool fast)
{
    mFastPanFactor = fast ? 2.0f : 1.0f;
    mMoveSpeed = getActiveCameraNode()->getPosition().z / 16.0f * mPanSpeedFactor * mFastPanFactor;
    mMoveSpeedAcceleration = 2.0f * mMoveSpeed;
    mTranslateVectorAccel = Ogre::Vector3(pan.x, pan.y, 0.0f) * mMoveSpeedAcceleration;
    mTranslateMaxSpeedFactor = Ogre::Vector2(std::abs(pan.x), std::abs(pan.y));
    mControlZoom = zoom;
    mControlSwivel = swivel;
    if(pan != Ogre::Vector2::ZERO || zoom != 0.0f || swivel != 0.0f)
    {
        mCameraIsFlying = mCameraIsRotating = false;
        mSwivelDegrees = Ogre::Degree(0.0f);
        mRotateLocalVector = Ogre::Vector3::ZERO;
    }
}

void CameraManager::zoomBy(Ogre::Real distance)
{
    mZChange += distance;
}

void CameraManager::setViewOrientation(const Ogre::Quaternion& root, const Ogre::Quaternion& tilt)
{
    Ogre::Vector3 target = getCameraViewTarget();
    Ogre::Real height = getActiveCameraNode()->getPosition().z;
    getActiveCameraNode()->setOrientation(root);
    getActiveCameraNode()->getChild(0)->setOrientation(tilt);
    Ogre::Vector3 position = target - getGroundOffset(height);
    position.z = height;
    clampToMap(position);
    getActiveCameraNode()->setPosition(position);
}

void CameraManager::orbitBy(Ogre::Real swivel, Ogre::Real pitch)
{
    mCameraIsRotating = mCameraIsFlying = false;
    mRotateLocalVector = Ogre::Vector3::ZERO;
    mSwivelDegrees = Ogre::Degree(0.0f);
    Ogre::Quaternion root = getActiveCameraNode()->getOrientation();
    Ogre::Quaternion tilt = getActiveCameraNode()->getChild(0)->getOrientation();
    Ogre::Real currentPitch = tilt.getPitch().valueDegrees();
    pitch = std::max(-currentPitch, std::min(50.0f - currentPitch, pitch));
    setViewOrientation(Ogre::Quaternion(Ogre::Degree(swivel), Ogre::Vector3::UNIT_Z) * root,
        tilt * Ogre::Quaternion(Ogre::Degree(pitch), Ogre::Vector3::UNIT_X));
}

void CameraManager::adjustUserView(Ogre::Real roll, Ogre::Real yaw, Ogre::Real pitch)
{
    move(fullStop);
    Ogre::Quaternion root = getActiveCameraNode()->getOrientation();
    Ogre::Quaternion tilt = getActiveCameraNode()->getChild(0)->getOrientation();
    tilt = tilt * Ogre::Quaternion(Ogre::Degree(roll), Ogre::Vector3::UNIT_Z)
        * Ogre::Quaternion(Ogre::Degree(yaw), Ogre::Vector3::UNIT_Y)
        * Ogre::Quaternion(Ogre::Degree(pitch), Ogre::Vector3::UNIT_X);
    // Ground-target navigation requires the camera to keep looking downwards.
    if((root * tilt * Ogre::Vector3::NEGATIVE_UNIT_Z).z < -0.1f)
        setViewOrientation(root, tilt);
}

void CameraManager::loadUserView(unsigned int slot)
{
    if(slot >= 3)
        return;
    std::istringstream values(ConfigManager::getSingleton().getInputValue(
        "UserCamera" + Helper::toString(slot + 1), "", false));
    Ogre::Quaternion root, tilt;
    if(!(values >> root.w >> root.x >> root.y >> root.z >> tilt.w >> tilt.x >> tilt.y >> tilt.z))
    {
        move(fullStop);
        setViewOrientation(Ogre::Quaternion::IDENTITY,
            Ogre::Quaternion(Ogre::Degree(DEFAULT_X_AXIS_VIEW), Ogre::Vector3::UNIT_X));
        return;
    }
    if(!std::isfinite(root.Norm()) || !std::isfinite(tilt.Norm()) || root.Norm() < 0.01f || tilt.Norm() < 0.01f)
        return;
    root.normalise();
    tilt.normalise();
    if((root * tilt * Ogre::Vector3::NEGATIVE_UNIT_Z).z >= -0.1f)
        return;
    move(fullStop);
    setViewOrientation(root, tilt);
}

bool CameraManager::storeUserView(unsigned int slot)
{
    if(slot >= 3)
        return false;
    const Ogre::Quaternion& root = getActiveCameraNode()->getOrientation();
    const Ogre::Quaternion& tilt = getActiveCameraNode()->getChild(0)->getOrientation();
    std::ostringstream values;
    values.precision(9);
    values << root.w << ' ' << root.x << ' ' << root.y << ' ' << root.z << ' '
        << tilt.w << ' ' << tilt.x << ' ' << tilt.y << ' ' << tilt.z;
    ConfigManager& config = ConfigManager::getSingleton();
    config.setInputValue("UserCamera" + Helper::toString(slot + 1), values.str());
    return config.saveUserConfig();
}

void CameraManager::move(const Direction direction, double aux)
{
    // NOTE : The camera loses the desired sense of left, right, top, down
    // when the camera pitch is more than 90 degrees.
    // So we invert the panning in that case.
    Ogre::Real currentPitch = getActiveCameraNode()->getOrientation().getPitch().valueDegrees();
    currentPitch = std::fmod(currentPitch, 90.0f);

    const bool scaledPan = aux > 0.0;
    const Ogre::Real maxSpeedFactor = scaledPan ?
        static_cast<Ogre::Real>(std::min(aux, 1.0)) : 1.0f;
    const auto applyPanAcceleration = [this](Ogre::Real& acceleration, Ogre::Real direction)
    {
        const Ogre::Real newAcceleration = direction * mMoveSpeedAcceleration;
        acceleration = newAcceleration;
    };

    switch (direction)
    {
    case moveRight:
        mTranslateMaxSpeedFactor.x = maxSpeedFactor;
        applyPanAcceleration(mTranslateVectorAccel.x, currentPitch <= 0.0f ? 1.0f : -1.0f);
        break;

    case stopRight:
        if(mTranslateVectorAccel.x >= 0)
        {
            mTranslateVectorAccel.x = 0;
            mTranslateMaxSpeedFactor.x = 1.0f;
        }
        break;

    case moveLeft:
        mTranslateMaxSpeedFactor.x = maxSpeedFactor;
        applyPanAcceleration(mTranslateVectorAccel.x, currentPitch <= 0.0f ? -1.0f : 1.0f);
        break;

    case stopLeft:
        if(mTranslateVectorAccel.x <= 0)
        {
            mTranslateVectorAccel.x = 0;
            mTranslateMaxSpeedFactor.x = 1.0f;
        }
        break;

    case moveBackward:
        mTranslateMaxSpeedFactor.y = maxSpeedFactor;
        applyPanAcceleration(mTranslateVectorAccel.y, currentPitch <= 0.0f ? -1.0f : 1.0f);
        break;

    case stopBackward:
        if(mTranslateVectorAccel.y <= 0)
        {
            mTranslateVectorAccel.y = 0;
            mTranslateMaxSpeedFactor.y = 1.0f;
        }
        break;

    case moveForward:
        mTranslateMaxSpeedFactor.y = maxSpeedFactor;
        applyPanAcceleration(mTranslateVectorAccel.y, currentPitch <= 0.0f ? 1.0f : -1.0f);
        break;

    case stopForward:
        if(mTranslateVectorAccel.y >= 0)
        {
            mTranslateVectorAccel.y = 0;
            mTranslateMaxSpeedFactor.y = 1.0f;
        }
        break;

    case moveUp:
        mZChange += 0.2f;
        break;

    case stopUp:
        break;

    case moveDown:
        mZChange -= 0.2f;
        break;

    case stopDown:
        break;

    case rotateLeft:
        mSwivelDegrees = 1.3 * ROTATION_SPEED;
        break;

    case stopRotRight:
        mSwivelDegrees = 0;
        break;

    case rotateRight:
        mSwivelDegrees = -1.3 * ROTATION_SPEED;
        break;

    case stopRotLeft:
        mSwivelDegrees = 0;
        break;

    case rotateUp:
        mRotateLocalVector.x = ROTATION_SPEED.valueDegrees();
        break;

    case stopRotDown:
        mRotateLocalVector.x = 0;
        break;

    case rotateDown:
        mRotateLocalVector.x = -ROTATION_SPEED.valueDegrees();
        break;

    case stopRotUp:
        mRotateLocalVector.x = 0;
        break;

    case randomRotateX:
        mRotateLocalVector.y = static_cast<Ogre::Real>(8.0*aux);
        break;

    case zeroRandomRotateX:
        mRotateLocalVector.y = 0.0;
        break;

    case randomRotateY:
        mRotateLocalVector.x = static_cast<Ogre::Real>(8.0*aux);
        break;

    case zeroRandomRotateY:
        mRotateLocalVector.x = 0.0;
        break;

    case fullStop:
        mTranslateVector = Ogre::Vector3::ZERO;
        mTranslateVectorAccel = Ogre::Vector3::ZERO;
        mRotateLocalVector = Ogre::Vector3::ZERO;
        mZChange = mControlZoom = mControlSwivel = 0.0f;
        mSwivelDegrees = Ogre::Degree(0.0f);
        mCameraIsFlying = mCameraIsRotating = false;
        mFastPanFactor = 1.0f;
        break;

    default:
        break;
    }
}

bool CameraManager::onFrameEnded()
{
     return true;
}

bool CameraManager::onFrameStarted()
{
     return true;
}

// bool CameraManager::saveCameraHistory(unsigned int ii)
// {
//     mCameraHistory.insert(std::pair<unsigned int, PosWithOrient>
//                           (ii,PosWithOrient{getActiveCameraPosition(),getActiveCameraOrientation()}));
//     return true;
// }
    
// bool CameraManager::restorePreviousCameraPosition(unsigned int ii)
// {
//     try
//     {
//         CameraManager::getActiveCamera()->setPosition( mCameraHistory.at(ii).vv );
//         CameraManager::getActiveCamera()->setOrientation( mCameraHistory.at(ii).qq );
//     }
//     catch(std::out_of_range& oor)
//     {
//         return false;
//     }
//     return true;
// }

bool CameraManager::isCameraMovingAtAll() const
{
    // FIXME: Don't compare floating point for equality!
    return (mTranslateVectorAccel.x != 0 ||
            mTranslateVectorAccel.y != 0 ||
            mTranslateVector.x != 0 ||
            mTranslateVector.y != 0 ||
            mZChange != 0 || mControlZoom != 0 || mControlSwivel != 0 ||
            mSwivelDegrees.valueDegrees() != 0 ||
            mRotateLocalVector.x != 0 || mRotateLocalVector.y != 0 ||
            mCameraIsFlying ||
            mCameraIsRotating);
}
