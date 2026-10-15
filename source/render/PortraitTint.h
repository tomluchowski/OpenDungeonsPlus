/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PORTRAITTINT_H
#define PORTRAITTINT_H

#include <stdint.h>
#include <string>
#include <vector>

//! \brief Colour variation of the illustrated creature portraits (config/portrait-tints.cfg).
//! Pure image maths without any Ogre or CEGUI dependency: every creature gets slightly different
//! hair, beard, eyes, skin and teeth colours, chosen from its name only, so that the result is the
//! same on every machine. tools/portraits/preview_tints.py mirrors this code for the offline preview.
class PortraitTint
{
public:
    //! \brief Loads the config file. Returns false if the file cannot be opened. Syntax problems are
    //! collected in getErrors() and the offending line is skipped.
    bool loadFromFile(const std::string& path);

    const std::vector<std::string>& getErrors() const
    { return mErrors; }

    //! \brief True if the portrait of this mesh has at least one region.
    bool hasMesh(const std::string& meshName) const;

    //! \brief Recolours the regions of the portrait of meshName for the creature creatureName.
    //! rgb holds width * height pixels with three floats (0 to 1) each, row by row.
    void apply(const std::string& meshName, const std::string& creatureName, std::vector<float>& rgb,
        uint32_t width, uint32_t height) const;

private:
    struct Colour
    {
        float mHue;
        float mSaturation;
        float mValue;
    };

    struct Palette
    {
        std::string mName;
        std::vector<Colour> mColours;
    };

    struct Region
    {
        Region() :
            mPalette(-1)
        {
            for(int i = 0; i < 3; ++i)
                mShift[i] = 0.0f;
            for(int i = 0; i < 4; ++i)
                mBox[i] = 0.0f;
            for(int i = 0; i < 2; ++i)
            {
                mHue[i] = 0.0f;
                mSaturation[i] = 0.0f;
                mValue[i] = 0.0f;
            }
        }

        std::string mName;
        //! Index into mPalettes, or -1 if the region only shifts its colours
        int mPalette;
        float mShift[3];
        float mBox[4];
        std::vector<float> mCutOut;
        float mHue[2];
        float mSaturation[2];
        float mValue[2];
    };

    struct Portrait
    {
        std::string mMesh;
        std::vector<Region> mRegions;
    };

    void addError(uint32_t lineNumber, const std::string& message);
    bool parseRegion(const std::vector<std::string>& columns, Region& region, std::string& error) const;
    void computeWeights(const Region& region, const std::vector<float>& hue, const std::vector<float>& saturation,
        const std::vector<float>& value, uint32_t width, uint32_t height, std::vector<float>& weights) const;

    std::vector<Palette> mPalettes;
    std::vector<Portrait> mPortraits;
    std::vector<std::string> mErrors;
    std::string mSource;
};

#endif
