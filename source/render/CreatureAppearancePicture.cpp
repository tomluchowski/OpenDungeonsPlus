/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/CreatureAppearancePicture.h"

#include "game/CreatureAppearance.h"
#include "render/AppearanceCompose.h"
#include "render/DungeonbookAppearanceConfig.h"
#include "render/DungeonbookQuirks.h"
#include "render/PortraitManifest.h"
#include "render/PortraitManifestRegistry.h"
#include "render/PortraitTint.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"
#include "utils/ResourceManager.h"

#include <Ogre.h>
#include <OgreDataStream.h>
#include <OgreHardwarePixelBuffer.h>

#include <CEGUI/BasicImage.h>
#include <CEGUI/ImageManager.h>
#include <CEGUI/RendererModules/Ogre/Renderer.h>
#include <CEGUI/System.h>
#include <CEGUI/Texture.h>

#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <vector>

namespace
{
//! Same shrink factor as the profile portrait, the textures are shown small
const uint32_t PICTURE_DOWNSCALE = 4;
const std::string PICTURE_PREFIX = "DungeonbookAppearance/";

//! Everything the client side keeps for the composed pictures
struct PictureState
{
    PictureState() :
        mInitialized(false)
    {
    }

    bool mInitialized;
    PortraitManifestRegistry mRegistry;
    DungeonbookAppearanceConfig mConfig;
    //! Colour regions of the neutral bases (config/dungeonbook-base-tints.cfg)
    PortraitTint mTint;
    //! Profile remarks (config/dungeonbook-quirks.cfg)
    DungeonbookQuirks mQuirks;
    //! Cached pictures, least recently used first
    std::vector<AppearanceCompose::CacheEntry> mEntries;
    //! Catalog ids and picture keys that failed; they are logged once and not tried again until the map is
    //! unloaded. Only failures that come from the files, never the empty appearance.
    std::set<std::string> mFailed;
};

PictureState& getRawState()
{
    static PictureState state;
    return state;
}

PictureState& getState()
{
    PictureState& state = getRawState();
    if(state.mInitialized)
        return state;

    state.mInitialized = true;
    std::string path = ConfigManager::getSingleton().getConfigPath();
    if(!path.empty() && (path[path.size() - 1] != '/') && (path[path.size() - 1] != '\\'))
        path += "/";

    state.mConfig.loadFromFile(path + "dungeonbook-appearance.cfg");
    const std::vector<std::string>& warnings = state.mConfig.getWarnings();
    for(std::vector<std::string>::const_iterator it = warnings.begin(); it != warnings.end(); ++it)
    {
        OD_LOG_WRN("Dungeonbook appearance: " + *it);
    }

    // Same root resolution as the server registry
    std::string root = state.mConfig.getAssetRoot();
    if(root.empty())
        root = "materials/portraits/variants";

    bool isAbsolute = (root.size() > 1) && ((root[1] == ':') || (root[0] == '/') || (root[0] == '\\'));
    if(!isAbsolute)
        root = ResourceManager::getSingleton().getGameDataPath() + root;

    state.mRegistry.setAssetRoot(root);

    // A missing file or a bad line only costs the colour variation of the bases
    state.mTint.loadFromFile(path + "dungeonbook-base-tints.cfg");
    const std::vector<std::string>& errors = state.mTint.getErrors();
    for(std::vector<std::string>::const_iterator it = errors.begin(); it != errors.end(); ++it)
    {
        OD_LOG_WRN("Dungeonbook base tints: " + *it);
    }

    // A missing file or a bad line only costs the remarks
    state.mQuirks.loadFromFile(path + "dungeonbook-quirks.cfg");
    const std::vector<std::string>& quirkWarnings = state.mQuirks.getWarnings();
    for(std::vector<std::string>::const_iterator it = quirkWarnings.begin(); it != quirkWarnings.end(); ++it)
    {
        OD_LOG_WRN("Dungeonbook quirks: " + *it);
    }
    return state;
}

void logRegistryMessages(PictureState& state)
{
    std::vector<std::string> messages = state.mRegistry.takeMessages();
    for(std::vector<std::string>::const_iterator it = messages.begin(); it != messages.end(); ++it)
    {
        OD_LOG_WRN("Dungeonbook appearance: " + *it);
    }
}

//! Logs the reason once per catalog id or picture key and remembers the failure
void rememberFailure(PictureState& state, const std::string& failedKey, const std::string& reason)
{
    if(state.mFailed.insert(failedKey).second)
    {
        OD_LOG_WRN("Dungeonbook appearance of " + failedKey + " falls back to the creature portrait: " + reason);
    }
}

//! Reads a png file whose size has to match the manifest into RGBA bytes
bool loadRgbaImage(const std::string& path, uint32_t width, uint32_t height, AppearanceCompose::RgbaImage& result,
    std::string& error)
{
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if(!file.is_open())
    {
        error = "cannot open " + path;
        return false;
    }

    std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if(bytes.empty())
    {
        error = "empty file " + path;
        return false;
    }

    try
    {
        Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream(&bytes[0], bytes.size(), false, true));
        Ogre::Image image;
        image.load(stream, "png");
        if((static_cast<uint32_t>(image.getWidth()) != width) || (static_cast<uint32_t>(image.getHeight()) != height))
        {
            error = "wrong size of " + path;
            return false;
        }

        result = AppearanceCompose::RgbaImage(width, height);
        Ogre::PixelBox box(width, height, 1, Ogre::PF_BYTE_RGBA, &result.mPixels[0]);
        Ogre::PixelUtil::bulkPixelConversion(image.getPixelBox(), box);
    }
    catch(const std::exception& e)
    {
        error = "cannot decode " + path + ": " + e.what();
        return false;
    }
    return true;
}

