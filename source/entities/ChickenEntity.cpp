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

#include "entities/ChickenEntity.h"

#include "entities/ChickenFlight.h"
#include "entities/ChickenPose.h"
#include "entities/Creature.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "network/ODPacket.h"
#include "network/ODServer.h"
#include "network/ServerNotification.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "render/RenderManager.h"
#include "gamemap/GameMap.h"
#include "gamemap/RoomObjectNavigation.h"
#include "rooms/Room.h"
#include "rooms/RoomHatchery.h"
#include "rooms/RoomType.h"
#include "ODApplication.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/Random.h"
#include "utils/LogManager.h"

#include <OgreAnimationState.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <iostream>
#include <map>

const int32_t NB_TURNS_OUTSIDE_HATCHERY_BEFORE_DIE = 30;
const int32_t NB_TURNS_DIE_BEFORE_REMOVE = 5;

//! \brief Creates an animal of the given hatchery in the given stage of life, with all counters at zero.
ChickenEntity::ChickenEntity(GameMap* gameMap, const std::string& hatcheryName, ChickenKind kind) :
    RenderedMovableEntity(gameMap, hatcheryName, getMeshNameForKind(kind), 0.0f, false),
    mChickenState(ChickenState::free),
    mKind(kind),
    mNbTurnLay(0),
    mAge(0),
    mBusyTurns(0),
    mLeavingCoop(false),
    mCoopDoor(Ogre::Vector2::ZERO),
    mCoopExit(Ogre::Vector2::ZERO),
    mScatterTurns(0),
    mRoomDriven(false),
    mFighting(false),
    mOnRoof(false),
    mHopFrom(Ogre::Vector3::ZERO),
    mHopTo(Ogre::Vector3::ZERO),
    mHopTurns(0),
    mHopTurnsLeft(0),
    mHopElapsed(0.0f),
    mFollowing(false),
    mFollowTarget(Ogre::Vector2::ZERO),
    mFollowGap(0.0),
    mMood(RoosterMood::strut),
    mMoodTurns(0),
    mSinceCrow(0),
    mApproachTurns(0),
    mHomeSeat(nullptr),
    mReturningHome(false),
    mReturnTurns(0),
    mReturnRetryTurns(0),
    mNbTurnOutsideHatchery(0),
    mNbTurnDie(0),
    mIsSlapped(false),
    mLockedEat(false)
{
}

ChickenEntity::ChickenEntity(GameMap* gameMap) :
    RenderedMovableEntity(gameMap),
    mChickenState(ChickenState::free),
    mKind(ChickenKind::hen),
    mNbTurnLay(0),
    mAge(0),
    mBusyTurns(0),
    mLeavingCoop(false),
    mCoopDoor(Ogre::Vector2::ZERO),
    mCoopExit(Ogre::Vector2::ZERO),
    mScatterTurns(0),
    mRoomDriven(false),
    mFighting(false),
    mOnRoof(false),
    mHopFrom(Ogre::Vector3::ZERO),
    mHopTo(Ogre::Vector3::ZERO),
    mHopTurns(0),
    mHopTurnsLeft(0),
    mHopElapsed(0.0f),
    mFollowing(false),
    mFollowTarget(Ogre::Vector2::ZERO),
    mFollowGap(0.0),
    mMood(RoosterMood::strut),
    mMoodTurns(0),
    mSinceCrow(0),
    mApproachTurns(0),
    mHomeSeat(nullptr),
    mReturningHome(false),
    mReturnTurns(0),
    mReturnRetryTurns(0),
    mNbTurnOutsideHatchery(0),
    mNbTurnDie(0),
    mIsSlapped(false),
    mLockedEat(false)
{
    setMeshName("Chicken");
}

std::string ChickenEntity::getMeshNameForKind(ChickenKind kind)
{
    switch(kind)
    {
        case ChickenKind::egg:
            return "ChickenEgg";
        case ChickenKind::chick:
            return "ChickenChick";
        case ChickenKind::rooster:
            return "ChickenRooster";
        default:
            return "Chicken";
    }
}

void ChickenEntity::createMeshLocal(NodeType nt)
{
    RenderedMovableEntity::createMeshLocal(nt);
    if(!getIsOnServerMap())
        RenderManager::getSingleton().rrCreateChickenLook(this);
}

void ChickenEntity::destroyMeshLocal(NodeType nt)
{
    if(!getIsOnServerMap())
        RenderManager::getSingleton().rrDestroyChickenLook(this);
    RenderedMovableEntity::destroyMeshLocal(nt);
}

