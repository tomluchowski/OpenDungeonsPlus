/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PORTRAITMANIFESTREGISTRY_H
#define PORTRAITMANIFESTREGISTRY_H

#include "render/PortraitManifest.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

//! \brief Cache of loaded portrait manifests: catalog id -> manifest, every manifest is read once.
//! Pure source without Ogre. A catalog id without a usable manifest is remembered as missing, so the
//! file is not read again and again. Messages (dropped entries, missing manifests) are collected for the
//! caller, which logs them once with takeMessages().
class PortraitManifestRegistry
{
public:
    PortraitManifestRegistry()
    {
    }

    //! \brief Folder with one sub folder per catalog id, each holding its manifest.cfg. Resets the cache.
    void setAssetRoot(const std::string& assetRoot);

    const std::string& getAssetRoot() const
    { return mAssetRoot; }

    //! \brief The manifest of the catalog id, loaded on first use. Returns nullptr if there is none or it
    //! is invalid as a whole (the caller then uses the fallback picture).
    const PortraitManifest* getManifest(const std::string& catalogId);

    //! \brief True if the folder of the catalog id holds a manifest.cfg file (it is not loaded or validated).
    bool hasCatalog(const std::string& catalogId) const;

    //! \brief Forgets everything that was loaded (also the missing ones).
    void clear();

    //! \brief Makes the next getManifest call load every catalog id again that failed before (missing or
    //! invalid manifest), so a manifest that was added or repaired later is found without a restart. The
    //! callers use it now and then (every few seconds), never every frame, because loading reads files.
    //! Manifests that loaded fine are kept. Returns true if there was a failed catalog id.
    bool retryFailed();

    //! \brief Hands out the messages collected since the last call.
    std::vector<std::string> takeMessages();

private:
    std::string mAssetRoot;
    //! nullptr values are catalog ids whose manifest is missing or invalid
    std::map<std::string, std::shared_ptr<PortraitManifest> > mManifests;
    std::vector<std::string> mMessages;
    //! The problem text of the catalog ids that failed, so a failure that stays the same is reported once
    std::map<std::string, std::string> mFailures;
};

#endif // PORTRAITMANIFESTREGISTRY_H
