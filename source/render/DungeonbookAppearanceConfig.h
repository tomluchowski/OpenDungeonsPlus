/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DUNGEONBOOKAPPEARANCECONFIG_H
#define DUNGEONBOOKAPPEARANCECONFIG_H

#include <stdint.h>
#include <istream>
#include <string>
#include <vector>

//! \brief Settings of the Dungeonbook appearance (config/dungeonbook-appearance.cfg).
//! Pure source without any Ogre dependency. Line based, TAB separated, '#' starts a comment:
//!   AssetRoot<TAB>folder with the <catalog-id>/manifest.cfg files, relative paths are resolved by the caller
//!                 against the game data path
//!   MaxCachedPictures<TAB>number of composed pictures kept at the same time
//!   MaxCacheMegabytes<TAB>memory limit of the composed pictures
//! Missing or bad entries keep their default and are listed in getWarnings(); the caller logs them once.
class DungeonbookAppearanceConfig
{
public:
    static const uint32_t DEFAULT_MAX_CACHED_PICTURES;
    static const uint32_t DEFAULT_MAX_CACHE_MEGABYTES;

    DungeonbookAppearanceConfig();

    //! \brief Loads the config file. Returns false if it cannot be opened; all defaults stay in place.
    bool loadFromFile(const std::string& path);

    //! \brief Same for a stream, source only names the file in the warnings.
    void loadFromStream(std::istream& is, const std::string& source);

    //! Empty if the config does not name one, then the caller uses materials/portraits/variants
    const std::string& getAssetRoot() const
    { return mAssetRoot; }

    uint32_t getMaxCachedPictures() const
    { return mMaxCachedPictures; }

    uint32_t getMaxCacheMegabytes() const
    { return mMaxCacheMegabytes; }

    const std::vector<std::string>& getWarnings() const
    { return mWarnings; }

private:
    void setDefaults();

    std::string mAssetRoot;
    uint32_t mMaxCachedPictures;
    uint32_t mMaxCacheMegabytes;
    std::vector<std::string> mWarnings;
};

#endif // DUNGEONBOOKAPPEARANCECONFIG_H
