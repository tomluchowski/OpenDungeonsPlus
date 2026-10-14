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
#include "gamemap/GameMap.h"
#include "render/ODFrameListener.h"
#include "render/RenderManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <OgreAxisAlignedBox.h>
#include <OgreCamera.h>
#include <OgreMath.h>
#include <OgreParticleSystem.h>
#include <OgreParticleSystemManager.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>

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
//! Start of a Match name that stands for a bridge (followed by the bridge mesh or the tile visual)
static const std::string BRIDGE_PREFIX = "bridge:";
//! Start of a Match name that stands for the type of a trap or door (Alarm, Gas, DoorSteel...)
static const std::string TRAP_PREFIX = "trap:";
//! The end of a mesh file name
static const std::string MESH_SUFFIX = ".mesh";
//! A full turn in radians
const double TWO_PI = 6.283185307179586;

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
    mRandom(RANDOM_SEED),
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
        it->second.mLastBusy = mClock;

    return (mClock - it->second.mLastBusy) >= after;
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

void RoomAmbience::destroyEmitter(Emitter& emitter)
{
    Ogre::SceneManager* sceneManager = RenderManager::getSingleton().getSceneManager();
    if(emitter.mNode != nullptr)
        emitter.mNode->detachAllObjects();
    if(emitter.mSystem != nullptr)
        sceneManager->destroyParticleSystem(emitter.mSystem);
    if(emitter.mNode != nullptr)
        sceneManager->destroySceneNode(emitter.mNode);

    emitter.mNode = nullptr;
    emitter.mSystem = nullptr;
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

        for(std::map<std::string, MotionNode>::iterator it = mMotionNodes.begin(); it != mMotionNodes.end(); ++it)
            restoreMotionNode(it->second);
    }

    mEmitters.clear();
    mOneShots.clear();
    mMotionNodes.clear();
    mParticleCandidates.clear();
    mMotionCandidates.clear();
    mClipCandidates.clear();
    mClipTimers.clear();
    mSeenVisual.clear();
    mSeenSizeX = 0;
    mSeenSizeY = 0;
    mKnownEntities.clear();
    mEntitiesInitialized = false;
    mScanTimer = 0.0;
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
    updateOneShots(dt);
    updateMotions(dt);
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
    Ogre::Vector3 direction = camera->getDerivedDirection();
    Ogre::Vector3 lookPoint = cameraPosition;
    if(direction.z < -MIN_LOOK_DOWN)
        lookPoint = cameraPosition + direction * (cameraPosition.z / -direction.z);

    ++mGeneration;
    mEventsThisScan = 0;
    mParticleCandidates.clear();
    mMotionCandidates.clear();
    mClipCandidates.clear();

    mCreaturePositions.clear();
    for(Creature* creature : mGameMap->getCreatures())
    {
        if(creature->getIsOnMap())
            mCreaturePositions.push_back(creature->getPosition());
    }

    scanObjects(camera, cameraPosition);
    scanTiles(camera, cameraPosition, lookPoint);
    scanEntityEvents(camera, cameraPosition);
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

            // An effect that already runs is kept a bit beyond its distance
            double limit = getDistanceLimit(effect) * VIEW_DISTANCE_MARGIN;
            if(distance > limit)
                continue;

            if(visible < 0)
                visible = isVisibleNear(camera, cameraPosition, position, OBJECT_VISIBLE_RADIUS, limit) ? 1 : 0;
            if(visible == 0)
                continue;

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
            else
            {
                if(busy < 0)
                    busy = isCreatureNear(position.x, position.y, mConfig.getOccupiedRadius()) ? 1 : 0;

                if(effect.mWhen == AmbienceWhen::occupied)
                    candidate.mActive = (busy == 1);
                else
                    candidate.mActive = isIdleLongEnough("o" + entity->getName(), busy == 1, effect.mAfter);
            }

            if(effect.mKind == AmbienceKind::particle)
            {
                if(candidate.mActive)
                    mParticleCandidates.push_back(candidate);
            }
            else if(effect.mKind == AmbienceKind::clip)
            {
                if(candidate.mActive)
                    mClipCandidates.push_back(candidate);
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

                if(effect.mNeedWall)
                {
                    if(wall < 0)
                    {
                        wall = 0;
                        for(Tile* neighbor : tile->getAllNeighbors())
                        {
                            if((neighbor != nullptr) && (neighbor->getFullness() > 0.0))
                            {
                                wall = 1;
                                break;
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
                        candidate.mActive = isIdleLongEnough(key, busy == 1, effect.mAfter);
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

                if(effect.mKind == AmbienceKind::particle)
                    mParticleCandidates.push_back(candidate);
            }
        }
    }
}

void RoomAmbience::scanEntityEvents(Ogre::Camera* camera, const Ogre::Vector3& cameraPosition)
{
    struct Change
    {
        std::string mEvent;
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

        mKnownEntities.erase(it++);
    }

    // Many changes at once are a map being loaded or revealed, not things being made
    bool settled = (mClock > SETTLE_SECONDS) && mEntitiesInitialized && (changes.size() <= MAX_EVENTS_PER_SCAN);
    mEntitiesInitialized = true;
    if(!settled)
        return;

    for(const Change& change : changes)
        triggerEvent(change.mEvent, change.mPosition, false);
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
                it->second.mSystem->setEmitting(true);
            }
            ++used;
            continue;
        }

        if(used >= budget)
            continue;

        Emitter emitter;
        if(!createParticleSystem(effect.mSystem, candidate.mPosition, effect.mName, emitter.mNode, emitter.mSystem))
            continue;

        emitter.mEffect = candidate.mEffect;
        emitter.mBaseWidth = emitter.mSystem->getDefaultWidth();
        emitter.mBaseHeight = emitter.mSystem->getDefaultHeight();
        emitter.mPhase = hashPhase(key);
        emitter.mSeen = true;
        mEmitters.insert(std::make_pair(key, emitter));
        ++used;
    }

    for(std::map<std::string, Emitter>::iterator it = mEmitters.begin(); it != mEmitters.end(); ++it)
    {
        if(it->second.mSeen || (it->second.mFade >= 0.0))
            continue;

        it->second.mFade = 0.0;
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
        if(effect.mClips.empty())
            continue;

        std::map<std::string, double>::iterator timerIt = mClipTimers.find(candidate.mTarget);
        if(timerIt == mClipTimers.end())
        {
            // The first clip comes after a random part of the interval, so not all objects move together
            std::uniform_real_distribution<double> first(0.0, effect.mEvery);
            mClipTimers[candidate.mTarget] = mClock + first(mRandom);
            continue;
        }

        if((mClock < timerIt->second) || (nbPlayed >= MAX_CLIPS_PER_SCAN))
            continue;

        RenderedMovableEntity* entity = mGameMap->getRenderedMovableEntity(candidate.mTarget);
        if((entity == nullptr) || entity->isMoving())
            continue;

        std::uniform_int_distribution<size_t> pick(0, effect.mClips.size() - 1);
        entity->setAnimationState(effect.mClips[pick(mRandom)], false, Ogre::Vector3::ZERO, true);
        std::uniform_real_distribution<double> next(CLIP_INTERVAL_MIN_FACTOR, CLIP_INTERVAL_MAX_FACTOR);
        timerIt->second = mClock + effect.mEvery * next(mRandom);
        ++nbPlayed;
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
        else if(effects[emitter.mEffect].mFlicker > 0.0)
        {
            double flicker = effects[emitter.mEffect].mFlicker;
            double rate = TWO_PI * effects[emitter.mEffect].mSpeed;
            double noise = 0.5 * (std::sin(rate * mClock + emitter.mPhase) + std::sin(FLICKER_SECOND_WAVE_RATE * rate * mClock + 2.0 * emitter.mPhase));
            double factor = 1.0 + flicker * noise;
            emitter.mSystem->setDefaultDimensions(static_cast<Ogre::Real>(emitter.mBaseWidth * factor),
                static_cast<Ogre::Real>(emitter.mBaseHeight * factor));
        }

        ++it;
    }
}

