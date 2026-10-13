/*
 *  Copyright (C) 2011-2016  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CAMPAIGNWORLD_H
#define CAMPAIGNWORLD_H

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

//! \brief A rectangle in pixels of the world map image.
struct CampaignWorldRect
{
    CampaignWorldRect():
        mX(0), mY(0), mWidth(0), mHeight(0)
    {
    }

    int mX;
    int mY;
    int mWidth;
    int mHeight;
};

//! \brief One province of the world map: how to find it in the id map and
//! which image layers show it.
struct CampaignWorldProvince
{
    CampaignWorldProvince():
        mMaskRed(0), mMaskGreen(0), mMaskBlue(0), mLayerX(0), mLayerY(0), mLiftX(0), mLiftY(0)
    {
    }

    std::string mId;
    std::string mName;
    uint8_t mMaskRed;
    uint8_t mMaskGreen;
    uint8_t mMaskBlue;
    //! Layer image file names, relative to the campaign art directory
    std::string mLayerLocked;
    std::string mLayerAvailable;
    std::string mLayerConquered;
    std::string mLayerLift;
    //! Where the state layers and the lift layer are drawn in map pixels
    int mLayerX;
    int mLayerY;
    int mLiftX;
    int mLiftY;
};

//! \brief A bonus site marker of the world map. The position is the centre of the icons.
struct CampaignWorldSite
{
    CampaignWorldSite():
        mX(0), mY(0)
    {
    }

    std::string mId;
    std::string mName;
    std::string mHost;
    int mX;
    int mY;
    std::string mIconHidden;
    std::string mIconFound;
    std::string mIconDone;
};

//! \brief The layout of the campaign world map (gui/campaign/campaign-world.json)
//! plus the id map used to find the province under a map pixel.
class CampaignWorld
{
public:
    CampaignWorld();

    //! \brief Reads the JSON description. Returns false if it is invalid or has no province.
    bool importDefinition(std::istream& is);

    int getWidth() const
    { return mWidth; }
    int getHeight() const
    { return mHeight; }
    const std::vector<CampaignWorldProvince>& getProvinces() const
    { return mProvinces; }
    const std::vector<CampaignWorldSite>& getSites() const
    { return mSites; }
    const CampaignWorldRect& getProgressPanel() const
    { return mProgressPanel; }
    const CampaignWorldRect& getTitleCartouche() const
    { return mTitleCartouche; }

    //! \brief Index of the province with that id, or the number of provinces if none.
    size_t findProvince(const std::string& id) const;

    //! \brief Sets the id map: width x height pixels of 3 bytes (red, green, blue).
    void setIdMap(int width, int height, const std::vector<uint8_t>& rgb);
    //! \brief Index of the province at the map pixel, or the number of provinces if there is none.
    size_t getProvinceAt(int x, int y) const;

private:
    int mWidth;
    int mHeight;
    std::vector<CampaignWorldProvince> mProvinces;
    std::vector<CampaignWorldSite> mSites;
    CampaignWorldRect mProgressPanel;
    CampaignWorldRect mTitleCartouche;
    std::map<uint32_t, size_t> mProvinceByMask;
    int mIdMapWidth;
    int mIdMapHeight;
    std::vector<uint8_t> mIdMap;
};

#endif // CAMPAIGNWORLD_H
