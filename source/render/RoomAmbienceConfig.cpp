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

#include "render/RoomAmbienceConfig.h"

#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace
{

//! \brief Reads the next not empty line and splits it in words. Returns false at the end of the file
bool readWords(std::istream& file, std::vector<std::string>& words)
{
    words.clear();
    std::string line;
    if(!Helper::readNextLineNotEmpty(file, line))
        return false;

    std::istringstream lineStream(line);
    std::string word;
    while(lineStream >> word)
        words.push_back(word);

    return true;
}

//! \brief Reads a yes/no value of the configuration file ("yes", "true" and "1" are true)
bool toBool(const std::string& text)
{
    return (text == "yes") || (text == "true") || (text == "1");
}

} // namespace

RoomAmbienceConfig::RoomAmbienceConfig() :
    mScanInterval(0.4),
    mMaxParticles(48),
    mMaxParticlesReduced(10),
    mMaxMotions(40),
    mMaxOneShots(8),
    mMaxMarks(6),
    mMaxFlights(6),
    mOccupiedRadius(2.2),
    mReducedDistanceFactor(0.55)
{
}

bool RoomAmbienceConfig::whenFromString(const std::string& text, AmbienceWhen& when)
{
    if(text == "Always")
        when = AmbienceWhen::always;
    else if(text == "Occupied")
        when = AmbienceWhen::occupied;
    else if(text == "Empty")
        when = AmbienceWhen::empty;
    else if(text == "Hit")
        when = AmbienceWhen::hit;
    else if(text == "Locked")
        when = AmbienceWhen::locked;
    else if(text == "Reloading")
        when = AmbienceWhen::reloading;
    else if(text == "Ready")
        when = AmbienceWhen::ready;
    else if(text == "LowHealth")
        when = AmbienceWhen::lowHealth;
    else if(text == "Vacated")
        when = AmbienceWhen::vacated;
    else if(text == "Sleeping")
        when = AmbienceWhen::sleeping;
    else
        return false;

    return true;
}

bool RoomAmbienceConfig::motionFromString(const std::string& text, AmbienceMotion& motion)
{
    if(text == "Sway")
        motion = AmbienceMotion::sway;
    else if(text == "Wobble")
        motion = AmbienceMotion::wobble;
    else if(text == "Spin")
        motion = AmbienceMotion::spin;
    else if(text == "Bob")
        motion = AmbienceMotion::bob;
    else if(text == "Pulse")
        motion = AmbienceMotion::pulse;
    else if(text == "Flicker")
        motion = AmbienceMotion::flicker;
    else
        return false;

    return true;
}

bool RoomAmbienceConfig::load(const std::string& fileName)
{
    mEffects.clear();
    return loadFile(fileName, false);
}

bool RoomAmbienceConfig::loadFile(const std::string& fileName, bool included)
{
    std::stringstream defFile;
    if(included)
    {
        // A missing included file is not an error: it only means that part is not installed
        std::ifstream probe(fileName.c_str());
        if(!probe.good())
        {
            OD_LOG_INF("Room ambience file not present, skipped: " + fileName);
            return true;
        }
    }

    OD_LOG_INF("Load room ambience file: " + fileName);
    if(!Helper::readFile(fileName, defFile, true))
    {
        OD_LOG_ERR("Couldn't read " + fileName);
        return false;
    }

    std::vector<std::string> words;
    if(!readWords(defFile, words) || (words.size() != 1) || (words[0] != "[RoomAmbience]"))
    {
        OD_LOG_ERR("Invalid room ambience start format in " + fileName);
        return false;
    }

    std::string folder;
    std::string::size_type slash = fileName.find_last_of("/\\");
    if(slash != std::string::npos)
        folder = fileName.substr(0, slash + 1);

    while(readWords(defFile, words))
    {
        if(words[0] == "[/RoomAmbience]")
            return true;

        bool ok = false;
        if(words[0] == "[Settings]")
            ok = loadSettings(defFile);
        else if(words[0] == "[Effects]")
            ok = loadEffects(defFile);
        else if((words[0] == "Include") && (words.size() == 2))
            ok = loadFile(folder + words[1], true);
        else
            OD_LOG_ERR("Unexpected tag in " + fileName + ": " + words[0]);

        if(!ok)
            return false;
    }

    OD_LOG_ERR("Missing [/RoomAmbience] in " + fileName);
    return false;
}

