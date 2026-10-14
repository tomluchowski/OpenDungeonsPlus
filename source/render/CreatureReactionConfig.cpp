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

#include "render/CreatureReactionConfig.h"

#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <algorithm>
#include <sstream>

namespace
{

//! Default number of reactions that may play at the same time
static const uint32_t DEFAULT_MAX_SIMULTANEOUS = 6;
//! Default distance from the camera in tiles beyond which a creature shows no reaction
static const double DEFAULT_MAX_CAMERA_DISTANCE = 45.0;
//! Default shortest delay in seconds between the reactions of the creatures of one group
static const double DEFAULT_GROUP_STAGGER_MIN = 0.12;
//! Default longest delay in seconds between the reactions of the creatures of one group
static const double DEFAULT_GROUP_STAGGER_MAX = 0.45;

//! Default seconds between two mood checks of a creature
static const double DEFAULT_MOOD_INTERVAL = 0.5;
//! Default number of creatures looked at in each mood check
static const uint32_t DEFAULT_MOOD_PER_TICK = 4;
//! Default chance that a walking creature shows a mood
static const double DEFAULT_MOOD_WALKING_CHANCE = 0.3;
//! Default seconds of standing idle until a creature is impatient
static const double DEFAULT_IMPATIENT_AFTER = 6.0;
//! Default seconds a creature stays proud after a victory or a level up
static const double DEFAULT_PROUD_SECONDS = 25.0;
//! Default seconds of standing idle until a creature is bored
static const double DEFAULT_BORED_AFTER = 20.0;
//! Default seconds of standing idle until a creature shows small habits
static const double DEFAULT_AMBIENT_AFTER = 3.0;
//! Default seconds of standing idle until a creature sits down
static const double DEFAULT_SIT_AFTER = 45.0;
//! Default seconds of standing idle until a creature lies down
static const double DEFAULT_LIE_AFTER = 100.0;
//! Default distance in world units within which creatures turn their head to an event
static const double DEFAULT_LOOK_RADIUS = 12.0;
//! Default chance that two creatures that meet show a short interaction
static const double DEFAULT_INTERACTION_CHANCE = 0.05;
//! Default distance in world units within which creatures that meet interact
static const double DEFAULT_INTERACTION_RADIUS = 3.5;
//! Default seconds a creature pauses after an interaction
static const double DEFAULT_INTERACTION_PAUSE = 5.0;

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

//! \brief True for the words "yes", "true" and "1", false for everything else
bool toBool(const std::string& text)
{
    return (text == "yes") || (text == "true") || (text == "1");
}

} // namespace

CreatureReactionConfig::CreatureReactionConfig() :
    mMaxSimultaneous(DEFAULT_MAX_SIMULTANEOUS),
    mMaxCameraDistance(DEFAULT_MAX_CAMERA_DISTANCE),
    mGroupStaggerMin(DEFAULT_GROUP_STAGGER_MIN),
    mGroupStaggerMax(DEFAULT_GROUP_STAGGER_MAX),
    mMoodInterval(DEFAULT_MOOD_INTERVAL),
    mMoodPerTick(DEFAULT_MOOD_PER_TICK),
    mMoodWalkingChance(DEFAULT_MOOD_WALKING_CHANCE),
    mImpatientAfter(DEFAULT_IMPATIENT_AFTER),
    mProudSeconds(DEFAULT_PROUD_SECONDS),
    mBoredAfter(DEFAULT_BORED_AFTER),
    mAmbientAfter(DEFAULT_AMBIENT_AFTER),
    mSitAfter(DEFAULT_SIT_AFTER),
    mLieAfter(DEFAULT_LIE_AFTER),
    mLookRadius(DEFAULT_LOOK_RADIUS),
    mInteractionChance(DEFAULT_INTERACTION_CHANCE),
    mInteractionRadius(DEFAULT_INTERACTION_RADIUS),
    mInteractionPause(DEFAULT_INTERACTION_PAUSE),
    mDefaultGroup("Fighters")
{
}

