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

#include "render/RoomAmbience.h"

#include "camera/CameraManager.h"
#include "entities/Creature.h"
#include "entities/GameEntityType.h"
#include "entities/RenderedMovableEntity.h"
#include "entities/Tile.h"
#include "game/HeartHealthRing.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "network/ODClient.h"
#include "render/ODFrameListener.h"
#include "render/RenderManager.h"
#include "sound/SoundEffectsManager.h"
#include "traps/Trap.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <OgreAnimationState.h>
#include <OgreAxisAlignedBox.h>
#include <OgreCamera.h>
#include <OgreEntity.h>
#include <OgreException.h>
#include <OgreMath.h>
#include <OgreParticleSystem.h>
#include <OgreParticleSystemManager.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>
#include <utility>

template<> RoomAmbience* Ogre::Singleton<RoomAmbience>::msSingleton = nullptr;

namespace
{

//! How long a stopped emitter keeps running its last particles before it is removed
const double EMITTER_FADE_SECONDS = 3.0;
//! How fast a moved object eases in and out of its motion (intensity per second)
const double MOTION_EASE_SPEED = 1.5;
//! Largest radius around the look point that is scanned
const double MAX_SCAN_RADIUS = 45.0;
//! Room tiles changing in one scan above this number are a map load, not building
const uint32_t MAX_EVENTS_PER_SCAN = 6;
//! Seed of the dice of the ambience, which are separate from the game random sequence
const uint32_t RANDOM_SEED = 12345;
//! Seconds between two prunings of the lists of old entries
const double PRUNE_INTERVAL_SECONDS = 30.0;
//! Seconds after which the busy info of a target that nobody asked for is dropped
const double BUSY_KEEP_SECONDS = 600.0;
//! Seconds after which the clip timer of an object that was not seen is dropped
const double CLIP_TIMER_KEEP_SECONDS = 120.0;
//! An effect that already runs is kept this much beyond its view distance, so it does not flicker at the limit
const double VIEW_DISTANCE_MARGIN = 1.2;
//! Radius in tiles that must be in view for an object or an event to count as seen
const double OBJECT_VISIBLE_RADIUS = 1.5;
//! Radius in tiles that must be in view for a room tile to count as seen
const double TILE_VISIBLE_RADIUS = 1.0;
//! Value in the list of seen tile visuals for a tile that was never seen
const uint8_t NEVER_SEEN_VISUAL = 255;
//! Tile visual value of a tile that is not known yet
const uint8_t UNKNOWN_VISUAL = 0;
//! Multipliers and shift that mix the tile coordinates into the hash that picks every n-th tile
const int32_t TILE_HASH_X = 73856093;
const int32_t TILE_HASH_Y = 19349663;
const uint32_t TILE_HASH_SHIFT = 3;
//! Tiles added to the occupied radius when a room tile asks whether a creature is close
const double TILE_OCCUPIED_MARGIN = 1.0;
//! Distance in tiles from the middle of a tile to the edge that an effect with WallSide is moved to
const float WALL_SIDE_SHIFT = 0.45f;
//! Distance in tiles within which an effect belongs to a dungeon heart for its beat
const double HEART_RATE_RADIUS = 3.0;
//! Seconds that the own dungeon heart counts as hit after its health went down, for the effects "When Hit"
const double HEART_HIT_SECONDS = 1.2;
//! The mesh of the dungeon heart
static const std::string HEART_MESH = "DungeonTempleObject";
//! A creature has to gain at least this much health between two scans for a healing effect
const double MIN_HEAL_AMOUNT = 0.5;
//! Seconds between two healing effects of the same creature
const double HEAL_SPACING_SECONDS = 4.0;
//! Seconds after the start during which changes count as a map being loaded, not as events
const double SETTLE_SECONDS = 3.0;
//! Seconds within which the same event does not start again
const double EVENT_REPEAT_SECONDS = 0.1;
//! Radians in one degree
const double DEGREES_TO_RADIANS = 0.0174532925;
//! The second wave of a flicker runs this much faster than the first, so the flicker does not repeat
const double FLICKER_SECOND_WAVE_RATE = 1.7;
//! Cycles per second of the swell of a wobble
const double WOBBLE_SWELL_RATE = 0.11;
//! The start phase of a hash is a whole number of these steps per radian ...
const uint32_t PHASE_STEPS = 6283;
//! ... divided by this, so it lies between 0 and 2 pi
const double PHASE_STEPS_PER_RADIAN = 1000.0;
//! The camera must look down at least this much (z of its direction) for the point where it meets the floor to be used
const float MIN_LOOK_DOWN = 0.05f;
//! Scan radius in tiles before a configuration was read
const double DEFAULT_SCAN_RADIUS = 30.0;
//! Smallest scan radius in tiles
const double MIN_SCAN_RADIUS = 10.0;
//! Most clips that one scan starts, so that not all objects of a room move at once
const uint32_t MAX_CLIPS_PER_SCAN = 3;
//! The next clip of an object comes after the interval of the effect times a factor between these two
const double CLIP_INTERVAL_MIN_FACTOR = 0.6;
const double CLIP_INTERVAL_MAX_FACTOR = 1.4;
//! Seconds that a looping clip of an object waits after a clip of an event (a bed that wakes) is over
const double CLIP_HOLD_MARGIN_SECONDS = 0.5;
//! Length in seconds that an event clip is assumed to have when its animation state is not known
const double DEFAULT_CLIP_SECONDS = 2.0;
//! Start of a Match name that stands for a bridge (followed by the bridge mesh or the tile visual)
static const std::string BRIDGE_PREFIX = "bridge:";
//! Start of a Match name that stands for the type of a trap or door (Alarm, Gas, DoorSteel...)
static const std::string TRAP_PREFIX = "trap:";
//! The end of a mesh file name
static const std::string MESH_SUFFIX = ".mesh";
//! A full turn in radians
const double TWO_PI = 6.283185307179586;
//! Seconds that a trap counts as reloading after the server sent the start of the reload; the long time only
//! protects against a ready message that was missed out of sight
const double RELOAD_STATE_SECONDS = 300.0;
//! Seconds that the reload look is kept after the server said that the trap is loaded, so that a short reload is seen at all
const double RELOAD_FADE_OUT_SECONDS = 2.0;
//! At least this many marks may be on the floor, whatever the configuration says
const uint32_t MIN_MARK_LIMIT = 1;
//! The shortest time a bolt or flying object is on its way, in seconds
const double MIN_FLIGHT_SECONDS = 0.05;
//! The slowest a bolt flickers, in flickers per second
const double MIN_BEAM_FLICKER_RATE = 1.0;
//! The shortest length a bolt is stretched to, so that it never has zero length
const double MIN_BEAM_LENGTH = 0.01;
//! A bolt is drawn this thick (times its width) at the thinnest, and up to this much thicker at random, and it grows
//! thinner as it fades
const double BEAM_THICKNESS_MIN = 0.55;
const double BEAM_THICKNESS_RANGE = 0.9;
//! The arc of a projectile is a parabola: its height at the middle of the way is the height of the arc, and this
//! factor makes the parabola 4 * progress * (1 - progress) reach 1 there
const double ARC_PARABOLA_FACTOR = 4.0;
//! A projectile that moved less than this (squared) since the last frame is not turned
const float MIN_PROJECTILE_MOVE_SQUARED = 0.000001f;
//! At most this many sounds wait for their delay at the same time
const size_t MAX_PENDING_SOUNDS = 32;
//! Seconds that a door counts as hit after the server sent a hit, for the effects "When Hit"
const double HIT_STATE_SECONDS = 1.0;
//! Seconds that a destroyed door counts as destroyed, so that its removal is not also reported as a sale
const double WRECKED_STATE_SECONDS = 5.0;
//! A door with this fraction of its health or less is hurt (the event DoorHurt)
const float DOOR_HURT_FRACTION = 0.5f;
//! At most this many remains of destroyed doors are shown at once
const size_t MAX_COLLAPSES = 4;
//! Seconds the remains of a door lie there after their clip, and seconds and depth of their sinking into the floor
const double COLLAPSE_HOLD_SECONDS = 2.0;
const double COLLAPSE_SINK_SECONDS = 1.0;
const double COLLAPSE_SINK_DEPTH = 0.3;
//! The type name of the barricade, which falls into a heap (clip Collapse) while every other door breaks (clip Destroyed)
static const std::string BARRICADE_TYPE = "DoorBarricade";
static const std::string COLLAPSE_CLIP = "Collapse";
static const std::string DESTROYED_CLIP = "Destroyed";
//! The state of the animation of a door that is closed
static const std::string DOOR_CLOSED_STATE = "Close";
//! How long the barrel of a cannon keeps the aim of the server after a shot, in seconds
const double TURRET_HOLD_SECONDS = 1.5;
//! A direction whose flat parts add up to less than this is no direction to turn toward
const float MIN_TURN_DIRECTION = 0.001f;
//! A turn that is less than this many radians off the wanted direction is finished
const double TURN_DONE_RADIANS = 0.005;
//! Half a turn in radians
const double HALF_TURN = 3.141592653589793;
//! A rest direction or a way to a creature with a smaller squared length than this is not used
const float MIN_AIM_LENGTH_SQUARED = 0.01f;
const float MIN_REST_LENGTH_SQUARED = 0.0001f;
//! Shakes per second of a view shake before the first one has started
const double DEFAULT_SHAKE_SPEED = 12.0;
//! The shortest time a view shake runs, in seconds
const double MIN_SHAKE_SECONDS = 0.05;
//! The slowest a view shake may be, in shakes per second
const double MIN_SHAKE_SPEED = 0.5;
//! The three axes of a view shake run at different speeds and start at different phases, so that it does not
//! look like a regular swing: factor of the speed and start phase of the second axis
const double SHAKE_SECOND_AXIS_SPEED_FACTOR = 1.31;
const double SHAKE_SECOND_AXIS_PHASE = 1.7;
//! Strength of the third axis (up and down) as a fraction of the strength, its speed factor and start phase
const double SHAKE_THIRD_AXIS_STRENGTH = 0.5;
const double SHAKE_THIRD_AXIS_SPEED_FACTOR = 0.83;
const double SHAKE_THIRD_AXIS_PHASE = 0.4;

//! \brief The point on the floor that a camera looks at; the camera position itself when it does not look down enough
Ogre::Vector3 getLookPoint(const Ogre::Vector3& cameraPosition, const Ogre::Vector3& direction)
{
    Ogre::Vector3 lookPoint = cameraPosition;
    if(direction.z < -MIN_LOOK_DOWN)
        lookPoint = cameraPosition + direction * (cameraPosition.z / -direction.z);

    return lookPoint;
}

//! \brief Turns the node about the vertical axis toward the flat direction wanted, by at most maxRadians.
//! The front of a node is its -Y direction (as in RenderManager::rrOrientEntityToward). It always turns from
//! the orientation the node has now, so an orientation the server gave it in the meantime is simply continued from.
void turnNodeToward(Ogre::SceneNode* node, const Ogre::Vector3& wanted, double maxRadians)
{
    Ogre::Vector3 front = node->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Y;
    if((std::fabs(front.x) + std::fabs(front.y) < MIN_TURN_DIRECTION) ||
       (std::fabs(wanted.x) + std::fabs(wanted.y) < MIN_TURN_DIRECTION))
    {
        return;
    }

    double delta = std::atan2(static_cast<double>(wanted.y), static_cast<double>(wanted.x)) -
        std::atan2(static_cast<double>(front.y), static_cast<double>(front.x));
    while(delta > HALF_TURN)
        delta -= TWO_PI;
    while(delta < -HALF_TURN)
        delta += TWO_PI;

    if(std::fabs(delta) < TURN_DONE_RADIANS)
        return;

    double step = std::max(-maxRadians, std::min(maxRadians, delta));
    node->rotate(Ogre::Quaternion(Ogre::Radian(static_cast<Ogre::Real>(step)), Ogre::Vector3::UNIT_Z), Ogre::Node::TS_WORLD);
}

//! \brief The key "x,y" under which the state of a trap on a tile is kept
std::string getTileKey(int32_t tileX, int32_t tileY)
{
    return Helper::toString(tileX) + "," + Helper::toString(tileY);
}

//! \brief The key of the tile that the given position lies on
std::string getPositionKey(const Ogre::Vector3& position)
{
    return getTileKey(static_cast<int32_t>(std::floor(position.x + 0.5f)), static_cast<int32_t>(std::floor(position.y + 0.5f)));
}

//! \brief True if the name fits the pattern, which is a plain name or has * as first or last character as wildcard
bool matchesPattern(const std::string& pattern, const std::string& name)
{
    if(pattern.empty())
        return false;

    if(pattern[0] == '*')
    {
        std::string tail = pattern.substr(1);
        return (name.size() >= tail.size()) && (name.compare(name.size() - tail.size(), tail.size(), tail) == 0);
    }

    if(pattern[pattern.size() - 1] == '*')
    {
        std::string head = pattern.substr(0, pattern.size() - 1);
        return name.compare(0, head.size(), head) == 0;
    }

    return pattern == name;
}

//! \brief A stable start phase (0 to 2 pi) derived from a text, so that equal objects do not move together
double hashPhase(const std::string& text)
{
    std::hash<std::string> hasher;
    return static_cast<double>(hasher(text) % PHASE_STEPS) / PHASE_STEPS_PER_RADIAN;
}

//! \brief Orders candidates by priority (higher first) and then by distance (closer first)
bool candidateBefore(const std::pair<int32_t, double>& a, const std::pair<int32_t, double>& b)
{
    if(a.first != b.first)
        return a.first > b.first;

    return a.second < b.second;
}

//! Orders indices by priority (high first) and then by distance (near first)
struct IndexOrder
{
    //! \brief Creates the order over the given priority and distance pairs, which must outlive it
    explicit IndexOrder(const std::vector<std::pair<int32_t, double> >& order) :
        mOrder(order)
    {}

