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

#ifndef CREATUREREACTIONCONFIG_H
#define CREATUREREACTIONCONFIG_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

//! \brief Priority of what a creature is doing or of a reaction. The lower the value, the
//! more important it is: a reaction only starts when its priority is lower than the one of
//! what the creature currently does, and a more important reaction replaces a running one.
enum class ReactionPriority
{
    death = 0,
    combat,
    held,
    event,
    work,
    mood,
    ambient,
    //! Not a reaction priority: the creature is only idling or walking
    none
};

//! \brief A short procedural movement of the creature model (cosmetic only)
struct ReactionMotion
{
    enum class Type
    {
        none,
        hop
    };

    //! \brief No motion: one hop of 0.2 world units that takes no time
    ReactionMotion() :
        mType(Type::none),
        mCount(1),
        mHeight(0.2),
        mDuration(0.0)
    {}

    Type mType;
    //! Number of hops
    uint32_t mCount;
    //! Height of one hop, in world units
    double mHeight;
    //! Time to do all the hops, in seconds
    double mDuration;
};

//! \brief One way to show a reaction. A variant is shown in the highest tier that is available
//! for the creature: a clip of its own (tier C), else a clip that every creature of the mesh
//! has (tier B), always together with the emote, the particle effect and the motion (tier A).
struct ReactionVariant
{
    //! \brief A variant with weight 1 that plays no clip, shows no emote or effect and has no filters.
    //! The icon and the effect are shown for 2 seconds. Cooldown and probability of the event apply.
    ReactionVariant() :
        mWeight(1.0),
        mClipSpeed(1.0),
        mFallbackSpeed(1.0),
        mFallbackStart(0.0),
        mFallbackEnd(1.0),
        mEmoteTime(2.0),
        mEffectTime(2.0),
        mCooldown(-1.0),
        mProbability(-1.0),
        mRequiresSleepNeed(false)
    {}

    std::string mName;
    double mWeight;

    //! Tier C: clip made for this reaction. Empty if none
    std::string mClip;
    double mClipSpeed;

    //! Tier B: clip every creature of the mesh has. Played from mFallbackStart to mFallbackEnd
    //! (fractions of the clip length). Empty if none
    std::string mFallbackClip;
    double mFallbackSpeed;
    double mFallbackStart;
    double mFallbackEnd;

    //! Tier A: icon shown above the head and particle effect on the creature. Empty if none
    std::string mEmote;
    double mEmoteTime;
    std::string mEffect;
    double mEffectTime;
    ReactionMotion mMotion;

    //! Overrides of the values of the event, negative if the ones of the event are used
    double mCooldown;
    double mProbability;

    //! Creature definition names, groups and jobs the variant is allowed for. An empty list
    //! allows everything
    std::vector<std::string> mCreatures;
    std::vector<std::string> mGroups;
    std::vector<std::string> mJobs;

    //! Only for creature types that need to sleep (see CreatureReactions::creatureNeedsSleep)
    bool mRequiresSleepNeed;
};

//! \brief A kind of reaction (what happened to the creature) with its variants
struct ReactionEvent
{
    //! \brief An event of priority "event" with a cooldown of 10 seconds, a probability of 1 and at most
    //! 3 creatures in a group reaction
    ReactionEvent() :
        mPriority(ReactionPriority::event),
        mCooldown(10.0),
        mProbability(1.0),
        mGroupMax(3)
    {}

    std::string mName;
    ReactionPriority mPriority;
    //! Time in seconds before the same creature can show a reaction of this kind again
    double mCooldown;
    //! Chance that a trigger leads to a reaction
    double mProbability;
    //! Maximum number of creatures that react when a whole group is triggered
    uint32_t mGroupMax;
    std::vector<ReactionVariant> mVariants;
};

//! \brief Named list of creature definitions. Used by variants to choose what fits a creature
struct ReactionGroup
{
    std::string mName;
    std::vector<std::string> mCreatures;
};

//! \brief Content of config/creatureReactions.cfg
class CreatureReactionConfig
{
public:
    //! \brief Sets the default values of the settings (nothing is loaded yet)
    CreatureReactionConfig();

    //! \brief Reads the file. Returns false (and keeps what could be read) if it is missing or
    //! malformed: the reactions are then simply not available.
    bool load(const std::string& fileName);

    //! \brief The event with this name, or nullptr if the file has none
    const ReactionEvent* getEvent(const std::string& name) const;

    //! \brief Names of the groups the creature definition is in. If it is in none, the default
    //! group is returned.
    std::vector<std::string> getGroupsOf(const std::string& creatureName) const;

    //! \brief Converts the name used in the file to a priority. ok is false (and "event" is returned)
    //! if the name is unknown.
    static ReactionPriority priorityFromString(const std::string& text, bool& ok);

    //! \brief The name of the priority as it is written in the file
    static std::string priorityToString(ReactionPriority priority);

    //! \brief All events of the file by name
    inline const std::map<std::string, ReactionEvent>& getEvents() const
    { return mEvents; }

    //! \brief The number of reactions that may run at the same time
    inline uint32_t getMaxSimultaneous() const
    { return mMaxSimultaneous; }

    //! \brief The distance from the camera (world units) beyond which no reactions are shown
    inline double getMaxCameraDistance() const
    { return mMaxCameraDistance; }

    //! \brief The shortest time (seconds) between the reactions of two creatures of a group
    inline double getGroupStaggerMin() const
    { return mGroupStaggerMin; }

    //! \brief The longest time (seconds) between the reactions of two creatures of a group
    inline double getGroupStaggerMax() const
    { return mGroupStaggerMax; }

private:
    //! \brief Reads the [Settings] block up to its closing tag. Returns false if it is malformed.
    bool loadSettings(std::istream& file);

    //! \brief Reads the [Groups] block up to its closing tag. Returns false if it is malformed.
    bool loadGroups(std::istream& file);

    //! \brief Reads the [Events] block up to its closing tag. Returns false if it is malformed.
    bool loadEvents(std::istream& file);

    //! \brief Reads the lines of one [Event] up to its closing tag into the event
    bool loadEvent(std::istream& file, ReactionEvent& event);

    //! \brief Reads the lines of one [Variant] up to its closing tag into the variant
    bool loadVariant(std::istream& file, ReactionVariant& variant);

    //! \brief Number of reactions that may run at the same time
    uint32_t mMaxSimultaneous;

    //! \brief Distance from the camera (world units) beyond which no reactions are shown
    double mMaxCameraDistance;

    //! \brief Shortest and longest time (seconds) between the reactions of two creatures of a group
    double mGroupStaggerMin;
    double mGroupStaggerMax;

    //! \brief Group used for a creature that is in no group
    std::string mDefaultGroup;

    //! \brief The groups of the file
    std::vector<ReactionGroup> mGroups;

    //! \brief The events of the file by name
    std::map<std::string, ReactionEvent> mEvents;
};

#endif // CREATUREREACTIONCONFIG_H
