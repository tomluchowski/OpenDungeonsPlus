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

#ifndef ROOMAMBIENCECONFIG_H
#define ROOMAMBIENCECONFIG_H

#include <OgreVector3.h>

#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

//! \brief What an ambience effect is attached to
enum class AmbienceTarget
{
    //! A rendered object (bookshelf, anvil, ...), found by its mesh name
    object,
    //! A room tile, found by its tile visual name (libraryRoom, ...)
    tile,
    //! A one-shot effect at the place where something happened (room built, research done, ...)
    event
};

//! \brief When a looping effect is shown
enum class AmbienceWhen
{
    always,
    //! A creature is close to the target
    occupied,
    //! No creature was close to the target for the time given in mAfter
    empty,
    //! The target (a door) took a hit a moment ago
    hit,
    //! The target (a door) is closed, which is what a lock by the keeper does
    locked,
    //! The target (a trap) reloads or is empty, as the server reported it
    reloading,
    //! The target (a trap) is loaded and ready, the opposite of reloading
    ready
};

//! \brief What an ambience effect does when it is shown
enum class AmbienceKind
{
    //! A particle system (also used as glow, with a flicker)
    particle,
    //! A procedural movement of the object node
    motion,
    //! Now and then a clip of the object's own mesh (a chicken scratching)
    clip,
    //! A short shake of the view (events only): Amount = strength in world units, Speed = shakes per second,
    //! Duration in seconds, MaxDistance = distance of the event from the middle of the view beyond which it is not felt
    shake,
    //! Slowly turns an object (a cannon) toward creatures within Amount tiles and back to where it stood when none
    //! is near, at Speed degrees per second (objects only)
    turn,
    //! A sound of the family given by Family: played at the event, or now and then (Every) at an object
    sound
};

//! \brief How an effect of the kind motion moves the node of its object
enum class AmbienceMotion
{
    //! Slow rocking around an axis
    sway,
    //! Rocking that swells and fades (hits on a dummy, a creaking beam)
    wobble,
    //! Turning around the vertical axis
    spin,
    //! Moving up and down
    bob,
    //! Growing and shrinking
    pulse,
    //! Uneven brightness-like scaling
    flicker
};

//! \brief One effect read from the room ambience configuration
struct AmbienceEffect
{
    //! \brief Creates an effect with the defaults that the configuration file may override
    AmbienceEffect() :
        mTarget(AmbienceTarget::object),
        mWhen(AmbienceWhen::always),
        mKind(AmbienceKind::particle),
        mMotion(AmbienceMotion::sway),
        mAfter(30.0),
        mOffset(Ogre::Vector3::ZERO),
        mAxis(Ogre::Vector3::UNIT_X),
        mAmount(1.0),
        mSpeed(1.0),
        mFlicker(0.0),
        mDuration(3.0),
        mDelay(0.0),
        mEvery(10.0),
        mChance(1.0),
        mSpacing(1),
        mMaxDistance(28.0),
        mPriority(5),
        mReduced(false),
        mNeedWall(false),
        mOwnerOnly(false)
    {}