    //! \brief True if the entry with index a comes before the entry with index b
    bool operator()(uint32_t a, uint32_t b) const
    {
        return candidateBefore(mOrder[a], mOrder[b]);
    }

    //! The priority and distance pairs the indices point into
    const std::vector<std::pair<int32_t, double> >& mOrder;
};

} // namespace

RoomAmbience::RoomAmbience(GameMap* gameMap, const std::string& configPath) :
    mGameMap(gameMap),
    mConfigPath(configPath),
    mMode(Mode::full),
    mClock(0.0),
    mScanTimer(0.0),
    mPruneTimer(0.0),
    mUniqueNumber(0),
    mScanRadius(DEFAULT_SCAN_RADIUS),
    mSeenSizeX(0),
    mSeenSizeY(0),
    mEventsThisScan(0),
    mShakeTime(0.0),
    mShakeTotal(1.0),
    mShakeAmount(0.0),
    mShakeSpeed(DEFAULT_SHAKE_SPEED),
    mShakePhase(0.0),
    mShakeApplied(Ogre::Vector3::ZERO),
    mRandom(RANDOM_SEED),
    mCreaturesInitialized(false),
    mLastHeartHP(-1.0),
    mGeneration(0),
    mEntitiesInitialized(false)
{
    reloadConfig();
}

RoomAmbience::~RoomAmbience()
{
}

bool RoomAmbience::reloadConfig()
{
    stopAll();
    bool loaded = mConfig.load(mConfigPath + "roomAmbience.cfg");
    buildIndex();
    return loaded;
}

RoomAmbience::Mode RoomAmbience::modeFromString(const std::string& text)
{
    if(text == "reduced")
        return Mode::reduced;
    if(text == "off")
        return Mode::off;

    return Mode::full;
}

std::string RoomAmbience::modeToString(Mode mode)
{
    switch(mode)
    {
        case Mode::reduced:
            return "reduced";
        case Mode::off:
            return "off";
        default:
            return "full";
    }
}

void RoomAmbience::setMode(Mode mode)
{
    if(mMode == mode)
        return;

    mMode = mode;
    // Everything running is rebuilt by the next scan with the limits of the new mode
    stopAll();
}

void RoomAmbience::buildIndex()
{
    mTileEffects.assign(static_cast<size_t>(TileVisual::countTileVisual), std::vector<uint32_t>());
    mObjectEffectsMemo.clear();
    mEventEffects.clear();
    mBridgeEffects.clear();
    mScanRadius = MIN_SCAN_RADIUS;

    const std::vector<AmbienceEffect>& effects = mConfig.getEffects();
    for(uint32_t i = 0; i < effects.size(); ++i)
    {
        const AmbienceEffect& effect = effects[i];
        mScanRadius = std::max(mScanRadius, effect.mMaxDistance);
        if(effect.mTarget == AmbienceTarget::tile)
        {
            for(const std::string& name : effect.mMatch)
            {
                // Bridges are not rooms for the client: "bridge:<mesh>" or "bridge:<tile visual>"
                if(name.compare(0, BRIDGE_PREFIX.size(), BRIDGE_PREFIX) == 0)
                {
                    mBridgeEffects[name.substr(BRIDGE_PREFIX.size())].push_back(i);
                    continue;
                }

                // A room that this build does not have is simply never found
                TileVisual visual = Tile::tileVisualFromString(name);
                if(visual == TileVisual::nullTileVisual)
                    continue;

                mTileEffects[static_cast<size_t>(visual)].push_back(i);
            }
        }
        else if(effect.mTarget == AmbienceTarget::event)
        {
            mEventEffects[effect.mEvent].push_back(i);
        }
    }

    mScanRadius = std::min(mScanRadius, MAX_SCAN_RADIUS);
}

const std::vector<uint32_t>& RoomAmbience::getObjectEffects(const std::string& meshName, const std::string& kind)
{
    std::string memoKey = meshName + "|" + kind;
    std::map<std::string, std::vector<uint32_t> >::iterator it = mObjectEffectsMemo.find(memoKey);
    if(it != mObjectEffectsMemo.end())
        return it->second;

    std::vector<uint32_t> list;
    const std::vector<AmbienceEffect>& effects = mConfig.getEffects();
    for(uint32_t i = 0; i < effects.size(); ++i)
    {
        const AmbienceEffect& effect = effects[i];
        if(effect.mTarget != AmbienceTarget::object)
            continue;

        for(const std::string& pattern : effect.mMatch)
        {
            // "trap:<type>" matches the type of a trap or door (Alarm, Gas, DoorSteel...), all others the mesh
            bool matches = (pattern.compare(0, TRAP_PREFIX.size(), TRAP_PREFIX) == 0) ?
                (!kind.empty() && matchesPattern(pattern.substr(TRAP_PREFIX.size()), kind)) : matchesPattern(pattern, meshName);
            if(matches)
            {
                list.push_back(i);
                break;
            }
        }
    }

    std::pair<std::map<std::string, std::vector<uint32_t> >::iterator, bool> inserted =
        mObjectEffectsMemo.insert(std::make_pair(memoKey, list));
    return inserted.first->second;
}

std::vector<uint32_t> RoomAmbience::getBridgeEffects(Tile* tile) const
{
    std::vector<uint32_t> list;
    std::string meshName = tile->getMeshName();
    if((meshName.size() > MESH_SUFFIX.size())
        && (meshName.compare(meshName.size() - MESH_SUFFIX.size(), MESH_SUFFIX.size(), MESH_SUFFIX) == 0))
        meshName = meshName.substr(0, meshName.size() - MESH_SUFFIX.size());

    std::map<std::string, std::vector<uint32_t> >::const_iterator it = mBridgeEffects.find(meshName);
    if(it != mBridgeEffects.end())
        list.insert(list.end(), it->second.begin(), it->second.end());

    it = mBridgeEffects.find(Tile::tileVisualToString(tile->getTileVisual()));
    if(it != mBridgeEffects.end())
        list.insert(list.end(), it->second.begin(), it->second.end());

    return list;
}

bool RoomAmbience::isEffectUsable(const AmbienceEffect& effect) const
{
    if(mMode == Mode::off)
        return false;

    if(mMode == Mode::reduced)
        return effect.mReduced;

    return true;
}

double RoomAmbience::getDistanceLimit(const AmbienceEffect& effect) const
{
    if(mMode == Mode::reduced)
        return effect.mMaxDistance * mConfig.getReducedDistanceFactor();

    return effect.mMaxDistance;
}

bool RoomAmbience::isSleeperNear(double x, double y, double radius) const
{
    double radiusSquared = radius * radius;
    for(const Ogre::Vector3& position : mSleeperPositions)
    {
        double dx = position.x - x;
        double dy = position.y - y;
        if((dx * dx + dy * dy) <= radiusSquared)
            return true;
    }

    return false;
}

bool RoomAmbience::hasClip(MovableGameEntity* entity, const std::string& clip)
{
    std::string key = entity->getMeshName() + "|" + clip;
    std::map<std::string, bool>::const_iterator it = mClipKnown.find(key);
    if(it != mClipKnown.end())
        return it->second;

    if(RenderManager::getSingletonPtr() == nullptr)
        return false;

    bool entityFound = false;
    bool known = RenderManager::getSingleton().rrHasObjectClip(entity, clip, entityFound);
    if(entityFound)
        mClipKnown[key] = known;
    return known;
}

bool RoomAmbience::isCreatureNear(double x, double y, double radius) const
{
    double radiusSquared = radius * radius;
    for(const Ogre::Vector3& position : mCreaturePositions)
    {
        double dx = position.x - x;
        double dy = position.y - y;
        if((dx * dx + dy * dy) <= radiusSquared)
            return true;
    }

    return false;
}

bool RoomAmbience::isLocalHeartBelow(const Ogre::Vector3& position, double below) const
{
    ODClient* client = ODClient::getSingletonPtr();
    Player* localPlayer = mGameMap->getLocalPlayer();
    if((client == nullptr) || (localPlayer == nullptr) || (localPlayer->getSeat() == nullptr))
        return false;

    // Only the health of the own heart is known to the client
    Tile* tile = mGameMap->getTile(static_cast<int32_t>(std::floor(position.x + 0.5f)),
        static_cast<int32_t>(std::floor(position.y + 0.5f)));
    if((tile == nullptr) || (tile->getSeat() != localPlayer->getSeat()))
        return false;

    const HeartHealthRing::BadgeState& badge = client->getHeartBadge();
    // Before the first message of the server the health is not known
    if(badge.mHP < 0.0)
        return false;

    return static_cast<double>(badge.mFraction) < below;
}

double RoomAmbience::getHeartRateFactor(const Ogre::Vector3& position) const
{
    double best = 1.0;
    double bestDistance = HEART_RATE_RADIUS * HEART_RATE_RADIUS;
    for(std::vector<HeartRate>::const_iterator it = mHeartRates.begin(); it != mHeartRates.end(); ++it)
    {
        double dx = it->mPosition.x - position.x;
        double dy = it->mPosition.y - position.y;
        double distance = dx * dx + dy * dy;
        if(distance <= bestDistance)
        {
            bestDistance = distance;
            best = it->mFactor;
        }
    }

    return best;
}

bool RoomAmbience::isIdleLongEnough(const std::string& key, bool busy, double after)
{
    std::map<std::string, BusyInfo>::iterator it = mBusy.find(key);
    if(it == mBusy.end())
    {
        // A target counts as empty only after it was seen without a creature for the given time
        BusyInfo info;
        info.mLastBusy = mClock;
        it = mBusy.insert(std::make_pair(key, info)).first;
    }

    it->second.mLastTouched = mClock;
    if(busy)
    {
        it->second.mLastBusy = mClock;
        it->second.mEverBusy = true;
    }

    return (mClock - it->second.mLastBusy) >= after;
}

bool RoomAmbience::isIdleState(const AmbienceEffect& effect, const std::string& key, bool busy)
{
    bool idle = isIdleLongEnough(key, busy, effect.mAfter);
    if(effect.mWhen != AmbienceWhen::vacated)
        return idle;

    // Only a target that was used while it was watched counts as left behind
    std::map<std::string, BusyInfo>::iterator it = mBusy.find(key);
    return idle && (it != mBusy.end()) && it->second.mEverBusy;
}

bool RoomAmbience::isVisibleNear(Ogre::Camera* camera, const Ogre::Vector3& cameraPosition,
        const Ogre::Vector3& position, double radius, double limit) const
{
    if((position - cameraPosition).length() > limit)
        return false;

    Ogre::Vector3 extent(static_cast<Ogre::Real>(radius), static_cast<Ogre::Real>(radius), static_cast<Ogre::Real>(radius));
    return camera->isVisible(Ogre::AxisAlignedBox(position - extent, position + extent));
}

bool RoomAmbience::createParticleSystem(const std::string& system, const Ogre::Vector3& position,
        const std::string& baseName, Ogre::SceneNode*& node, Ogre::ParticleSystem*& particleSystem)
{
    if(Ogre::ParticleSystemManager::getSingleton().getTemplate(system) == nullptr)
    {
        if(mMissingSystems.insert(system).second)
            OD_LOG_WRN("Room ambience: unknown particle system " + system);
        return false;
    }

    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    std::string name = "RoomAmbience_" + baseName + "_" + Helper::toString(++mUniqueNumber);
    node = sceneManager->getRootSceneNode()->createChildSceneNode(name + "_node", position);
    particleSystem = sceneManager->createParticleSystem(name, system);
    node->attachObject(particleSystem);
    return true;
}

bool RoomAmbience::createModel(const std::string& mesh, const Ogre::Vector3& position, double yaw,
        const std::string& baseName, Ogre::SceneNode*& node, Ogre::Entity*& entity)
{
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    std::string name = "RoomAmbience_" + baseName + "_" + Helper::toString(++mUniqueNumber);
    try
    {
        entity = sceneManager->createEntity(name, mesh);
    }
    catch(const Ogre::Exception&)
    {
        entity = nullptr;
        if(mMissingSystems.insert(mesh).second)
            OD_LOG_WRN("Room ambience: unknown mesh " + mesh);
        return false;
    }

    entity->setCastShadows(false);
    node = sceneManager->getRootSceneNode()->createChildSceneNode(name + "_node", position);
    node->setOrientation(Ogre::Quaternion(Ogre::Degree(static_cast<Ogre::Real>(yaw)), Ogre::Vector3::UNIT_Z));
    node->attachObject(entity);
    return true;
}

