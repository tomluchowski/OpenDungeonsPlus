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

#ifndef SOCIALDATA_H
#define SOCIALDATA_H

#include <istream>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

namespace social
{

//! \brief Name pools of one group of creature classes (see config/social-names.cfg).
struct NameGroup
{
    NameGroup() :
        mAgeMin(1),
        mAgeMax(30)
    {
        mGenderWeights[0] = 1;
        mGenderWeights[1] = 1;
        mGenderWeights[2] = 1;
    }

    //! Name of the group ("monster" is the fallback group)
    std::string mName;
    //! Creature classes served by the group
    std::vector<std::string> mClasses;
    //! Lowest age a creature of the group can get
    int32_t mAgeMin;
    //! Highest age a creature of the group can get
    int32_t mAgeMax;
    //! Weights of female, male and unspecified
    uint32_t mGenderWeights[3];
    //! Given names for female, male and unspecified
    std::vector<std::string> mGiven[3];
    //! Surnames, used for some creatures instead of a title
    std::vector<std::string> mSurnames;
    //! Titles, used for some creatures instead of a surname
    std::vector<std::string> mTitles;
    std::vector<std::string> mHometowns;
    //! Replacements of the age number for some creatures
    std::vector<std::string> mAgeJokes;
};

//! \brief A text of config/social-texts.cfg together with the scope it applies to
//! ("*", a group name, a creature class name, "worker" or "fighter").
struct ScopedText
{
    std::string mScope;
    std::string mText;
};

//! \brief Loads config/social-names.cfg and config/social-texts.cfg.
//! The files are read line based (columns separated by tabs, values may contain spaces).
//! Nothing here needs Ogre, CEGUI or the game: errors are collected and can be read with
//! getErrors() so that the caller can log them once. A missing or broken file never
//! throws; the generator then falls back to built-in texts.
class SocialData
{
public:
    static const std::string FALLBACK_GROUP;

    SocialData();

    //! \brief Loads both files from the given directory (with a trailing slash or not).
    //! Returns true if both could be read without errors.
    bool loadFromDirectory(const std::string& directory);
    bool loadNamesFile(const std::string& path);
    bool loadTextsFile(const std::string& path);
    bool loadNames(std::istream& input, const std::string& source);
    bool loadTexts(std::istream& input, const std::string& source);

    //! \brief Group serving the class. Classes without a group use the group "monster"
    //! (and are remembered in getUnmappedClasses()).
    const NameGroup& getGroupForClass(const std::string& className) const;

    //! \brief True if a name group explicitly lists the class.
    bool hasGroupForClass(const std::string& className) const;

    const std::vector<NameGroup>& getGroups() const
    { return mGroups; }

    //! \brief Texts of the given key ("Job", "Like", "MoodLine:Hungry", "Post:eat", ...)
    //! whose scope is one of the given scopes, in file order.
    void getTexts(const std::string& key, const std::vector<std::string>& scopes,
        std::vector<std::string>& texts) const;

    //! \brief Number of texts stored for a key, whatever the scope.
    uint32_t getTextCount(const std::string& key) const;

    const std::vector<std::string>& getErrors() const
    { return mErrors; }

    const std::set<std::string>& getUnmappedClasses() const
    { return mUnmappedClasses; }

private:
    void clearNames();
    void clearTexts();
    void ensureFallbackGroup();
    bool parseNameGroupLine(const std::vector<std::string>& fields, NameGroup& group, std::string& error);
    void addError(const std::string& source, uint32_t lineNumber, const std::string& message);

    //! All name groups in file order
    std::vector<NameGroup> mGroups;
    //! Index into mGroups for every listed creature class
    std::map<std::string, std::size_t> mClassToGroup;
    //! Texts by key, in file order
    std::map<std::string, std::vector<ScopedText> > mTexts;
    //! Messages of everything that could not be read
    std::vector<std::string> mErrors;
    //! Classes asked for that no group lists (filled by const lookups)
    mutable std::set<std::string> mUnmappedClasses;
};

}

#endif // SOCIALDATA_H
