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
        //! Jumps up and down
        hop,
        //! Trembles sideways
        shake,
        //! Squashes (or, with a negative amount, stretches) the model, like bending the knees
        squash,
        //! Turns around its vertical axis
        spin,
        //! Turns towards the camera, stays for a moment and turns back
        turn,
        //! Looks left and right (amount is the angle in degrees)
        look,
        //! Turns towards a point (a wall, a room, a neighbour, the place of an event), stays and turns back
        lookat,
        //! Sinks down (amount is the share of the height), stays and gets up again
        sit,
        //! Tips over onto the side (amount is the angle in degrees), stays and gets up again
        lie,
        //! Nods off (count nods, amount is their depth) and jumps up in the end
        startle,
        //! Steps forward (backward with a negative amount) along where it looks and returns, like a thrust or a recoil
        lunge
    };

    //! \brief No motion: one hop of 0.2 world units that takes no time
    ReactionMotion() :
        mType(Type::none),
        mCount(1),
        mAmount(0.2),
        mDuration(0.0)
    {}

    Type mType;
    //! Number of hops, shakes, squashes or turns around
    uint32_t mCount;
    //! Height of a hop or width of a shake in world units, share of the size for a squash
    double mAmount;
    //! Time to do the whole motion, in seconds
    double mDuration;
};

//! \brief A small sprite prop shown while the reaction plays (balls, a coin, pebbles, ...). It is a few
//! billboards that move on a path computed from the size of the creature, and is removed when the reaction ends.
struct ReactionProp
{
    enum class Path
    {
        none,
        //! Balls thrown from hand to hand, now and then one falls
        juggle,
        //! A small toy that runs up and down
        yoyo,
        //! A coin thrown up and caught
        flip,
        //! Pebbles stacked one by one, then the tower falls
        stack,
        //! A pebble thrown against the wall and caught
        toss,
        //! A tiny critter that runs around the feet and away
        critter,
        //! A tool balanced on a finger
        balance,
        //! Marks drawn on the ground that fade out
        doodle,
        //! The shadow of a hand on the wall
        shadow,
        //! Pebbles pushed along the floor
        kick
    };

    ReactionProp() :
        mPath(Path::none),
        mCount(1),
        mSize(0.1),
        mSeconds(0.0)
    {}

    Path mPath;
    //! Name of the sprite: the material is CreatureProp_<name>
    std::string mSprite;
    //! Number of balls, pebbles, marks or kicks
    uint32_t mCount;
    //! Size of one sprite as a share of the height of the creature
    double mSize;
    //! Time the prop is shown, in seconds
    double mSeconds;
};

//! \brief A particle effect put on the creature
struct ReactionEffect
{
    ReactionEffect() :
        mTime(2.0)
    {}

    std::string mName;
    double mTime;
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
        mCooldown(-1.0),
        mProbability(-1.0),
        mRequiresSleepNeed(false),
        mRequiresWall(false),
        mRequiresNeighbour(false),
        mLateEmoteDelay(0.0),
        mLateEmoteTime(2.0),
        mLateEffectDelay(0.0),
        mLateEffectTime(1.0)
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

    //! Tier A: icon shown above the head and particle effects on the creature. Empty if none
    std::string mEmote;
    double mEmoteTime;
    std::vector<ReactionEffect> mEffects;
    ReactionMotion mMotion;

    //! Overrides of the values of the event, negative if the ones of the event are used
    double mCooldown;
    double mProbability;

    //! Creature definition names, groups and jobs the variant is allowed for. An empty list
    //! allows everything
    std::vector<std::string> mCreatures;
    std::vector<std::string> mGroups;
    std::vector<std::string> mJobs;
    //! Mood classes the variant is allowed for: happy (Happy), neutral (Neutral or not known), unhappy (Upset,
    //! Angry, Furious). An empty list allows everything.
    std::vector<std::string> mMoods;

    //! Only for creature types that need to sleep (see CreatureReactions::creatureNeedsSleep)
    bool mRequiresSleepNeed;

    //! Only when a wall tile is next to the creature (the wall is where the motion 'lookat' and the props turn to)
    bool mRequiresWall;
    //! Only when another creature is close (the motion 'lookat' turns to it)
    bool mRequiresNeighbour;
    //! Name of a room type (for example Hatchery): only when the creature's seat has such a room, the
    //! motion 'lookat' turns to the closest tile of it. Empty if not used.
    std::string mLookAtRoom;

    //! Sprite prop shown while the reaction plays
    ReactionProp mProp;

