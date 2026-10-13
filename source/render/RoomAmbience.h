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

#ifndef ROOMAMBIENCE_H
#define ROOMAMBIENCE_H

#include "render/RoomAmbienceConfig.h"

#include <OgrePrerequisites.h>
#include <OgreQuaternion.h>
#include <OgreSingleton.h>
#include <OgreVector3.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

class GameMap;

/*! \brief Cosmetic life in the rooms (dust, glow, sparks, moving objects), client side only.
 *
 * Nothing here is known to the server or saved: the effects are derived from what the client map
 * already shows (room tiles, the objects in the rooms, creatures standing close, things that appear
 * or vanish). Which effect belongs to which room or object is read from config/roomAmbience.cfg.
 * Only what is in view and close to the camera is animated, and the number of particle systems and
 * moving objects is limited. The option "Room ambience" (full / reduced / off) switches the amount.
 */
class RoomAmbience : public Ogre::Singleton<RoomAmbience>
{
public:
    enum class Mode
    {
        //! Everything is shown
        full,
        //! Few effects, no more than a handful of particle systems, shorter distance
        reduced,
        //! Nothing is shown
        off
    };

    //! \brief Creates the ambience of a map and reads roomAmbience.cfg from the given configuration folder
    RoomAmbience(GameMap* gameMap, const std::string& configPath);
    //! \brief Destroys the ambience; the running effects are removed with the scene
    ~RoomAmbience();

    //! \brief Called every frame while the game is running
    void update(Ogre::Real timeSinceLastFrame);

    //! \brief Removes every running effect and puts the moved objects back
    void stopAll();

    //! \brief Sets the amount of effects shown; a change removes every running effect, the next scan rebuilds them
    void setMode(Mode mode);
    //! \brief The amount of effects that is shown now
    inline Mode getMode() const
    { return mMode; }
    //! \brief Reads the name of a mode (full, reduced or off); anything else gives full
    static Mode modeFromString(const std::string& text);
    //! \brief The name of a mode as it is stored in the settings
    static std::string modeToString(Mode mode);

    //! \brief Reads the configuration file again and rebuilds the effect index; returns false if it could not be used
    bool reloadConfig();
    //! \brief The effects and limits that were read from the configuration file
    inline const RoomAmbienceConfig& getConfig() const
    { return mConfig; }

    //! \brief Shows the one-shot effects of the given event at the given place.
    //! forced ignores the chance, the view test and the mode "reduced". visualName is the tile visual
    //! (room) the event is about; empty = the one of the tile at the position. Returns the number of effects started
    uint32_t triggerEvent(const std::string& eventName, const Ogre::Vector3& position, bool forced,
        const std::string& visualName = std::string());

    //! \brief The number of particle systems that run now (for the console)
    inline uint32_t getNbParticleSystems() const
    { return static_cast<uint32_t>(mEmitters.size() + mOneShots.size()); }
    //! \brief The number of objects that are moved now (for the console)
    inline uint32_t getNbMovedObjects() const
    { return static_cast<uint32_t>(mMotionNodes.size()); }

private:
    struct Emitter
    {
        //! \brief Creates an emitter that is not attached to anything yet
        Emitter() :
            mEffect(0), mNode(nullptr), mSystem(nullptr), mFade(-1.0), mBaseWidth(1.0), mBaseHeight(1.0),
            mPhase(0.0), mSeen(false)
        {}

        //! Index of the effect in the configuration
        uint32_t mEffect;
        //! The scene node the particle system or the model hangs on
        Ogre::SceneNode* mNode;
        //! The running particle system (none for an effect of the kind model)
        Ogre::ParticleSystem* mSystem;
        //! Seconds since the emitter was told to stop, negative while it is running
        double mFade;
        //! Particle width at creation, the flicker scales it
        double mBaseWidth;
        //! Particle height at creation, the flicker scales it
        double mBaseHeight;
        //! Start phase of the flicker, so that equal effects do not flicker together
        double mPhase;
        //! True while the last scan still wanted this emitter
        bool mSeen;
    };

    //! \brief A particle system of an event effect that is removed after its duration
    struct OneShot
    {
        //! \brief Creates a one-shot effect that is not started yet
        OneShot() :
            mNode(nullptr), mSystem(nullptr), mLife(0.0)
        {}