void RoomAmbience::moveModel(Emitter& emitter, const AmbienceEffect& effect, double timeSinceLastFrame)
{
    // The decoration is moved around its own base pose
    double wave = std::sin(TWO_PI * effect.mSpeed * mClock + emitter.mPhase);
    Ogre::Quaternion orientation = emitter.mBaseOrientation;
    Ogre::Vector3 position = emitter.mBasePosition;
    Ogre::Vector3 scale = Ogre::Vector3::UNIT_SCALE;
    switch(effect.mMotion)
    {
        case AmbienceMotion::sway:
        {
            orientation = orientation * Ogre::Quaternion(Ogre::Degree(static_cast<Ogre::Real>(effect.mAmount * wave)), effect.mAxis);
            break;
        }
        case AmbienceMotion::wobble:
        {
            double envelope = std::sin(TWO_PI * WOBBLE_SWELL_RATE * mClock + emitter.mPhase);
            envelope = (envelope > 0.0) ? envelope * envelope : 0.0;
            orientation = orientation * Ogre::Quaternion(
                Ogre::Degree(static_cast<Ogre::Real>(effect.mAmount * wave * envelope)), effect.mAxis);
            break;
        }
        case AmbienceMotion::bob:
        {
            position.z += static_cast<Ogre::Real>(effect.mAmount * wave);
            break;
        }
        case AmbienceMotion::pulse:
        {
            scale = scale * static_cast<Ogre::Real>(1.0 + effect.mAmount * wave);
            break;
        }
        case AmbienceMotion::spin:
        {
            emitter.mCycle += effect.mSpeed * timeSinceLastFrame * DEGREES_TO_RADIANS;
            orientation = orientation * Ogre::Quaternion(Ogre::Radian(static_cast<Ogre::Real>(emitter.mCycle)), effect.mAxis);
            break;
        }
        case AmbienceMotion::flicker:
        {
            double noise = 0.5 * (std::sin(TWO_PI * effect.mSpeed * mClock + emitter.mPhase)
                + std::sin(TWO_PI * effect.mSpeed * FLICKER_SECOND_WAVE_RATE * mClock + 2.0 * emitter.mPhase));
            scale = scale * static_cast<Ogre::Real>(1.0 + effect.mAmount * noise);
            break;
        }
    }

    emitter.mNode->setOrientation(orientation);
    emitter.mNode->setPosition(position);
    emitter.mNode->setScale(scale);
}

void RoomAmbience::destroyEmitter(Emitter& emitter)
{
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    if(emitter.mNode != nullptr)
        emitter.mNode->detachAllObjects();
    if(emitter.mSystem != nullptr)
        sceneManager->destroyParticleSystem(emitter.mSystem);
    if(emitter.mEntity != nullptr)
        sceneManager->destroyEntity(emitter.mEntity);
    if(emitter.mNode != nullptr)
        sceneManager->destroySceneNode(emitter.mNode);

    emitter.mNode = nullptr;
    emitter.mSystem = nullptr;
    emitter.mEntity = nullptr;
}

void RoomAmbience::restoreMotionNode(MotionNode& motionNode)
{
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    if(!sceneManager->hasSceneNode(motionNode.mNodeName))
        return;

    Ogre::SceneNode* node = sceneManager->getSceneNode(motionNode.mNodeName);
    if(motionNode.mMovesOrientation)
        node->setOrientation(motionNode.mBaseOrientation);
    if(motionNode.mMovesPosition)
        node->setPosition(motionNode.mBasePosition);
    if(motionNode.mMovesScale)
        node->setScale(motionNode.mBaseScale);
}

void RoomAmbience::stopAll()
{
    if(RenderManager::getSingletonPtr() != nullptr)
    {
        for(std::map<std::string, Emitter>::iterator it = mEmitters.begin(); it != mEmitters.end(); ++it)
            destroyEmitter(it->second);

        for(OneShot& oneShot : mOneShots)
        {
            Emitter emitter;
            emitter.mNode = oneShot.mNode;
            emitter.mSystem = oneShot.mSystem;
            destroyEmitter(emitter);
        }

        for(OneShot& mark : mMarks)
        {
            Emitter emitter;
            emitter.mNode = mark.mNode;
            emitter.mSystem = mark.mSystem;
            destroyEmitter(emitter);
        }

        for(std::map<std::string, MotionNode>::iterator it = mMotionNodes.begin(); it != mMotionNodes.end(); ++it)
            restoreMotionNode(it->second);

        for(std::map<std::string, Turret>::iterator it = mTurrets.begin(); it != mTurrets.end(); ++it)
            restoreTurret(it->second);

        for(Collapse& collapse : mCollapses)
            destroyCollapse(collapse);

        for(Flight& flight : mFlights)
            destroyFlight(flight);
    }

    mEmitters.clear();
    mOneShots.clear();
    mMarks.clear();
    mCollapses.clear();
    mFlights.clear();
    mPendingSounds.clear();
    mTurrets.clear();
    mShakeTime = 0.0;
    clearShake();
    mMotionNodes.clear();
    mParticleCandidates.clear();
    mMotionCandidates.clear();
    mClipCandidates.clear();
    mClipTimers.clear();
    mSeenVisual.clear();
    mSeenSizeX = 0;
    mSeenSizeY = 0;
    mKnownEntities.clear();
    mKnownCreatures.clear();
    mCreaturesInitialized = false;
    mLastHeartHP = -1.0;
    mEntitiesInitialized = false;
    mScanTimer = 0.0;
    mHeartRates.clear();
    mExtras.reset();
}

void RoomAmbience::update(Ogre::Real timeSinceLastFrame)
{
    if(mMode == Mode::off)
        return;

    double dt = static_cast<double>(timeSinceLastFrame);
    mClock += dt;
    mScanTimer += dt;
    if(mScanTimer >= mConfig.getScanInterval())
    {
        mScanTimer = 0.0;
        scan();
    }

    updateEmitters(dt);
    updateOneShots(mOneShots, dt);
    updateOneShots(mMarks, dt);
    updatePendingSounds();
    updateCollapses(dt);
    updateFlights(dt);
    updateShake(dt);
    updateMotions(dt);
    updateTurrets(dt);
}

void RoomAmbience::scan()
{
    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
    if((frameListener == nullptr) || (mGameMap == nullptr) || (mGameMap->getMapSizeX() <= 0))
        return;

    Ogre::Camera* camera = frameListener->getCameraManager()->getActiveCamera();
    if(camera == nullptr)
        return;

    Ogre::Vector3 cameraPosition = camera->getDerivedPosition();
    Ogre::Vector3 lookPoint = getLookPoint(cameraPosition, camera->getDerivedDirection());

    ++mGeneration;
    mEventsThisScan = 0;
    mParticleCandidates.clear();
    mMotionCandidates.clear();
    mClipCandidates.clear();

    mCreaturePositions.clear();
    mSleeperPositions.clear();
    mCreatureSpots.clear();
    for(Creature* creature : mGameMap->getCreatures())
    {
        if(creature->getIsOnMap())
        {
            mCreaturePositions.push_back(creature->getPosition());
            Ogre::AnimationState* creatureState = creature->getAnimationState();
            if((creatureState != nullptr) && (creatureState->getAnimationName() == EntityAnimation::sleep_anim))
                mSleeperPositions.push_back(creature->getPosition());
            CreatureSpot spot;
            spot.mPosition = creature->getPosition();
            spot.mSeat = creature->getSeat();
            mCreatureSpots.push_back(spot);
        }
    }

    // Before the objects, so that the hit pulse of the heart starts in the same scan
    scanHeartHit();
    scanObjects(camera, cameraPosition);
    scanTiles(camera, cameraPosition, lookPoint);
    scanEntityEvents(camera, cameraPosition);
    scanCreatureEvents();
    mExtras.scan(*this, mGameMap, mClock);
    reconcile();
    playClips();

    mPruneTimer += mConfig.getScanInterval();
    if(mPruneTimer >= PRUNE_INTERVAL_SECONDS)
    {
        mPruneTimer = 0.0;
        for(std::map<std::string, BusyInfo>::iterator it = mBusy.begin(); it != mBusy.end();)
        {
            if((mClock - it->second.mLastTouched) > BUSY_KEEP_SECONDS)
                mBusy.erase(it++);
            else
                ++it;
        }

        for(std::map<std::string, double>::iterator it = mClipTimers.begin(); it != mClipTimers.end();)
        {
            if(it->second < (mClock - CLIP_TIMER_KEEP_SECONDS))
                mClipTimers.erase(it++);
            else
                ++it;
        }

        for(std::map<std::string, double>::iterator it = mClipHoldUntil.begin(); it != mClipHoldUntil.end();)
        {
            if(it->second < mClock)
                mClipHoldUntil.erase(it++);
            else
                ++it;
        }
    }
}

void RoomAmbience::scanObjects(Ogre::Camera* camera, const Ogre::Vector3& cameraPosition)
{
    const std::vector<AmbienceEffect>& effects = mConfig.getEffects();
    const std::vector<RenderedMovableEntity*>& entities = mGameMap->getRenderedMovableEntities();
    for(RenderedMovableEntity* entity : entities)
    {
        if(entity->getEntityNode() == nullptr)
            continue;

        // The trap and door types share meshes; their entity names start with the type (Alarm_3_...)
        std::string kind;
        if(entity->getObjectType() == GameEntityType::trapEntity)
            kind = entity->getName().substr(0, entity->getName().find('_'));
        // A boulder that rolls (the missile of the boulder trap, mesh Boulder) gets its dust trail. One that lies still
        // gets nothing, and neither do the other missiles (cannonballs, blasts, stones): they have no dust of their own
        else if((entity->getObjectType() == GameEntityType::missileObject) && entity->isMoving() &&
                (entity->getMeshName() == "Boulder"))
            kind = "BoulderMoving";

        const std::vector<uint32_t>& list = getObjectEffects(entity->getMeshName(), kind);
        if(list.empty())
            continue;

        const Ogre::Vector3& position = entity->getPosition();
        double distance = (position - cameraPosition).length();
        // Offsets are given for the object as it is modelled, so they turn with the object
        Ogre::Quaternion turn(Ogre::Degree(entity->getRotationAngle()), Ogre::Vector3::UNIT_Z);
        int32_t visible = -1;
        int32_t busy = -1;
        for(uint32_t index : list)
        {
            const AmbienceEffect& effect = effects[index];
            if(!isEffectUsable(effect))
                continue;

            // Only the keeper the object belongs to sees it (the glint of a secret door must not give it away)
            if(effect.mOwnerOnly)
            {
                Player* localPlayer = mGameMap->getLocalPlayer();
                if((localPlayer == nullptr) || (localPlayer->getSeat() == nullptr) ||
                   (entity->getSeat() != localPlayer->getSeat()))
                {
                    continue;
                }
            }

            // An effect that already runs is kept a bit beyond its distance
            double limit = getDistanceLimit(effect) * VIEW_DISTANCE_MARGIN;
            if(distance > limit)
                continue;

            if(visible < 0)
                visible = isVisibleNear(camera, cameraPosition, position, OBJECT_VISIBLE_RADIUS, limit) ? 1 : 0;
            if(visible == 0)
                continue;

            if(effect.mKind == AmbienceKind::turn)
            {
                // Not a candidate: the object is followed from frame to frame by updateTurrets
                std::map<std::string, Turret>::iterator turretIt = mTurrets.find(entity->getName());
                if(turretIt == mTurrets.end())
                {
                    if((mMotionNodes.size() + mTurrets.size()) >= mConfig.getMaxMotions())
                        continue;

                    Turret turret;
                    turret.mNodeName = entity->getEntityNode()->getName();
                    Ogre::Vector3 front = entity->getEntityNode()->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Y;
                    front.z = 0.0f;
                    if(front.squaredLength() > MIN_REST_LENGTH_SQUARED)
                        turret.mRestFront = front;
                    turretIt = mTurrets.insert(std::make_pair(entity->getName(), turret)).first;
                }

                turretIt->second.mEffect = index;
                turretIt->second.mPosition = position;
                turretIt->second.mSeat = entity->getSeat();
                turretIt->second.mGeneration = mGeneration;
                continue;
            }

            Candidate candidate;
            candidate.mEffect = index;
            candidate.mTarget = entity->getName();
            candidate.mPosition = position + turn * effect.mOffset;
            candidate.mDistance = distance;
            candidate.mPriority = effect.mPriority;
            if(effect.mWhen == AmbienceWhen::always)
            {
                candidate.mActive = true;
            }
            else if(effect.mWhen == AmbienceWhen::locked)
            {
                // A door that is closed was locked by its keeper
                candidate.mActive = (entity->getAnimationStateName() == DOOR_CLOSED_STATE);
            }
            else if(effect.mWhen == AmbienceWhen::hit)
            {
                std::map<std::string, double>::const_iterator hitIt = mHitUntil.find(getPositionKey(position));
                candidate.mActive = (hitIt != mHitUntil.end()) && (hitIt->second > mClock);
            }
            else if((effect.mWhen == AmbienceWhen::reloading) || (effect.mWhen == AmbienceWhen::ready))
            {
                std::map<std::string, double>::const_iterator reloadIt = mReloadingUntil.find(getPositionKey(position));
                bool reloading = (reloadIt != mReloadingUntil.end()) && (reloadIt->second > mClock);
                if(effect.mWhen == AmbienceWhen::reloading)
                    candidate.mActive = reloading;
                else
                    candidate.mActive = !reloading;
            }
            else if(effect.mWhen == AmbienceWhen::lowHealth)
            {
                candidate.mActive = isLocalHeartBelow(position, effect.mBelow);
            }
            else if(effect.mWhen == AmbienceWhen::sleeping)
            {
                candidate.mActive = isSleeperNear(position.x, position.y, mConfig.getOccupiedRadius());
            }
            else
            {
                if(busy < 0)
                    busy = isCreatureNear(position.x, position.y, mConfig.getOccupiedRadius()) ? 1 : 0;

                if(effect.mWhen == AmbienceWhen::occupied)
                    candidate.mActive = (busy == 1);
                else
                    candidate.mActive = isIdleState(effect, "o" + entity->getName(), busy == 1);
            }

            if(effect.mKind == AmbienceKind::particle)
            {
                if(candidate.mActive)
                    mParticleCandidates.push_back(candidate);
            }
            else if((effect.mKind == AmbienceKind::clip) || (effect.mKind == AmbienceKind::sound))
            {
                if(candidate.mActive)
                    mClipCandidates.push_back(candidate);
                else if(effect.mLoop)
                    stopLoopClip(entity, effect);
            }
            else
            {
                mMotionCandidates.push_back(candidate);
            }
        }
    }
}