bool RoomAmbienceConfig::loadSettings(std::istream& file)
{
    std::vector<std::string> words;
    while(readWords(file, words))
    {
        if(words[0] == "[/Settings]")
            return true;

        if(words.size() < 2)
        {
            OD_LOG_ERR("Setting without value: " + words[0]);
            return false;
        }

        if(words[0] == "ScanInterval")
            mScanInterval = Helper::toDouble(words[1]);
        else if(words[0] == "MaxParticles")
            mMaxParticles = Helper::toUInt32(words[1]);
        else if(words[0] == "MaxParticlesReduced")
            mMaxParticlesReduced = Helper::toUInt32(words[1]);
        else if(words[0] == "MaxMotions")
            mMaxMotions = Helper::toUInt32(words[1]);
        else if(words[0] == "MaxOneShots")
            mMaxOneShots = Helper::toUInt32(words[1]);
        else if(words[0] == "MaxMarks")
            mMaxMarks = Helper::toUInt32(words[1]);
        else if(words[0] == "MaxFlights")
            mMaxFlights = Helper::toUInt32(words[1]);
        else if(words[0] == "OccupiedRadius")
            mOccupiedRadius = Helper::toDouble(words[1]);
        else if(words[0] == "ReducedDistanceFactor")
            mReducedDistanceFactor = Helper::toDouble(words[1]);
        else
            OD_LOG_ERR("Unknown room ambience setting: " + words[0]);
    }

    OD_LOG_ERR("Missing [/Settings] in the room ambience file");
    return false;
}

bool RoomAmbienceConfig::loadEffects(std::istream& file)
{
    std::vector<std::string> words;
    while(readWords(file, words))
    {
        if(words[0] == "[/Effects]")
            return true;

        if(words[0] != "[Effect]")
        {
            OD_LOG_ERR("Unexpected tag in the room ambience effects: " + words[0]);
            return false;
        }

        if(!loadEffect(file))
            return false;
    }

    OD_LOG_ERR("Missing [/Effects] in the room ambience file");
    return false;
}

