/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/PortraitManifest.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace
{
//! Draw order of the slots. Slots with other names, or listed in another order, are dropped.
const char* const SLOT_ORDER[] = { "build", "outfit", "hair", "ears", "eyes", "nose", "mouth", "chin", "helmet",
    "scar", "neck" };
const int NB_SLOT_NAMES = 11;

int getSlotRank(const std::string& name)
{
    for(int i = 0; i < NB_SLOT_NAMES; ++i)
    {
        if(name == SLOT_ORDER[i])
            return i;
    }
    return -1;
}

std::vector<std::string> splitColumns(const std::string& line)
{
    std::vector<std::string> columns;
    std::string::size_type start = 0;
    while(true)
    {
        std::string::size_type tab = line.find('\t', start);
        std::string column = line.substr(start, (tab == std::string::npos) ? std::string::npos : tab - start);
        // Strip trailing CR and blanks
        while(!column.empty() && ((column[column.size() - 1] == '\r') || (column[column.size() - 1] == ' ')))
            column.erase(column.size() - 1);
        columns.push_back(column);
        if(tab == std::string::npos)
            break;
        start = tab + 1;
    }
    return columns;
}

bool parseUint(const std::string& text, uint32_t& value)
{
    if(text.empty() || (text.find_first_not_of("0123456789") != std::string::npos) || (text.size() > 9))
        return false;
    value = static_cast<uint32_t>(std::strtoul(text.c_str(), nullptr, 10));
    return true;
}

std::string directoryOf(const std::string& path)
{
    std::string::size_type pos = path.find_last_of("/\\");
    if(pos == std::string::npos)
        return ".";
    return path.substr(0, pos);
}

std::string joinPath(const std::string& directory, const std::string& file)
{
    return directory + "/" + file;
}

struct RawSlot
{
    PortraitManifest::Slot mSlot;
    uint32_t mLine;
};

struct RawOption
{
    PortraitManifest::Option mOption;
    uint32_t mLine;
};

bool optionNumberLess(const PortraitManifest::Option& a, const PortraitManifest::Option& b)
{
    return a.mNumber < b.mNumber;
}
}

bool PortraitManifest::readPngSize(const std::string& path, uint32_t& width, uint32_t& height)
{
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if(!file.is_open())
        return false;
    unsigned char header[24];
    file.read(reinterpret_cast<char*>(header), 24);
    if(file.gcount() != 24)
        return false;
    const unsigned char signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    for(int i = 0; i < 8; ++i)
    {
        if(header[i] != signature[i])
            return false;
    }
    // The first chunk must be IHDR
    if((header[12] != 'I') || (header[13] != 'H') || (header[14] != 'D') || (header[15] != 'R'))
        return false;
    width = (static_cast<uint32_t>(header[16]) << 24) | (static_cast<uint32_t>(header[17]) << 16) |
        (static_cast<uint32_t>(header[18]) << 8) | static_cast<uint32_t>(header[19]);
    height = (static_cast<uint32_t>(header[20]) << 24) | (static_cast<uint32_t>(header[21]) << 16) |
        (static_cast<uint32_t>(header[22]) << 8) | static_cast<uint32_t>(header[23]);
    return (width > 0) && (height > 0);
}

void PortraitManifest::clear()
{
    mBasePath.clear();
    mBaseWidth = 0;
    mBaseHeight = 0;
    mSlots.clear();
    mOptions.clear();
    mErrors.clear();
}

void PortraitManifest::addError(const std::string& source, uint32_t lineNumber, const std::string& text)
{
    std::ostringstream message;
    message << source;
    if(lineNumber > 0)
        message << ":" << lineNumber;
    message << ": " << text;
    mErrors.push_back(message.str());
}

bool PortraitManifest::loadFromFile(const std::string& path)
{
    std::ifstream file(path.c_str());
    if(!file.is_open())
    {
        clear();
        addError(path, 0, "cannot open the portrait manifest");
        return false;
    }
    return loadFromStream(file, directoryOf(path), path);
}