void RoomAmbience::scanTiles(Ogre::Camera* camera, const Ogre::Vector3& cameraPosition, const Ogre::Vector3& lookPoint)
{
    int32_t sizeX = mGameMap->getMapSizeX();
    int32_t sizeY = mGameMap->getMapSizeY();
    if((sizeX != mSeenSizeX) || (sizeY != mSeenSizeY))
    {
        mSeenSizeX = sizeX;
        mSeenSizeY = sizeY;
        mSeenVisual.assign(static_cast<size_t>(sizeX) * static_cast<size_t>(sizeY), NEVER_SEEN_VISUAL);
    }

    const std::vector<AmbienceEffect>& effects = mConfig.getEffects();
    int32_t centerX = static_cast<int32_t>(std::floor(lookPoint.x + 0.5f));
    int32_t centerY = static_cast<int32_t>(std::floor(lookPoint.y + 0.5f));
    int32_t radius = static_cast<int32_t>(std::ceil(mScanRadius));
    int32_t minX = std::max(0, centerX - radius);
    int32_t maxX = std::min(sizeX - 1, centerX + radius);
    int32_t minY = std::max(0, centerY - radius);
    int32_t maxY = std::min(sizeY - 1, centerY + radius);
    const uint8_t firstRoomVisual = static_cast<uint8_t>(TileVisual::dungeonTempleRoom);
    for(int32_t x = minX; x <= maxX; ++x)
    {
        for(int32_t y = minY; y <= maxY; ++y)
        {
            Tile* tile = mGameMap->getTile(x, y);
            if(tile == nullptr)
                continue;

            uint8_t current = static_cast<uint8_t>(tile->getTileVisual());
            if(current >= mTileEffects.size())
                continue;

            size_t seenIndex = static_cast<size_t>(x) * static_cast<size_t>(sizeY) + static_cast<size_t>(y);
            uint8_t previous = mSeenVisual[seenIndex];
            mSeenVisual[seenIndex] = current;
            bool currentRoom = (current >= firstRoomVisual) && (current < static_cast<uint8_t>(TileVisual::countTileVisual));
            // A tile that was not known yet (visual 0) coming into view is not a room being built
            if((previous != NEVER_SEEN_VISUAL) && (previous != UNKNOWN_VISUAL) && (previous != current))
            {
                bool previousRoom = (previous >= firstRoomVisual);
                if(currentRoom && !previousRoom)
                {
                    // A new room starts with no dust
                    BusyInfo info;
                    info.mLastBusy = mClock;
                    info.mLastTouched = mClock;
                    mBusy["t" + Helper::toString(x) + "_" + Helper::toString(y)] = info;
                    if(mEventsThisScan < MAX_EVENTS_PER_SCAN)
                    {
                        ++mEventsThisScan;
                        triggerEvent("RoomBuilt", tile->getPosition(), false, Tile::tileVisualToString(tile->getTileVisual()));
                    }
                }
                else if(previousRoom && !currentRoom)
                {
                    if(mEventsThisScan < MAX_EVENTS_PER_SCAN)
                    {
                        ++mEventsThisScan;
                        triggerEvent("RoomSold", tile->getPosition(), false, Tile::tileVisualToString(static_cast<TileVisual>(previous)));
                    }
                }
            }

            std::vector<uint32_t> bridgeList;
            const std::vector<uint32_t>* listPointer = &mNoEffects;
            if(!mTileEffects[current].empty())
            {
                listPointer = &mTileEffects[current];
            }
            else if(!mBridgeEffects.empty() && tile->getHasBridge()
                && ((tile->getTileVisual() == TileVisual::lavaGround) || (tile->getTileVisual() == TileVisual::waterGround)))
            {
                bridgeList = getBridgeEffects(tile);
                listPointer = &bridgeList;
            }

            if(listPointer->empty())
                continue;

            const std::vector<uint32_t>& list = *listPointer;

            const Ogre::Vector3& position = tile->getPosition();
            double distance = (position - cameraPosition).length();
            int32_t visible = -1;
            int32_t busy = -1;
            int32_t wall = -1;
            Ogre::Vector3 wallShift = Ogre::Vector3::ZERO;
            std::string key = "t" + Helper::toString(x) + "_" + Helper::toString(y);
            for(uint32_t index : list)
            {
                const AmbienceEffect& effect = effects[index];
                if(!isEffectUsable(effect))
                    continue;

                double limit = getDistanceLimit(effect) * VIEW_DISTANCE_MARGIN;
                if(distance > limit)
                    continue;

                if(effect.mSpacing > 1)
                {
                    uint32_t hash = static_cast<uint32_t>(x * TILE_HASH_X) ^ static_cast<uint32_t>(y * TILE_HASH_Y);
                    if(((hash >> TILE_HASH_SHIFT) % effect.mSpacing) != 0)
                        continue;
                }

                if(effect.mNeedWall || effect.mWallSide)
                {
                    if(wall < 0)
                    {
                        wall = 0;
                        for(Tile* neighbor : tile->getAllNeighbors())
                        {
                            if((neighbor != nullptr) && (neighbor->getFullness() > 0.0))
                            {
                                wall = 1;
                                // The edge of the tile that touches a wall (a neighbor straight beside the tile is preferred)
                                int32_t stepX = neighbor->getX() - x;
                                int32_t stepY = neighbor->getY() - y;
                                if((stepX == 0) || (stepY == 0))
                                {
                                    wallShift = Ogre::Vector3(static_cast<Ogre::Real>(stepX) * WALL_SIDE_SHIFT,
                                        static_cast<Ogre::Real>(stepY) * WALL_SIDE_SHIFT, 0.0f);
                                    break;
                                }
                            }
                        }
                    }

                    if(wall == 0)
                        continue;
                }

                if(visible < 0)
                    visible = isVisibleNear(camera, cameraPosition, position, TILE_VISIBLE_RADIUS, limit) ? 1 : 0;
                if(visible == 0)
                    continue;

                Candidate candidate;
                candidate.mEffect = index;
                candidate.mTarget = key;
                candidate.mPosition = position + effect.mOffset;
                if(effect.mWallSide)
                    candidate.mPosition += wallShift;
                if(effect.mKind == AmbienceKind::model)
                {
                    // The decoration looks away from the wall it hangs on (front along +Y when not turned)
                    if((wallShift.x != 0.0f) || (wallShift.y != 0.0f))
                        candidate.mYaw = Ogre::Degree(Ogre::Math::ATan2(wallShift.x, -wallShift.y)).valueDegrees();
                }
                candidate.mDistance = distance;
                candidate.mPriority = effect.mPriority;
                if(effect.mWhen == AmbienceWhen::always)
                {
                    candidate.mActive = true;
                }
                else
                {
                    if(busy < 0)
                        busy = isCreatureNear(position.x, position.y, mConfig.getOccupiedRadius() + TILE_OCCUPIED_MARGIN) ? 1 : 0;

                    if(effect.mWhen == AmbienceWhen::occupied)
                        candidate.mActive = (busy == 1);
                    else
                        candidate.mActive = isIdleState(effect, key, busy == 1);
                }

                if(effect.mKind == AmbienceKind::motion)
                {
                    // Only the bridge of a tile can be moved; a motion of another tile has nothing to move
                    if(listPointer == &bridgeList)
                    {
                        candidate.mNodeName = tile->getOgreNamePrefix() + tile->getName() + "_bridgeMesh_node";
                        mMotionCandidates.push_back(candidate);
                    }
                    continue;
                }

                if(!candidate.mActive)
                    continue;

                if((effect.mKind == AmbienceKind::particle) || (effect.mKind == AmbienceKind::model))
                    mParticleCandidates.push_back(candidate);
            }
        }
    }
}

void RoomAmbience::scanEntityEvents(Ogre::Camera* camera, const Ogre::Vector3& cameraPosition)
{
    struct Change
    {
        //! Name of the event
        std::string mEvent;
        //! Trap or door type the event is about (empty = use the tile)
        std::string mVisual;
        //! Creature the event is about (empty = none)
        std::string mCreature;
        //! Where the thing is
        Ogre::Vector3 mPosition;
    };

    std::vector<Change> changes;
    const std::vector<RenderedMovableEntity*>& entities = mGameMap->getRenderedMovableEntities();
    for(RenderedMovableEntity* entity : entities)
    {
        const std::string& name = entity->getName();
        std::map<std::string, EntitySnapshot>::iterator it = mKnownEntities.find(name);
        if(it != mKnownEntities.end())
        {
            it->second.mGeneration = mGeneration;
            it->second.mPosition = entity->getPosition();
            continue;
        }

        EntitySnapshot snapshot;
        snapshot.mType = static_cast<uint32_t>(entity->getObjectType());
        snapshot.mPosition = entity->getPosition();
        snapshot.mGeneration = mGeneration;
        mKnownEntities.insert(std::make_pair(name, snapshot));
        if(!mEntitiesInitialized)
            continue;

        Change change;
        change.mPosition = snapshot.mPosition;
        switch(entity->getObjectType())
        {
            case GameEntityType::skillEntity:
                change.mEvent = "ResearchDone";
                break;
            case GameEntityType::craftedTrap:
                change.mEvent = "ItemCrafted";
                break;
            case GameEntityType::chickenEntity:
                change.mEvent = "ChickenArrived";
                break;
            case GameEntityType::trapEntity:
                // A trap or door that was built (or that the client sees for the first time)
                change.mEvent = "TrapBuilt";
                change.mVisual = name.substr(0, name.find('_'));
                break;
            default:
                break;
        }

        if(!change.mEvent.empty())
            changes.push_back(change);
    }

    // Creatures that appear on a portal
    for(Creature* creature : mGameMap->getCreatures())
    {
        std::string key = "c:" + creature->getName();
        std::map<std::string, EntitySnapshot>::iterator it = mKnownEntities.find(key);
        if(it != mKnownEntities.end())
        {
            it->second.mGeneration = mGeneration;
            continue;
        }

        EntitySnapshot snapshot;
        snapshot.mType = static_cast<uint32_t>(GameEntityType::creature);
        snapshot.mPosition = creature->getPosition();
        snapshot.mGeneration = mGeneration;
        mKnownEntities.insert(std::make_pair(key, snapshot));
        if(mEntitiesInitialized)
        {
            Change change;
            change.mEvent = "CreatureArrived";
            change.mPosition = snapshot.mPosition;
            change.mCreature = creature->getName();
            // A creature that appears on a tile of a crypt of its own keeper was raised there
            Tile* arrivalTile = creature->getPositionTile();
            if((arrivalTile != nullptr) && (arrivalTile->getTileVisual() == TileVisual::cryptRoom) &&
               (arrivalTile->getSeat() == creature->getSeat()))
            {
                change.mEvent = "CryptRaised";
            }
            changes.push_back(change);
        }
    }

    for(std::map<std::string, EntitySnapshot>::iterator it = mKnownEntities.begin(); it != mKnownEntities.end();)
    {
        if(it->second.mGeneration == mGeneration)
        {
            ++it;
            continue;
        }

        if(mEntitiesInitialized && (it->second.mType == static_cast<uint32_t>(GameEntityType::treasuryObject)))
        {
            Change change;
            change.mEvent = "GoldDeposited";
            change.mPosition = it->second.mPosition;
            changes.push_back(change);
        }
        else if(mEntitiesInitialized && (it->second.mType == static_cast<uint32_t>(GameEntityType::trapEntity)))
        {
            // A trap or door that is gone: sold, unless it was destroyed (reported with its own effect) or the
            // client only lost sight of it
            int32_t soldX = static_cast<int32_t>(std::floor(it->second.mPosition.x + 0.5f));
            int32_t soldY = static_cast<int32_t>(std::floor(it->second.mPosition.y + 0.5f));
            Tile* soldTile = mGameMap->getTile(soldX, soldY);
            std::map<std::string, double>::const_iterator wreckedIt = mWreckedUntil.find(getTileKey(soldX, soldY));
            bool wrecked = (wreckedIt != mWreckedUntil.end()) && (wreckedIt->second > mClock);
            // The state of a trap that reloads is not needed any more
            mReloadingUntil.erase(getTileKey(soldX, soldY));
            if(!wrecked && (soldTile != nullptr) && soldTile->getLocalPlayerHasVision())
            {
                Change change;
                change.mEvent = "TrapSold";
                change.mVisual = it->first.substr(0, it->first.find('_'));
                change.mPosition = it->second.mPosition;
                changes.push_back(change);
            }
        }

        mKnownEntities.erase(it++);
    }

    // Many changes at once are a map being loaded or revealed, not things being made
    bool settled = (mClock > SETTLE_SECONDS) && mEntitiesInitialized && (changes.size() <= MAX_EVENTS_PER_SCAN);
    mEntitiesInitialized = true;
    if(!settled)
        return;

    for(const Change& change : changes)
    {
        // Creatures that wake or are raised together each get their clips
        bool together = (change.mEvent == "CreatureWoke") || (change.mEvent == "CryptRaised");
        triggerEvent(change.mEvent, change.mPosition, false, change.mVisual, together, nullptr, change.mCreature);
    }
}