//! Destroys the CEGUI image and texture of a cached picture (the Ogre texture belongs to the CEGUI texture)
void destroyPicture(const std::string& name)
{
    CEGUI::ImageManager& images = CEGUI::ImageManager::getSingleton();
    CEGUI::Renderer& renderer = *CEGUI::System::getSingleton().getRenderer();
    if(images.isDefined(name))
        images.destroy(name);
    if(renderer.isTextureDefined(name))
        renderer.destroyTexture(name);
}

//! Composes, colours and uploads the picture. Returns nullptr and sets error if the files are unusable.
const CEGUI::Image* buildPicture(PictureState& state, const std::string& creatureName,
    const CreatureAppearance& appearance, const PortraitManifest& manifest, const std::string& name,
    uint64_t& bytes, bool& baseFailed, std::string& error)
{
    baseFailed = false;
    AppearanceCompose::RgbaImage base;
    if(!loadRgbaImage(manifest.getBasePath(), manifest.getBaseWidth(), manifest.getBaseHeight(), base, error))
    {
        baseFailed = true;
        return nullptr;
    }

    // The parts in the order of the Slot lines of the manifest
    std::vector<AppearanceCompose::Part> parts;
    const std::vector<PortraitManifest::Slot>& slots = manifest.getSlots();
    for(std::vector<PortraitManifest::Slot>::const_iterator it = slots.begin(); it != slots.end(); ++it)
    {
        uint32_t number = appearance.getChoice(it->mName);
        if(number == 0)
            continue;

        const PortraitManifest::Option* option = manifest.findOption(it->mName, number);
        if(option == nullptr)
            continue;

        AppearanceCompose::Part part;
        part.mSlot = it->mName;
        part.mX = it->mX;
        part.mY = it->mY;
        if(!loadRgbaImage(option->mPath, it->mWidth, it->mHeight, part.mImage, error))
            return nullptr;

        parts.push_back(part);
    }

    AppearanceCompose::RgbaImage composed = AppearanceCompose::compose(base, parts,
        AppearanceCompose::isHelmetDamageClipped(appearance.getCatalogId()));
    std::vector<AppearanceCompose::Part>().swap(parts);
    base = AppearanceCompose::RgbaImage();

    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> data = AppearanceCompose::flattenAndTint(composed, PICTURE_DOWNSCALE, &state.mTint,
        appearance.getCatalogId(), creatureName, width, height);
    if(data.empty())
    {
        error = "the composed picture is empty";
        return nullptr;
    }

    CEGUI::OgreRenderer& renderer = static_cast<CEGUI::OgreRenderer&>(*CEGUI::System::getSingleton().getRenderer());
    CEGUI::ImageManager& images = CEGUI::ImageManager::getSingleton();
    Ogre::TexturePtr texture;
    try
    {
        texture = Ogre::TextureManager::getSingleton().createManual(name, "General", Ogre::TEX_TYPE_2D,
            width, height, 0, Ogre::PF_BYTE_RGBA, Ogre::TU_DEFAULT);
        texture->getBuffer()->blitFromMemory(Ogre::PixelBox(width, height, 1, Ogre::PF_BYTE_RGBA, &data[0]));

        CEGUI::Texture& guiTexture = renderer.createTexture(name, texture, true);
        CEGUI::BasicImage& image = static_cast<CEGUI::BasicImage&>(images.create("BasicImage", name));
        image.setTexture(&guiTexture);
        image.setArea(CEGUI::Rectf(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)));
        image.setAutoScaled(CEGUI::ASM_Disabled);
        bytes = static_cast<uint64_t>(width) * height * 4;
        return &image;
    }
    catch(const std::exception& e)
    {
        error = std::string("texture creation failed: ") + e.what();
    }

    if(images.isDefined(name))
        images.destroy(name);
    if(renderer.isTextureDefined(name))
        renderer.destroyTexture(name);
    else if(texture)
        Ogre::TextureManager::getSingleton().remove(texture->getHandle());
    return nullptr;
}
}