        //! The scene node the particle system hangs on
        Ogre::SceneNode* mNode;
        //! The running particle system
        Ogre::ParticleSystem* mSystem;
        //! Seconds left before the effect is removed
        double mLife;
    };

    //! \brief One motion of an object, with its own phase and ease
    struct MotionInstance
    {
        //! \brief Creates a motion that is not started yet
        MotionInstance() :
            mEffect(0), mPhase(0.0), mIntensity(0.0), mWanted(false)
        {}

        //! Index of the effect in the configuration
        uint32_t mEffect;
        //! Start phase of the motion, so that equal objects do not move together
        double mPhase;
        //! How far the motion has eased in, from 0 to 1
        double mIntensity;
        //! True while the last scan still wanted this motion
        bool mWanted;
    };

    //! \brief An object that is moved, with what is needed to put it back
    struct MotionNode
    {
        //! \brief Creates a motion node that is not attached to a scene node yet
        MotionNode() :
            mBasePosition(Ogre::Vector3::ZERO), mBaseScale(Ogre::Vector3::UNIT_SCALE), mClock(0.0), mSeen(false)
        {}

        //! Name of the scene node that is moved
        std::string mNodeName;
        //! Orientation the node is put back to
        Ogre::Quaternion mBaseOrientation;
        //! Position the node is put back to
        Ogre::Vector3 mBasePosition;
        //! Scale the node is put back to
        Ogre::Vector3 mBaseScale;
        //! Seconds the motions of this node have been running
        double mClock;
        //! True while the last scan still found the object
        bool mSeen;
        //! The motions that move this node now
        std::vector<MotionInstance> mMotions;
    };

    //! \brief A place where an effect could be shown, found by the scan
    struct Candidate
    {
        //! \brief Creates a candidate that is not set yet
        Candidate() :
            mEffect(0), mPosition(Ogre::Vector3::ZERO), mDistance(0.0), mPriority(0), mActive(false)
        {}

        //! Index of the effect in the configuration
        uint32_t mEffect;
        //! Name of the entity (objects) or the tile (tiles)
        std::string mTarget;
        //! Where the effect would be shown
        Ogre::Vector3 mPosition;
        //! Distance of the place to the camera
        double mDistance;
        //! Priority of the effect, higher ones are served first
        int32_t mPriority;
        //! True if the conditions of the effect hold now
        bool mActive;
    };

    //! \brief When a creature was last close to a target (used for the effects that need an empty room)
    struct BusyInfo
    {
        //! \brief Creates the info of a target that nobody was close to yet
        BusyInfo() :
            mLastBusy(0.0), mLastTouched(0.0)
        {}

        //! Clock time at which a creature was last close
        double mLastBusy;
        //! Clock time at which the info was last asked for, old infos are dropped
        double mLastTouched;
    };

    //! \brief Sorts the effects of the configuration by tile visual, object and event, so that a scan finds them quickly
    void buildIndex();
    //! \brief Looks at what the camera sees and starts or stops the effects of the objects, tiles and events there
    void scan();
    //! \brief Finds the objects near the camera that have effects and collects the candidates for them
    void scanObjects(Ogre::Camera* camera, const Ogre::Vector3& cameraPosition);
    //! \brief Finds the room tiles near the look point that have effects and collects the candidates for them
    void scanTiles(Ogre::Camera* camera, const Ogre::Vector3& cameraPosition, const Ogre::Vector3& lookPoint);
    //! \brief Raises the events for rooms that were built or sold and for objects that appeared or vanished
    void scanEntityEvents(Ogre::Camera* camera, const Ogre::Vector3& cameraPosition);
    //! \brief Starts the emitters and motions that the candidates ask for within the limits and stops the others
    void reconcile();
    //! \brief Flickers the running emitters and removes those that were stopped long enough
    void updateEmitters(double timeSinceLastFrame);
    //! \brief Removes the one-shot effects whose time is over
    void updateOneShots(double timeSinceLastFrame);
    //! \brief Moves the objects by their motions and puts back the ones that are not moved any more
    void updateMotions(double timeSinceLastFrame);
    //! \brief Removes the particle system or model of an emitter and its scene node
    void destroyEmitter(Emitter& emitter);
    //! \brief Puts a moved node back to the position, orientation and scale it had before
    void restoreMotionNode(MotionNode& motionNode);

