/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/DungeonbookAppearanceConfig.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

const uint32_t DungeonbookAppearanceConfig::DEFAULT_MAX_CACHED_PICTURES = 64;
const uint32_t DungeonbookAppearanceConfig::DEFAULT_MAX_CACHE_MEGABYTES = 64;

namespace
{
std::string trimBlanks(const std::string& text)
{
    std::string::size_type first = text.find_first_not_of(" \t\r");
    if(first == std::string::npos)
        return std::string();
    std::string::size_type last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

bool parsePositive(const std::string& text, uint32_t& value)
{
    if(text.empty() || (text.find_first_not_of("0123456789") != std::string::npos) || (text.size() > 9))
        return false;
    uint32_t parsed = static_cast<uint32_t>(std::strtoul(text.c_str(), nullptr, 10));
    if(parsed == 0)
        return false;
    value = parsed;
    return true;
}
}

DungeonbookAppearanceConfig::DungeonbookAppearanceConfig()
{
    setDefaults();
}

void DungeonbookAppearanceConfig::setDefaults()
{
    mAssetRoot.clear();
    mMaxCachedPictures = DEFAULT_MAX_CACHED_PICTURES;
    mMaxCacheMegabytes = DEFAULT_MAX_CACHE_MEGABYTES;
}

bool DungeonbookAppearanceConfig::loadFromFile(const std::string& path)
{
    setDefaults();
    mWarnings.clear();
    std::ifstream file(path.c_str());
    if(!file.is_open())
    {
        mWarnings.push_back(path + ": cannot open the Dungeonbook appearance config, using defaults");
        return false;
    }
    loadFromStream(file, path);
    return true;
}

void DungeonbookAppearanceConfig::loadFromStream(std::istream& is, const std::string& source)
{
    setDefaults();
    mWarnings.clear();

    bool hasAssetRoot = false;
    bool hasMaxPictures = false;
    bool hasMaxMegabytes = false;

    std::string line;
    uint32_t lineNumber = 0;
    while(std::getline(is, line))
    {
        ++lineNumber;
        std::string::size_type comment = line.find('#');
        if(comment != std::string::npos)
            line = line.substr(0, comment);
        if(trimBlanks(line).empty())
            continue;
        std::string::size_type tab = line.find('\t');
        std::string key = trimBlanks(line.substr(0, tab));
        std::string value;
        if(tab != std::string::npos)
            value = trimBlanks(line.substr(tab + 1));

        std::ostringstream where;
        where << source << ":" << lineNumber << ": ";
        if(key == "AssetRoot")
        {
            // An empty value keeps the default path of the caller
            mAssetRoot = value;
            hasAssetRoot = !value.empty();
        }
        else if(key == "MaxCachedPictures")
        {
            if(parsePositive(value, mMaxCachedPictures))
                hasMaxPictures = true;
            else
                mWarnings.push_back(where.str() + "bad MaxCachedPictures, using the default");
        }
        else if(key == "MaxCacheMegabytes")
        {
            if(parsePositive(value, mMaxCacheMegabytes))
                hasMaxMegabytes = true;
            else
                mWarnings.push_back(where.str() + "bad MaxCacheMegabytes, using the default");
        }
        else
            mWarnings.push_back(where.str() + "unknown entry " + key);
    }

    // The defaults are set again for bad values, since parsePositive leaves the value untouched on failure
    if(!hasAssetRoot)
        mWarnings.push_back(source + ": AssetRoot not set, using the default asset folder");
    if(!hasMaxPictures)
    {
        mMaxCachedPictures = DEFAULT_MAX_CACHED_PICTURES;
        mWarnings.push_back(source + ": MaxCachedPictures not set, using the default");
    }
    if(!hasMaxMegabytes)
    {
        mMaxCacheMegabytes = DEFAULT_MAX_CACHE_MEGABYTES;
        mWarnings.push_back(source + ": MaxCacheMegabytes not set, using the default");
    }
}
