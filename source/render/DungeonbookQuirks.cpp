/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/DungeonbookQuirks.h"

#include "game/CreatureAppearance.h"
#include "render/PortraitManifest.h"

#include <algorithm>
#include <fstream>
#include <sstream>

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

//! Candidate with the value that decides whether it is picked
struct ScoredRemark
{
    uint32_t mScore;
    std::size_t mIndex;
};

bool isLowerScore(const ScoredRemark& a, const ScoredRemark& b)
{
    if(a.mScore != b.mScore)
        return a.mScore < b.mScore;
    return a.mIndex < b.mIndex;
}

bool isLowerIndex(const ScoredRemark& a, const ScoredRemark& b)
{
    return a.mIndex < b.mIndex;
}
}

bool DungeonbookQuirks::loadFromFile(const std::string& path)
{
    mTexts.clear();
    mWarnings.clear();
    std::ifstream file(path.c_str());
    if(!file.is_open())
    {
        mWarnings.push_back(path + ": cannot open the Dungeonbook quirks, no remarks are shown");
        return false;
    }
    loadFromStream(file, path);
    return true;
}

void DungeonbookQuirks::loadFromStream(std::istream& is, const std::string& source)
{
    mTexts.clear();
    mWarnings.clear();

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

        std::vector<std::string> columns;
        std::string::size_type start = 0;
        while(true)
        {
            std::string::size_type tab = line.find('\t', start);
            if(tab == std::string::npos)
            {
                columns.push_back(trimBlanks(line.substr(start)));
                break;
            }
            columns.push_back(trimBlanks(line.substr(start, tab - start)));
            start = tab + 1;
        }

        std::ostringstream where;
        where << source << ":" << lineNumber << ": ";
        if((columns.size() != 3) || columns[0].empty() || columns[1].empty() || columns[2].empty())
        {
            mWarnings.push_back(where.str() + "expected slot, option name and text separated by TAB, line dropped");
            continue;
        }

        std::pair<std::string, std::string> key(columns[0], columns[1]);
        if(mTexts.count(key) != 0)
        {
            mWarnings.push_back(where.str() + "repeated entry " + columns[0] + " " + columns[1] + ", line dropped");
            continue;
        }
        mTexts[key] = columns[2];
    }
}

const std::string* DungeonbookQuirks::find(const std::string& slot, const std::string& optionName) const
{
    std::map<std::pair<std::string, std::string>, std::string>::const_iterator it =
        mTexts.find(std::pair<std::string, std::string>(slot, optionName));
    if(it == mTexts.end())
        return nullptr;
    return &it->second;
}

namespace DungeonbookQuirkLogic
{
const uint32_t MAX_REMARKS = 2;

std::vector<std::string> collectRemarks(const DungeonbookQuirks& quirks, const PortraitManifest& manifest,
    const CreatureAppearance& appearance)
{
    std::vector<std::string> candidates;
    const std::vector<CreatureAppearance::Choice>& choices = appearance.getChoices();
    for(std::vector<CreatureAppearance::Choice>::const_iterator it = choices.begin(); it != choices.end(); ++it)
    {
        const PortraitManifest::Option* option = manifest.findOption(it->mSlot, it->mNumber);
        if(option == nullptr)
            continue;

        const std::string* text = quirks.find(it->mSlot, option->mName);
        if(text != nullptr)
            candidates.push_back(*text);
    }
    return candidates;
}

std::vector<std::string> pickRemarks(const std::vector<std::string>& candidates, const std::string& creatureName,
    const CreatureAppearance& appearance, uint32_t maxCount)
{
    if(candidates.size() <= maxCount)
        return candidates;

    const std::string seed = creatureName + "|" + CreatureAppearanceLogic::toToken(appearance) + "|";
    std::vector<ScoredRemark> scored;
    for(std::size_t i = 0; i < candidates.size(); ++i)
    {
        ScoredRemark remark;
        remark.mScore = CreatureAppearanceLogic::stableHash(seed + candidates[i]);
        remark.mIndex = i;
        scored.push_back(remark);
    }

    std::sort(scored.begin(), scored.end(), isLowerScore);
    scored.resize(maxCount);
    std::sort(scored.begin(), scored.end(), isLowerIndex);

    std::vector<std::string> result;
    for(std::vector<ScoredRemark>::const_iterator it = scored.begin(); it != scored.end(); ++it)
        result.push_back(candidates[it->mIndex]);
    return result;
}

std::vector<std::string> selectRemarks(const DungeonbookQuirks& quirks, const PortraitManifest& manifest,
    const CreatureAppearance& appearance, const std::string& creatureName)
{
    return pickRemarks(collectRemarks(quirks, manifest, appearance), creatureName, appearance, MAX_REMARKS);
}

std::string formatRemarks(const std::vector<std::string>& remarks)
{
    if(remarks.empty())
        return std::string();

    std::string text = "Quirks:";
    for(std::vector<std::string>::const_iterator it = remarks.begin(); it != remarks.end(); ++it)
        text += "\n- " + *it;
    return text;
}
}
