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
#include "render/RoomAmbienceExtras.h"
#include "render/WallTorchView.h"

#include <OgrePrerequisites.h>
#include <OgreQuaternion.h>
#include <OgreSingleton.h>
#include <OgreVector3.h>

#include <cstdint>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

class GameMap;
class Seat;
class Tile;

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
    //! (room) the event is about; empty = the one of the tile at the position. noThrottle lets the same event
    //! start again within a tenth of a second (a spell cast on several creatures at once). owner is the seat the
    //! thing of the event belongs to: effects with OwnerOnly are only shown to the keeper of that seat (never when
    //! the owner is not given). Returns the number of effects started
    uint32_t triggerEvent(const std::string& eventName, const Ogre::Vector3& position, bool forced,
        const std::string& visualName = std::string(), bool noThrottle = false, const Seat* owner = nullptr);

    //! \brief Replaces the wall torches that are shown by the list the server sent. The client
    //! derives nothing; the torches are drawn as they are listed (see WallTorchView)
    void setWallTorchSpots(const std::vector<WallTorchSpot>& spots);

    //! \brief Plays the sound of the family (a folder below sounds/Spatial) at the position
    void playSound(const std::string& family, const Ogre::Vector3& position);

    //! \brief A trap or door effect sent by the server (ServerNotificationType::trapEffect).
    //! \param kind A TrapEffectKind value.
    //! \param tileX X of the tile of the trap or door.
    //! \param tileY Y of the tile of the trap or door.
    //! \param typeName The type of the trap or door (Alarm, DoorSteel, ...), matched like a tile visual in "Match" of
    //! the event effects.
    //! \param fraction The health a door has left (0 to 1); 1 for every trap.
    //! Shows the events TrapFired, TrapLinked, DoorHit, DoorHurt (health at half or less) or DoorWrecked at the tile.
    //! The kinds reloading and ready only set the state for the effects "When Reloading" and "When Ready"; a kind
    //! that is not known is ignored.
    void notifyTrapEffect(int32_t kind, int32_t tileX, int32_t tileY, const std::string& typeName, float fraction);

    //! \brief The local keeper has sent a spell cast: shows the event SpellFxHandCast at the place of the keeper's hand
    void noteHandCast(const Ogre::Vector3& handPosition);

    //! \brief Moves the camera by the current shake. Called just before the frame is rendered; clearShake()
    //! takes it away again after the frame, so nothing else ever sees the shaken camera.
    void applyShake();
    //! \brief Takes the offset of applyShake() from the camera again; does nothing when no shake was applied
    void clearShake();

    //! \brief Forgets the heart rates of the last scan; RoomAmbienceExtras gives the current ones after every scan
    inline void clearHeartRates()
    { mHeartRates.clear(); }
    //! \brief Tells that the dungeon heart at the position beats faster by the given factor (1 = calm)
    inline void addHeartRate(const Ogre::Vector3& position, double factor)
    { mHeartRates.push_back(HeartRate(position, factor)); }

    //! \brief The number of particle systems that run now (for the console)
    inline uint32_t getNbParticleSystems() const
    { return static_cast<uint32_t>(mEmitters.size() + mOneShots.size() + mMarks.size()); }
    //! \brief The number of objects that are moved now (for the console)
    inline uint32_t getNbMovedObjects() const
    { return static_cast<uint32_t>(mMotionNodes.size() + mTurrets.size()); }