ReactionPriority CreatureReactionConfig::priorityFromString(const std::string& text, bool& ok)
{
    ok = true;
    if(text == "death")
        return ReactionPriority::death;
    if(text == "combat")
        return ReactionPriority::combat;
    if(text == "held")
        return ReactionPriority::held;
    if(text == "event")
        return ReactionPriority::event;
    if(text == "work")
        return ReactionPriority::work;
    if(text == "mood")
        return ReactionPriority::mood;
    if(text == "ambient")
        return ReactionPriority::ambient;

    ok = false;
    return ReactionPriority::event;
}

std::string CreatureReactionConfig::priorityToString(ReactionPriority priority)
{
    switch(priority)
    {
        case ReactionPriority::death:
            return "death";
        case ReactionPriority::combat:
            return "combat";
        case ReactionPriority::held:
            return "held";
        case ReactionPriority::event:
            return "event";
        case ReactionPriority::work:
            return "work";
        case ReactionPriority::mood:
            return "mood";
        case ReactionPriority::ambient:
            return "ambient";
        default:
            return "none";
    }
}

const ReactionEvent* CreatureReactionConfig::getEvent(const std::string& name) const
{
    std::map<std::string, ReactionEvent>::const_iterator it = mEvents.find(name);
    if(it == mEvents.end())
        return nullptr;

    return &it->second;
}

std::vector<std::string> CreatureReactionConfig::getGroupsOf(const std::string& creatureName) const
{
    std::vector<std::string> groups;
    for(const ReactionGroup& group : mGroups)
    {
        if(std::find(group.mCreatures.begin(), group.mCreatures.end(), creatureName) != group.mCreatures.end())
            groups.push_back(group.mName);
    }

    // A creature without an entry uses the default group
    if(groups.empty())
        groups.push_back(mDefaultGroup);

    return groups;
}

bool CreatureReactionConfig::load(const std::string& fileName)
{
    OD_LOG_INF("Load creature reactions file: " + fileName);
    std::stringstream defFile;
    if(!Helper::readFile(fileName, defFile, true))
    {
        OD_LOG_ERR("Couldn't read " + fileName);
        return false;
    }

    std::vector<std::string> words;
    if(!readWords(defFile, words) || (words.size() != 1) || (words[0] != "[CreatureReactions]"))
    {
        OD_LOG_ERR("Invalid creature reactions start format in " + fileName);
        return false;
    }

    while(readWords(defFile, words))
    {
        if(words[0] == "[/CreatureReactions]")
            return true;

        bool ok = false;
        if(words[0] == "[Settings]")
            ok = loadSettings(defFile);
        else if(words[0] == "[Groups]")
            ok = loadGroups(defFile);
        else if(words[0] == "[Events]")
            ok = loadEvents(defFile);
        else
            OD_LOG_ERR("Unexpected tag in " + fileName + ": " + words[0]);

        if(!ok)
            return false;
    }

    OD_LOG_ERR("Missing [/CreatureReactions] in " + fileName);
    return false;
}

