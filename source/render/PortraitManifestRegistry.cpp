/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/PortraitManifestRegistry.h"

#include <fstream>

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

    std::vector<std::string> messages = manifest->getErrors();
    if(!loaded)
        messages.push_back("Portrait manifest of " + catalogId + " is missing or invalid: " + path);

    std::string text;
    for(std::vector<std::string>::const_iterator message = messages.begin(); message != messages.end(); ++message)
        text += *message + "\n";

    // A problem is reported when it is new; a manifest that stays broken is not reported at every retry
    std::map<std::string, std::string>::iterator failure = mFailures.find(catalogId);
    if(!text.empty() && ((failure == mFailures.end()) || (failure->second != text)))
        mMessages.insert(mMessages.end(), messages.begin(), messages.end());

    if(!loaded)
    {
        mFailures[catalogId] = text;
        manifest.reset();
    }
    else if(failure != mFailures.end())
    {
        mFailures.erase(failure);
    }

    mManifests[catalogId] = manifest;
    return manifest.get();
}

bool PortraitManifestRegistry::hasCatalog(const std::string& catalogId) const
{
    if(catalogId.empty())
        return false;

    std::ifstream file((mAssetRoot + catalogId + "/manifest.cfg").c_str());
    return file.good();
}

void PortraitManifestRegistry::clear()
{
    mManifests.clear();
    mFailures.clear();
}

bool PortraitManifestRegistry::retryFailed()
{
    bool anyFailed = false;
    std::map<std::string, std::shared_ptr<PortraitManifest> >::iterator it = mManifests.begin();
    while(it != mManifests.end())
    {
        if(it->second)
        {
            ++it;
            continue;
        }

        anyFailed = true;
        mManifests.erase(it++);
    }
    return anyFailed;
}

std::vector<std::string> PortraitManifestRegistry::takeMessages()
{
    std::vector<std::string> messages;
    messages.swap(mMessages);
    return messages;
}
