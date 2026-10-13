/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CREATUREPORTRAIT_H
#define CREATUREPORTRAIT_H

#include <OgrePrerequisites.h>
#include <string>

namespace CEGUI
{
    class Image;
}

//! Render the existing creature model to an isolated, static portrait texture.
//! The caller owns the returned texture and its TextureManager registration.
Ogre::TexturePtr createCreaturePortrait(const std::string& meshName, const std::string& textureName);

//! Return the cached portrait image, owned by the current CEGUI system.
const CEGUI::Image& getCreaturePortraitImage(const std::string& meshName);

//! Prefer the authored population-panel illustration, falling back for custom meshes.
const CEGUI::Image& getCreaturePanelPortraitImage(const std::string& meshName);

//! Return a square crop sharing the cached population-panel portrait texture.
const CEGUI::Image& getCreatureHandIconImage(const std::string& meshName);

#endif