    //! Name of the effect, used in the log
    std::string mName;
    //! What the effect is attached to (an object, a tile or an event)
    AmbienceTarget mTarget;
    //! Mesh names (object, with * as first or last character as wildcard) or tile visual names (tile, event)
    std::vector<std::string> mMatch;
    //! When a looping effect is shown
    AmbienceWhen mWhen;
    //! Event name (target event)
    std::string mEvent;
    //! What the effect does (a particle system, a motion or a clip)
    AmbienceKind mKind;
    //! Particle system template (kind particle)
    std::string mSystem;
    //! Clips to choose from (kind clip)
    std::vector<std::string> mClips;
    //! How the object is moved (kind motion)
    AmbienceMotion mMotion;
    //! Seconds without a creature before an empty room effect starts
    double mAfter;
    //! Added to the position of the target
    Ogre::Vector3 mOffset;
    //! Axis a motion works around or along
    Ogre::Vector3 mAxis;
    //! Degrees (sway, wobble), units (bob), fraction (pulse, flicker)
    double mAmount;
    //! Cycles per second (degrees per second for spin)
    double mSpeed;
    //! Size change of the particle billboards, 0 = steady
    double mFlicker;
    //! Seconds a one-shot effect is kept
    double mDuration;
    //! Sound family, as in the folders below sounds/Spatial (kind sound)
    std::string mFamily;
    //! Seconds after the event until the sound is played (kind sound, events only)
    double mDelay;
    //! Average seconds between two clips (kind clip)
    double mEvery;
    //! Chance that an event effect is shown
    double mChance;
    //! Tile targets: only every n-th tile gets the effect
    uint32_t mSpacing;
    //! Distance to the camera up to which the effect runs
    double mMaxDistance;
    //! Higher priority effects are served first when the budget is used up
    int32_t mPriority;
    //! Kept in the mode "reduced"
    bool mReduced;
    //! Tile targets: only tiles beside a wall
    bool mNeedWall;
    //! Shown (or heard) only by the keeper the object or the thing of the event belongs to (the glint and the sounds of a
    //! secret door must not give it away)
    bool mOwnerOnly;
};

/*! \brief Settings and effects of config/roomAmbience.cfg
 *
 * The file has the root tag [RoomAmbience] with a [Settings] block, an [Effects] block with
 * [Effect] entries and optional "Include <file>" lines (same folder, a missing file is skipped).
 */
class RoomAmbienceConfig
{
public:
    //! \brief Creates an empty configuration with the default limits
    RoomAmbienceConfig();

    //! \brief Reads the file, replacing what was loaded before. Returns false if it could not be used
    bool load(const std::string& fileName);

    //! \brief The effects read from the file and its includes, in file order
    const std::vector<AmbienceEffect>& getEffects() const
    { return mEffects; }

    //! \brief Seconds between two scans of the visible rooms and objects
    double getScanInterval() const
    { return mScanInterval; }
    //! \brief The most looping particle systems that may run at once (the lower limit in the mode reduced)
    uint32_t getMaxParticles(bool reduced) const
    { return reduced ? mMaxParticlesReduced : mMaxParticles; }
    //! \brief The most objects that may be moved by motions at once
    uint32_t getMaxMotions() const
    { return mMaxMotions; }
    //! \brief The most one-shot particle systems that may run at once
    uint32_t getMaxOneShots() const
    { return mMaxOneShots; }
    //! \brief Distance in tiles within which a creature counts as close to an object or tile
    double getOccupiedRadius() const
    { return mOccupiedRadius; }
    //! \brief Factor that shortens the view distance of every effect in the mode reduced
    double getReducedDistanceFactor() const
    { return mReducedDistanceFactor; }

    //! \brief Reads the name of a "When" condition; returns false for an unknown name
    static bool whenFromString(const std::string& text, AmbienceWhen& when);
    //! \brief Reads the name of a motion; returns false for an unknown name
    static bool motionFromString(const std::string& text, AmbienceMotion& motion);

private:
    //! \brief Reads one file; included is true for a file named by an "Include" line (a missing one is skipped)
    bool loadFile(const std::string& fileName, bool included);
    //! \brief Reads the [Settings] block
    bool loadSettings(std::istream& file);
    //! \brief Reads the [Effects] block, one [Effect] entry after the other
    bool loadEffects(std::istream& file);
    //! \brief Reads one [Effect] entry and adds it to mEffects
    bool loadEffect(std::istream& file);

    //! The effects read from the file and its includes, in file order
    std::vector<AmbienceEffect> mEffects;
    //! Seconds between two scans
    double mScanInterval;
    //! Most looping particle systems that may run at once
    uint32_t mMaxParticles;
    //! The same limit in the mode reduced
    uint32_t mMaxParticlesReduced;
    //! Most objects that may be moved at once
    uint32_t mMaxMotions;
    //! Most one-shot particle systems that may run at once
    uint32_t mMaxOneShots;
    //! Distance in tiles within which a creature counts as close
    double mOccupiedRadius;
    //! Factor that shortens the view distance in the mode reduced
    double mReducedDistanceFactor;
};

#endif // ROOMAMBIENCECONFIG_H