void RoomAmbience::updateOneShots(double timeSinceLastFrame)
{
    for(std::vector<OneShot>::iterator it = mOneShots.begin(); it != mOneShots.end();)
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
        it = mOneShots.erase(it);
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
        const std::string& visualName)
{
    if((mMode == Mode::off) || (RenderManager::getSingletonPtr() == nullptr))
        return 0;

    std::map<std::string, std::vector<uint32_t> >::const_iterator listIt = mEventEffects.find(eventName);
    if(listIt == mEventEffects.end())
        return 0;

    if(!forced)
    {
        std::map<std::string, double>::iterator lastIt = mLastEventTime.find(eventName);
        if((lastIt != mLastEventTime.end()) && ((mClock - lastIt->second) < EVENT_REPEAT_SECONDS))
            return 0;
    }

    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
    Ogre::Camera* camera = (frameListener != nullptr) ? frameListener->getCameraManager()->getActiveCamera() : nullptr;
    Ogre::Vector3 cameraPosition = (camera != nullptr) ? camera->getDerivedPosition() : Ogre::Vector3::ZERO;

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

        if(!forced)
        {
            if(!isEffectUsable(effect) || (camera == nullptr))
                continue;

            if(!isVisibleNear(camera, cameraPosition, position, OBJECT_VISIBLE_RADIUS, getDistanceLimit(effect)))
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

            if(mOneShots.size() >= mConfig.getMaxOneShots())
                continue;
        }

        OneShot oneShot;
        if(!createParticleSystem(effect.mSystem, position + effect.mOffset, effect.mName, oneShot.mNode, oneShot.mSystem))
            continue;

        oneShot.mLife = effect.mDuration;
        mOneShots.push_back(oneShot);
        ++nbStarted;
    }

    if(nbStarted > 0)
        mLastEventTime[eventName] = mClock;

    return nbStarted;
}