bool PortraitManifest::loadFromStream(std::istream& is, const std::string& directory,
    const std::string& source)
{
    clear();

    std::string baseFile;
    uint32_t baseLine = 0;
    std::vector<RawSlot> rawSlots;
    std::vector<RawOption> rawOptions;

    std::string line;
    uint32_t lineNumber = 0;
    while(std::getline(is, line))
    {
        ++lineNumber;
        std::string::size_type comment = line.find('#');
        if(comment != std::string::npos)
            line = line.substr(0, comment);
        if(line.find_first_not_of(" \t\r") == std::string::npos)
            continue;
        std::vector<std::string> columns = splitColumns(line);
        const std::string& key = columns[0];
        if(key == "Base")
        {
            if((columns.size() < 2) || columns[1].empty())
                addError(source, lineNumber, "Base without a path");
            else if(!baseFile.empty())
                addError(source, lineNumber, "second Base line ignored");
            else
            {
                baseFile = columns[1];
                baseLine = lineNumber;
            }
        }
        else if(key == "Slot")
        {
            RawSlot raw;
            raw.mLine = lineNumber;
            raw.mSlot.mX = 0;
            raw.mSlot.mY = 0;
            raw.mSlot.mWidth = 0;
            raw.mSlot.mHeight = 0;
            if((columns.size() < 6) || columns[1].empty() || !parseUint(columns[2], raw.mSlot.mX) ||
                !parseUint(columns[3], raw.mSlot.mY) || !parseUint(columns[4], raw.mSlot.mWidth) ||
                !parseUint(columns[5], raw.mSlot.mHeight) || (raw.mSlot.mWidth == 0) ||
                (raw.mSlot.mHeight == 0))
            {
                addError(source, lineNumber, "bad Slot line");
                continue;
            }
            raw.mSlot.mName = columns[1];
            rawSlots.push_back(raw);
        }
        else if(key == "Option")
        {
            RawOption raw;
            raw.mLine = lineNumber;
            raw.mOption.mNumber = 0;
            if((columns.size() < 5) || columns[1].empty() || !parseUint(columns[2], raw.mOption.mNumber) ||
                (raw.mOption.mNumber == 0) || columns[3].empty() || columns[4].empty())
            {
                addError(source, lineNumber, "bad Option line");
                continue;
            }
            raw.mOption.mSlot = columns[1];
            raw.mOption.mName = columns[3];
            raw.mOption.mFile = columns[4];
            raw.mOption.mPath = joinPath(directory, columns[4]);
            rawOptions.push_back(raw);
        }
        else
            addError(source, lineNumber, "unknown line type " + key);
    }

    // The base decides over the whole manifest
    if(baseFile.empty())
    {
        addError(source, 0, "no Base line, manifest is invalid");
        return false;
    }
    mBasePath = joinPath(directory, baseFile);
    if(!readPngSize(mBasePath, mBaseWidth, mBaseHeight))
    {
        addError(source, baseLine, "base image is missing or not a png: " + baseFile);
        return false;
    }

    // Slots: known name, listed once and in draw order, inside the base
    int lastRank = -1;
    for(std::vector<RawSlot>::const_iterator it = rawSlots.begin(); it != rawSlots.end(); ++it)
    {
        const Slot& slot = it->mSlot;
        int rank = getSlotRank(slot.mName);
        if(rank < 0)
            addError(source, it->mLine, "unknown slot " + slot.mName);
        else if(findSlot(slot.mName) != nullptr)
            addError(source, it->mLine, "slot " + slot.mName + " listed twice");
        else if(rank < lastRank)
            addError(source, it->mLine, "slot " + slot.mName + " is out of draw order");
        else if((slot.mX + slot.mWidth > mBaseWidth) || (slot.mY + slot.mHeight > mBaseHeight))
            addError(source, it->mLine, "slot " + slot.mName + " lies outside the base");
        else
        {
            mSlots.push_back(slot);
            lastRank = rank;
        }
    }

    // Options: valid slot, unique number, file exists and has the size of the slot
    for(std::vector<RawOption>::const_iterator it = rawOptions.begin(); it != rawOptions.end(); ++it)
    {
        const Option& option = it->mOption;
        const Slot* slot = findSlot(option.mSlot);
        if(slot == nullptr)
        {
            addError(source, it->mLine, "option " + option.mName + " of unusable slot " + option.mSlot);
            continue;
        }
        if(findOption(option.mSlot, option.mNumber) != nullptr)
        {
            addError(source, it->mLine, "option number used twice in slot " + option.mSlot);
            continue;
        }
        uint32_t width = 0;
        uint32_t height = 0;
        if(!readPngSize(option.mPath, width, height))
        {
            addError(source, it->mLine, "part file is missing or not a png: " + option.mFile);
            continue;
        }
        if((width != slot->mWidth) || (height != slot->mHeight))
        {
            std::ostringstream text;
            text << "part " << option.mFile << " has size " << width << "x" << height << ", slot " <<
                slot->mName << " needs " << slot->mWidth << "x" << slot->mHeight;
            addError(source, it->mLine, text.str());
            continue;
        }
        mOptions.push_back(option);
    }

    // A slot without any usable part is not part of the picture
    std::vector<Slot> usableSlots;
    for(std::vector<Slot>::const_iterator it = mSlots.begin(); it != mSlots.end(); ++it)
    {
        if(getOptionsOfSlot(it->mName).empty())
            addError(source, 0, "slot " + it->mName + " has no usable option and is dropped");
        else
            usableSlots.push_back(*it);
    }
    mSlots = usableSlots;

    if(mSlots.empty())
    {
        addError(source, 0, "no usable slot, manifest is invalid");
        return false;
    }
    return true;
}

std::vector<PortraitManifest::Option> PortraitManifest::getOptionsOfSlot(const std::string& slot) const
{
    std::vector<Option> result;
    for(std::vector<Option>::const_iterator it = mOptions.begin(); it != mOptions.end(); ++it)
    {
        if(it->mSlot == slot)
            result.push_back(*it);
    }
    std::sort(result.begin(), result.end(), optionNumberLess);
    return result;
}

const PortraitManifest::Option* PortraitManifest::findOption(const std::string& slot, uint32_t number) const
{
    for(std::vector<Option>::const_iterator it = mOptions.begin(); it != mOptions.end(); ++it)
    {
        if((it->mSlot == slot) && (it->mNumber == number))
            return &(*it);
    }
    return nullptr;
}

const PortraitManifest::Slot* PortraitManifest::findSlot(const std::string& slot) const
{
    for(std::vector<Slot>::const_iterator it = mSlots.begin(); it != mSlots.end(); ++it)
    {
        if(it->mName == slot)
            return &(*it);
    }
    return nullptr;
}
