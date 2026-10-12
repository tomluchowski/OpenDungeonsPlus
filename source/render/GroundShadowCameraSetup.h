/*
 * Copyright (C) 2026 OpenDungeons Team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef GROUNDSHADOWCAMERASETUP_H
#define GROUNDSHADOWCAMERASETUP_H

#include <OgreCamera.h>
#include <OgreMovablePlane.h>
#include <OgreShadowCameraSetupPlaneOptimal.h>

// Fit point-light shadows to the visible dungeon floor, independently of the
// main camera's near clip and the height of the hand light.
class GroundShadowCameraSetup : public Ogre::ShadowCameraSetup
{
public:
    //! \brief Uses the dungeon floor (the plane z = 0) as the plane the shadow camera is fitted to.
    GroundShadowCameraSetup() : mGroundPlane(Ogre::Vector3::UNIT_Z, 0.0f)
    {
    }

    //! \brief Fits shadowCamera to the ground plane and keeps its culling frustum in sync.
    void getShadowCamera(const Ogre::SceneManager* scene, const Ogre::Camera* camera,
        const Ogre::Viewport* viewport, const Ogre::Light* light, Ogre::Camera* shadowCamera,
        size_t iteration) const override
    {
        Ogre::PlaneOptimalShadowCameraSetup setup(&mGroundPlane);
        setup.getShadowCamera(scene, camera, viewport, light, shadowCamera, iteration);

        // Ogre may use a separate default frustum to select shadow casters.
        // It must cover the same projection; getViewMatrix() alone returns
        // that old culling view instead of the shadow camera's own view.
        Ogre::Frustum* cullingFrustum = shadowCamera->getCullingFrustum();
        if(cullingFrustum != nullptr)
        {
            cullingFrustum->setCustomViewMatrix(true, shadowCamera->getViewMatrix(true));
            cullingFrustum->setCustomProjectionMatrix(true, shadowCamera->getProjectionMatrix());
        }
    }

private:
    //! The floor plane handed to Ogre's plane-optimal shadow camera setup.
    Ogre::MovablePlane mGroundPlane;
};

#endif