bool RoomAmbienceConfig::loadEffect(std::istream& file)
{
    AmbienceEffect effect;
    std::vector<std::string> words;
    while(readWords(file, words))
    {
        const std::string& key = words[0];
        if(key == "[/Effect]")
        {
            if(effect.mName.empty())
            {
                OD_LOG_ERR("Room ambience effect without a name");
                return false;
            }
            mEffects.push_back(effect);
            return true;
        }

        if(words.size() < 2)
        {
            OD_LOG_ERR("Room ambience effect key without value: " + key);
            return false;
        }

        if(key == "Name")
        {
            effect.mName = words[1];
        }
        else if(key == "Target")
        {
            if(words[1] == "Object")
                effect.mTarget = AmbienceTarget::object;
            else if(words[1] == "Tile")
                effect.mTarget = AmbienceTarget::tile;
            else if(words[1] == "Event")
                effect.mTarget = AmbienceTarget::event;
            else
            {
                OD_LOG_ERR("Unknown room ambience target: " + words[1]);
                return false;
            }
        }
        else if(key == "Match")
        {
            for(uint32_t i = 1; i < words.size(); ++i)
                effect.mMatch.push_back(words[i]);
        }
        else if(key == "When")
        {
            if(!whenFromString(words[1], effect.mWhen))
            {
                OD_LOG_ERR("Unknown room ambience condition: " + words[1]);
                return false;
            }
        }
        else if(key == "Event")
        {
            effect.mEvent = words[1];
        }
        else if(key == "Kind")
        {
            if(words[1] == "Particle")
                effect.mKind = AmbienceKind::particle;
            else if(words[1] == "Motion")
                effect.mKind = AmbienceKind::motion;
            else if(words[1] == "Clip")
                effect.mKind = AmbienceKind::clip;
            else if(words[1] == "Shake")
                effect.mKind = AmbienceKind::shake;
            else if(words[1] == "Turn")
                effect.mKind = AmbienceKind::turn;
            else if(words[1] == "Sound")
                effect.mKind = AmbienceKind::sound;
            else if(words[1] == "Beam")
                effect.mKind = AmbienceKind::beam;
            else if(words[1] == "Projectile")
                effect.mKind = AmbienceKind::projectile;
            else if(words[1] == "Mark")
                effect.mKind = AmbienceKind::mark;
            else if(words[1] == "Model")
                effect.mKind = AmbienceKind::model;
            else if(words[1] == "CreatureClip")
                effect.mKind = AmbienceKind::creatureClip;
            else
            {
                OD_LOG_ERR("Unknown room ambience kind: " + words[1]);
                return false;
            }
        }
        else if(key == "System")
        {
            effect.mSystem = words[1];
        }
        else if(key == "Mesh")
        {
            effect.mMesh = words[1];
        }
        else if(key == "Land")
        {
            effect.mLand = words[1];
        }
        else if((key == "From") && (words.size() >= 4))
        {
            effect.mFrom = Ogre::Vector3(Helper::toFloat(words[1]), Helper::toFloat(words[2]), Helper::toFloat(words[3]));
        }
        else if(key == "Family")
        {
            effect.mFamily = words[1];
        }
        else if(key == "Sound")
        {
            effect.mSound = words[1];
        }
        else if(key == "Delay")
        {
            effect.mDelay = Helper::toDouble(words[1]);
        }
        else if(key == "Clips")
        {
            for(uint32_t i = 1; i < words.size(); ++i)
                effect.mClips.push_back(words[i]);
        }
        else if(key == "Object")
        {
            for(uint32_t i = 1; i < words.size(); ++i)
                effect.mObjects.push_back(words[i]);
        }
        else if(key == "Loop")
        {
            effect.mLoop = toBool(words[1]);
        }
        else if(key == "Every")
        {
            effect.mEvery = Helper::toDouble(words[1]);
        }
        else if(key == "Motion")
        {
            if(!motionFromString(words[1], effect.mMotion))
            {
                OD_LOG_ERR("Unknown room ambience motion: " + words[1]);
                return false;
            }
        }
        else if(key == "After")
        {
            effect.mAfter = Helper::toDouble(words[1]);
        }
        else if((key == "Offset") && (words.size() >= 4))
        {
            effect.mOffset = Ogre::Vector3(Helper::toFloat(words[1]), Helper::toFloat(words[2]), Helper::toFloat(words[3]));
        }
        else if((key == "Axis") && (words.size() >= 4))
        {
            effect.mAxis = Ogre::Vector3(Helper::toFloat(words[1]), Helper::toFloat(words[2]), Helper::toFloat(words[3]));
            if(effect.mAxis.squaredLength() > 0.0001f)
                effect.mAxis.normalise();
            else
                effect.mAxis = Ogre::Vector3::UNIT_X;
        }
        else if(key == "Amount")
        {
            effect.mAmount = Helper::toDouble(words[1]);
        }
        else if(key == "Speed")
        {
            effect.mSpeed = Helper::toDouble(words[1]);
        }
        else if(key == "Flicker")
        {
            effect.mFlicker = Helper::toDouble(words[1]);
        }
        else if(key == "Duration")
        {
            effect.mDuration = Helper::toDouble(words[1]);
        }
        else if(key == "Chance")
        {
            effect.mChance = Helper::toDouble(words[1]);
        }
        else if(key == "Spacing")
        {
            effect.mSpacing = std::max<uint32_t>(1, Helper::toUInt32(words[1]));
        }
        else if(key == "MaxDistance")
        {
            effect.mMaxDistance = Helper::toDouble(words[1]);
        }
        else if(key == "Priority")
        {
            effect.mPriority = Helper::toInt(words[1]);
        }
        else if(key == "Reduced")
        {
            effect.mReduced = toBool(words[1]);
        }
        else if(key == "Below")
        {
            effect.mBelow = Helper::toDouble(words[1]);
        }
        else if(key == "NeedWall")
        {
            effect.mNeedWall = toBool(words[1]);
        }
        else if(key == "WallSide")
        {
            effect.mWallSide = toBool(words[1]);
        }
        else if(key == "HeartRate")
        {
            effect.mHeartRate = toBool(words[1]);
        }
        else if(key == "OwnerOnly")
        {
            effect.mOwnerOnly = toBool(words[1]);
        }
        else
        {
            OD_LOG_ERR("Unknown room ambience effect key: " + key);
            return false;
        }
    }

    OD_LOG_ERR("Missing [/Effect] in the room ambience file");
    return false;
}
