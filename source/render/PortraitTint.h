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

    //! \brief Reads the shift amplitude (hue, saturation, value) of the first shifting region called regionName
    //! in the portrait block meshName. Returns false if there is none.
    bool getRegionShift(const std::string& meshName, const std::string& regionName, float shift[3]) const;

    //! \brief Gives every region called regionName that only shifts colours and has a shift of exactly zero
    //! the given amplitude (used for the skin of the neutral bases, whose regions are delivered with the
    //! artwork without an amplitude). Regions with their own amplitude and palette regions stay as they are.
    void setShiftWhereNone(const std::string& regionName, const float shift[3]);

    //! \brief Recolours the regions of the portrait of meshName for the creature creatureName.
    //! rgb holds width * height pixels with three floats (0 to 1) each, row by row.
    //! coverage (optional, width * height values 0 to 1) scales how strongly each pixel takes part, and
    //! the mean brightness of a palette region is taken over the covered pixels only. It is used for the
    //! parts of the Dungeonbook pictures, where transparent pixels must not count.
    void apply(const std::string& meshName, const std::string& creatureName, std::vector<float>& rgb,
        uint32_t width, uint32_t height, const std::vector<float>* coverage = nullptr) const;

private:
    //! A palette colour in hue (degrees), saturation and value (0 to 1)
    struct Colour
    {
        float mHue;
        float mSaturation;
        float mValue;
    };

    //! A named list of colours a region picks one from
    struct Palette
    {
        std::string mName;
        std::vector<Colour> mColours;
    };

    struct Region
    {
        Region() :
            mPalette(-1),
            mHasEllipse(false)
        {
            for(int i = 0; i < 3; ++i)
                mShift[i] = 0.0f;
            for(int i = 0; i < 4; ++i)
            {
                mBox[i] = 0.0f;
                mEllipse[i] = 0.0f;
            }
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
        //! Largest random shift of hue (degrees), saturation and value (factor)
        float mShift[3];
        //! ellipse=cx,cy,rx,ry replaces the box: centre as fractions of width and height, both radii as fractions
        //! of the width (so the ellipse is measured in pixels like the image)
        bool mHasEllipse;
        float mEllipse[4];
        //! Left, top, right and bottom edge of the region (0 to 1 of the image)
        float mBox[4];
        //! Boxes (four numbers each) cut out of the region
        std::vector<float> mCutOut;
        //! Lowest and highest hue, saturation and value of the pixels that belong to the region
        float mHue[2];
        float mSaturation[2];
        float mValue[2];
    };

    //! The regions of the portrait of one mesh
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
    //! Messages of the lines that could not be read
    std::vector<std::string> mErrors;
    //! Path of the loaded file, used in the error messages
    std::string mSource;
};

#endif