bool CreatureReactionConfig::loadSettings(std::istream& file)
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

        if(words[0] == "MaxSimultaneous")
            mMaxSimultaneous = Helper::toUInt32(words[1]);
        else if(words[0] == "MaxCameraDistance")
            mMaxCameraDistance = Helper::toDouble(words[1]);
        else if(words[0] == "GroupStaggerMin")
            mGroupStaggerMin = Helper::toDouble(words[1]);
        else if(words[0] == "GroupStaggerMax")
            mGroupStaggerMax = Helper::toDouble(words[1]);
        else if(words[0] == "MoodInterval")
            mMoodInterval = Helper::toDouble(words[1]);
        else if(words[0] == "MoodPerTick")
            mMoodPerTick = Helper::toUInt32(words[1]);
        else if(words[0] == "MoodWalkingChance")
            mMoodWalkingChance = Helper::toDouble(words[1]);
        else if(words[0] == "ImpatientAfter")
            mImpatientAfter = Helper::toDouble(words[1]);
        else if(words[0] == "ProudSeconds")
            mProudSeconds = Helper::toDouble(words[1]);
        else if(words[0] == "BoredAfter")
            mBoredAfter = Helper::toDouble(words[1]);
        else if(words[0] == "AmbientAfter")
            mAmbientAfter = Helper::toDouble(words[1]);
        else if(words[0] == "SitAfter")
            mSitAfter = Helper::toDouble(words[1]);
        else if(words[0] == "LieAfter")
            mLieAfter = Helper::toDouble(words[1]);
        else if(words[0] == "LookRadius")
            mLookRadius = Helper::toDouble(words[1]);
        else if(words[0] == "InteractionChance")
            mInteractionChance = Helper::toDouble(words[1]);
        else if(words[0] == "InteractionRadius")
            mInteractionRadius = Helper::toDouble(words[1]);
        else if(words[0] == "InteractionPause")
            mInteractionPause = Helper::toDouble(words[1]);
        else if(words[0] == "DefaultGroup")
            mDefaultGroup = words[1];
        else
            OD_LOG_WRN("Unknown creature reactions setting: " + words[0]);
    }

    OD_LOG_ERR("Missing [/Settings]");
    return false;
}

bool CreatureReactionConfig::loadGroups(std::istream& file)
{
    std::vector<std::string> words;
    while(readWords(file, words))
    {
        if(words[0] == "[/Groups]")
            return true;

        if(words[0] != "[Group]")
        {
            OD_LOG_ERR("Expecting [Group] but got: " + words[0]);
            return false;
        }

        ReactionGroup group;
        bool closed = false;
        while(readWords(file, words))
        {
            if(words[0] == "[/Group]")
            {
                closed = true;
                break;
            }

            if(words[0] == "Name" && words.size() >= 2)
                group.mName = words[1];
            else if(words[0] == "Creatures")
                group.mCreatures.assign(words.begin() + 1, words.end());
            else
                OD_LOG_WRN("Unknown creature reactions group key: " + words[0]);
        }

        if(!closed || group.mName.empty())
        {
            OD_LOG_ERR("Invalid creature reactions group");
            return false;
        }

        mGroups.push_back(group);
    }

    OD_LOG_ERR("Missing [/Groups]");
    return false;
}

bool CreatureReactionConfig::loadEvents(std::istream& file)
{
    std::vector<std::string> words;
    while(readWords(file, words))
    {
        if(words[0] == "[/Events]")
            return true;

        if(words[0] != "[Event]")
        {
            OD_LOG_ERR("Expecting [Event] but got: " + words[0]);
            return false;
        }

        ReactionEvent event;
        if(!loadEvent(file, event))
            return false;

        mEvents[event.mName] = event;
    }

    OD_LOG_ERR("Missing [/Events]");
    return false;
}

bool CreatureReactionConfig::loadEvent(std::istream& file, ReactionEvent& event)
{
    std::vector<std::string> words;
    while(readWords(file, words))
    {
        if(words[0] == "[/Event]")
        {
            if(event.mName.empty())
            {
                OD_LOG_ERR("Creature reaction event without name");
                return false;
            }
            return true;
        }

        if(words[0] == "[Variant]")
        {
            ReactionVariant variant;
            if(!loadVariant(file, variant))
                return false;

            event.mVariants.push_back(variant);
            continue;
        }

        if(words.size() < 2)
        {
            OD_LOG_ERR("Creature reaction event key without value: " + words[0]);
            return false;
        }

        if(words[0] == "Name")
        {
            event.mName = words[1];
        }
        else if(words[0] == "Priority")
        {
            bool ok = false;
            event.mPriority = priorityFromString(words[1], ok);
            if(!ok)
            {
                OD_LOG_ERR("Unknown creature reaction priority: " + words[1]);
                return false;
            }
        }
        else if(words[0] == "Cooldown")
        {
            event.mCooldown = Helper::toDouble(words[1]);
        }
        else if(words[0] == "Probability")
        {
            event.mProbability = Helper::toDouble(words[1]);
        }
        else if(words[0] == "GroupMax")
        {
            event.mGroupMax = Helper::toUInt32(words[1]);
        }
        else if(words[0] == "WhileWorking")
        {
            event.mWhileWorking = toBool(words[1]);
        }
        else if(words[0] == "InHand")
        {
            event.mInHand = toBool(words[1]);
        }
        else if(words[0] == "Dying")
        {
            event.mDying = toBool(words[1]);
        }
        else
        {
            OD_LOG_WRN("Unknown creature reaction event key: " + words[0]);
        }
    }

    OD_LOG_ERR("Missing [/Event]");
    return false;
}

