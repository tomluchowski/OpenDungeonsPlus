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

    //! \brief Forgets everything that was loaded (also the missing ones).
    void clear();

    //! \brief Hands out the messages collected since the last call.
    std::vector<std::string> takeMessages();

private:
    std::string mAssetRoot;
    //! nullptr values are catalog ids whose manifest is missing or invalid
    std::map<std::string, std::shared_ptr<PortraitManifest> > mManifests;
    std::vector<std::string> mMessages;
};

#endif // PORTRAITMANIFESTREGISTRY_H