    //! A second icon that starts later in the reaction (nodding off, then startled). Empty if none.
    std::string mLateEmote;
    double mLateEmoteDelay;
    double mLateEmoteTime;
    //! A second particle effect that starts later (the tower falls, then the dust). Empty if none.
    std::string mLateEffect;
    double mLateEffectDelay;
    double mLateEffectTime;

    //! Name of an event another creature close by can show after this one starts (the yawn that spreads).
    //! Empty if none.
    std::string mSpreads;
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
        mGroupMax(3),
        mWhileWorking(false),
        mInHand(false),
        mDying(false)
    {}

    std::string mName;
    ReactionPriority mPriority;
    //! Time in seconds before the same creature can show a reaction of this kind again
    double mCooldown;
    //! Chance that a trigger leads to a reaction
    double mProbability;
    //! Maximum number of creatures that react when a whole group is triggered
    uint32_t mGroupMax;
    //! The event decorates a long running work or sleep animation of the creature (sleeping, digging,
    //! claiming): that animation does not count as busy for this event, and no clip is put over it
    bool mWhileWorking;
    //! The event is shown on a creature that the keeper holds in the hand (it is not on the map then)
    bool mInHand;
    //! The event decorates the death animation of a creature: the death animation does not count as busy for
    //! it, no clip is put over it and it goes on although the creature is no longer alive
    bool mDying;
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

    //! Seconds between two looks at the moods of a few creatures
    inline double getMoodInterval() const
    { return mMoodInterval; }

    //! Number of creatures looked at in each of these looks
    inline uint32_t getMoodPerTick() const
    { return mMoodPerTick; }

    //! Chance (0 to 1) that a mood is shown on a creature that walks instead of standing
    inline double getMoodWalkingChance() const
    { return mMoodWalkingChance; }

    //! Seconds a creature has to stand idle until it is impatient
    inline double getImpatientAfter() const
    { return mImpatientAfter; }

    //! Seconds a creature stays proud after a victory or a level up
    inline double getProudSeconds() const
    { return mProudSeconds; }

    //! Seconds a creature has to stand idle until it is bored
    inline double getBoredAfter() const
    { return mBoredAfter; }

    //! Seconds a creature has to stand idle until it shows small habits, sits down or lies down
    inline double getAmbientAfter() const
    { return mAmbientAfter; }

    //! Seconds a creature has to stand idle until it sits down
    inline double getSitAfter() const
    { return mSitAfter; }

    //! Seconds a creature has to stand idle until it lies down
    inline double getLieAfter() const
    { return mLieAfter; }

    //! Creatures closer than this to an event (world units) turn their head to it
    inline double getLookRadius() const
    { return mLookRadius; }

    //! Chance (0 to 1) that a creature that is looked at starts a short meeting with a creature close by
    inline double getInteractionChance() const
    { return mInteractionChance; }

    //! Creatures closer than this (world units) can have a short meeting
    inline double getInteractionRadius() const
    { return mInteractionRadius; }

    //! Seconds between the starts of two meetings, over all creatures
    inline double getInteractionPause() const
    { return mInteractionPause; }

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

    //! \brief Seconds between two looks at the moods of a few creatures
    double mMoodInterval;

    //! \brief Number of creatures looked at in each of these looks
    uint32_t mMoodPerTick;

    //! \brief Chance (0 to 1) that a mood is shown on a creature that walks instead of standing
    double mMoodWalkingChance;

    //! \brief Seconds a creature has to stand idle until it is impatient
    double mImpatientAfter;

    //! \brief Seconds a creature stays proud after a victory or a level up
    double mProudSeconds;

    //! \brief Seconds a creature has to stand idle until it is bored
    double mBoredAfter;

    //! \brief Seconds a creature has to stand idle until it shows small habits
    double mAmbientAfter;

    //! \brief Seconds a creature has to stand idle until it sits down
    double mSitAfter;

    //! \brief Seconds a creature has to stand idle until it lies down
    double mLieAfter;

    //! \brief Distance (world units) within which creatures turn their head to an event
    double mLookRadius;

    //! \brief Chance (0 to 1) that two creatures that meet show a short interaction
    double mInteractionChance;

    //! \brief Distance (world units) within which creatures that meet interact
    double mInteractionRadius;

    //! \brief Seconds a creature pauses after an interaction before it can take part in another
    double mInteractionPause;

    //! \brief Group used for a creature that is in no group
    std::string mDefaultGroup;

    //! \brief The groups of the file
    std::vector<ReactionGroup> mGroups;

    //! \brief The events of the file by name
    std::map<std::string, ReactionEvent> mEvents;
};

#endif // CREATUREREACTIONCONFIG_H