void RoomAmbience::scanHeartHit()
{
    ODClient* client = ODClient::getSingletonPtr();
    Player* localPlayer = mGameMap->getLocalPlayer();
    if((client == nullptr) || (localPlayer == nullptr) || (localPlayer->getSeat() == nullptr))
        return;

    // Only the health of the own heart is known to the client (the heart badge)
    double hp = client->getHeartBadge().mHP;
    double previous = mLastHeartHP;
    mLastHeartHP = hp;
    // Before the first message of the server the health is not known; a map that is still loading is no hit
    if((hp < 0.0) || (previous < 0.0) || (hp >= previous) || (mClock <= SETTLE_SECONDS))
        return;

    const std::vector<RenderedMovableEntity*>& entities = mGameMap->getRenderedMovableEntities();
    for(RenderedMovableEntity* entity : entities)
    {
        if(entity->getMeshName() != HEART_MESH)
            continue;

        const Ogre::Vector3& position = entity->getPosition();
        int32_t tileX = static_cast<int32_t>(std::floor(position.x + 0.5f));
        int32_t tileY = static_cast<int32_t>(std::floor(position.y + 0.5f));
        Tile* tile = mGameMap->getTile(tileX, tileY);
        if((tile == nullptr) || (tile->getSeat() != localPlayer->getSeat()))
            continue;

        mHitUntil[getTileKey(tileX, tileY)] = mClock + HEART_HIT_SECONDS;
        triggerEvent("HeartHit", position, false, "dungeonTempleRoom");
    }
}

void RoomAmbience::scanCreatureEvents()
{
    struct Change
    {
        //! \brief Creates a change that is not set yet
        Change() :
            mPosition(Ogre::Vector3::ZERO)
        {}

        //! Name of the event
        std::string mEvent;
        //! Where the creature is
        Ogre::Vector3 mPosition;
        //! Tile visual (room) the creature stands in
        std::string mVisual;
        //! Name of the creature
        std::string mCreature;
    };

    std::vector<Change> changes;
    for(Creature* creature : mGameMap->getCreatures())
    {
        if(!creature->getIsOnMap())
            continue;

        std::string key = creature->getName();
        Tile* tile = creature->getPositionTile();
        std::string visual;
        if(tile != nullptr)
            visual = Tile::tileVisualToString(tile->getTileVisual());

        bool sleeping = false;
        Ogre::AnimationState* animationState = creature->getAnimationState();
        if(animationState != nullptr)
            sleeping = (animationState->getAnimationName() == EntityAnimation::sleep_anim);

        bool enemyInGuardRoom = false;
        if((tile != nullptr) && (tile->getTileVisual() == TileVisual::guardRoom) && (tile->getSeat() != nullptr) &&
           (creature->getSeat() != nullptr))
        {
            enemyInGuardRoom = !tile->getSeat()->isAlliedSeat(creature->getSeat());
        }

        double hp = creature->getHP();
        bool prisoner = creature->isInContainment();
        Seat* creatureSeat = creature->getSeat();
        std::map<std::string, CreatureSnapshot>::iterator it = mKnownCreatures.find(key);
        if(it == mKnownCreatures.end())
        {
            // A creature seen for the first time only teaches its state
            CreatureSnapshot snapshot;
            snapshot.mHp = hp;
            snapshot.mSleeping = sleeping;
            snapshot.mPrisoner = prisoner;
            snapshot.mSeat = creatureSeat;
            snapshot.mEnemyInGuardRoom = enemyInGuardRoom;
            snapshot.mGeneration = mGeneration;
            mKnownCreatures.insert(std::make_pair(key, snapshot));
            continue;
        }

        CreatureSnapshot& snapshot = it->second;
        snapshot.mGeneration = mGeneration;
        if(mCreaturesInitialized)
        {
            Change change;
            change.mPosition = creature->getPosition();
            change.mVisual = visual;
            change.mCreature = key;
            if(snapshot.mSleeping && !sleeping && (visual == "dormitoryRoom"))
            {
                change.mEvent = "CreatureWoke";
                changes.push_back(change);
            }

            if(enemyInGuardRoom && !snapshot.mEnemyInGuardRoom)
            {
                change.mEvent = "EnemyEntered";
                changes.push_back(change);
            }

            if(prisoner && !snapshot.mPrisoner && (visual == "prisonRoom"))
            {
                change.mEvent = "PrisonerArrived";
                changes.push_back(change);
            }

            // A creature that changes its keeper in a torture chamber has been converted
            if((snapshot.mSeat != nullptr) && (creatureSeat != nullptr) && (snapshot.mSeat != creatureSeat) &&
               (visual == "tortureRoom"))
            {
                change.mEvent = "TortureConverted";
                changes.push_back(change);
            }

            if((visual == "templeRoom") && (hp >= (snapshot.mHp + MIN_HEAL_AMOUNT)) &&
               ((mClock - snapshot.mLastHealed) >= HEAL_SPACING_SECONDS))
            {
                snapshot.mLastHealed = mClock;
                change.mEvent = "CreatureHealed";
                changes.push_back(change);
            }
        }

        snapshot.mHp = hp;
        snapshot.mSleeping = sleeping;
        snapshot.mPrisoner = prisoner;
        snapshot.mSeat = creatureSeat;
        snapshot.mEnemyInGuardRoom = enemyInGuardRoom;
    }

    for(std::map<std::string, CreatureSnapshot>::iterator it = mKnownCreatures.begin(); it != mKnownCreatures.end();)
    {
        if(it->second.mGeneration == mGeneration)
            ++it;
        else
            mKnownCreatures.erase(it++);
    }

    // Many changes at once are a map being loaded or revealed, not creatures acting
    bool settled = (mClock > SETTLE_SECONDS) && mCreaturesInitialized && (changes.size() <= MAX_EVENTS_PER_SCAN);
    mCreaturesInitialized = true;
    if(!settled)
        return;

    for(const Change& change : changes)
    {
        // Creatures that wake or are raised together each get their clips
        bool together = (change.mEvent == "CreatureWoke") || (change.mEvent == "CryptRaised");
        triggerEvent(change.mEvent, change.mPosition, false, change.mVisual, together, nullptr, change.mCreature);
    }
}

void RoomAmbience::reconcile()
{
    const std::vector<AmbienceEffect>& effects = mConfig.getEffects();

    // Particle systems that run for something that is in view
    std::vector<std::pair<int32_t, double> > order;
    for(const Candidate& candidate : mParticleCandidates)
        order.push_back(std::make_pair(candidate.mPriority, candidate.mDistance));

    std::vector<uint32_t> sorted;
    for(uint32_t i = 0; i < order.size(); ++i)
        sorted.push_back(i);
    std::stable_sort(sorted.begin(), sorted.end(), IndexOrder(order));

    for(std::map<std::string, Emitter>::iterator it = mEmitters.begin(); it != mEmitters.end(); ++it)
        it->second.mSeen = false;

    uint32_t budget = mConfig.getMaxParticles(mMode == Mode::reduced);
    uint32_t used = static_cast<uint32_t>(mOneShots.size());
    for(uint32_t i = 0; i < sorted.size(); ++i)
    {
        const Candidate& candidate = mParticleCandidates[sorted[i]];
        const AmbienceEffect& effect = effects[candidate.mEffect];
        std::string key = Helper::toString(candidate.mEffect) + "|" + candidate.mTarget;
        std::map<std::string, Emitter>::iterator it = mEmitters.find(key);
        if(it != mEmitters.end())
        {
            it->second.mSeen = true;
            if(it->second.mFade >= 0.0)
            {
                it->second.mFade = -1.0;
                if(it->second.mSystem != nullptr)
                    it->second.mSystem->setEmitting(true);
            }
            ++used;
            continue;
        }

        if(used >= budget)
            continue;

        Emitter emitter;
        if(effect.mKind == AmbienceKind::model)
        {
            if(!createModel(effect.mMesh, candidate.mPosition, candidate.mYaw, effect.mName, emitter.mNode, emitter.mEntity))
                continue;

            emitter.mBaseOrientation = emitter.mNode->getOrientation();
            emitter.mBasePosition = emitter.mNode->getPosition();
        }
        else
        {
            if(!createParticleSystem(effect.mSystem, candidate.mPosition, effect.mName, emitter.mNode, emitter.mSystem))
                continue;

            emitter.mBaseWidth = emitter.mSystem->getDefaultWidth();
            emitter.mBaseHeight = emitter.mSystem->getDefaultHeight();
        }

        emitter.mEffect = candidate.mEffect;
        emitter.mPhase = hashPhase(key);
        emitter.mSeen = true;
        mEmitters.insert(std::make_pair(key, emitter));
        ++used;
    }

    for(std::map<std::string, Emitter>::iterator it = mEmitters.begin(); it != mEmitters.end(); ++it)
    {
        if(it->second.mSeen || (it->second.mFade >= 0.0))
            continue;

        // A decoration has no particles to let run out, it is removed at once
        it->second.mFade = (it->second.mSystem != nullptr) ? 0.0 : EMITTER_FADE_SECONDS;
        if(it->second.mSystem != nullptr)
            it->second.mSystem->setEmitting(false);
    }

    // Moved objects
    order.clear();
    for(const Candidate& candidate : mMotionCandidates)
        order.push_back(std::make_pair(candidate.mPriority, candidate.mDistance));

    sorted.clear();
    for(uint32_t i = 0; i < order.size(); ++i)
        sorted.push_back(i);
    std::stable_sort(sorted.begin(), sorted.end(), IndexOrder(order));

    for(std::map<std::string, MotionNode>::iterator it = mMotionNodes.begin(); it != mMotionNodes.end(); ++it)
    {
        it->second.mSeen = false;
        for(MotionInstance& instance : it->second.mMotions)
            instance.mWanted = false;
    }

    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    uint32_t maxMotions = mConfig.getMaxMotions();
    for(uint32_t i = 0; i < sorted.size(); ++i)
    {
        const Candidate& candidate = mMotionCandidates[sorted[i]];
        std::map<std::string, MotionNode>::iterator it = mMotionNodes.find(candidate.mTarget);
        if(it == mMotionNodes.end())
        {
            // Only objects that should move now are started
            if(!candidate.mActive || (mMotionNodes.size() >= maxMotions))
                continue;

            Ogre::SceneNode* node = nullptr;
            if(!candidate.mNodeName.empty())
            {
                if(sceneManager->hasSceneNode(candidate.mNodeName))
                    node = sceneManager->getSceneNode(candidate.mNodeName);
            }
            else
            {
                RenderedMovableEntity* entity = mGameMap->getRenderedMovableEntity(candidate.mTarget);
                if(entity != nullptr)
                    node = entity->getEntityNode();
            }

            if(node == nullptr)
                continue;

            MotionNode motionNode;
            motionNode.mNodeName = node->getName();
            motionNode.mBaseOrientation = node->getOrientation();
            motionNode.mBasePosition = node->getPosition();
            motionNode.mBaseScale = node->getScale();
            it = mMotionNodes.insert(std::make_pair(candidate.mTarget, motionNode)).first;
        }

        MotionNode& motionNode = it->second;
        motionNode.mSeen = true;
        MotionInstance* found = nullptr;
        for(MotionInstance& instance : motionNode.mMotions)
        {
            if(instance.mEffect == candidate.mEffect)
            {
                found = &instance;
                break;
            }
        }

        if(found == nullptr)
        {
            if(!candidate.mActive)
                continue;

            MotionInstance instance;
            instance.mEffect = candidate.mEffect;
            instance.mPhase = hashPhase(candidate.mTarget + effects[candidate.mEffect].mName);
            motionNode.mMotions.push_back(instance);
            switch(effects[candidate.mEffect].mMotion)
            {
                case AmbienceMotion::bob:
                    motionNode.mMovesPosition = true;
                    break;
                case AmbienceMotion::pulse:
                case AmbienceMotion::flicker:
                    motionNode.mMovesScale = true;
                    break;
                default:
                    motionNode.mMovesOrientation = true;
                    break;
            }
            found = &motionNode.mMotions.back();
        }

        found->mWanted = candidate.mActive;
    }
}

