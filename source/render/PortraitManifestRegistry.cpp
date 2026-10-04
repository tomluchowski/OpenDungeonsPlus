/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/PortraitManifestRegistry.h"

void PortraitManifestRegistry::setAssetRoot(const std::string& assetRoot)
{
    mAssetRoot = assetRoot;
    if(!mAssetRoot.empty() && mAssetRoot[mAssetRoot.size() - 1] != '/' && mAssetRoot[mAssetRoot.size() - 1] != '\\')
        mAssetRoot += "/";

    clear();
}

const PortraitManifest* PortraitManifestRegistry::getManifest(const std::string& catalogId)
{
    if(catalogId.empty())
        return nullptr;

    std::map<std::string, std::shared_ptr<PortraitManifest> >::iterator it = mManifests.find(catalogId);
    if(it != mManifests.end())
        return it->second.get();

    std::shared_ptr<PortraitManifest> manifest(new PortraitManifest());
    std::string path = mAssetRoot + catalogId + "/manifest.cfg";
    bool loaded = manifest->loadFromFile(path);

    const std::vector<std::string>& errors = manifest->getErrors();
    for(std::vector<std::string>::const_iterator error = errors.begin(); error != errors.end(); ++error)
        mMessages.push_back(*error);

    if(!loaded)
    {
        mMessages.push_back("Portrait manifest of " + catalogId + " is missing or invalid: " + path);
        manifest.reset();
    }

    mManifests[catalogId] = manifest;
    return manifest.get();
}

void PortraitManifestRegistry::clear()
{
    mManifests.clear();
}

std::vector<std::string> PortraitManifestRegistry::takeMessages()
{
    std::vector<std::string> messages;
    messages.swap(mMessages);
    return messages;
}
