/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CREATUREAPPEARANCEPICTURE_H
#define CREATUREAPPEARANCEPICTURE_H

#include <string>

class CreatureAppearance;
class PortraitManifest;

namespace CEGUI
{
    class Image;
}

//! \brief The Dungeonbook picture of one creature: its neutral base plus the parts of its appearance,
//! composed on the CPU, coloured like the profile portrait (config/dungeonbook-base-tints.cfg, same
//! PortraitTint code, colours chosen from the creature name) and handed out as a CEGUI image.
//!
//! Returns nullptr if the caller has to use the fallback, the creature-bar portrait of the creature
//! (getCreatureProfilePortraitImage): the appearance is empty (no catalog id yet; this can change at
//! runtime, so the caller must ask again on every fill and must not remember the fallback), the
//! manifest or base of the catalog id is missing or invalid, or the picture could not be built. The
//! reason is logged once per catalog id (or picture).
//!
//! The picture is composed on the first request and cached by catalog id, options and tint. The cache
//! is limited by config/dungeonbook-appearance.cfg (MaxCachedPictures, MaxCacheMegabytes), the least
//! recently used pictures are released first. The newest few pictures are never released, so the caller
//! should set the returned image on its window right away and not keep the pointer.
//! Must be called from the main thread only (CEGUI and Ogre), never from per-frame code.
const CEGUI::Image* getCreatureAppearanceImage(const std::string& creatureName, const CreatureAppearance& appearance);

//! \brief The manifest of a catalog id from the client side registry (loaded once), nullptr if there is
//! none. For the profile remarks, which are keyed by slot and option name.
const PortraitManifest* getClientPortraitManifest(const std::string& catalogId);

//! \brief Releases all composed pictures and forgets the loaded manifests and failures (end of a game,
//! next to clearCreatureProfilePortraits()).
void clearCreatureAppearancePictures();

#endif // CREATUREAPPEARANCEPICTURE_H