void RoomAmbience::playClips()
{
    const std::vector<AmbienceEffect>& effects = mConfig.getEffects();
    uint32_t nbPlayed = 0;
    for(const Candidate& candidate : mClipCandidates)
    {
        const AmbienceEffect& effect = effects[candidate.mEffect];
        bool isSound = (effect.mKind == AmbienceKind::sound);
        if(isSound ? effect.mFamily.empty() : effect.mClips.empty())
            continue;

        if(effect.mLoop && !isSound)
        {
            // A clip that runs as long as the condition holds: started once, not by a timer. An object without the clip
            // is left alone
            RenderedMovableEntity* loopEntity = mGameMap->getRenderedMovableEntity(candidate.mTarget);
            if((loopEntity == nullptr) || loopEntity->isMoving())
                continue;

            std::map<std::string, double>::const_iterator holdIt = mClipHoldUntil.find(candidate.mTarget);
            if((holdIt != mClipHoldUntil.end()) && (holdIt->second > mClock))
                continue;

            const std::string& loopClip = effect.mClips[0];
            Ogre::AnimationState* loopState = loopEntity->getAnimationState();
            if((loopState != nullptr) && (loopState->getAnimationName() == loopClip) && loopState->getLoop())
                continue;

            if(hasClip(loopEntity, loopClip))
                loopEntity->setAnimationState(loopClip, true, Ogre::Vector3::ZERO, false);
            continue;
        }

        // A sound has a timer of its own, so an object can have a clip and sounds
        std::string timerKey = isSound ? (candidate.mTarget + "|" + effect.mName) : candidate.mTarget;
        std::map<std::string, double>::iterator timerIt = mClipTimers.find(timerKey);
        if(timerIt == mClipTimers.end())
        {
            // The first clip comes after a random part of the interval, so not all objects move together
            std::uniform_real_distribution<double> first(0.0, effect.mEvery);
            mClipTimers[timerKey] = mClock + first(mRandom);
            continue;
        }

        if((mClock < timerIt->second) || (nbPlayed >= MAX_CLIPS_PER_SCAN))
            continue;

        std::uniform_real_distribution<double> next(CLIP_INTERVAL_MIN_FACTOR, CLIP_INTERVAL_MAX_FACTOR);
        if(isSound)
        {
            playSound(effect.mFamily, candidate.mPosition);
            timerIt->second = mClock + effect.mEvery * next(mRandom);
            ++nbPlayed;
            continue;
        }

        RenderedMovableEntity* entity = mGameMap->getRenderedMovableEntity(candidate.mTarget);
        if((entity == nullptr) || entity->isMoving())
            continue;

        std::uniform_int_distribution<size_t> pick(0, effect.mClips.size() - 1);
        entity->setAnimationState(effect.mClips[pick(mRandom)], false, Ogre::Vector3::ZERO, true);
        timerIt->second = mClock + effect.mEvery * next(mRandom);
        ++nbPlayed;
    }
}

void RoomAmbience::stopLoopClip(RenderedMovableEntity* entity, const AmbienceEffect& effect)
{
    if(effect.mClips.empty())
        return;

    Ogre::AnimationState* state = entity->getAnimationState();
    if((state == nullptr) || !state->getLoop() || (state->getAnimationName() != effect.mClips[0]))
        return;

    // Played once more but already at its end: the object stays in the pose the clip ends in
    entity->setAnimationState(effect.mClips[0], false, Ogre::Vector3::ZERO, false);
    state = entity->getAnimationState();
    if(state != nullptr)
        state->setTimePosition(state->getLength());
}

bool RoomAmbience::playEventClip(const AmbienceEffect& effect, const Ogre::Vector3& position)
{
    if(effect.mClips.empty() || effect.mObjects.empty() || (mGameMap == nullptr))
        return false;

    RenderedMovableEntity* nearest = nullptr;
    double nearestSquared = effect.mAmount * effect.mAmount;
    const std::vector<RenderedMovableEntity*>& entities = mGameMap->getRenderedMovableEntities();
    for(RenderedMovableEntity* entity : entities)
    {
        if((entity->getEntityNode() == nullptr) || entity->isMoving())
            continue;

        bool matches = false;
        for(const std::string& pattern : effect.mObjects)
        {
            if(matchesPattern(pattern, entity->getMeshName()))
            {
                matches = true;
                break;
            }
        }
        if(!matches)
            continue;

        double dx = entity->getPosition().x - position.x;
        double dy = entity->getPosition().y - position.y;
        double distanceSquared = dx * dx + dy * dy;
        if(distanceSquared <= nearestSquared)
        {
            nearestSquared = distanceSquared;
            nearest = entity;
        }
    }

    if(nearest == nullptr)
        return false;

    for(const std::string& clip : effect.mClips)
    {
        if(!hasClip(nearest, clip))
            continue;

        nearest->setAnimationState(clip, false, Ogre::Vector3::ZERO, false);
        // The looping clip of the object waits until this one is over
        Ogre::AnimationState* state = nearest->getAnimationState();
        double length = (state != nullptr) ? static_cast<double>(state->getLength()) : DEFAULT_CLIP_SECONDS;
        mClipHoldUntil[nearest->getName()] = mClock + length + CLIP_HOLD_MARGIN_SECONDS;
        return true;
    }

    return false;
}

bool RoomAmbience::playCreatureClip(const AmbienceEffect& effect, const std::string& creatureName)
{
    if(effect.mClips.empty() || creatureName.empty() || (mGameMap == nullptr))
        return false;

    Creature* creature = mGameMap->getCreature(creatureName);
    if((creature == nullptr) || !creature->getIsOnMap() || creature->isMoving())
        return false;

    for(const std::string& clip : effect.mClips)
    {
        if(!hasClip(creature, clip))
            continue;

        // Once, then the creature goes on with its idle clip
        creature->setAnimationState(clip, false, Ogre::Vector3::ZERO, true);
        return true;
    }

    return false;
}

void RoomAmbience::playSound(const std::string& family, const Ogre::Vector3& position)
{
    if(SoundEffectsManager::getSingletonPtr() == nullptr)
        return;

    SoundEffectsManager::getSingleton().playSpatialSound(family, position.x, position.y);
}

void RoomAmbience::updatePendingSounds()
{
    for(std::vector<PendingSound>::iterator it = mPendingSounds.begin(); it != mPendingSounds.end();)
    {
        if(mClock < it->mDue)
        {
            ++it;
            continue;
        }

        playSound(it->mFamily, it->mPosition);
        it = mPendingSounds.erase(it);
    }
}

void RoomAmbience::updateEmitters(double timeSinceLastFrame)
{
    const std::vector<AmbienceEffect>& effects = mConfig.getEffects();
    for(std::map<std::string, Emitter>::iterator it = mEmitters.begin(); it != mEmitters.end();)
    {
        Emitter& emitter = it->second;
        if(emitter.mFade >= 0.0)
        {
            emitter.mFade += timeSinceLastFrame;
            if(emitter.mFade >= EMITTER_FADE_SECONDS)
            {
                destroyEmitter(emitter);
                mEmitters.erase(it++);
                continue;
            }
        }
        else if(emitter.mEntity != nullptr)
        {
            // Amount 0 is a decoration that stands still
            if(effects[emitter.mEffect].mAmount > 0.0)
                moveModel(emitter, effects[emitter.mEffect], timeSinceLastFrame);
        }
        else if(effects[emitter.mEffect].mFlicker > 0.0)
        {
            double flicker = effects[emitter.mEffect].mFlicker;
            double cycles = effects[emitter.mEffect].mSpeed * mClock;
            if(effects[emitter.mEffect].mHeartRate)
            {
                // The beat is summed up, so a change of the heart rate does not make the glow jump
                emitter.mCycle += effects[emitter.mEffect].mSpeed
                    * getHeartRateFactor(emitter.mNode->_getDerivedPosition()) * timeSinceLastFrame;
                cycles = emitter.mCycle;
            }
            double rate = TWO_PI * cycles;
            double noise = 0.5 * (std::sin(rate + emitter.mPhase) + std::sin(FLICKER_SECOND_WAVE_RATE * rate + 2.0 * emitter.mPhase));
            double factor = 1.0 + flicker * noise;
            emitter.mSystem->setDefaultDimensions(static_cast<Ogre::Real>(emitter.mBaseWidth * factor),
                static_cast<Ogre::Real>(emitter.mBaseHeight * factor));
        }

        ++it;
    }
}

void RoomAmbience::updateOneShots(std::vector<OneShot>& oneShots, double timeSinceLastFrame)
{
    for(std::vector<OneShot>::iterator it = oneShots.begin(); it != oneShots.end();)
    {
        it->mLife -= timeSinceLastFrame;
        if(it->mLife > 0.0)
        {
            ++it;
            continue;
        }

        Emitter emitter;
        emitter.mNode = it->mNode;
        emitter.mSystem = it->mSystem;
        destroyEmitter(emitter);
        it = oneShots.erase(it);
    }
}

void RoomAmbience::updateMotions(double timeSinceLastFrame)
{
    const std::vector<AmbienceEffect>& effects = mConfig.getEffects();
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    for(std::map<std::string, MotionNode>::iterator it = mMotionNodes.begin(); it != mMotionNodes.end();)
    {
        MotionNode& motionNode = it->second;
        if(!sceneManager->hasSceneNode(motionNode.mNodeName))
        {
            mMotionNodes.erase(it++);
            continue;
        }

        motionNode.mClock += timeSinceLastFrame;
        Ogre::Quaternion orientation = motionNode.mBaseOrientation;
        Ogre::Vector3 position = motionNode.mBasePosition;
        Ogre::Vector3 scale = motionNode.mBaseScale;
        for(std::vector<MotionInstance>::iterator mit = motionNode.mMotions.begin(); mit != motionNode.mMotions.end();)
        {
            MotionInstance& instance = *mit;
            const AmbienceEffect& effect = effects[instance.mEffect];
            double step = MOTION_EASE_SPEED * timeSinceLastFrame;
            if(instance.mWanted)
                instance.mIntensity = std::min(1.0, instance.mIntensity + step);
            else
                instance.mIntensity = std::max(0.0, instance.mIntensity - step);

            if((instance.mIntensity <= 0.0) && !instance.mWanted)
            {
                mit = motionNode.mMotions.erase(mit);
                continue;
            }

            double wave = std::sin(TWO_PI * effect.mSpeed * motionNode.mClock + instance.mPhase);
            if(effect.mHeartRate)
            {
                // The beat is summed up, so a change of the heart rate does not make the object jump
                instance.mCycle += effect.mSpeed * getHeartRateFactor(motionNode.mBasePosition) * timeSinceLastFrame;
                wave = std::sin(TWO_PI * instance.mCycle + instance.mPhase);
            }
            switch(effect.mMotion)
            {
                case AmbienceMotion::sway:
                {
                    double angle = effect.mAmount * wave * instance.mIntensity;
                    orientation = orientation * Ogre::Quaternion(Ogre::Degree(static_cast<Ogre::Real>(angle)), effect.mAxis);
                    break;
                }
                case AmbienceMotion::wobble:
                {
                    // Swells and fades about every nine seconds, like something that was hit now and then
                    double envelope = std::sin(TWO_PI * WOBBLE_SWELL_RATE * motionNode.mClock + instance.mPhase);
                    envelope = (envelope > 0.0) ? envelope * envelope : 0.0;
                    double angle = effect.mAmount * wave * envelope * instance.mIntensity;
                    orientation = orientation * Ogre::Quaternion(Ogre::Degree(static_cast<Ogre::Real>(angle)), effect.mAxis);
                    break;
                }
                case AmbienceMotion::spin:
                {
                    // The turned angle is summed up, so a fading spin stops where it is instead of jumping back
                    instance.mPhase += effect.mSpeed * instance.mIntensity * timeSinceLastFrame * DEGREES_TO_RADIANS;
                    orientation = orientation * Ogre::Quaternion(Ogre::Radian(static_cast<Ogre::Real>(instance.mPhase)), effect.mAxis);
                    break;
                }
                case AmbienceMotion::bob:
                {
                    position.z += static_cast<Ogre::Real>(effect.mAmount * wave * instance.mIntensity);
                    break;
                }
                case AmbienceMotion::pulse:
                {
                    scale = scale * static_cast<Ogre::Real>(1.0 + effect.mAmount * wave * instance.mIntensity);
                    break;
                }
                case AmbienceMotion::flicker:
                {
                    double noise = 0.5 * (std::sin(TWO_PI * effect.mSpeed * motionNode.mClock + instance.mPhase)
                        + std::sin(TWO_PI * effect.mSpeed * FLICKER_SECOND_WAVE_RATE * motionNode.mClock + 2.0 * instance.mPhase));
                    scale = scale * static_cast<Ogre::Real>(1.0 + effect.mAmount * noise * instance.mIntensity);
                    break;
                }
            }

            ++mit;
        }

        if(motionNode.mMotions.empty())
        {
            restoreMotionNode(motionNode);
            mMotionNodes.erase(it++);
            continue;
        }

        // Only what the motions drive is written, so an object that is moved by something else keeps its place
        Ogre::SceneNode* node = sceneManager->getSceneNode(motionNode.mNodeName);
        if(motionNode.mMovesOrientation)
            node->setOrientation(orientation);
        if(motionNode.mMovesPosition)
            node->setPosition(position);
        if(motionNode.mMovesScale)
            node->setScale(scale);
        ++it;
    }
}