private:
    struct Emitter
    {
        //! \brief Creates an emitter that is not attached to anything yet
        Emitter() :
            mEffect(0), mNode(nullptr), mSystem(nullptr), mEntity(nullptr), mBaseOrientation(Ogre::Quaternion::IDENTITY),
            mBasePosition(Ogre::Vector3::ZERO), mFade(-1.0), mBaseWidth(1.0), mBaseHeight(1.0), mPhase(0.0), mCycle(0.0),
            mSeen(false)
        {}

        //! Index of the effect in the configuration
        uint32_t mEffect;
        //! The scene node the particle system or the model hangs on
        Ogre::SceneNode* mNode;
        //! The running particle system (none for an effect of the kind model)
        Ogre::ParticleSystem* mSystem;
        //! The decoration mesh of an effect of the kind model (no particle system then)
        Ogre::Entity* mEntity;
        //! Orientation of the decoration when it was placed, its motion moves around it
        Ogre::Quaternion mBaseOrientation;
        //! Position of the decoration when it was placed, its motion moves around it
        Ogre::Vector3 mBasePosition;
        //! Seconds since the emitter was told to stop, negative while it is running
        double mFade;
        //! Particle width at creation, the flicker scales it
        double mBaseWidth;
        //! Particle height at creation, the flicker scales it
        double mBaseHeight;
        //! Start phase of the flicker, so that equal effects do not flicker together
        double mPhase;
        //! Radians turned so far by a decoration with the motion spin
        double mCycle;
        //! True while the last scan still wanted this emitter
        bool mSeen;
    };

    //! \brief A bolt (kind beam) or a flying object (kind projectile) on its way to the target
    struct Flight
    {
        //! \brief Creates a flight that is not attached to a scene node yet
        Flight() :
            mNode(nullptr), mEntity(nullptr), mTrailNode(nullptr), mTrailSystem(nullptr), mBeam(false),
            mStart(Ogre::Vector3::ZERO), mEnd(Ogre::Vector3::ZERO), mAge(0.0), mDuration(0.4), mWidth(1.0),
            mArc(0.0), mFlickerRate(20.0), mFlickerClock(0.0)
        {}

        //! The scene node that carries the mesh
        Ogre::SceneNode* mNode;
        //! The mesh of the bolt or of the flying object (none for a trail only)
        Ogre::Entity* mEntity;
        //! The scene node of the trail
        Ogre::SceneNode* mTrailNode;
        //! The particle system that trails behind a projectile
        Ogre::ParticleSystem* mTrailSystem;
        //! True for a bolt, false for a projectile
        bool mBeam;
        //! Where it starts
        Ogre::Vector3 mStart;
        //! The target
        Ogre::Vector3 mEnd;
        //! Seconds since the start
        double mAge;
        //! Seconds it needs to arrive (a bolt stays that long)
        double mDuration;
        //! Width of a bolt
        double mWidth;
        //! Height of the arc of a projectile
        double mArc;
        //! Flickers per second of a bolt
        double mFlickerRate;
        //! Seconds since the last flicker of a bolt
        double mFlickerClock;
        //! Event raised at the target on arrival (projectile)
        std::string mLand;
    };

    //! \brief The remains of a destroyed door, which plays the clip Collapse (barricade) or Destroyed (every other
    //! door) and then sinks into the floor
    struct Collapse
    {
        //! \brief Creates remains that are not attached to a scene node yet
        Collapse() :
            mNode(nullptr), mEntity(nullptr), mAge(0.0), mBaseHeight(0.0)
        {}

        //! The scene node of the ghost copy of the door
        Ogre::SceneNode* mNode;
        //! The ghost copy of the door that plays the clip
        Ogre::Entity* mEntity;
        //! Seconds since the clip started
        double mAge;
        //! Height of the node at the start, from which it sinks
        double mBaseHeight;
        //! Name of the clip that is played
        std::string mClip;
    };

    //! \brief A cannon (or another object of an effect of kind turn) that follows creatures in range with its barrel
    struct Turret
    {
        //! \brief Creates a turret that is not attached to an object yet
        Turret() :
            mEffect(0), mPosition(Ogre::Vector3::ZERO), mRestFront(Ogre::Vector3::NEGATIVE_UNIT_Y), mSeat(nullptr),
            mGeneration(0)
        {}

        //! Name of the scene node of the object that is turned
        std::string mNodeName;
        //! Index of the effect (kind turn) that drives the turret
        uint32_t mEffect;
        //! Where the object stands
        Ogre::Vector3 mPosition;
        //! Direction (flat) the object looked in when it was first seen, where it turns back to
        Ogre::Vector3 mRestFront;
        //! The seat the object belongs to, to tell enemies from friends
        Seat* mSeat;
        //! Scan in which the object was last seen; an older turret is sent back to rest and removed
        uint32_t mGeneration;
    };

    //! \brief Where a creature was at the last scan and whose it is
    struct CreatureSpot
    {
        //! \brief Creates a spot at the origin that belongs to nobody
        CreatureSpot() :
            mPosition(Ogre::Vector3::ZERO), mSeat(nullptr)
        {}

        //! Where the creature was
        Ogre::Vector3 mPosition;
        //! The seat the creature belongs to
        Seat* mSeat;
    };

    //! \brief A sound that waits for its time (Delay of an event effect)
    struct PendingSound
    {
        //! \brief Creates a sound that is due at the start
        PendingSound() :
            mPosition(Ogre::Vector3::ZERO), mDue(0.0)
        {}

        //! The sound family to play
        std::string mFamily;
        //! Where it is played
        Ogre::Vector3 mPosition;
        //! Clock time at which it is played
        double mDue;
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
            mEffect(0), mPhase(0.0), mCycle(0.0), mIntensity(0.0), mWanted(false)
        {}

        //! Index of the effect in the configuration
        uint32_t mEffect;
        //! Start phase of the motion, so that equal objects do not move together
        double mPhase;
        //! Beats so far of a motion that follows the heart rate, summed up so that a change of the rate does not jump
        double mCycle;
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
            mMovesPosition(false), mMovesOrientation(false), mMovesScale(false),
            mBasePosition(Ogre::Vector3::ZERO), mBaseScale(Ogre::Vector3::UNIT_SCALE), mClock(0.0), mSeen(false)
        {}

        //! Name of the scene node that is moved
        std::string mNodeName;
        //! Which parts of the node are driven by the motions (put back only these)
        bool mMovesPosition;
        //! True if a motion drives the orientation of the node
        bool mMovesOrientation;
        //! True if a motion drives the scale of the node
        bool mMovesScale;
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
            mEffect(0), mPosition(Ogre::Vector3::ZERO), mDistance(0.0), mPriority(0), mYaw(0.0), mActive(false)
        {}

        //! Index of the effect in the configuration
        uint32_t mEffect;
        //! Name of the entity (objects) or the tile (tiles)
        std::string mTarget;
        //! Name of the scene node to move when the target is not an entity (bridge of a tile)
        std::string mNodeName;
        //! Where the effect would be shown
        Ogre::Vector3 mPosition;
        //! Distance of the place to the camera
        double mDistance;
        //! Priority of the effect, higher ones are served first
        int32_t mPriority;
        //! Degrees around the vertical axis (kind model): the decoration looks away from the wall
        double mYaw;
        //! True if the conditions of the effect hold now
        bool mActive;
    };

    //! \brief How fast a dungeon heart beats, as told by RoomAmbienceExtras
    struct HeartRate
    {
        //! \brief Creates the beat of the heart at the given position
        HeartRate(const Ogre::Vector3& position, double factor) :
            mPosition(position), mFactor(factor)
        {}

        //! Where the heart stands
        Ogre::Vector3 mPosition;
        //! Beats per second as a multiple of the calm beat
        double mFactor;
    };

    //! \brief When a creature was last close to a target (used for the effects that need an empty room)
    struct BusyInfo
    {
        //! \brief Creates the info of a target that nobody was close to yet
        BusyInfo() :
            mLastBusy(0.0), mLastTouched(0.0), mEverBusy(false)
        {}

        //! Clock time at which a creature was last close
        double mLastBusy;
        //! Clock time at which the info was last asked for, old infos are dropped
        double mLastTouched;
        //! True when a creature was close at some time while the target was watched (used for "Vacated")
        bool mEverBusy;
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
    //! \brief Raises the events of creatures: one that wakes in a dormitory (CreatureWoke), an enemy that enters a guard
    //! room (EnemyEntered), a new prisoner (PrisonerArrived), one that changes its keeper in a torture chamber
    //! (TortureConverted) and one that is healed in a temple (CreatureHealed)
    void scanCreatureEvents();
    //! \brief Raises the event HeartHit and starts the state "When Hit" of the own dungeon heart when its health went down
    void scanHeartHit();
    //! \brief Starts the emitters and motions that the candidates ask for within the limits and stops the others
    void reconcile();
    //! \brief Plays the clips of the objects whose time has come
    void playClips();
    //! \brief Flickers the running emitters and removes those that were stopped long enough
    void updateEmitters(double timeSinceLastFrame);
    //! \brief Removes the one-shot effects whose time is over
    //! \brief Ends the one-shot effects of the list whose time is over
    void updateOneShots(std::vector<OneShot>& oneShots, double timeSinceLastFrame);
    //! \brief Plays the sounds whose delay is over
    void updatePendingSounds();
    //! \brief Moves the objects by their motions and puts back the ones that are not moved any more
    //! \brief Lets a destroyed door (the door entity of the given type on the tile, which the server is about to
    //! remove) play the clip Collapse (barricade) or Destroyed (every other door) of its skeleton on a ghost copy
    void startCollapse(int32_t tileX, int32_t tileY, const std::string& typeName);
    //! \brief Moves the clips of the remains of destroyed doors on, lets them lie a moment and sinks them
    void updateCollapses(double timeSinceLastFrame);
    //! \brief Removes the ghost copy of a destroyed door from the scene
    void destroyCollapse(Collapse& collapse);
    //! \brief Lets the objects of effects of kind turn follow the creatures near them, or go back to rest
    void updateTurrets(double timeSinceLastFrame);
    //! \brief Starts a beam or a projectile (effect kinds beam and projectile) for an event at the given place
    void startFlight(const AmbienceEffect& effect, const Ogre::Vector3& position);
    //! \brief Moves the bolts and flying objects on and raises the landing events of those that arrived
    void updateFlights(double timeSinceLastFrame);
    //! \brief Removes the scene objects of a bolt or flying object
    void destroyFlight(Flight& flight);
    //! \brief Turns the object of a turret back to the direction it looked in when it was first seen
    void restoreTurret(const Turret& turret);
    //! \brief Lets a running view shake fade out and moves its phase on
    void updateShake(double timeSinceLastFrame);
    //! \brief Starts a view shake of the effect (kind shake) for an event at the given place
    //! \param effect The shake effect; Amount is its strength at the middle of the view.
    //! \param position Where the event happened.
    //! \param lookPoint The point on the floor the camera looks at, from which the distance is measured.
    void startShake(const AmbienceEffect& effect, const Ogre::Vector3& position, const Ogre::Vector3& lookPoint);
    void updateMotions(double timeSinceLastFrame);
    //! \brief Removes the particle system or model of an emitter and its scene node
    void destroyEmitter(Emitter& emitter);
    //! \brief Creates the scene node and mesh of a decoration (effect kind model)
    //! \param mesh The mesh file of the decoration.
    //! \param position Where it stands.
    //! \param yaw Degrees around the vertical axis.
    //! \param baseName Start of the names of the scene objects.
    //! \param node Receives the scene node.
    //! \param entity Receives the entity.
    //! \return False if the mesh does not exist (then nothing was created).
    bool createModel(const std::string& mesh, const Ogre::Vector3& position, double yaw, const std::string& baseName,
        Ogre::SceneNode*& node, Ogre::Entity*& entity);
    //! \brief Moves a decoration by the motion of its effect, around the pose it was placed in
    void moveModel(Emitter& emitter, const AmbienceEffect& effect, double timeSinceLastFrame);
    //! \brief Puts a moved node back to the position, orientation and scale it had before
    void restoreMotionNode(MotionNode& motionNode);

    //! \brief True if a creature was within the radius of the point at the last scan
    bool isCreatureNear(double x, double y, double radius) const;
    //! \brief True if the dungeon heart of the local keeper at the position has less health than the given fraction
    //! \param position Where the heart stands.
    //! \param below The health fraction (0 to 1) under which the heart counts as low.
    bool isLocalHeartBelow(const Ogre::Vector3& position, double below) const;
    //! \brief The heart rate factor of the heart nearest to the position (1 when no heart is near)
    double getHeartRateFactor(const Ogre::Vector3& position) const;
    //! \brief Updates and returns the state "no creature for a while" of a target
    bool isIdleLongEnough(const std::string& key, bool busy, double after);
    //! \brief The state "empty" (When Empty) or "left behind" (When Vacated) of a target
    //! \param effect The effect; mWhen tells which of the two is asked and mAfter is the time in seconds.
    //! \param key The key under which the state of the target is kept.
    //! \param busy True if a creature is close to the target now.
    //! \return For When Empty true once no creature was close for mAfter seconds; for When Vacated the same, but only
    //! for a target that was used at some time while it was watched.
    bool isIdleState(const AmbienceEffect& effect, const std::string& key, bool busy);
    //! \brief The distance to the camera up to which the effect runs in the current mode
    double getDistanceLimit(const AmbienceEffect& effect) const;
    //! \brief True if the effect is shown in the current mode
    bool isEffectUsable(const AmbienceEffect& effect) const;
    bool createParticleSystem(const std::string& system, const Ogre::Vector3& position, const std::string& baseName,
        Ogre::SceneNode*& node, Ogre::ParticleSystem*& particleSystem);
    //! \brief The effects of the given kind for the objects with the given mesh name, remembered per mesh
    const std::vector<uint32_t>& getObjectEffects(const std::string& meshName, const std::string& kind);
    //! \brief The effects for a bridge tile, found by the mesh of its bridge and the visual of the ground under it
    std::vector<uint32_t> getBridgeEffects(Tile* tile) const;
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
    //! Effects of bridge tiles, per bridge mesh and per tile visual (lavaGround, waterGround)
    std::map<std::string, std::vector<uint32_t> > mBridgeEffects;
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
    //! Ground marks (kind mark), oldest first
    std::vector<OneShot> mMarks;
    //! The objects that are moved, per node name
    std::map<std::string, MotionNode> mMotionNodes;
    //! When a creature was last close, per target
    std::map<std::string, BusyInfo> mBusy;
    //! When an event was last raised at a place, so that the same event does not start twice at once
    std::map<std::string, double> mLastEventTime;
    //! Time until which a trap (key "x,y" of its tile) counts as reloading or empty, for the effects
    //! "When Reloading" and "When Ready"
    std::map<std::string, double> mReloadingUntil;
    //! Time until which a door (key "x,y" of its tile) counts as hit, for the effects "When Hit"
    std::map<std::string, double> mHitUntil;
    //! Time until which a door (key "x,y" of its tile) counts as destroyed, so that it is not also reported as sold
    std::map<std::string, double> mWreckedUntil;
    //! The remains of destroyed doors that are shown
    std::vector<Collapse> mCollapses;
    //! The bolts and flying objects that are on their way
    std::vector<Flight> mFlights;
    //! Sounds that wait for their delay
    std::vector<PendingSound> mPendingSounds;
    //! Objects that follow creatures (kind turn), per entity name
    std::map<std::string, Turret> mTurrets;
    //! Time until which a trap (key "x,y" of its tile) keeps its turn, because the server aims it for a shot
    std::map<std::string, double> mTurretHoldUntil;
    //! Where the creatures were at the last scan
    std::vector<CreatureSpot> mCreatureSpots;
    //! View shake: seconds left and in total, strength in world units at the start, shakes per second, phase
    double mShakeTime;
    double mShakeTotal;
    double mShakeAmount;
    double mShakeSpeed;
    double mShakePhase;
    //! Offset the camera node was moved by in applyShake (zero when not applied)
    Ogre::Vector3 mShakeApplied;

    //! Positions of the creatures on the map at the last scan
    std::vector<Ogre::Vector3> mCreaturePositions;
    //! The places where particle effects could be shown, found by the last scan
    std::vector<Candidate> mParticleCandidates;
    //! The objects that could be moved, found by the last scan
    std::vector<Candidate> mMotionCandidates;
    //! The objects that could play a clip, found by the last scan
    std::vector<Candidate> mClipCandidates;
    //! Time at which an object plays its next clip (kind clip)
    std::map<std::string, double> mClipTimers;
    //! How fast the dungeon hearts beat, found at the last scan
    std::vector<HeartRate> mHeartRates;
    //! Moments that are read from what the client already knows (heart rate)
    RoomAmbienceExtras mExtras;
    //! The wall torches the server listed, drawn as they are listed
    WallTorchView mWallTorches;
    //! Dice of its own, so the game random sequence is untouched
    std::mt19937 mRandom;

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

    //! \brief What was seen of a creature at the last scan
    struct CreatureSnapshot
    {
        //! \brief Creates a snapshot that is not set yet
        CreatureSnapshot() :
            mHp(0.0), mSleeping(false), mPrisoner(false), mSeat(nullptr), mEnemyInGuardRoom(false),
            mLastHealed(-std::numeric_limits<double>::max()), mGeneration(0)
        {}

        //! Health of the creature
        double mHp;
        //! True while the creature plays its sleep clip
        bool mSleeping;
        //! True while the creature is a prisoner
        bool mPrisoner;
        //! The keeper the creature serves
        Seat* mSeat;
        //! True while the creature of another keeper stands in a guard room
        bool mEnemyInGuardRoom;
        //! Clock time at which a healing effect was last shown for the creature (never at the start)
        double mLastHealed;
        //! Number of the scan that last saw the creature
        uint32_t mGeneration;
    };
    //! What was seen of each creature at the last scan, by creature name
    std::map<std::string, CreatureSnapshot> mKnownCreatures;
    //! False until the first scan has filled mKnownCreatures, so that nothing is raised for the first look
    bool mCreaturesInitialized;
    //! Health of the own dungeon heart at the last scan (negative while unknown)
    double mLastHeartHP;
    //! Number of the current scan
    uint32_t mGeneration;
    //! False until the first scan has filled mKnownEntities, so that nothing is raised for the first look
    bool mEntitiesInitialized;
};

#endif // ROOMAMBIENCE_H