    //! \brief True if a creature was within the radius of the point at the last scan
    bool isCreatureNear(double x, double y, double radius) const;
    //! \brief Updates and returns the state "no creature for a while" of a target
    bool isIdleLongEnough(const std::string& key, bool busy, double after);
    //! \brief The distance to the camera up to which the effect runs in the current mode
    double getDistanceLimit(const AmbienceEffect& effect) const;
    //! \brief True if the effect is shown in the current mode
    bool isEffectUsable(const AmbienceEffect& effect) const;
    bool createParticleSystem(const std::string& system, const Ogre::Vector3& position, const std::string& baseName,
        Ogre::SceneNode*& node, Ogre::ParticleSystem*& particleSystem);
    //! \brief The effects of the objects with the given mesh name, remembered per mesh
    const std::vector<uint32_t>& getObjectEffects(const std::string& meshName);
    bool isVisibleNear(Ogre::Camera* camera, const Ogre::Vector3& cameraPosition, const Ogre::Vector3& position,
        double radius, double limit) const;

    //! The map whose rooms and objects are shown
    GameMap* mGameMap;
    //! The folder the configuration files are read from
    std::string mConfigPath;
    //! The effects and limits read from the configuration
    RoomAmbienceConfig mConfig;
    //! The amount of effects that is shown
    Mode mMode;
    //! Seconds since the ambience was created, advanced by every update
    double mClock;
    //! Seconds since the last scan
    double mScanTimer;
    //! Seconds since old entries of the busy and timer lists were dropped
    double mPruneTimer;
    //! Counter for the names of the scene nodes and particle systems that are created
    uint32_t mUniqueNumber;
    //! Names of missing particle systems already reported
    std::set<std::string> mMissingSystems;

    //! Effects per tile visual (indexed by the TileVisual value), per mesh name (memo) and per event
    std::vector<std::vector<uint32_t> > mTileEffects;
    //! Effects per mesh name, remembered after the first lookup
    std::map<std::string, std::vector<uint32_t> > mObjectEffectsMemo;
    //! Effects per event name
    std::map<std::string, std::vector<uint32_t> > mEventEffects;
    //! Empty list returned when nothing matches
    std::vector<uint32_t> mNoEffects;
    //! Effects of objects that are matched by a wildcard
    std::vector<uint32_t> mObjectWildcardEffects;
    //! Largest distance around the look point that is scanned
    double mScanRadius;

    //! The running looping effects, per target
    std::map<std::string, Emitter> mEmitters;
    //! The running one-shot effects of events
    std::vector<OneShot> mOneShots;
    //! The objects that are moved, per node name
    std::map<std::string, MotionNode> mMotionNodes;
    //! When a creature was last close, per target
    std::map<std::string, BusyInfo> mBusy;
    //! When an event was last raised at a place, so that the same event does not start twice at once
    std::map<std::string, double> mLastEventTime;

    //! Positions of the creatures on the map at the last scan
    std::vector<Ogre::Vector3> mCreaturePositions;
    //! The places where particle effects could be shown, found by the last scan
    std::vector<Candidate> mParticleCandidates;
    //! The objects that could be moved, found by the last scan
    std::vector<Candidate> mMotionCandidates;

    //! Tile visual seen at the last scan per tile (255 = never seen), used to find rooms built or sold
    std::vector<uint8_t> mSeenVisual;
    //! Width of the map when mSeenVisual was filled
    int32_t mSeenSizeX;
    //! Height of the map when mSeenVisual was filled
    int32_t mSeenSizeY;
    //! Events raised by the current scan, a map load is told from building by this count
    uint32_t mEventsThisScan;

    //! \brief What was seen of an object at the last scan
    struct EntitySnapshot
    {
        //! \brief Creates a snapshot that is not set yet
        EntitySnapshot() :
            mType(0), mPosition(Ogre::Vector3::ZERO), mGeneration(0)
        {}

        //! The type of the object
        uint32_t mType;
        //! Position of the object
        Ogre::Vector3 mPosition;
        //! Number of the scan that last saw the object
        uint32_t mGeneration;
    };
    //! What was seen of each object at the last scan, by object name
    std::map<std::string, EntitySnapshot> mKnownEntities;
    //! Number of the current scan
    uint32_t mGeneration;
    //! False until the first scan has filled mKnownEntities, so that nothing is raised for the first look
    bool mEntitiesInitialized;
};

#endif // ROOMAMBIENCE_H
