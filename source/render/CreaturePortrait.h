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

//! Portrait of one creature: the illustrated portrait of its class with the colour variation of
//! config/portrait-tints.cfg (hair, beard, eyes, skin, teeth), chosen from the creature name.
//! Returns a top-square view; classes without tint entries share the panel portrait texture. The tinted
//! image is created on first use and cached until clearCreatureProfilePortraits().
//! gender is the profile gender ("Female", "Male" or empty): if portrait-<mesh>-<gender>.png exists
//! (lower case gender) that image is used, otherwise the base portrait of the mesh.
const CEGUI::Image& getCreatureProfilePortraitImage(const std::string& creatureName, const std::string& meshName,
    const std::string& gender);

//! Destroy the tinted portraits of all creatures (end of a game).
void clearCreatureProfilePortraits();

#endif
