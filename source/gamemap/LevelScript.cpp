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

#include "gamemap/LevelScript.h"

#include <algorithm>
#include <cstdlib>
#include <istream>
#include <ostream>
#include <sstream>

namespace
{

//! \brief Reads a decimal number. Returns false if the text is empty or has other characters.
bool parseInt(const std::string& str, int64_t& value)
{
    if(str.empty())
        return false;

    char* end = nullptr;
    long long parsed = std::strtoll(str.c_str(), &end, 10);
    if(*end != '\0')
        return false;

    value = static_cast<int64_t>(parsed);
    return true;
}

//! \brief Reads a decimal number that fits 32 bits (as parseInt).
bool parseInt32(const std::string& str, int32_t& value)
{
    int64_t tmp = 0;
    if(!parseInt(str, tmp))
        return false;

    value = static_cast<int32_t>(tmp);
    return true;
}

//! \brief Removes spaces, tabs and line ends from both ends of the text.
std::string trim(const std::string& str)
{
    std::string::size_type first = str.find_first_not_of(" \t\r\n");
    if(first == std::string::npos)
        return std::string();

    std::string::size_type last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

//! \brief Splits the line into the tokens that are separated by white space.
std::vector<std::string> split(const std::string& line)
{
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string token;
    while(ss >> token)
        tokens.push_back(token);

    return tokens;
}

//! \brief Returns what follows the first count tokens of the line, trimmed.
std::string restOfLine(const std::string& line, uint32_t count)
{
    std::string::size_type pos = 0;
    for(uint32_t i = 0; i < count; ++i)
    {
        pos = line.find_first_not_of(" \t", pos);
        if(pos == std::string::npos)
            return std::string();

        pos = line.find_first_of(" \t", pos);
        if(pos == std::string::npos)
            return std::string();
    }
    return trim(line.substr(pos));
}

//! \brief Reads a comparison operator into the condition. Returns false if the token is none.
bool parseOperator(const std::string& token, LevelScriptCondition& cond)
{
    if(!levelScriptParseCompare(token, cond.mCompare))
        return false;

    cond.mAtLeast = (cond.mCompare == LevelScriptCompare::atLeast);
    return true;
}

//! \brief Reads a Cond line.
//! \param t The tokens of the line, t[0] is "Cond".
//! \param cond The condition to fill.
//! \return False if the line is malformed.
bool parseCondition(const std::vector<std::string>& t, LevelScriptCondition& cond)
{
    // t[0] is "Cond"
    if(t.size() < 2)
        return false;

    const std::string& type = t[1];
    if(type == "time")
    {
        cond.mType = LevelScriptConditionType::time;
        return (t.size() == 3) && parseInt(t[2], cond.mNumber);
    }
    if(type == "region")
    {
        cond.mType = LevelScriptConditionType::region;
        cond.mNumber = 1;
        int32_t corner = 0;
        if((t.size() == 4) || ((t.size() >= 6) && !parseInt32(t[3], corner)))
        {
            cond.mName = t[3];
            if(t.size() >= 6)
            {
                if(!parseOperator(t[4], cond) || !parseInt(t[5], cond.mNumber))
                    return false;
            }
            if(t.size() == 7)
                cond.mName2 = t[6];
            else if(t.size() > 7)
                return false;

            return parseInt32(t[2], cond.mSeatId);
        }
        return (t.size() == 7) &&
            parseInt32(t[2], cond.mSeatId) &&
            parseInt32(t[3], cond.mX1) &&
            parseInt32(t[4], cond.mY1) &&
            parseInt32(t[5], cond.mX2) &&
            parseInt32(t[6], cond.mY2);
    }
    if(type == "creatures")
    {
        cond.mType = LevelScriptConditionType::creatures;
        if((t.size() != 5) && (t.size() != 6))
            return false;
        if(!parseOperator(t[3], cond))
            return false;
        if(t.size() == 6)
            cond.mName2 = t[5];

        return parseInt32(t[2], cond.mSeatId) && parseInt(t[4], cond.mNumber);
    }
    if((type == "gold") || (type == "mana") || (type == "mined"))
    {
        if(type == "gold")
            cond.mType = LevelScriptConditionType::gold;
        else if(type == "mana")
            cond.mType = LevelScriptConditionType::mana;
        else
            cond.mType = LevelScriptConditionType::goldMined;

        if(t.size() != 5)
            return false;
        if(!parseOperator(t[3], cond))
            return false;

        return parseInt32(t[2], cond.mSeatId) && parseInt(t[4], cond.mNumber);
    }
    if(type == "claimed")
    {
        cond.mType = LevelScriptConditionType::claimed;
        if(t.size() != 5)
            return false;

        cond.mName = t[3];
        if(t[4] == "all")
        {
            cond.mNumber = -1;
            return parseInt32(t[2], cond.mSeatId);
        }
        return parseInt32(t[2], cond.mSeatId) && parseInt(t[4], cond.mNumber) && (cond.mNumber > 0);
    }
    if(type == "room")
    {
        cond.mType = LevelScriptConditionType::room;
        if((t.size() != 5) && (t.size() != 6))
            return false;

        cond.mName = t[3];
        if(t.size() == 6)
            return parseInt32(t[2], cond.mSeatId) && parseOperator(t[4], cond) && parseInt(t[5], cond.mNumber);

        return parseInt32(t[2], cond.mSeatId) && parseInt(t[4], cond.mNumber);
    }
    if(type == "goal")
    {
        cond.mType = LevelScriptConditionType::goal;
        if(t.size() != 4)
            return false;

        cond.mName = t[3];
        return parseInt32(t[2], cond.mSeatId);
    }
    if(type == "flag")
    {
        cond.mType = LevelScriptConditionType::flag;
        if((t.size() != 4) && (t.size() != 5))
            return false;

        cond.mName = t[2];
        cond.mCompare = LevelScriptCompare::equal;
        cond.mAtLeast = false;
        if(t.size() == 5)
        {
            if(!parseOperator(t[3], cond))
                return false;
        }
        const std::string& value = t[t.size() - 1];
        if(!value.empty() && (value[0] == '@'))
        {
            cond.mName2 = value.substr(1);
            return (t.size() == 5) && !cond.mName2.empty();
        }
        return parseInt(value, cond.mNumber);
    }
    if(type == "event")
    {
        cond.mType = LevelScriptConditionType::creatureEvent;
        cond.mNumber = 1;
        if((t.size() != 4) && (t.size() != 6))
            return false;

        cond.mName = t[2];
        cond.mName2 = t[3];
        if(t.size() == 6)
            return parseOperator(t[4], cond) && parseInt(t[5], cond.mNumber);

        return true;
    }
    if(type == "portal")
    {
        cond.mType = LevelScriptConditionType::portalActive;
        if((t.size() != 4) || ((t[3] != "on") && (t[3] != "off")))
            return false;

        cond.mNumber = (t[3] == "on") ? 1 : 0;
        return parseInt32(t[2], cond.mSeatId);
    }
    if(type == "alive")
    {
        cond.mType = LevelScriptConditionType::creatureAlive;
        if((t.size() != 3) && (t.size() != 4))
            return false;

        cond.mName = t[2];
        cond.mNumber = 1;
        if(t.size() == 4)
        {
            if((t[3] != "0") && (t[3] != "1"))
                return false;

            cond.mNumber = (t[3] == "1") ? 1 : 0;
        }
        return true;
    }
    if(type == "reached")
    {
        cond.mType = LevelScriptConditionType::creatureReached;
        if(t.size() != 5)
            return false;

        cond.mName = t[2];
        if(t[3] == "region")
        {
            cond.mName2 = t[4];
            return !cond.mName2.empty();
        }
        return (t[3] == "heart") && parseInt32(t[4], cond.mSeatId);
    }
    if(type == "defeated")
    {
        cond.mType = LevelScriptConditionType::seatDefeated;
        return (t.size() == 3) && parseInt32(t[2], cond.mSeatId);
    }
    if((type == "owns") || (type == "spell"))
    {
        cond.mType = (type == "owns") ? LevelScriptConditionType::ownsCreature : LevelScriptConditionType::spellKnown;
        if(t.size() != 4)
            return false;

        cond.mName = t[3];
        return parseInt32(t[2], cond.mSeatId);
    }
    if(type == "health")
    {
        cond.mType = LevelScriptConditionType::creatureHealth;
        if(t.size() != 5)
            return false;

        cond.mName = t[2];
        return parseOperator(t[3], cond) && parseInt(t[4], cond.mNumber);
    }
    if((type == "built") || (type == "roomtiles") || (type == "roomsize"))
    {
        if(type == "built")
            cond.mType = LevelScriptConditionType::trapsBuilt;
        else if(type == "roomtiles")
            cond.mType = LevelScriptConditionType::roomTiles;
        else
            cond.mType = LevelScriptConditionType::largestRoom;

        if(t.size() != 6)
            return false;

        cond.mName = t[3];
        return parseInt32(t[2], cond.mSeatId) && parseOperator(t[4], cond) && parseInt(t[5], cond.mNumber);
    }
    if(type == "timer")
    {
        cond.mType = LevelScriptConditionType::timer;
        if(t.size() != 5)
            return false;

        cond.mName = t[2];
        return parseOperator(t[3], cond) && parseInt(t[4], cond.mNumber);
    }
    return false;
}

//! \brief Reads an Action line.
//! \param line The whole line (a message keeps its spaces).
//! \param t The tokens of the line, t[0] is "Action".
//! \param action The action to fill.
//! \return False if the line is malformed.
bool parseAction(const std::string& line, const std::vector<std::string>& t, LevelScriptAction& action)
{
    // t[0] is "Action"
    if(t.size() < 2)
        return false;

    const std::string& type = t[1];
    if((type == "message") || (type == "objective"))
    {
        action.mType = (type == "message") ? LevelScriptActionType::message : LevelScriptActionType::objective;
        if(t.size() < 4)
            return false;

        action.mText = restOfLine(line, 3);
        return parseInt32(t[2], action.mSeatId) && !action.mText.empty();
    }
    if(type == "spawn")
    {
        action.mType = LevelScriptActionType::spawn;
        if(t.size() < 7)
            return false;

        if(!parseInt32(t[2], action.mSeatId) ||
           !parseInt32(t[3], action.mX) ||
           !parseInt32(t[4], action.mY) ||
           !parseInt32(t[5], action.mTargetSeatId))
        {
            return false;
        }

        for(std::size_t i = 6; i < t.size(); ++i)
        {
            if(t[i].compare(0, 6, "party=") == 0)
            {
                action.mParty = t[i].substr(6);
                if(action.mParty.empty())
                    return false;

                continue;
            }

            std::string::size_type colon = t[i].find(':');
            if((colon == std::string::npos) || (colon == 0))
                return false;

            std::string levelText = t[i].substr(colon + 1);
            std::string creatureName;
            std::string::size_type at = levelText.find('@');
            if(at != std::string::npos)
            {
                creatureName = levelText.substr(at + 1);
                levelText = levelText.substr(0, at);
                if(creatureName.empty())
                    return false;
            }

            int64_t level = 0;
            if(!parseInt(levelText, level) || (level < 1))
                return false;

            action.mCreatures.push_back(std::pair<std::string, uint32_t>(t[i].substr(0, colon), static_cast<uint32_t>(level)));
            action.mCreatureNames.push_back(creatureName);
        }
        return true;
    }
    if(type == "gold")
    {
        action.mType = LevelScriptActionType::gold;
        return (t.size() == 4) && parseInt32(t[2], action.mSeatId) && parseInt(t[3], action.mNumber);
    }
    if(type == "setflag")
    {
        action.mType = LevelScriptActionType::setFlag;
        if(t.size() != 4)
            return false;

        action.mText = t[2];
        return parseInt(t[3], action.mNumber);
    }
    if(type == "addflag")
    {
        action.mType = LevelScriptActionType::addFlag;
        if(t.size() != 4)
            return false;

        action.mText = t[2];
        return parseInt(t[3], action.mNumber);
    }
    if((type == "win") || (type == "lose"))
    {
        action.mType = (type == "win") ? LevelScriptActionType::win : LevelScriptActionType::lose;
        return (t.size() == 3) && parseInt32(t[2], action.mSeatId);
    }
    if(type == "reveal")
    {
        action.mType = LevelScriptActionType::reveal;
        if(t.size() != 4)
            return false;

        action.mText = t[3];
        return parseInt32(t[2], action.mSeatId);
    }
    if(type == "make")
    {
        action.mType = LevelScriptActionType::make;
        if(t.size() != 4)
            return false;

        action.mText = t[3];
        return parseInt32(t[2], action.mSeatId);
    }
    if(type == "timelimit")
    {
        action.mType = LevelScriptActionType::timeLimit;
        return (t.size() == 3) && parseInt(t[2], action.mNumber) && (action.mNumber >= 0);
    }
    if(type == "countdown")
    {
        action.mType = LevelScriptActionType::countdown;
        return (t.size() == 3) && parseInt(t[2], action.mNumber) && (action.mNumber >= 0);
    }
    if(type == "wavecountdown")
    {
        action.mType = LevelScriptActionType::waveCountdown;
        return (t.size() == 3) && parseInt(t[2], action.mNumber) && (action.mNumber >= 0);
    }
    if(type == "portal")
    {
        action.mType = LevelScriptActionType::portalStatus;
        if((t.size() != 4) || ((t[3] != "on") && (t[3] != "off")))
            return false;

        action.mNumber = (t[3] == "on") ? 1 : 0;
        return parseInt32(t[2], action.mSeatId);
    }
    if(type == "available")
    {
        action.mType = LevelScriptActionType::creatureAvailable;
        if((t.size() != 5) || ((t[4] != "0") && (t[4] != "1")))
            return false;

        action.mText = t[3];
        action.mNumber = (t[4] == "1") ? 1 : 0;
        return parseInt32(t[2], action.mSeatId);
    }
    if(type == "remove")
    {
        action.mType = LevelScriptActionType::removeCreature;
        if(t.size() != 3)
            return false;

        action.mText = t[2];
        return true;
    }
    if(type == "timer")
    {
        action.mType = LevelScriptActionType::startTimer;
        if((t.size() != 3) && (t.size() != 4))
            return false;

        action.mText = t[2];
        return (t.size() == 3) || (parseInt(t[3], action.mNumber) && (action.mNumber >= 0));
    }
    return false;
}

//! \brief Writes a condition as one Cond line.
void writeCondition(std::ostream& os, const LevelScriptCondition& c)
{
    os << "Cond\t";
    switch(c.mType)
    {
        case LevelScriptConditionType::time:
            os << "time\t" << c.mNumber;
            break;
        case LevelScriptConditionType::region:
            if(!c.mName.empty())
            {
                os << "region\t" << c.mSeatId << "\t" << c.mName;
                if((c.mCompare != LevelScriptCompare::atLeast) || (c.mNumber != 1) || !c.mName2.empty())
                {
                    os << "\t" << levelScriptCompareToken(c.mCompare) << "\t" << c.mNumber;
                    if(!c.mName2.empty())
                        os << "\t" << c.mName2;
                }
            }
            else
                os << "region\t" << c.mSeatId << "\t" << c.mX1 << "\t" << c.mY1 << "\t" << c.mX2 << "\t" << c.mY2;
            break;
        case LevelScriptConditionType::creatures:
            os << "creatures\t" << c.mSeatId << "\t" << levelScriptCompareToken(c.mCompare) << "\t" << c.mNumber;
            if(!c.mName2.empty())
                os << "\t" << c.mName2;
            break;
        case LevelScriptConditionType::seatDefeated:
            os << "defeated\t" << c.mSeatId;
            break;
        case LevelScriptConditionType::portalActive:
            os << "portal\t" << c.mSeatId << "\t" << (c.mNumber != 0 ? "on" : "off");
            break;
        case LevelScriptConditionType::creatureAlive:
            os << "alive\t" << c.mName;
            if(c.mNumber == 0)
                os << "\t0";
            break;
        case LevelScriptConditionType::creatureReached:
            if(c.mName2.empty())
                os << "reached\t" << c.mName << "\theart\t" << c.mSeatId;
            else
                os << "reached\t" << c.mName << "\tregion\t" << c.mName2;
            break;
        case LevelScriptConditionType::creatureEvent:
            os << "event\t" << c.mName << "\t" << c.mName2;
            if((c.mCompare != LevelScriptCompare::atLeast) || (c.mNumber != 1))
                os << "\t" << levelScriptCompareToken(c.mCompare) << "\t" << c.mNumber;
            break;
        case LevelScriptConditionType::ownsCreature:
            os << "owns\t" << c.mSeatId << "\t" << c.mName;
            break;
        case LevelScriptConditionType::spellKnown:
            os << "spell\t" << c.mSeatId << "\t" << c.mName;
            break;
        case LevelScriptConditionType::creatureHealth:
            os << "health\t" << c.mName << "\t" << levelScriptCompareToken(c.mCompare) << "\t" << c.mNumber;
            break;
        case LevelScriptConditionType::trapsBuilt:
            os << "built\t" << c.mSeatId << "\t" << c.mName << "\t" << levelScriptCompareToken(c.mCompare) << "\t" << c.mNumber;
            break;
        case LevelScriptConditionType::roomTiles:
            os << "roomtiles\t" << c.mSeatId << "\t" << c.mName << "\t" << levelScriptCompareToken(c.mCompare) << "\t" << c.mNumber;
            break;
        case LevelScriptConditionType::largestRoom:
            os << "roomsize\t" << c.mSeatId << "\t" << c.mName << "\t" << levelScriptCompareToken(c.mCompare) << "\t" << c.mNumber;
            break;
        case LevelScriptConditionType::room:
            os << "room\t" << c.mSeatId << "\t" << c.mName << "\t";
            if(c.mCompare != LevelScriptCompare::atLeast)
                os << levelScriptCompareToken(c.mCompare) << "\t";

            os << c.mNumber;
            break;
        case LevelScriptConditionType::goal:
            os << "goal\t" << c.mSeatId << "\t" << c.mName;
            break;
        case LevelScriptConditionType::flag:
            if((c.mCompare == LevelScriptCompare::equal) && c.mName2.empty())
            {
                os << "flag\t" << c.mName << "\t" << c.mNumber;
            }
            else
            {
                os << "flag\t" << c.mName << "\t" << levelScriptCompareToken(c.mCompare) << "\t";
                if(c.mName2.empty())
                    os << c.mNumber;
                else
                    os << "@" << c.mName2;
            }
            break;
        case LevelScriptConditionType::timer:
            os << "timer\t" << c.mName << "\t" << levelScriptCompareToken(c.mCompare) << "\t" << c.mNumber;
            break;
        case LevelScriptConditionType::gold:
            os << "gold\t" << c.mSeatId << "\t" << levelScriptCompareToken(c.mCompare) << "\t" << c.mNumber;
            break;
        case LevelScriptConditionType::mana:
            os << "mana\t" << c.mSeatId << "\t" << levelScriptCompareToken(c.mCompare) << "\t" << c.mNumber;
            break;
        case LevelScriptConditionType::goldMined:
            os << "mined\t" << c.mSeatId << "\t" << levelScriptCompareToken(c.mCompare) << "\t" << c.mNumber;
            break;
        case LevelScriptConditionType::claimed:
            os << "claimed\t" << c.mSeatId << "\t" << c.mName << "\t";
            if(c.mNumber < 0)
                os << "all";
            else
                os << c.mNumber;
            break;
    }
    os << "\n";
}

//! \brief Writes an action as one Action line.
void writeAction(std::ostream& os, const LevelScriptAction& a)
{
    os << "Action\t";
    switch(a.mType)
    {
        case LevelScriptActionType::message:
            os << "message\t" << a.mSeatId << "\t" << a.mText;
            break;
        case LevelScriptActionType::objective:
            os << "objective\t" << a.mSeatId << "\t" << a.mText;
            break;
        case LevelScriptActionType::spawn:
            os << "spawn\t" << a.mSeatId << "\t" << a.mX << "\t" << a.mY << "\t" << a.mTargetSeatId;
            if(!a.mParty.empty())
                os << "\tparty=" << a.mParty;
            for(std::size_t i = 0; i < a.mCreatures.size(); ++i)
            {
                os << "\t" << a.mCreatures[i].first << ":" << a.mCreatures[i].second;
                if((i < a.mCreatureNames.size()) && !a.mCreatureNames[i].empty())
                    os << "@" << a.mCreatureNames[i];
            }
            break;
        case LevelScriptActionType::gold:
            os << "gold\t" << a.mSeatId << "\t" << a.mNumber;
            break;
        case LevelScriptActionType::setFlag:
            os << "setflag\t" << a.mText << "\t" << a.mNumber;
            break;
        case LevelScriptActionType::addFlag:
            os << "addflag\t" << a.mText << "\t" << a.mNumber;
            break;
        case LevelScriptActionType::win:
            os << "win\t" << a.mSeatId;
            break;
        case LevelScriptActionType::lose:
            os << "lose\t" << a.mSeatId;
            break;
        case LevelScriptActionType::reveal:
            os << "reveal\t" << a.mSeatId << "\t" << a.mText;
            break;
        case LevelScriptActionType::make:
            os << "make\t" << a.mSeatId << "\t" << a.mText;
            break;
        case LevelScriptActionType::timeLimit:
            os << "timelimit\t" << a.mNumber;
            break;
        case LevelScriptActionType::countdown:
            os << "countdown\t" << a.mNumber;
            break;
        case LevelScriptActionType::waveCountdown:
            os << "wavecountdown\t" << a.mNumber;
            break;
        case LevelScriptActionType::startTimer:
            os << "timer\t" << a.mText << "\t" << a.mNumber;
            break;
        case LevelScriptActionType::portalStatus:
            os << "portal\t" << a.mSeatId << "\t" << (a.mNumber != 0 ? "on" : "off");
            break;
        case LevelScriptActionType::creatureAvailable:
            os << "available\t" << a.mSeatId << "\t" << a.mText << "\t" << (a.mNumber != 0 ? 1 : 0);
            break;
        case LevelScriptActionType::removeCreature:
            os << "remove\t" << a.mText;
            break;
    }
    os << "\n";
}

} // namespace

const int64_t LevelScript::TIME_LIMIT_NOT_SET;
const int64_t LevelScript::TIME_LIMIT_REMOVED;
const int64_t LevelScript::COUNTDOWN_NOT_SET;

bool LevelScript::importFromStream(std::istream& is)
{
    clear();

    LevelScriptTrigger trigger;
    bool inTrigger = false;
    std::string rawLine;
    while(std::getline(is, rawLine))
    {
        std::string line = trim(rawLine);
        if(line.empty() || (line[0] == '#'))
            continue;

        if(line == "[/Triggers]")
            return !inTrigger;

        if(line == "[Trigger]")
        {
            if(inTrigger)
                return false;

            inTrigger = true;
            trigger = LevelScriptTrigger();
            continue;
        }

        if(line == "[/Trigger]")
        {
            if(!inTrigger || trigger.mConditions.empty() || trigger.mActions.empty())
                return false;

            inTrigger = false;
            mTriggers.push_back(trigger);
            continue;
        }

        std::vector<std::string> t = split(line);
        const std::string& key = t[0];
        if(key == "Flag")
        {
            int64_t value = 0;
            if(inTrigger || (t.size() != 3) || !parseInt(t[2], value))
                return false;

            mFlags[t[1]] = value;
        }
        else if(key == "Timer")
        {
            int64_t running = 0;
            LevelScriptTimer timer;
            if(inTrigger || (t.size() != 4) || !parseInt(t[2], running) || !parseInt(t[3], timer.mTurns))
                return false;

            timer.mRunning = (running != 0);
            mTimers[t[1]] = timer;
        }
        else if(key == "PortalOff")
        {
            int32_t seatId = 0;
            if(inTrigger || (t.size() != 2) || !parseInt32(t[1], seatId))
                return false;

            mPortalOff.insert(seatId);
        }
        else if(key == "Block")
        {
            int32_t seatId = 0;
            if(inTrigger || (t.size() != 3) || !parseInt32(t[1], seatId))
                return false;

            mBlocked.insert(std::make_pair(seatId, t[2]));
        }
        else if(key == "Event")
        {
            int64_t count = 0;
            if(inTrigger || (t.size() != 4) || !parseInt(t[3], count))
                return false;

            mEvents[std::make_pair(t[1], t[2])] = count;
        }
        else if(key == "Member")
        {
            if(inTrigger || (t.size() != 3))
                return false;

            mPartyMembers[t[1]] = t[2];
        }
        else if(key == "TimeLimit")
        {
            if(inTrigger || (t.size() != 2) || !parseInt(t[1], mTimeLimitSeconds))
                return false;
        }
        else if(key == "Countdown")
        {
            if(inTrigger || (t.size() != 2) || !parseInt(t[1], mCountdownSeconds))
                return false;
        }
        else if(key == "WaveCountdown")
        {
            if(inTrigger || (t.size() != 2) || !parseInt(t[1], mWaveCountdownSeconds))
                return false;
        }
        else if(key == "Region")
        {
            LevelScriptRegion region;
            if(inTrigger || (t.size() != 6) ||
               !parseInt32(t[2], region.mX1) || !parseInt32(t[3], region.mY1) ||
               !parseInt32(t[4], region.mX2) || !parseInt32(t[5], region.mY2))
            {
                return false;
            }

            region.mName = t[1];
            setRegion(region);
        }
        else if(!inTrigger)
        {
            return false;
        }
        else if(key == "Name")
        {
            if(t.size() != 2)
                return false;

            trigger.mName = t[1];
        }
        else if(key == "Mode")
        {
            if((t.size() == 2) && (t[1] == "once"))
            {
                trigger.mRepeat = false;
            }
            else if((t.size() == 3) && (t[1] == "repeat") && parseInt(t[2], trigger.mCooldownSeconds))
            {
                trigger.mRepeat = true;
            }
            else
            {
                return false;
            }
        }
        else if(key == "Cond")
        {
            LevelScriptCondition cond;
            if(!parseCondition(t, cond))
                return false;

            trigger.mConditions.push_back(cond);
        }
        else if(key == "Action")
        {
            LevelScriptAction action;
            if(!parseAction(line, t, action))
                return false;

            trigger.mActions.push_back(action);
        }
        else if(key == "State")
        {
            int64_t fired = 0;
            if((t.size() != 3) || !parseInt(t[1], fired) || !parseInt(t[2], trigger.mLastFiredTurn))
                return false;

            trigger.mTimesFired = static_cast<uint32_t>(fired);
        }
        else
        {
            return false;
        }
    }

    // The end tag is missing
    return false;
}

void LevelScript::exportToStream(std::ostream& os) const
{
    os << "[Triggers]\n";
    for(const std::pair<const std::string, int64_t>& flag : mFlags)
        os << "Flag\t" << flag.first << "\t" << flag.second << "\n";

    for(const std::pair<const std::string, LevelScriptTimer>& timer : mTimers)
        os << "Timer\t" << timer.first << "\t" << (timer.second.mRunning ? 1 : 0) << "\t" << timer.second.mTurns << "\n";

    for(int32_t seatId : mPortalOff)
        os << "PortalOff\t" << seatId << "\n";

    for(const std::pair<int32_t, std::string>& block : mBlocked)
        os << "Block\t" << block.first << "\t" << block.second << "\n";

    for(const std::pair<const std::pair<std::string, std::string>, int64_t>& event : mEvents)
        os << "Event\t" << event.first.first << "\t" << event.first.second << "\t" << event.second << "\n";

    for(const std::pair<const std::string, std::string>& member : mPartyMembers)
        os << "Member\t" << member.first << "\t" << member.second << "\n";

    if(mTimeLimitSeconds != TIME_LIMIT_NOT_SET)
        os << "TimeLimit\t" << mTimeLimitSeconds << "\n";

    if(mCountdownSeconds != COUNTDOWN_NOT_SET)
        os << "Countdown\t" << mCountdownSeconds << "\n";

    if(mWaveCountdownSeconds != COUNTDOWN_NOT_SET)
        os << "WaveCountdown\t" << mWaveCountdownSeconds << "\n";

    for(const LevelScriptRegion& region : mRegions)
    {
        os << "Region\t" << region.mName << "\t" << region.mX1 << "\t" << region.mY1
           << "\t" << region.mX2 << "\t" << region.mY2 << "\n";
    }

    for(const LevelScriptTrigger& trigger : mTriggers)
    {
        os << "[Trigger]\n";
        os << "Name\t" << trigger.mName << "\n";
        if(trigger.mRepeat)
            os << "Mode\trepeat\t" << trigger.mCooldownSeconds << "\n";
        else
            os << "Mode\tonce\n";

        for(const LevelScriptCondition& cond : trigger.mConditions)
            writeCondition(os, cond);

        for(const LevelScriptAction& action : trigger.mActions)
            writeAction(os, action);

        if(trigger.mTimesFired > 0)
            os << "State\t" << trigger.mTimesFired << "\t" << trigger.mLastFiredTurn << "\n";

        os << "[/Trigger]\n";
    }
    os << "[/Triggers]\n";
}

void LevelScript::clear()
{
    mTriggers.clear();
    mFlags.clear();
    mTimers.clear();
    mEvents.clear();
    mPortalOff.clear();
    mBlocked.clear();
    mPartyMembers.clear();
    mWatched.clear();
    mWatchedValid = false;
    mRegions.clear();
    mTimeLimitSeconds = TIME_LIMIT_NOT_SET;
    mCountdownSeconds = COUNTDOWN_NOT_SET;
    mWaveCountdownSeconds = COUNTDOWN_NOT_SET;
}

void LevelScript::rebaseTimeLimit(int64_t elapsedSeconds)
{
    if(mTimeLimitSeconds >= 0)
        mTimeLimitSeconds = std::max<int64_t>(0, mTimeLimitSeconds - elapsedSeconds);

    if(mCountdownSeconds >= 0)
        mCountdownSeconds = std::max<int64_t>(0, mCountdownSeconds - elapsedSeconds);

    if(mWaveCountdownSeconds >= 0)
        mWaveCountdownSeconds = std::max<int64_t>(0, mWaveCountdownSeconds - elapsedSeconds);
}

bool LevelScriptRegion::contains(int32_t x, int32_t y) const
{
    return (x >= std::min(mX1, mX2)) && (x <= std::max(mX1, mX2)) &&
           (y >= std::min(mY1, mY2)) && (y <= std::max(mY1, mY2));
}

const LevelScriptRegion* LevelScript::getRegion(const std::string& name) const
{
    for(const LevelScriptRegion& region : mRegions)
    {
        if(region.mName == name)
            return &region;
    }
    return nullptr;
}

void LevelScript::setRegion(const LevelScriptRegion& region)
{
    for(LevelScriptRegion& other : mRegions)
    {
        if(other.mName == region.mName)
        {
            other = region;
            return;
        }
    }
    mRegions.push_back(region);
}

bool LevelScript::removeRegion(const std::string& name)
{
    for(std::vector<LevelScriptRegion>::iterator it = mRegions.begin(); it != mRegions.end(); ++it)
    {
        if(it->mName == name)
        {
            mRegions.erase(it);
            return true;
        }
    }
    return false;
}

std::string LevelScript::getRegionNameAt(int32_t x, int32_t y) const
{
    for(const LevelScriptRegion& region : mRegions)
    {
        if(region.contains(x, y))
            return region.mName;
    }
    return std::string();
}

std::string LevelScript::getFreeRegionName() const
{
    for(uint32_t number = 1; ; ++number)
    {
        std::ostringstream name;
        name << "Region" << number;
        if(getRegion(name.str()) == nullptr)
            return name.str();
    }
}

void LevelScript::setRegions(const std::vector<LevelScriptRegion>& regions)
{
    mRegions = regions;
}

int64_t LevelScript::getFlag(const std::string& name) const
{
    std::map<std::string, int64_t>::const_iterator it = mFlags.find(name);
    if(it == mFlags.end())
        return 0;

    return it->second;
}

void LevelScript::setFlag(const std::string& name, int64_t value)
{
    mFlags[name] = value;
}

int64_t LevelScript::getTimerTurns(const std::string& name) const
{
    std::map<std::string, LevelScriptTimer>::const_iterator it = mTimers.find(name);
    if(it == mTimers.end())
        return 0;

    return it->second.mTurns;
}

void LevelScript::startTimer(const std::string& name, int64_t turns)
{
    LevelScriptTimer& timer = mTimers[name];
    timer.mRunning = true;
    timer.mTurns = turns;
}

void LevelScript::advanceTimers()
{
    for(std::pair<const std::string, LevelScriptTimer>& timer : mTimers)
    {
        if(timer.second.mRunning)
            ++timer.second.mTurns;
    }
}

bool levelScriptCompare(int64_t value, LevelScriptCompare op, int64_t reference)
{
    switch(op)
    {
        case LevelScriptCompare::atLeast:
            return value >= reference;
        case LevelScriptCompare::atMost:
            return value <= reference;
        case LevelScriptCompare::equal:
            return value == reference;
        case LevelScriptCompare::notEqual:
            return value != reference;
        case LevelScriptCompare::greater:
            return value > reference;
        case LevelScriptCompare::less:
            return value < reference;
    }
    return false;
}

const char* levelScriptCompareToken(LevelScriptCompare op)
{
    switch(op)
    {
        case LevelScriptCompare::atLeast:
            return ">=";
        case LevelScriptCompare::atMost:
            return "<=";
        case LevelScriptCompare::equal:
            return "==";
        case LevelScriptCompare::notEqual:
            return "!=";
        case LevelScriptCompare::greater:
            return ">";
        case LevelScriptCompare::less:
            return "<";
    }
    return "==";
}

bool levelScriptParseCompare(const std::string& token, LevelScriptCompare& op)
{
    static const LevelScriptCompare all[] = {
        LevelScriptCompare::atLeast, LevelScriptCompare::atMost, LevelScriptCompare::equal,
        LevelScriptCompare::notEqual, LevelScriptCompare::greater, LevelScriptCompare::less};
    for(LevelScriptCompare candidate : all)
    {
        if(token == levelScriptCompareToken(candidate))
        {
            op = candidate;
            return true;
        }
    }
    return false;
}

void LevelScript::recordEvent(const std::string& creatureName, const std::string& eventName)
{
    if(!mWatchedValid)
    {
        mWatched.clear();
        for(const LevelScriptTrigger& trigger : mTriggers)
        {
            for(const LevelScriptCondition& cond : trigger.mConditions)
            {
                if(cond.mType == LevelScriptConditionType::creatureEvent)
                    mWatched.insert(cond.mName);
            }
        }
        mWatchedValid = true;
    }

    if(mWatched.empty())
        return;

    if(mWatched.count(creatureName) > 0)
        ++mEvents[std::make_pair(creatureName, eventName)];

    std::map<std::string, std::string>::const_iterator member = mPartyMembers.find(creatureName);
    if((member != mPartyMembers.end()) && (mWatched.count(member->second) > 0))
        ++mEvents[std::make_pair(member->second, eventName)];
}

int64_t LevelScript::getEventCount(const std::string& tag, const std::string& eventName) const
{
    std::map<std::pair<std::string, std::string>, int64_t>::const_iterator it =
        mEvents.find(std::make_pair(tag, eventName));
    if(it == mEvents.end())
        return 0;

    return it->second;
}

void LevelScript::addPartyMember(const std::string& party, const std::string& creatureName)
{
    mPartyMembers[creatureName] = party;
}

std::vector<std::string> LevelScript::getPartyMemberNames(const std::string& party) const
{
    std::vector<std::string> names;
    for(const std::pair<const std::string, std::string>& member : mPartyMembers)
    {
        if(member.second == party)
            names.push_back(member.first);
    }
    return names;
}

void LevelScript::setPortalOff(int32_t seatId, bool off)
{
    if(off)
        mPortalOff.insert(seatId);
    else
        mPortalOff.erase(seatId);
}

bool LevelScript::isPortalOff(int32_t seatId) const
{
    return mPortalOff.count(seatId) > 0;
}

void LevelScript::setCreatureBlocked(int32_t seatId, const std::string& className, bool blocked)
{
    if(blocked)
        mBlocked.insert(std::make_pair(seatId, className));
    else
        mBlocked.erase(std::make_pair(seatId, className));
}

bool LevelScript::isCreatureBlocked(int32_t seatId, const std::string& className) const
{
    return mBlocked.count(std::make_pair(seatId, className)) > 0;
}
