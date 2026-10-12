/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PORTRAITMANIFEST_H
#define PORTRAITMANIFEST_H

#include <stdint.h>
#include <istream>
#include <string>
#include <vector>

//! \brief Manifest of one neutral portrait base and its selectable parts (Dungeonbook appearance).
//! Pure source without any Ogre or CEGUI dependency. The file is line based, columns are separated
//! by TAB and '#' starts a comment:
//!   Base<TAB>path of the neutral base png, relative to the manifest
//!   Slot<TAB>slot<TAB>x<TAB>y<TAB>w<TAB>h      (one per slot, in draw order)
//!   Option<TAB>slot<TAB>n<TAB>name<TAB>file[<TAB>flip-x] (n starts at 1, never renumbered)
//! Invalid entries are dropped and collected in getErrors() (the caller logs them once). If nothing
//! usable is left, loadFromFile() returns false and the caller falls back to the preview portrait.
class PortraitManifest
{
public:
    struct Slot
    {
        std::string mName;
        uint32_t mX;
        uint32_t mY;
        uint32_t mWidth;
        uint32_t mHeight;
    };

    struct Option
    {
        std::string mSlot;
        uint32_t mNumber;
        std::string mName;
        //! File name as written in the manifest
        std::string mFile;
        //! Path of the file: directory of the manifest plus mFile
        std::string mPath;
        //! Optional horizontal reflection of this option inside its slot
        bool mFlipX;
    };

    PortraitManifest() :
        mBaseWidth(0),
        mBaseHeight(0)
    {
    }

    //! \brief Reads width and height from the IHDR chunk of a PNG file without decoding it.
    //! Returns false if the file cannot be read or is not a PNG.
    static bool readPngSize(const std::string& path, uint32_t& width, uint32_t& height);

    //! \brief Loads and validates the manifest file. Returns false if the manifest is invalid as a whole
    //! (cannot be opened, base missing or unreadable, no usable slot left).
    bool loadFromFile(const std::string& path);

    //! \brief Same for a stream. directory is the folder against which relative paths are resolved,
    //! source only names the manifest in error messages.
    bool loadFromStream(std::istream& is, const std::string& directory, const std::string& source);

    const std::string& getBasePath() const
    { return mBasePath; }

    uint32_t getBaseWidth() const
    { return mBaseWidth; }

    uint32_t getBaseHeight() const
    { return mBaseHeight; }

    //! Slots in draw order
    const std::vector<Slot>& getSlots() const
    { return mSlots; }

    //! All valid options, in file order
    const std::vector<Option>& getOptions() const
    { return mOptions; }

    //! Valid options of one slot, ordered by number
    std::vector<Option> getOptionsOfSlot(const std::string& slot) const;

    //! Returns nullptr if the slot has no valid option with this number
    const Option* findOption(const std::string& slot, uint32_t number) const;

    //! Returns nullptr if there is no such slot
    const Slot* findSlot(const std::string& slot) const;

    //! Problems found while loading, one text per dropped entry
    const std::vector<std::string>& getErrors() const
    { return mErrors; }

private:
    void clear();
    void addError(const std::string& source, uint32_t lineNumber, const std::string& text);

    std::string mBasePath;
    uint32_t mBaseWidth;
    uint32_t mBaseHeight;
    std::vector<Slot> mSlots;
    std::vector<Option> mOptions;
    std::vector<std::string> mErrors;
};

#endif // PORTRAITMANIFEST_H