uint32_t RoomAmbience::triggerEvent(const std::string& eventName, const Ogre::Vector3& position, bool forced,
        const std::string& visualName, bool noThrottle, const Seat* owner, const std::string& creatureName)
{
    if((mMode == Mode::off) || (RenderManager::getSingletonPtr() == nullptr))
        return 0;

    std::map<std::string, std::vector<uint32_t> >::const_iterator listIt = mEventEffects.find(eventName);
    if(listIt == mEventEffects.end())
        return 0;

    if(!forced)
    {
        std::map<std::string, double>::iterator lastIt = mLastEventTime.find(eventName);
        if(!noThrottle && (lastIt != mLastEventTime.end()) && ((mClock - lastIt->second) < EVENT_REPEAT_SECONDS))
            return 0;
    }

    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
    Ogre::Camera* camera = (frameListener != nullptr) ? frameListener->getCameraManager()->getActiveCamera() : nullptr;
    Ogre::Vector3 cameraPosition = (camera != nullptr) ? camera->getDerivedPosition() : Ogre::Vector3::ZERO;
    // The point on the floor the camera looks at, from which the strength of a shake is measured
    Ogre::Vector3 lookPoint = cameraPosition;
    if(camera != nullptr)
        lookPoint = getLookPoint(cameraPosition, camera->getDerivedDirection());

    std::string visual = visualName;
    int32_t tileX = static_cast<int32_t>(std::floor(position.x + 0.5f));
    int32_t tileY = static_cast<int32_t>(std::floor(position.y + 0.5f));
    if(visual.empty() && (mGameMap != nullptr))
    {
        Tile* tile = mGameMap->getTile(tileX, tileY);
        if(tile != nullptr)
            visual = Tile::tileVisualToString(tile->getTileVisual());
    }

    const std::vector<AmbienceEffect>& effects = mConfig.getEffects();
    uint32_t nbStarted = 0;
    for(uint32_t index : listIt->second)
    {
        const AmbienceEffect& effect = effects[index];
        if(!effect.mMatch.empty() && (std::find(effect.mMatch.begin(), effect.mMatch.end(), visual) == effect.mMatch.end()))
            continue;

        // Only the keeper the thing belongs to sees or hears it (the sound of a secret door must not give it away);
        // without a known owner nobody does, except for the debug command (forced)
        if(effect.mOwnerOnly && !forced)
        {
            Player* localPlayer = (mGameMap != nullptr) ? mGameMap->getLocalPlayer() : nullptr;
            if((owner == nullptr) || (localPlayer == nullptr) || (localPlayer->getSeat() != owner))
                continue;
        }

        bool isShake = (effect.mKind == AmbienceKind::shake);
        bool isSound = (effect.mKind == AmbienceKind::sound);
        bool isFlight = (effect.mKind == AmbienceKind::beam) || (effect.mKind == AmbienceKind::projectile);
        bool isMark = (effect.mKind == AmbienceKind::mark);
        bool isClip = (effect.mKind == AmbienceKind::clip) || (effect.mKind == AmbienceKind::creatureClip);
        if(!forced)
        {
            if(!isEffectUsable(effect) || (camera == nullptr))
                continue;

            // A shake is felt wherever the event is, as far as its own MaxDistance reaches; a mark must be there when
            // the view comes by later
            if(!isShake && !isMark && !isVisibleNear(camera, cameraPosition, position, OBJECT_VISIBLE_RADIUS, getDistanceLimit(effect)))
                continue;

            if(effect.mSpacing > 1)
            {
                uint32_t hash = static_cast<uint32_t>(tileX * TILE_HASH_X) ^ static_cast<uint32_t>(tileY * TILE_HASH_Y);
                if(((hash >> TILE_HASH_SHIFT) % effect.mSpacing) != 0)
                    continue;
            }

            if(effect.mChance < 1.0)
            {
                std::uniform_real_distribution<double> dice(0.0, 1.0);
                if(dice(mRandom) > effect.mChance)
                    continue;
            }

            if(!isShake && !isSound && !isFlight && !isMark && !isClip && (mOneShots.size() >= mConfig.getMaxOneShots()))
                continue;
        }

        if(isSound)
        {
            if(effect.mFamily.empty())
                continue;

            if(effect.mDelay > 0.0)
            {
                // The sound of a trap that is loaded again, a door that falls shut
                PendingSound pending;
                pending.mFamily = effect.mFamily;
                pending.mPosition = position;
                pending.mDue = mClock + effect.mDelay;
                if(mPendingSounds.size() < MAX_PENDING_SOUNDS)
                    mPendingSounds.push_back(pending);
            }
            else
            {
                playSound(effect.mFamily, position);
            }
            ++nbStarted;
            continue;
        }

        if(isClip)
        {
            // Clips of the skeletons: the nearest matching object or the creature of the event, nothing if there is none
            bool played = (effect.mKind == AmbienceKind::clip) ? playEventClip(effect, position) :
                playCreatureClip(effect, creatureName);
            if(played)
                ++nbStarted;
            continue;
        }

        if(isFlight)
        {
            uint32_t nbBefore = static_cast<uint32_t>(mFlights.size());
            startFlight(effect, position);
            if(mFlights.size() > nbBefore)
                ++nbStarted;
            continue;
        }

        if(isShake)
        {
            if(mMode != Mode::off)
            {
                startShake(effect, position, lookPoint);
                ++nbStarted;
            }
            continue;
        }

        // A particle effect may come with a sound of its own (the gong of the temple)
        if(!effect.mSound.empty() && (mMode == Mode::full))
            playSound(effect.mSound, position);

        OneShot oneShot;
        if(!createParticleSystem(effect.mSystem, position + effect.mOffset, effect.mName, oneShot.mNode, oneShot.mSystem))
            continue;

        oneShot.mLife = effect.mDuration;
        if(isMark)
        {
            // The oldest mark makes room for the new one
            while(mMarks.size() >= std::max<uint32_t>(MIN_MARK_LIMIT, mConfig.getMaxMarks()))
            {
                Emitter oldest;
                oldest.mNode = mMarks.front().mNode;
                oldest.mSystem = mMarks.front().mSystem;
                destroyEmitter(oldest);
                mMarks.erase(mMarks.begin());
            }
            mMarks.push_back(oneShot);
        }
        else
        {
            mOneShots.push_back(oneShot);
        }
        ++nbStarted;
    }

    if(nbStarted > 0)
        mLastEventTime[eventName] = mClock;

    return nbStarted;
}

void RoomAmbience::startCollapse(int32_t tileX, int32_t tileY, const std::string& typeName)
{
    if(mGameMap == nullptr)
        return;

    // Only the remains of the door the server is about to remove are shown, so a few at most
    if(mCollapses.size() >= MAX_COLLAPSES)
        return;

    // The barricade falls into a heap, every other door breaks with the clip Destroyed
    const std::string& clipName = (typeName == BARRICADE_TYPE) ? COLLAPSE_CLIP : DESTROYED_CLIP;
    const std::string entityPrefix = typeName + "_";

    const std::vector<RenderedMovableEntity*>& entities = mGameMap->getRenderedMovableEntities();
    for(RenderedMovableEntity* entity : entities)
    {
        if((entity->getObjectType() != GameEntityType::trapEntity) || (entity->getEntityNode() == nullptr))
            continue;

        if(entity->getName().compare(0, entityPrefix.length(), entityPrefix) != 0)
            continue;

        const Ogre::Vector3& position = entity->getPosition();
        if((static_cast<int32_t>(std::floor(position.x + 0.5f)) != tileX) ||
           (static_cast<int32_t>(std::floor(position.y + 0.5f)) != tileY))
        {
            continue;
        }

        if(entity->getMeshName().empty())
            return;

        Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
        std::string name = "RoomAmbience_Collapse_" + Helper::toString(++mUniqueNumber);
        Ogre::Entity* ghost = nullptr;
        try
        {
            ghost = sceneManager->createEntity(name, entity->getMeshName() + MESH_SUFFIX);
        }
        catch(const Ogre::Exception&)
        {
            return;
        }

        // Without the clip (an old skeleton) there is nothing to show
        if(!ghost->hasSkeleton() || !ghost->getAllAnimationStates()->hasAnimationState(clipName))
        {
            sceneManager->destroyEntity(ghost);
            return;
        }

        Collapse collapse;
        collapse.mEntity = ghost;
        collapse.mClip = clipName;
        collapse.mNode = sceneManager->getRootSceneNode()->createChildSceneNode(name + "_node",
            entity->getEntityNode()->_getDerivedPosition(), entity->getEntityNode()->_getDerivedOrientation());
        collapse.mNode->setScale(entity->getEntityNode()->_getDerivedScale());
        collapse.mNode->attachObject(ghost);
        collapse.mBaseHeight = collapse.mNode->getPosition().z;
        Ogre::AnimationState* state = ghost->getAnimationState(clipName);
        state->setLoop(false);
        state->setTimePosition(0.0f);
        state->setEnabled(true);
        mCollapses.push_back(collapse);
        return;
    }
}

void RoomAmbience::updateCollapses(double timeSinceLastFrame)
{
    for(std::vector<Collapse>::iterator it = mCollapses.begin(); it != mCollapses.end();)
    {
        // The length of the clip of the door skeleton
        const double clipLength = static_cast<double>(it->mEntity->getAnimationState(it->mClip)->getLength());
        it->mAge += timeSinceLastFrame;
        if(it->mAge >= clipLength + COLLAPSE_HOLD_SECONDS + COLLAPSE_SINK_SECONDS)
        {
            destroyCollapse(*it);
            it = mCollapses.erase(it);
            continue;
        }

        if(it->mAge < clipLength)
        {
            it->mEntity->getAnimationState(it->mClip)->addTime(static_cast<Ogre::Real>(timeSinceLastFrame));
        }
        else if(it->mAge > clipLength + COLLAPSE_HOLD_SECONDS)
        {
            double sunk = (it->mAge - clipLength - COLLAPSE_HOLD_SECONDS) / COLLAPSE_SINK_SECONDS;
            Ogre::Vector3 position = it->mNode->getPosition();
            position.z = static_cast<Ogre::Real>(it->mBaseHeight - sunk * COLLAPSE_SINK_DEPTH);
            it->mNode->setPosition(position);
        }

        ++it;
    }
}

void RoomAmbience::destroyCollapse(Collapse& collapse)
{
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    if(collapse.mNode != nullptr)
        collapse.mNode->detachAllObjects();
    if(collapse.mEntity != nullptr)
        sceneManager->destroyEntity(collapse.mEntity);
    if(collapse.mNode != nullptr)
        sceneManager->destroySceneNode(collapse.mNode);

    collapse.mNode = nullptr;
    collapse.mEntity = nullptr;
}

void RoomAmbience::restoreTurret(const Turret& turret)
{
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    if(!sceneManager->hasSceneNode(turret.mNodeName))
        return;

    turnNodeToward(sceneManager->getSceneNode(turret.mNodeName), turret.mRestFront, TWO_PI);
}

