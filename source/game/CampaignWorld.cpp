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

#include "game/CampaignWorld.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <istream>

namespace
{
typedef boost::property_tree::ptree Tree;

uint32_t maskKey(int red, int green, int blue)
{
    return (static_cast<uint32_t>(red) << 16) | (static_cast<uint32_t>(green) << 8) | static_cast<uint32_t>(blue);
}

//! Reads a JSON array of integers; returns false if it has fewer than count entries
bool readInts(const Tree& node, size_t count, std::vector<int>& values)
{
    values.clear();
    for(Tree::const_iterator it = node.begin(); it != node.end(); ++it)
        values.push_back(it->second.get_value<int>());
    return values.size() >= count;
}

bool readRect(const Tree& parent, const char* key, CampaignWorldRect& rect)
{
    boost::optional<const Tree&> node = parent.get_child_optional(key);
    std::vector<int> values;
    if(!node || !readInts(*node, 4, values))
        return false;

    rect.mX = values[0];
    rect.mY = values[1];
    rect.mWidth = values[2];
    rect.mHeight = values[3];
    return true;
}

bool readPoint(const Tree& parent, const char* key, int& x, int& y)
{
    boost::optional<const Tree&> node = parent.get_child_optional(key);
    std::vector<int> values;
    if(!node || !readInts(*node, 2, values))
        return false;

    x = values[0];
    y = values[1];
    return true;
}
} // namespace

CampaignWorld::CampaignWorld():
    mWidth(0),
    mHeight(0),
    mIdMapWidth(0),
    mIdMapHeight(0)
{
}

bool CampaignWorld::importDefinition(std::istream& is)
{
    mWidth = 0;
    mHeight = 0;
    mProvinces.clear();
    mSites.clear();
    mProvinceByMask.clear();
    mProgressPanel = CampaignWorldRect();
    mTitleCartouche = CampaignWorldRect();

    try
    {
        Tree root;
        boost::property_tree::read_json(is, root);

        std::vector<int> values;
        if(!readInts(root.get_child("size"), 2, values))
            return false;
        mWidth = values[0];
        mHeight = values[1];

        const Tree& provinces = root.get_child("provinces");
        for(Tree::const_iterator it = provinces.begin(); it != provinces.end(); ++it)
        {
            const Tree& node = it->second;
            CampaignWorldProvince province;
            province.mId = node.get<std::string>("id");
            province.mName = node.get<std::string>("name");
            if(!readInts(node.get_child("mask_rgb"), 3, values))
                return false;
            province.mMaskRed = static_cast<uint8_t>(values[0]);
            province.mMaskGreen = static_cast<uint8_t>(values[1]);
            province.mMaskBlue = static_cast<uint8_t>(values[2]);
            province.mLayerLocked = node.get<std::string>("layers.locked");
            province.mLayerAvailable = node.get<std::string>("layers.available");
            province.mLayerConquered = node.get<std::string>("layers.conquered");
            province.mLayerLift = node.get<std::string>("layers.lift");
            if(!readPoint(node, "layer_origin", province.mLayerX, province.mLayerY)
                || !readPoint(node, "lift_origin", province.mLiftX, province.mLiftY))
                return false;

            mProvinceByMask[maskKey(province.mMaskRed, province.mMaskGreen, province.mMaskBlue)] = mProvinces.size();
            mProvinces.push_back(province);
        }

        const Tree& constRoot = root;
        boost::optional<const Tree&> sites = constRoot.get_child_optional("sites");
        if(sites)
        {
            for(Tree::const_iterator it = sites->begin(); it != sites->end(); ++it)
            {
                const Tree& node = it->second;
                CampaignWorldSite site;
                site.mId = node.get<std::string>("id");
                site.mName = node.get<std::string>("name");
                site.mHost = node.get<std::string>("host");
                if(!readPoint(node, "pos", site.mX, site.mY))
                    return false;
                site.mIconHidden = node.get<std::string>("icons.hidden");
                site.mIconFound = node.get<std::string>("icons.found");
                site.mIconDone = node.get<std::string>("icons.done");
                mSites.push_back(site);
            }
        }

        if(!readRect(root, "progress_panel", mProgressPanel) || !readRect(root, "title_cartouche", mTitleCartouche))
            return false;
    }
    catch(const boost::property_tree::ptree_error&)
    {
        mProvinces.clear();
        mSites.clear();
        mProvinceByMask.clear();
        return false;
    }

    return (mWidth > 0) && (mHeight > 0) && !mProvinces.empty();
}

size_t CampaignWorld::findProvince(const std::string& id) const
{
    for(size_t i = 0; i < mProvinces.size(); ++i)
    {
        if(mProvinces[i].mId == id)
            return i;
    }
    return mProvinces.size();
}

void CampaignWorld::setIdMap(int width, int height, const std::vector<uint8_t>& rgb)
{
    if((width <= 0) || (height <= 0) || (rgb.size() < static_cast<size_t>(width) * height * 3))
    {
        mIdMapWidth = 0;
        mIdMapHeight = 0;
        mIdMap.clear();
        return;
    }

    mIdMapWidth = width;
    mIdMapHeight = height;
    mIdMap = rgb;
}

size_t CampaignWorld::getProvinceAt(int x, int y) const
{
    if((x < 0) || (y < 0) || (x >= mIdMapWidth) || (y >= mIdMapHeight))
        return mProvinces.size();

    size_t offset = (static_cast<size_t>(y) * mIdMapWidth + x) * 3;
    std::map<uint32_t, size_t>::const_iterator it = mProvinceByMask.find(
        maskKey(mIdMap[offset], mIdMap[offset + 1], mIdMap[offset + 2]));
    if(it == mProvinceByMask.end())
        return mProvinces.size();

    return it->second;
}