bool CreatureReactionConfig::loadVariant(std::istream& file, ReactionVariant& variant)
{
    std::vector<std::string> words;
    while(readWords(file, words))
    {
        if(words[0] == "[/Variant]")
            return true;

        if(words.size() < 2)
        {
            OD_LOG_ERR("Creature reaction variant key without value: " + words[0]);
            return false;
        }

        if(words[0] == "Name")
        {
            variant.mName = words[1];
        }
        else if(words[0] == "Weight")
        {
            variant.mWeight = Helper::toDouble(words[1]);
        }
        else if(words[0] == "Clip")
        {
            variant.mClip = words[1];
            if(words.size() >= 3)
                variant.mClipSpeed = Helper::toDouble(words[2]);
        }
        else if(words[0] == "Fallback")
        {
            // Fallback <clip> [speed [start end]]
            variant.mFallbackClip = words[1];
            if(words.size() >= 3)
                variant.mFallbackSpeed = Helper::toDouble(words[2]);
            if(words.size() >= 5)
            {
                variant.mFallbackStart = Helper::toDouble(words[3]);
                variant.mFallbackEnd = Helper::toDouble(words[4]);
            }
        }
        else if(words[0] == "Emote")
        {
            variant.mEmote = words[1];
            if(words.size() >= 3)
                variant.mEmoteTime = Helper::toDouble(words[2]);
        }
        else if(words[0] == "Effect")
        {
            ReactionEffect effect;
            effect.mName = words[1];
            if(words.size() >= 3)
                effect.mTime = Helper::toDouble(words[2]);
            variant.mEffects.push_back(effect);
        }
        else if(words[0] == "Motion")
        {
            // Motion <hop|shake|squash|spin|turn|look|lookat|sit|startle> <count> <amount> <seconds>
            ReactionMotion::Type type = ReactionMotion::Type::none;
            if(words[1] == "hop")
                type = ReactionMotion::Type::hop;
            else if(words[1] == "shake")
                type = ReactionMotion::Type::shake;
            else if(words[1] == "squash")
                type = ReactionMotion::Type::squash;
            else if(words[1] == "spin")
                type = ReactionMotion::Type::spin;
            else if(words[1] == "turn")
                type = ReactionMotion::Type::turn;
            else if(words[1] == "look")
                type = ReactionMotion::Type::look;
            else if(words[1] == "lookat")
                type = ReactionMotion::Type::lookat;
            else if(words[1] == "sit")
                type = ReactionMotion::Type::sit;
            else if(words[1] == "lie")
                type = ReactionMotion::Type::lie;
            else if(words[1] == "startle")
                type = ReactionMotion::Type::startle;
            else if(words[1] == "lunge")
                type = ReactionMotion::Type::lunge;

            if((type == ReactionMotion::Type::none) || (words.size() < 5))
            {
                OD_LOG_ERR("Invalid creature reaction motion: " + words[1]);
                return false;
            }
            variant.mMotion.mType = type;
            variant.mMotion.mCount = Helper::toUInt32(words[2]);
            variant.mMotion.mAmount = Helper::toDouble(words[3]);
            variant.mMotion.mDuration = Helper::toDouble(words[4]);
        }
        else if(words[0] == "Cooldown")
        {
            variant.mCooldown = Helper::toDouble(words[1]);
        }
        else if(words[0] == "Probability")
        {
            variant.mProbability = Helper::toDouble(words[1]);
        }
        else if(words[0] == "Creatures")
        {
            variant.mCreatures.assign(words.begin() + 1, words.end());
        }
        else if(words[0] == "Groups")
        {
            variant.mGroups.assign(words.begin() + 1, words.end());
        }
        else if(words[0] == "Jobs")
        {
            variant.mJobs.assign(words.begin() + 1, words.end());
        }
        else if(words[0] == "Moods")
        {
            variant.mMoods.assign(words.begin() + 1, words.end());
        }
        else if(words[0] == "RequiresSleepNeed")
        {
            variant.mRequiresSleepNeed = toBool(words[1]);
        }
        else if(words[0] == "RequiresWall")
        {
            variant.mRequiresWall = toBool(words[1]);
        }
        else if(words[0] == "RequiresNeighbour")
        {
            variant.mRequiresNeighbour = toBool(words[1]);
        }
        else if(words[0] == "LookAtRoom")
        {
            variant.mLookAtRoom = words[1];
        }
        else if(words[0] == "LateEmote")
        {
            // LateEmote <name> <delay> <seconds>
            variant.mLateEmote = words[1];
            if(words.size() >= 3)
                variant.mLateEmoteDelay = Helper::toDouble(words[2]);
            if(words.size() >= 4)
                variant.mLateEmoteTime = Helper::toDouble(words[3]);
        }
        else if(words[0] == "Prop")
        {
            // Prop <juggle|yoyo|flip|stack|toss|critter|balance|doodle|shadow|kick> <sprite> <count> <size> <seconds>
            ReactionProp::Path path = ReactionProp::Path::none;
            if(words[1] == "juggle")
                path = ReactionProp::Path::juggle;
            else if(words[1] == "yoyo")
                path = ReactionProp::Path::yoyo;
            else if(words[1] == "flip")
                path = ReactionProp::Path::flip;
            else if(words[1] == "stack")
                path = ReactionProp::Path::stack;
            else if(words[1] == "toss")
                path = ReactionProp::Path::toss;
            else if(words[1] == "critter")
                path = ReactionProp::Path::critter;
            else if(words[1] == "balance")
                path = ReactionProp::Path::balance;
            else if(words[1] == "doodle")
                path = ReactionProp::Path::doodle;
            else if(words[1] == "shadow")
                path = ReactionProp::Path::shadow;
            else if(words[1] == "kick")
                path = ReactionProp::Path::kick;

            if((path == ReactionProp::Path::none) || (words.size() < 6))
            {
                OD_LOG_ERR("Invalid creature reaction prop: " + words[1]);
                return false;
            }
            variant.mProp.mPath = path;
            variant.mProp.mSprite = words[2];
            variant.mProp.mCount = Helper::toUInt32(words[3]);
            variant.mProp.mSize = Helper::toDouble(words[4]);
            variant.mProp.mSeconds = Helper::toDouble(words[5]);
        }
        else if(words[0] == "LateEffect")
        {
            // LateEffect <name> <delay> <seconds>
            variant.mLateEffect = words[1];
            if(words.size() >= 3)
                variant.mLateEffectDelay = Helper::toDouble(words[2]);
            if(words.size() >= 4)
                variant.mLateEffectTime = Helper::toDouble(words[3]);
        }
        else if(words[0] == "Spreads")
        {
            variant.mSpreads = words[1];
        }
        else
        {
            OD_LOG_WRN("Unknown creature reaction variant key: " + words[0]);
        }
    }

    OD_LOG_ERR("Missing [/Variant]");
    return false;
}