void RoomAmbience::updateTurrets(double timeSinceLastFrame)
{
    if(mTurrets.empty())
        return;

    const std::vector<AmbienceEffect>& effects = mConfig.getEffects();
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    for(std::map<std::string, Turret>::iterator it = mTurrets.begin(); it != mTurrets.end();)
    {
        Turret& turret = it->second;
        if(!sceneManager->hasSceneNode(turret.mNodeName))
        {
            mTurrets.erase(it++);
            continue;
        }

        // Out of view, out of reach of the effect or switched off: back to rest, where nobody sees it
        if((turret.mGeneration != mGeneration) || (turret.mEffect >= effects.size()))
        {
            restoreTurret(turret);
            mTurrets.erase(it++);
            continue;
        }

        const AmbienceEffect& effect = effects[turret.mEffect];
        std::map<std::string, double>::const_iterator holdIt = mTurretHoldUntil.find(getPositionKey(turret.mPosition));
        if((holdIt != mTurretHoldUntil.end()) && (holdIt->second > mClock))
        {
            ++it;
            continue;
        }

        // Enemies first, otherwise the nearest creature of anyone, within the range of the effect
        double rangeSquared = effect.mAmount * effect.mAmount;
        double bestEnemy = rangeSquared;
        double bestOther = rangeSquared;
        const CreatureSpot* enemy = nullptr;
        const CreatureSpot* other = nullptr;
        for(const CreatureSpot& spot : mCreatureSpots)
        {
            double dx = static_cast<double>(spot.mPosition.x - turret.mPosition.x);
            double dy = static_cast<double>(spot.mPosition.y - turret.mPosition.y);
            double distanceSquared = dx * dx + dy * dy;
            bool isEnemy = (turret.mSeat != nullptr) && (spot.mSeat != nullptr) && !turret.mSeat->isAlliedSeat(spot.mSeat);
            if(isEnemy)
            {
                if(distanceSquared < bestEnemy)
                {
                    bestEnemy = distanceSquared;
                    enemy = &spot;
                }
            }
            else if(distanceSquared < bestOther)
            {
                bestOther = distanceSquared;
                other = &spot;
            }
        }

        const CreatureSpot* chosen = (enemy != nullptr) ? enemy : other;
        Ogre::Vector3 wanted = turret.mRestFront;
        if(chosen != nullptr)
        {
            Ogre::Vector3 toCreature(chosen->mPosition.x - turret.mPosition.x, chosen->mPosition.y - turret.mPosition.y, 0.0f);
            if(toCreature.squaredLength() > MIN_AIM_LENGTH_SQUARED)
                wanted = toCreature;
        }

        turnNodeToward(sceneManager->getSceneNode(turret.mNodeName), wanted,
            effect.mSpeed * timeSinceLastFrame * DEGREES_TO_RADIANS);
        ++it;
    }
}

void RoomAmbience::startFlight(const AmbienceEffect& effect, const Ogre::Vector3& position)
{
    if(mFlights.size() >= mConfig.getMaxFlights())
        return;

    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    Flight flight;
    flight.mBeam = (effect.mKind == AmbienceKind::beam);
    flight.mStart = position + effect.mFrom;
    flight.mEnd = position + effect.mOffset;
    flight.mDuration = std::max(MIN_FLIGHT_SECONDS, effect.mDuration);
    flight.mWidth = effect.mAmount;
    flight.mArc = flight.mBeam ? 0.0 : effect.mAmount;
    flight.mFlickerRate = std::max(MIN_BEAM_FLICKER_RATE, effect.mSpeed);
    flight.mLand = effect.mLand;

    std::string name = "RoomAmbience_" + effect.mName + "_" + Helper::toString(++mUniqueNumber);
    flight.mNode = sceneManager->getRootSceneNode()->createChildSceneNode(name + "_node", flight.mStart);
    if(!effect.mMesh.empty())
    {
        try
        {
            flight.mEntity = sceneManager->createEntity(name, effect.mMesh + MESH_SUFFIX);
        }
        catch(const Ogre::Exception&)
        {
            if(mMissingSystems.insert(effect.mMesh).second)
                OD_LOG_WRN("Room ambience: unknown mesh " + effect.mMesh);
            sceneManager->destroySceneNode(flight.mNode);
            return;
        }
        flight.mEntity->setCastShadows(false);
        flight.mNode->attachObject(flight.mEntity);
    }

    if(!effect.mSystem.empty())
        createParticleSystem(effect.mSystem, flight.mStart, effect.mName + "Trail", flight.mTrailNode, flight.mTrailSystem);

    if((flight.mEntity == nullptr) && (flight.mTrailSystem == nullptr))
    {
        sceneManager->destroySceneNode(flight.mNode);
        return;
    }

    mFlights.push_back(flight);
}

void RoomAmbience::updateFlights(double timeSinceLastFrame)
{
    std::vector<std::pair<std::string, Ogre::Vector3> > arrivals;
    for(std::vector<Flight>::iterator it = mFlights.begin(); it != mFlights.end();)
    {
        Flight& flight = *it;
        flight.mAge += timeSinceLastFrame;
        if(flight.mAge >= flight.mDuration)
        {
            if(!flight.mLand.empty())
                arrivals.push_back(std::make_pair(flight.mLand, flight.mEnd));
            destroyFlight(flight);
            it = mFlights.erase(it);
            continue;
        }

        double progress = flight.mAge / flight.mDuration;
        Ogre::Vector3 position = flight.mStart + (flight.mEnd - flight.mStart) * static_cast<Ogre::Real>(progress);
        Ogre::Vector3 direction = flight.mEnd - flight.mStart;
        if(flight.mBeam)
        {
            // The bolt always spans from its start to the target. The mesh runs along its y axis with length 1; it is
            // turned around that axis and made thinner or thicker a few times a second, which makes it flicker
            double length = std::max(MIN_BEAM_LENGTH, static_cast<double>(direction.length()));
            direction.normalise();
            flight.mFlickerClock += timeSinceLastFrame;
            if((flight.mFlickerClock * flight.mFlickerRate >= 1.0) || (flight.mAge == timeSinceLastFrame))
            {
                flight.mFlickerClock = 0.0;
                std::uniform_real_distribution<double> dice(0.0, 1.0);
                double roll = dice(mRandom) * TWO_PI;
                double thickness = (BEAM_THICKNESS_MIN + BEAM_THICKNESS_RANGE * dice(mRandom)) * flight.mWidth *
                    std::sqrt(1.0 - progress);
                Ogre::Quaternion turn = Ogre::Quaternion(Ogre::Radian(static_cast<Ogre::Real>(roll)), Ogre::Vector3::UNIT_Y);
                flight.mNode->setOrientation(Ogre::Vector3::UNIT_Y.getRotationTo(direction) * turn);
                flight.mNode->setScale(static_cast<Ogre::Real>(thickness), static_cast<Ogre::Real>(length),
                    static_cast<Ogre::Real>(thickness));
            }
            flight.mNode->setPosition(flight.mStart);
        }
        else
        {
            position.z += static_cast<Ogre::Real>(flight.mArc * ARC_PARABOLA_FACTOR * progress * (1.0 - progress));
            Ogre::Vector3 previous = flight.mNode->getPosition();
            flight.mNode->setPosition(position);
            Ogre::Vector3 moved = position - previous;
            if((flight.mEntity != nullptr) && (moved.squaredLength() > MIN_PROJECTILE_MOVE_SQUARED))
            {
                moved.normalise();
                flight.mNode->setOrientation(Ogre::Vector3::UNIT_Y.getRotationTo(moved));
            }
            if(flight.mTrailNode != nullptr)
                flight.mTrailNode->setPosition(position);
        }

        ++it;
    }

    for(std::pair<std::string, Ogre::Vector3>& arrival : arrivals)
        triggerEvent(arrival.first, arrival.second, false, std::string(), true);
}

void RoomAmbience::destroyFlight(Flight& flight)
{
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    if(flight.mNode != nullptr)
        flight.mNode->detachAllObjects();
    if(flight.mEntity != nullptr)
        sceneManager->destroyEntity(flight.mEntity);
    if(flight.mNode != nullptr)
        sceneManager->destroySceneNode(flight.mNode);

    Emitter trail;
    trail.mNode = flight.mTrailNode;
    trail.mSystem = flight.mTrailSystem;
    destroyEmitter(trail);

    flight.mNode = nullptr;
    flight.mEntity = nullptr;
    flight.mTrailNode = nullptr;
    flight.mTrailSystem = nullptr;
}

void RoomAmbience::noteHandCast(const Ogre::Vector3& handPosition)
{
    triggerEvent("SpellFxHandCast", handPosition, false, std::string(), true);
}

void RoomAmbience::startShake(const AmbienceEffect& effect, const Ogre::Vector3& position,
        const Ogre::Vector3& lookPoint)
{
    double distance = std::hypot(static_cast<double>(position.x - lookPoint.x),
        static_cast<double>(position.y - lookPoint.y));
    if((effect.mMaxDistance <= 0.0) || (distance >= effect.mMaxDistance))
        return;

    double strength = effect.mAmount * (1.0 - distance / effect.mMaxDistance);
    // A shake that is still running is not made weaker by a new one
    double running = (mShakeTotal > 0.0) ? (mShakeAmount * mShakeTime / mShakeTotal) : 0.0;
    if((mShakeTime > 0.0) && (running > strength))
        return;

    mShakeAmount = strength;
    mShakeTotal = std::max(MIN_SHAKE_SECONDS, effect.mDuration);
    mShakeTime = mShakeTotal;
    mShakeSpeed = std::max(MIN_SHAKE_SPEED, effect.mSpeed);
}

void RoomAmbience::updateShake(double timeSinceLastFrame)
{
    if(mShakeTime <= 0.0)
        return;

    mShakeTime -= timeSinceLastFrame;
    if(mShakeTime < 0.0)
        mShakeTime = 0.0;
    mShakePhase += timeSinceLastFrame * mShakeSpeed * TWO_PI;
}

void RoomAmbience::applyShake()
{
    if((mShakeTime <= 0.0) || (mMode == Mode::off) || (mGameMap == nullptr) || mGameMap->getGamePaused())
        return;

    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
    if(frameListener == nullptr)
        return;

    Ogre::SceneNode* cameraNode = frameListener->getCameraManager()->getActiveCameraNode();
    if((cameraNode == nullptr) || (cameraNode->numChildren() == 0))
        return;

    // The camera hangs on the first child of the camera node; moving it by a small offset moves the picture
    Ogre::Node* viewNode = cameraNode->getChild(0);
    double strength = mShakeAmount * (mShakeTime / mShakeTotal);
    Ogre::Vector3 offset(static_cast<Ogre::Real>(strength * std::sin(mShakePhase)),
        static_cast<Ogre::Real>(strength * std::sin(mShakePhase * SHAKE_SECOND_AXIS_SPEED_FACTOR + SHAKE_SECOND_AXIS_PHASE)),
        static_cast<Ogre::Real>(strength * SHAKE_THIRD_AXIS_STRENGTH *
            std::sin(mShakePhase * SHAKE_THIRD_AXIS_SPEED_FACTOR + SHAKE_THIRD_AXIS_PHASE)));
    viewNode->setPosition(viewNode->getPosition() + offset);
    mShakeApplied = offset;
}

void RoomAmbience::clearShake()
{
    if(mShakeApplied == Ogre::Vector3::ZERO)
        return;

    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
    Ogre::Vector3 applied = mShakeApplied;
    mShakeApplied = Ogre::Vector3::ZERO;
    if(frameListener == nullptr)
        return;

    Ogre::SceneNode* cameraNode = frameListener->getCameraManager()->getActiveCameraNode();
    if((cameraNode == nullptr) || (cameraNode->numChildren() == 0))
        return;

    Ogre::Node* viewNode = cameraNode->getChild(0);
    viewNode->setPosition(viewNode->getPosition() - applied);
}

void RoomAmbience::notifyTrapEffect(int32_t kind, int32_t tileX, int32_t tileY, const std::string& typeName, float fraction)
{
    if(mMode == Mode::off)
        return;

    Ogre::Vector3 position(static_cast<Ogre::Real>(tileX), static_cast<Ogre::Real>(tileY), 0.0f);
    switch(static_cast<TrapEffectKind>(kind))
    {
        case TrapEffectKind::fired:
            // The server has just aimed the trap at its target: the barrel is not turned away for a moment
            mTurretHoldUntil[getTileKey(tileX, tileY)] = mClock + TURRET_HOLD_SECONDS;
            triggerEvent("TrapFired", position, false, typeName);
            break;
        case TrapEffectKind::linked:
            triggerEvent("TrapLinked", position, false, typeName);
            break;
        case TrapEffectKind::doorHit:
            mHitUntil[getTileKey(tileX, tileY)] = mClock + HIT_STATE_SECONDS;
            triggerEvent("DoorHit", position, false, typeName);
            if(fraction <= DOOR_HURT_FRACTION)
                triggerEvent("DoorHurt", position, false, typeName);
            break;
        case TrapEffectKind::doorWrecked:
            mWreckedUntil[getTileKey(tileX, tileY)] = mClock + WRECKED_STATE_SECONDS;
            triggerEvent("DoorWrecked", position, false, typeName);
            startCollapse(tileX, tileY, typeName);
            break;
        case TrapEffectKind::reloading:
            // Reloading, or empty after the last shot: the state is kept until the server says ready
            mReloadingUntil[getTileKey(tileX, tileY)] = mClock + RELOAD_STATE_SECONDS;
            break;
        case TrapEffectKind::ready:
        {
            // The reload look fades out for a moment, so that a short reload is seen at all
            std::map<std::string, double>::iterator reloadIt = mReloadingUntil.find(getTileKey(tileX, tileY));
            if((reloadIt != mReloadingUntil.end()) && (reloadIt->second > (mClock + RELOAD_FADE_OUT_SECONDS)))
                reloadIt->second = mClock + RELOAD_FADE_OUT_SECONDS;
            break;
        }
        default:
            break;
    }
}