const CEGUI::Image* getCreatureAppearanceImage(const std::string& creatureName, const CreatureAppearance& appearance)
{
    // No catalog id (yet): nothing is remembered, the next fill asks again
    if(appearance.isEmpty())
        return nullptr;

    PictureState& state = getState();
    const std::string& catalogId = appearance.getCatalogId();
    const std::string key = AppearanceCompose::makePictureKey(appearance, creatureName);
    if((state.mFailed.count(catalogId) != 0) || (state.mFailed.count(key) != 0))
        return nullptr;

    const std::string name = PICTURE_PREFIX + key;
    CEGUI::ImageManager& images = CEGUI::ImageManager::getSingleton();
    if(AppearanceCompose::cacheTouch(state.mEntries, key) && images.isDefined(name))
        return &images.get(name);

    const PortraitManifest* manifest = state.mRegistry.getManifest(catalogId);
    logRegistryMessages(state);
    if(manifest == nullptr)
    {
        rememberFailure(state, catalogId, "no valid manifest");
        return nullptr;
    }

    uint64_t bytes = 0;
    bool baseFailed = false;
    std::string error;
    const CEGUI::Image* image = buildPicture(state, creatureName, appearance, *manifest, name, bytes, baseFailed,
        error);
    if(image == nullptr)
    {
        // A broken base fails every creature of the catalog id, a broken part only this look
        rememberFailure(state, baseFailed ? catalogId : key, error);
        return nullptr;
    }

    AppearanceCompose::cacheAdd(state.mEntries, key, bytes);
    uint64_t maxBytes = static_cast<uint64_t>(state.mConfig.getMaxCacheMegabytes()) * 1024 * 1024;
    std::vector<std::string> evicted = AppearanceCompose::cacheEvict(state.mEntries,
        state.mConfig.getMaxCachedPictures(), maxBytes, AppearanceCompose::CACHE_KEEP_NEWEST);
    for(std::vector<std::string>::const_iterator it = evicted.begin(); it != evicted.end(); ++it)
        destroyPicture(PICTURE_PREFIX + *it);

    return image;
}

const PortraitManifest* getClientPortraitManifest(const std::string& catalogId)
{
    PictureState& state = getState();
    const PortraitManifest* manifest = state.mRegistry.getManifest(catalogId);
    logRegistryMessages(state);
    return manifest;
}

std::vector<std::string> getCreatureAppearanceRemarks(const std::string& creatureName,
    const CreatureAppearance& appearance)
{
    if(appearance.isEmpty())
        return std::vector<std::string>();

    PictureState& state = getState();
    const PortraitManifest* manifest = state.mRegistry.getManifest(appearance.getCatalogId());
    logRegistryMessages(state);
    if(manifest == nullptr)
        return std::vector<std::string>();

    return DungeonbookQuirkLogic::selectRemarks(state.mQuirks, *manifest, appearance, creatureName);
}

void clearCreatureAppearancePictures()
{
    PictureState& state = getRawState();
    if(!state.mInitialized)
        return;

    for(std::vector<AppearanceCompose::CacheEntry>::const_iterator it = state.mEntries.begin();
        it != state.mEntries.end(); ++it)
    {
        destroyPicture(PICTURE_PREFIX + it->mKey);
    }
    state.mEntries.clear();
    state.mFailed.clear();
    state.mRegistry.clear();
}