void ChickenEntity::doUpkeep()
{
    // If we are dead, we remove the chicken
    if(mChickenState == ChickenState::eaten)
    {
        // No need to remove the chicken from its tile as it has already been in eatChicken
        // or when dying
        removeFromGameMap();
        deleteYourself();
        return;
    }

    if(!getIsOnMap())
        return;

    Tile* tile = getPositionTile();
    if(tile == nullptr)
    {
        OD_LOG_ERR("entityName=" + getName());
        return;
    }

    if(mChickenState == ChickenState::dying)
    {
        if(mNbTurnDie < NB_TURNS_DIE_BEFORE_REMOVE)
        {
            ++mNbTurnDie;
            return;
        }
        removeFromGameMap();
        deleteYourself();
        return;
    }

    Room* currentHatchery = nullptr;
    if(tile->getCoveringRoom() != nullptr)
    {
        Room* room = tile->getCoveringRoom();
        if(room->getType() == RoomType::hatchery)
        {
            if(((mKind != ChickenKind::egg) && (mKind != ChickenKind::chick)) ||
               (mHomeSeat == nullptr) || (room->getSeat() == mHomeSeat))
                currentHatchery = room;
        }
    }

    if(currentHatchery != nullptr)
    {
        mNbTurnOutsideHatchery = 0;
        mReturningHome = false;
        mReturnTurns = 0;
        mReturnRetryTurns = 0;
        // The hatchery it is in is its home (also for a chicken loaded from a save)
        mHomeSeat = currentHatchery->getSeat();
    }
    else
        ++mNbTurnOutsideHatchery;

    // A rooster that was dropped outside of a hatchery runs back to the nearest hatchery of his keeper. A hen
    // does the same as long as nobody is after it
    const bool henMayReturn = (mKind == ChickenKind::hen) && !mLockedEat;
    if(((mKind == ChickenKind::rooster) || henMayReturn) && (currentHatchery == nullptr) && !mIsSlapped && runBackToHatchery(tile))
    {
        mNbTurnOutsideHatchery = 0;
        return;
    }

    // Eggs and chicks that are dropped anywhere but in a hatchery are lost soon
    if(((mKind == ChickenKind::egg) || (mKind == ChickenKind::chick)) && (currentHatchery == nullptr) &&
       (mNbTurnOutsideHatchery >= static_cast<int32_t>(ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryYoungLostTurns", 2.0))))
    {
        removeEntityFromPositionTile();
        mChickenState = ChickenState::eaten;
        clearDestinations(EntityAnimation::idle_anim, true, true);
        return;
    }

    // If we are outside a hatchery for too long, we die
    if(mIsSlapped || (mNbTurnOutsideHatchery >= NB_TURNS_OUTSIDE_HATCHERY_BEFORE_DIE))
    {
        mChickenState = ChickenState::dying;
        clearDestinations(EntityAnimation::die_anim, false, false);
        return;
    }

    ChickenFlight::tick(mFlight);

    // An egg does not move
    if(mKind == ChickenKind::egg)
        return;

    // The flight from the floor to a roof and back: a step every turn, nothing else meanwhile
    if(mHopTurnsLeft > 0)
    {
        continueHop();
        return;
    }

    if(mScatterTurns > 0)
        --mScatterTurns;

    // The pose of a laying hen or a cackling one: the animal stays where it is for a moment
    if(mBusyTurns > 0)
    {
        --mBusyTurns;
        if(mBusyTurns == 0)
        {
            if(mLeavingCoop)
            {
                // The entrance pose is over: through the door of the coop to the free place on the floor
                std::vector<Ogre::Vector2> path;
                if(getPosition().x < mCoopDoor.x)
                    path.push_back(mCoopDoor);
                path.push_back(mCoopExit);
                setWalkPath(EntityAnimation::walk_anim, EntityAnimation::idle_anim, true, true, path, false);
            }
            else
                setAnimationState(EntityAnimation::idle_anim, true);
        }
        return;
    }

    if(mLeavingCoop)
    {
        if(isMoving())
            return;
        mLeavingCoop = false;
    }

    // The hatchery moves the rooster itself for the moods that need it
    if((mKind == ChickenKind::rooster) && mRoomDriven)
        return;

    // Handle normal behaviour : move or pick (if not already moving)
    if(isMoving())
        return;

    // A hungry creature that comes to eat this chicken makes it hop away (short, rare, limited);
    // not while the hatchery scatters the hen, which is its own flight
    if((mScatterTurns == 0) && tryFlee(tile, currentHatchery))
        return;

    // Chicks stay in line behind the animal in front of them, a hen walks to her nest when the hatchery sends her
    if(((mKind == ChickenKind::chick) || (mKind == ChickenKind::hen)) && mFollowing)
    {
        if(walkToward(mFollowTarget, mFollowGap, EntityAnimation::walk_anim))
            return;

        // Close enough: wait there (a hen pecks at the food)
        if(Ogre::Vector2(getPosition().x, getPosition().y).distance(mFollowTarget) <= mFollowGap + 0.3)
        {
            if(mKind == ChickenKind::hen)
                setAnimationState("Pick", true);
            else
                setAnimationState(EntityAnimation::idle_anim, true);
            return;
        }
    }

    wander(currentHatchery);
}

void ChickenEntity::wander(Room* currentHatchery)
{
    // Short pauses: the animal pecks instead of walking
    const uint32_t pausePercent = static_cast<uint32_t>(
        ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryWanderPausePercent", 50.0));
    if(Random::Uint(0, 99) < pausePercent)
    {
        setAnimationState("Pick");
        return;
    }

    // Outside of a hatchery the animal does not roam
    if((currentHatchery == nullptr) || (currentHatchery->getType() != RoomType::hatchery))
        return;

    // Free points anywhere in the room (not bound to the tiles), reached over one soft curve. No walk distortion:
    // the way already keeps its distance to the walls
    const Ogre::Vector2 start(getPosition().x, getPosition().y);
    std::vector<Ogre::Vector2> path;
    if(!static_cast<RoomHatchery*>(currentHatchery)->planWanderPath(start, path))
        return;
    const std::string& walkAnim = (mKind == ChickenKind::rooster) ? ChickenPose::strut : EntityAnimation::walk_anim;
    setWalkPath(walkAnim, EntityAnimation::idle_anim, true, true, path, false);
}

void ChickenEntity::setLockEat(const Creature& worker, bool lock)
{
    mLockedEat = lock;
    mLockOwner = lock ? worker.getName() : std::string();
}

bool ChickenEntity::tryFlee(Tile* tile, Room* currentHatchery)
{
    // Only inside the hatchery, only while a creature holds the lock to eat this chicken
    if(!mLockedEat || (mChickenState != ChickenState::free) || (currentHatchery == nullptr))
        return false;

    Creature* eater = getGameMap()->getCreature(mLockOwner);
    if((eater == nullptr) || !eater->getIsOnMap())
        return false;

    const Ogre::Vector2 eaterPosition(eater->getPosition().x, eater->getPosition().y);
    const Ogre::Vector2 start(getPosition().x, getPosition().y);
    const double distance = (eaterPosition - start).length();
    if(!ChickenFlight::shouldFlee(mFlight, distance, true))
        return false;

    // A short hop to a free point of the room (no step from tile to tile), away from the creature
    if(currentHatchery->getType() != RoomType::hatchery)
        return false;

    std::vector<Ogre::Vector2> path;
    if(!static_cast<RoomHatchery*>(currentHatchery)->planFleePath(start, eaterPosition, path))
        return false;

    setWalkPath(EntityAnimation::walk_anim, EntityAnimation::idle_anim, true, true, path, false);
    ChickenFlight::registerFlight(mFlight, ODApplication::turnsPerSecond);
    return true;
}

bool ChickenEntity::walkToward(const Ogre::Vector2& target, double stopDistance, const std::string& walkAnim)
{
    const Ogre::Vector2 start(getPosition().x, getPosition().y);
    Ogre::Vector2 delta = target - start;
    double dist = delta.length();
    if(dist <= stopDistance + 0.05)
        return false;

    Ogre::Vector2 point = target;
    if(stopDistance > 0.0)
        point = target - delta * static_cast<Ogre::Real>(stopDistance / dist);

    // The animal stays in the room it is in
    Tile* currentTile = getPositionTile();
    Tile* targetTile = getGameMap()->getTile(Helper::round(point.x), Helper::round(point.y));
    if((currentTile == nullptr) || (targetTile == nullptr) || targetTile->isFullTile())
        return false;
    if((currentTile->getCoveringRoom() != nullptr) && (targetTile->getCoveringRoom() != currentTile->getCoveringRoom()))
        return false;

    const std::vector<RoomObjectPath::Obstacle> obstacles = RoomObjectNavigation::collect(*getGameMap(), 0.1f);
    if(!RoomObjectPath::clearSegment(obstacles, start, point, true))
        return false;

    std::vector<Ogre::Vector2> path;
    path.push_back(point);
    setWalkPath(walkAnim, EntityAnimation::idle_anim, true, true, path, false);
    return true;
}

void ChickenEntity::playPose(const std::string& pose, uint32_t turns)
{
    mBusyTurns = turns;
    clearDestinations(pose, true, false);
}

void ChickenEntity::mountHen(ChickenEntity& hen)
{
    // He climbs on her facing the way she looks, and the clients are told which hen it is
    Ogre::Vector3 direction = hen.getWalkDirection();
    if(direction.squaredLength() < 0.000001f)
        direction = Ogre::Vector3::NEGATIVE_UNIT_Y;
    setWalkDirection(direction);
    mMountHenName = hen.getName();
    // The climb on the hen, the treading and the climb down last three turns together
    hen.playPose(ChickenPose::cackle, 3);
    playPose(ChickenPose::mount, 3);
    for(Seat* seat : mSeatsWithVisionNotified)
    {
        if(seat->getPlayer() == nullptr || !seat->getPlayer()->getIsHuman())
            continue;
        ServerNotification* notification = new ServerNotification(ServerNotificationType::chickenMount, seat->getPlayer());
        notification->mPacket << getName() << hen.getName();
        ODServer::getSingleton().queueServerNotification(notification);
    }
}

void ChickenEntity::emergeFromCoop(const Ogre::Vector2& door, const Ogre::Vector2& exit)
{
    mCoopDoor = door;
    mCoopExit = exit;
    mLeavingCoop = true;
    playPose(ChickenPose::emerge, 1);
}

bool ChickenEntity::scatterTo(const Ogre::Vector2& spot, uint32_t turns)
{
    mFollowing = false;
    mBusyTurns = 0;
    if(!walkToward(spot, 0.0, ChickenPose::flee))
        return false;

    mScatterTurns = turns;
    return true;
}

void ChickenEntity::setFollowTarget(const Ogre::Vector2& target, double gap)
{
    mFollowing = true;
    mFollowTarget = target;
    mFollowGap = gap;
}

void ChickenEntity::clearFollowTarget()
{
    mFollowing = false;
}

namespace
{
//! Seconds of the RoofFlight clip of the skeleton; the clip is stretched to the length of the flight
const float ROOF_FLIGHT_CLIP_SECONDS = 4.0f;
//! The rooster stays on the ground while he crouches (the first part of the flight) and has settled before the wings
//! are folded (the last part): only this part of the flight is travel
const float ROOF_FLIGHT_CROUCH_PART = 0.12f;
const float ROOF_FLIGHT_TRAVEL_PART = 0.73f;
//! The travel is a lift (up to this part of it), the way across (the next ROOF_FLIGHT_ACROSS_PART of it) and the
//! settling (from ROOF_FLIGHT_LAND_START on)
const float ROOF_FLIGHT_LIFT_END = 0.3f;
const float ROOF_FLIGHT_ACROSS_PART = 0.4f;
const float ROOF_FLIGHT_LAND_START = 0.7f;
//! Tiles of clearance above the higher end of the flight, for the scale of the rooster (long tail)
const float ROOF_FLIGHT_CLEARANCE = 0.4f;

//! \brief The place of a flying rooster at a part of the flight (0 = start, 1 = end). The route lifts vertically
//! outside the coop, crosses above its ridge and settles vertically on the perch; exchanging the two ends gives
//! exactly the reverse route for the descent.
//! \param from Start of the flight.
//! \param to Goal of the flight.
//! \param progress Part of the flight that is over (0 to 1).
Ogre::Vector3 roofFlightPosition(const Ogre::Vector3& from, const Ogre::Vector3& to, Ogre::Real progress)
{
    // Grounded during crouching and settled before the wings finish folding.
    const Ogre::Real travel = std::max(0.0f, std::min(1.0f, (progress - ROOF_FLIGHT_CROUCH_PART) / ROOF_FLIGHT_TRAVEL_PART));
    if(travel <= 0.0f)
        return from;
    if(travel >= 1.0f)
        return to;
    const Ogre::Real clearance = ROOF_FLIGHT_CLEARANCE * static_cast<Ogre::Real>(
        ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryRoosterScale", 1.25));
    const Ogre::Real high = std::max(from.z, to.z) + clearance;
    const Ogre::Real lateral = std::max(0.0f, std::min(1.0f, (travel - ROOF_FLIGHT_LIFT_END) / ROOF_FLIGHT_ACROSS_PART));
    const Ogre::Real across = lateral * lateral * (3.0f - 2.0f * lateral);
    Ogre::Vector3 position = from + (to - from) * across;
    if(travel < ROOF_FLIGHT_LIFT_END)
    {
        const Ogre::Real lift = travel / ROOF_FLIGHT_LIFT_END;
        position.z = from.z + (high - from.z) * lift * lift * (3.0f - 2.0f * lift);
    }
    else if(travel > ROOF_FLIGHT_LAND_START)
    {
        const Ogre::Real land = (travel - ROOF_FLIGHT_LAND_START) / (1.0f - ROOF_FLIGHT_LAND_START);
        position.z = high + (to.z - high) * land * land * (3.0f - 2.0f * land);
    }
    else
        position.z = high;
    return position;
}
}

void ChickenEntity::startHop(const Ogre::Vector3& target)
{
    const uint32_t turns = static_cast<uint32_t>(std::max(1.0,
        ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryRoosterHopTurns", 4.0)));
    mBusyTurns = 0;
    mHopFrom = getPosition();
    mHopTo = target;
    mHopTurns = turns;
    mHopTurnsLeft = turns;
    mHopElapsed = 0.0f;
    clearDestinations(ChickenPose::roofFlight, false, false);
    for(Seat* seat : mSeatsWithVisionNotified)
    {
        if(seat->getPlayer() == nullptr || !seat->getPlayer()->getIsHuman())
            continue;
        ServerNotification* notification = new ServerNotification(
            ServerNotificationType::chickenRoofFlight, seat->getPlayer());
        notification->mPacket << getName() << mHopFrom << mHopTo << turns << mHopElapsed;
        ODServer::getSingleton().queueServerNotification(notification);
    }
}

void ChickenEntity::continueHop()
{
    --mHopTurnsLeft;
    mHopElapsed = static_cast<Ogre::Real>(mHopTurns - mHopTurnsLeft);
    RenderedMovableEntity::setPosition(roofFlightPosition(mHopFrom, mHopTo,
        mHopElapsed / static_cast<Ogre::Real>(mHopTurns)));
    if(mHopTurnsLeft == 0)
        setAnimationState(EntityAnimation::idle_anim, true);
}

void ChickenEntity::startRoofFlightFromServer(const Ogre::Vector3& from, const Ogre::Vector3& to,
    uint32_t turns, Ogre::Real elapsed)
{
    mHopFrom = from;
    mHopTo = to;
    mHopTurns = std::max<uint32_t>(1, turns);
    mHopElapsed = elapsed;
    mHopTurnsLeft = 1;
    setAnimationState(ChickenPose::roofFlight, false, Ogre::Vector3::ZERO, false);
    RenderedMovableEntity::setPosition(roofFlightPosition(from, to, elapsed / mHopTurns));
    if(getAnimationState() != nullptr)
        getAnimationState()->setTimePosition(ROOF_FLIGHT_CLIP_SECONDS * elapsed / mHopTurns);
}

double ChickenEntity::getAnimationSpeedFactor() const
{
    return (!getIsOnServerMap() && mHopTurnsLeft > 0) ? ROOF_FLIGHT_CLIP_SECONDS / mHopTurns : 1.0;
}

void ChickenEntity::update(Ogre::Real timeSinceLastFrame)
{
    RenderedMovableEntity::update(timeSinceLastFrame);
    if(getIsOnServerMap() || mHopTurnsLeft == 0)
        return;
    mHopElapsed = std::min(static_cast<Ogre::Real>(mHopTurns), mHopElapsed +
        static_cast<Ogre::Real>(ODApplication::turnsPerSecond * timeSinceLastFrame));
    RenderedMovableEntity::setPosition(roofFlightPosition(mHopFrom, mHopTo, mHopElapsed / mHopTurns));
    if(getAnimationState() != nullptr && getAnimationState()->getAnimationName() == "RoofFlight")
        getAnimationState()->setTimePosition(ROOF_FLIGHT_CLIP_SECONDS * mHopElapsed / mHopTurns);
    if(mHopElapsed >= mHopTurns)
        mHopTurnsLeft = 0;
}

void ChickenEntity::setPosition(const Ogre::Vector3& position, GameMap* gameMap)
{
    // An authoritative teleport (pickup/drop or another action) cancels the cosmetic flight.
    if(!getIsOnServerMap())
        mHopTurnsLeft = 0;
    RenderedMovableEntity::setPosition(position, gameMap);
}

void ChickenEntity::hopToRoof(const Ogre::Vector3& position)
{
    mOnRoof = true;
    startHop(position);
}

void ChickenEntity::hopDown(const Ogre::Vector2& position)
{
    if(!mOnRoof)
        return;

    mOnRoof = false;
    startHop(Ogre::Vector3(position.x, position.y, 0.0f));
}

void ChickenEntity::teleport(const Ogre::Vector3& position)
{
    mBusyTurns = 0;
    mHopTurnsLeft = 0;
    clearDestinations(EntityAnimation::idle_anim, true, false);
    moveTo(position);
}

void ChickenEntity::moveTo(const Ogre::Vector3& position)
{
    setPosition(position);
    if(!getIsOnServerMap())
        return;

    for(Seat* seat : mSeatsWithVisionNotified)
    {
        if(seat->getPlayer() == nullptr || !seat->getPlayer()->getIsHuman())
            continue;

        ServerNotification* notification = new ServerNotification(
            ServerNotificationType::entityTeleported, seat->getPlayer());
        notification->mPacket << getName() << position;
        ODServer::getSingleton().queueServerNotification(notification);
    }
}

bool ChickenEntity::runBackToHatchery(Tile* tile)
{
    ConfigManager& config = ConfigManager::getSingleton();
    const bool isHen = (mKind == ChickenKind::hen);
    if(isHen)
        ++mReturnTurns;

    if(mReturningHome && isMoving())
        return true;

    if(isHen)
    {
        // A hen does not search every turn and gives up after a while: then the rule of the turns
        // outside of a hatchery decides (it dies if it found no way)
        const uint32_t maxReturnTurns = static_cast<uint32_t>(std::max(0.0,
            config.getRoomConfigDoubleOrDefault("HatcheryReturnMaxTurns", 300.0)));
        if((maxReturnTurns > 0) && (mReturnTurns > maxReturnTurns))
            return false;

        if(mReturnRetryTurns > 0)
        {
            --mReturnRetryTurns;
            return false;
        }
    }

    // The hatchery of the keeper the animal belongs to; without that link, any hatchery will do
    Seat* seat = mHomeSeat;
    if(seat == nullptr)
        seat = tile->getSeat();
    std::vector<Room*> hatcheries;
    if(seat != nullptr)
        hatcheries = getGameMap()->getRoomsByTypeAndSeat(RoomType::hatchery, seat);
    else
        hatcheries = getGameMap()->getRoomsByType(RoomType::hatchery);
    if(hatcheries.empty())
    {
        failReturn();
        return false;
    }

    // Breadth-first search over the free tiles to the closest tile of a hatchery of the keeper
    const uint32_t maxTiles = static_cast<uint32_t>(std::max(1.0,
        config.getRoomConfigDoubleOrDefault("HatcheryReturnSearchTiles", 4000.0)));
    std::map<Tile*, Tile*> parents;
    std::deque<Tile*> open;
    parents[tile] = nullptr;
    open.push_back(tile);
    Tile* goal = nullptr;
    while(!open.empty() && (parents.size() < maxTiles))
    {
        Tile* current = open.front();
        open.pop_front();
        Room* room = current->getCoveringRoom();
        if((room != nullptr) && (room->getType() == RoomType::hatchery) && ((seat == nullptr) || (room->getSeat() == seat)))
        {
            goal = current;
            break;
        }

        static const int dx[4] = {1, -1, 0, 0};
        static const int dy[4] = {0, 0, 1, -1};
        for(int i = 0; i < 4; ++i)
        {
            Tile* next = getGameMap()->getTile(current->getX() + dx[i], current->getY() + dy[i]);
            if((next == nullptr) || (parents.count(next) > 0) || (next->getFullness() > 0.0))
                continue;

            TileType nextType = next->getType();
            if((nextType != TileType::dirt) && (nextType != TileType::gold) && (nextType != TileType::rock))
                continue;

            parents[next] = current;
            open.push_back(next);
        }
    }
    if(goal == nullptr)
    {
        failReturn();
        return false;
    }

    std::vector<Tile*> reversed;
    for(Tile* step = goal; (step != nullptr) && (step != tile); step = parents[step])
        reversed.push_back(step);

    // Walk the first part of the way, the next turns go on from there
    const uint32_t maxPathTiles = static_cast<uint32_t>(std::max(1.0,
        config.getRoomConfigDoubleOrDefault("HatcheryReturnPathTiles", 30.0)));
    std::vector<Ogre::Vector2> path;
    for(std::vector<Tile*>::reverse_iterator it = reversed.rbegin(); (it != reversed.rend()) && (path.size() < maxPathTiles); ++it)
        path.push_back(Ogre::Vector2((*it)->getX(), (*it)->getY()));
    if(path.empty())
        return false;

    mReturningHome = true;
    // The server walk path sends the walk to the clients like any other movement
    setWalkPath(isHen ? EntityAnimation::walk_anim : ChickenPose::flee, EntityAnimation::idle_anim, true, true, path, false);
    return true;
}

void ChickenEntity::failReturn()
{
    // No way back found: wait before looking again (the turns outside a hatchery keep counting)
    mReturnRetryTurns = static_cast<uint32_t>(std::max(0.0,
        ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryReturnRetryTurns", 5.0)));
}

void ChickenEntity::addTileToListIfPossible(int x, int y, Room* currentHatchery, std::vector<Tile*>& possibleTileMove)
{
    Tile* tile = getGameMap()->getTile(x, y);
    if(tile == nullptr)
        return;

    if(tile->getFullness() > 0.0)
        return;

    TileType tileType = tile->getType();
    switch(tileType)
    {
        case TileType::dirt:
        case TileType::gold:
        case TileType::rock:
        {
            break;
        }
        default:
            return;

    }

    if((currentHatchery != nullptr) && (currentHatchery != tile->getCoveringBuilding()))
        return;

    // We can move on this tile
    possibleTileMove.push_back(tile);
}

GameEntityType ChickenEntity::getObjectType() const
{
    return GameEntityType::chickenEntity;
}

bool ChickenEntity::tryPickup(Seat* seat)
{
    if(!getIsOnMap())
        return false;

    // We do not let it be picked up as it will be removed during next upkeep. However, this is
    // true only on server side. On client side, if a chicken is available, it can be picked up (it will
    // be up to the server to validate or not) because the client do not know the chicken state.
    if(getIsOnServerMap() && (mChickenState != ChickenState::free))
        return false;

    Tile* tile = getPositionTile();
    if(tile == nullptr)
    {
        OD_LOG_ERR("entityName=" + getName());
        return false;
    }

    if(getGameMap()->isInEditorMode())
        return true;

    if(!tile->isClaimedForSeat(seat))
        return false;

    if(tile->getSeat() != seat)
        return false;

    return true;
}

//! \brief The keeper's hand takes the animal: it is taken out of the room's work, and a rooster protests loudly.
void ChickenEntity::pickup()
{
    if(getIsOnServerMap() && ((mKind == ChickenKind::egg) || (mKind == ChickenKind::chick)))
    {
        Tile* tile = getPositionTile();
        if(tile != nullptr)
            mHomeSeat = tile->getSeat();
    }
    // The rooster protests loudly when the hand takes him
    if(getIsOnServerMap() && (mKind == ChickenKind::rooster))
    {
        Tile* pickupTile = getPositionTile();
        if(pickupTile != nullptr)
            RoomHatchery::fireProtest(*pickupTile);
    }
    mScatterTurns = 0;
    mOnRoof = false;
    mHopTurnsLeft = 0;
    mFighting = false;
    mRoomDriven = false;
    mBusyTurns = 0;
    mLeavingCoop = false;
    mFollowing = false;
    mReturningHome = false;
    removeEntityFromPositionTile();
    RenderedMovableEntity::pickup();

    // In the hand he puffs up and flaps until he is put down (the pose ends with the next animation he gets)
    if(getIsOnServerMap() && (mKind == ChickenKind::rooster))
        clearDestinations(ChickenPose::protest, true, false);
}

bool ChickenEntity::tryDrop(Seat* seat, Tile* tile)
{
    if (tile->isFullTile())
        return false;

    // In editor mode, we allow to drop an object in dirt, claimed or gold tiles
    if(getGameMap()->isInEditorMode() &&
       (tile->getTileVisual() == TileVisual::dirtGround || tile->getTileVisual() == TileVisual::goldGround || tile->getTileVisual() == TileVisual::rockGround))
    {
        return true;
    }

    // we cannot drop a chicken on a tile we don't see
    if(!seat->hasVisionOnTile(tile))
        return false;

    // Otherwise, we allow to drop an object only on allied claimed tiles
    if(tile->isClaimedForSeat(seat))
        return true;

    return false;
}

void ChickenEntity::correctEntityMovePosition(Ogre::Vector2& position)
{
    static const double offset = 0.3;
    if(position.x > 0)
        position.x += Random::Double(-offset, offset);

    if(position.y > 0)
        position.y += Random::Double(-offset, offset);

    // if(position.z > 0)
    //     position.z += Random::Double(-offset, offset);
}

void ChickenEntity::setKind(ChickenKind kind)
{
    if(mKind == kind)
        return;

    mKind = kind;
    mAge = 0;
    setMeshName(getMeshNameForKind(kind));
    if(!getIsOnServerMap())
        return;

    for(Seat* seat : mSeatsWithVisionNotified)
    {
        if(seat->getPlayer() == nullptr || !seat->getPlayer()->getIsHuman())
            continue;

        ServerNotification* notification = new ServerNotification(
            ServerNotificationType::chickenKindChanged, seat->getPlayer());
        notification->mPacket << getName() << static_cast<uint32_t>(mKind);
        ODServer::getSingleton().queueServerNotification(notification);
    }
}

void ChickenEntity::setKindFromServer(ChickenKind kind)
{
    ChickenKind oldKind = mKind;
    mKind = kind;
    mAge = 0;

    // The egg has a mesh of its own: when it hatches the shell breaks and the chick takes its place
    if(getMeshName() != getMeshNameForKind(kind))
    {
        bool hadMesh = getEntityNode() != nullptr;
        if(hadMesh)
            destroyMesh();
        setMeshName(getMeshNameForKind(kind));
        if(hadMesh)
        {
            createMesh();
            // The chick that comes out of the egg plays its hatching clip once
            const bool hatching = (oldKind == ChickenKind::egg) && (kind == ChickenKind::chick);
            RenderManager::getSingleton().rrSetObjectAnimationState(this,
                hatching ? ChickenPose::hatchClip : EntityAnimation::idle_anim, !hatching);
        }
    }
    else
        RenderManager::getSingleton().rrUpdateChickenLook(this);

    if((oldKind == ChickenKind::egg) && (kind != ChickenKind::egg))
        RenderManager::getSingleton().rrChickenHatched(this);
}

bool ChickenEntity::countDownLay()
{
    if(mNbTurnLay > 1)
    {
        --mNbTurnLay;
        return false;
    }

    return true;
}

bool ChickenEntity::eatChicken(Creature* creature)
{
    if(!isEdible())
        return false;

    OD_LOG_INF("chicken=" + getName() + " eaten by " + creature->getName());

    removeEntityFromPositionTile();
    mChickenState = ChickenState::eaten;
    clearDestinations(EntityAnimation::idle_anim, true, true);
    return true;
}

bool ChickenEntity::trample(Creature* creature)
{
    if(!isFree() || (mKind != ChickenKind::egg))
        return false;

    OD_LOG_INF("egg=" + getName() + " trampled by " + creature->getName());

    removeEntityFromPositionTile();
    mChickenState = ChickenState::eaten;
    clearDestinations(EntityAnimation::idle_anim, true, true);
    return true;
}

bool ChickenEntity::loseFight()
{
    if(!isFree() || (mKind != ChickenKind::rooster))
        return false;

    OD_LOG_INF("rooster=" + getName() + " lost a fight");

    mFighting = false;
    mRoomDriven = false;
    mOnRoof = false;
    mHopTurnsLeft = 0;
    mBusyTurns = 0;
    mNbTurnDie = 0;
    mChickenState = ChickenState::dying;
    clearDestinations(EntityAnimation::die_anim, false, false);
    return true;
}

void ChickenEntity::notifyFight(const std::string& partnerName, uint32_t phase)
{
    if(!getIsOnServerMap())
        return;

    for(Seat* seat : mSeatsWithVisionNotified)
    {
        if(seat->getPlayer() == nullptr || !seat->getPlayer()->getIsHuman())
            continue;

        ServerNotification* notification = new ServerNotification(
            ServerNotificationType::chickenFight, seat->getPlayer());
        notification->mPacket << getName() << partnerName << phase;
        ODServer::getSingleton().queueServerNotification(notification);
    }
}

bool ChickenEntity::canSlap(Seat* seat)
{
    if(!getIsOnMap())
        return false;

    if(mKind != ChickenKind::hen)
        return false;

    // We do not let it be picked up as it will be removed during next upkeep. However, this is
    // true only on server side. On client side, if a chicken is available, it can be picked up (it will
    // be up to the server to validate or not) because the client do not know the chicken state.
    if(getIsOnServerMap() && (mChickenState != ChickenState::free))
        return false;

    Tile* tile = getPositionTile();
    if(tile == nullptr)
    {
        OD_LOG_ERR("entityName=" + getName());
        return false;
    }

    if(getGameMap()->isInEditorMode())
        return !mIsSlapped;

    if(!tile->isClaimedForSeat(seat))
        return false;

    if(tile->getSeat() != seat)
        return false;

    return !mIsSlapped;
}

ChickenEntity* ChickenEntity::getChickenEntityFromStream(GameMap* gameMap, std::istream& is)
{
    ChickenEntity* obj = new ChickenEntity(gameMap);
    obj->importFromStream(is);
    return obj;
}

ChickenEntity* ChickenEntity::getChickenEntityFromPacket(GameMap* gameMap, ODPacket& is)
{
    ChickenEntity* obj = new ChickenEntity(gameMap);
    obj->importFromPacket(is);
    return obj;
}

void ChickenEntity::exportToPacket(ODPacket& os, const Seat* seat) const
{
    RenderedMovableEntity::exportToPacket(os, seat);
    os << static_cast<uint32_t>(mKind);
    // A client that joins during a flight or a mounting pose is told how far it is
    os << mHopTurnsLeft;
    if(mHopTurnsLeft > 0)
        os << mHopFrom << mHopTo << mHopTurns << mHopElapsed;
    os << ((mBusyTurns > 0 && mPrevAnimationState == ChickenPose::mount) ? mMountHenName : std::string());
}

void ChickenEntity::importFromPacket(ODPacket& is)
{
    RenderedMovableEntity::importFromPacket(is);
    uint32_t kind = 0;
    OD_ASSERT_TRUE(is >> kind);
    mKind = static_cast<ChickenKind>(kind);
    setMeshName(getMeshNameForKind(mKind));
    OD_ASSERT_TRUE(is >> mHopTurnsLeft);
    if(mHopTurnsLeft > 0)
        OD_ASSERT_TRUE(is >> mHopFrom >> mHopTo >> mHopTurns >> mHopElapsed);
    OD_ASSERT_TRUE(is >> mMountHenName);
}

void ChickenEntity::exportToStream(std::ostream& os) const
{
    RenderedMovableEntity::exportToStream(os);
    os << mPosition.x << "\t" << mPosition.y << "\t" << mPosition.z << "\t";
    os << static_cast<uint32_t>(mKind) << "\t" << mNbTurnLay << "\t" << mAge << "\t";
    // Appended later: the way out of the coop (saves without it load with the animal outside)
    os << mLeavingCoop << "\t" << mCoopExit.x << "\t" << mCoopExit.y << "\t"
       << mCoopDoor.x << "\t" << mCoopDoor.y << "\t";
}

bool ChickenEntity::importFromStream(std::istream& is)
{
    if(!RenderedMovableEntity::importFromStream(is))
        return false;
    if(!(is >> mPosition.x >> mPosition.y >> mPosition.z))
        return false;

    // A rooster saved on a coop roof is still there
    mOnRoof = (mPosition.z > 0.3);

    // Saves written before the life cycle end here: those chickens are hens
    uint32_t kind = 0;
    uint32_t nbTurnLay = 0;
    uint32_t age = 0;
    if(is >> kind >> nbTurnLay >> age)
    {
        if(kind <= static_cast<uint32_t>(ChickenKind::egg))
            mKind = static_cast<ChickenKind>(kind);
        mNbTurnLay = nbTurnLay;
        mAge = age;
        setMeshName(getMeshNameForKind(mKind));

        bool leavingCoop = false;
        Ogre::Vector2 coopExit;
        if(is >> leavingCoop >> coopExit.x >> coopExit.y)
        {
            mLeavingCoop = leavingCoop;
            mCoopExit = coopExit;
            if(!(is >> mCoopDoor.x >> mCoopDoor.y))
            {
                is.clear();
                mCoopDoor = mCoopExit;
            }
            if(mLeavingCoop)
            {
                // The way out is started again: a short entrance pose, then the walk
                mBusyTurns = 1;
                mPrevAnimationState = ChickenPose::emerge;
                mPrevAnimationStateLoop = false;
            }
        }
        else
            is.clear();
    }
    else
        is.clear();

    return true;
}

std::string ChickenEntity::getChickenEntityStreamFormat()
{
    std::string format = RenderedMovableEntity::getRenderedMovableEntityStreamFormat();
    if(!format.empty())
        format += "\t";

    format += "PosX\tPosY\tPosZ\tKind\tLayTimer\tAge\tLeavingCoop\tCoopExitX\tCoopExitY\tCoopDoorX\tCoopDoorY";

    return format;
}


