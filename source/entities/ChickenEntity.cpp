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

#include "entities/ChickenPose.h"
#include "entities/Creature.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "network/ODPacket.h"
#include "network/ODServer.h"
#include "network/ServerNotification.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "gamemap/RoomObjectNavigation.h"
#include "rooms/Room.h"
#include "rooms/RoomType.h"
#include "utils/Helper.h"
#include "utils/Random.h"
#include "utils/LogManager.h"

#include <cmath>
#include <deque>
#include <iostream>
#include <map>

const int32_t NB_TURNS_OUTSIDE_HATCHERY_BEFORE_DIE = 30;
const int32_t NB_TURNS_DIE_BEFORE_REMOVE = 5;

ChickenEntity::ChickenEntity(GameMap* gameMap, const std::string& hatcheryName, ChickenKind kind) :
    RenderedMovableEntity(gameMap, hatcheryName, "Chicken", 0.0f, false),
    mChickenState(ChickenState::free),
    mKind(kind),
    mNbTurnLay(0),
    mAge(0),
    mBusyTurns(0),
    mCalm(false),
    mRoomDriven(false),
    mOnRoof(false),
    mFollowing(false),
    mFollowTarget(Ogre::Vector2::ZERO),
    mFollowGap(0.0),
    mMood(RoosterMood::strut),
    mMoodTurns(0),
    mSinceCrow(0),
    mHomeSeat(nullptr),
    mReturningHome(false),
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
    mCalm(false),
    mRoomDriven(false),
    mOnRoof(false),
    mFollowing(false),
    mFollowTarget(Ogre::Vector2::ZERO),
    mFollowGap(0.0),
    mMood(RoosterMood::strut),
    mMoodTurns(0),
    mSinceCrow(0),
    mHomeSeat(nullptr),
    mReturningHome(false),
    mNbTurnOutsideHatchery(0),
    mNbTurnDie(0),
    mIsSlapped(false),
    mLockedEat(false)
{
    setMeshName("Chicken");
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
            currentHatchery = room;
        }
    }

    if(currentHatchery != nullptr)
    {
        mNbTurnOutsideHatchery = 0;
        mReturningHome = false;
    }
    else
        ++mNbTurnOutsideHatchery;

    // A rooster that was dropped outside of a hatchery runs back to the nearest hatchery of his keeper
    if((mKind == ChickenKind::rooster) && (currentHatchery == nullptr) && !mIsSlapped && runBackToHatchery(tile))
    {
        mNbTurnOutsideHatchery = 0;
        return;
    }

    // If we are outside a hatchery for too long, we die
    if(mIsSlapped || (mNbTurnOutsideHatchery >= NB_TURNS_OUTSIDE_HATCHERY_BEFORE_DIE))
    {
        mChickenState = ChickenState::dying;
        clearDestinations(EntityAnimation::die_anim, false, false);
        return;
    }

    // An egg does not move
    if(mKind == ChickenKind::egg)
        return;

    // The pose of a laying hen or a cackling one: the animal stays where it is for a moment
    if(mBusyTurns > 0)
    {
        --mBusyTurns;
        if(mBusyTurns == 0)
            setAnimationState(EntityAnimation::idle_anim, true);
        return;
    }

    // The hatchery moves the rooster itself for the moods that need it
    if((mKind == ChickenKind::rooster) && mRoomDriven)
        return;

    // Handle normal behaviour : move or pick (if not already moving)
    if(isMoving())
        return;

    // Chicks stay in line behind the animal in front of them, hens run to the rooster when he calls them
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
                setAnimationState(mCalm ? ChickenPose::roost : EntityAnimation::idle_anim, true);
            return;
        }
    }

    // Sleeping hens and chicks (night, full hatchery) sit still
    if(mCalm && (mKind != ChickenKind::rooster))
    {
        setAnimationState(ChickenPose::roost, true);
        return;
    }

    wander(tile, currentHatchery);
}

void ChickenEntity::wander(Tile* tile, Room* currentHatchery)
{
    // We might not move
    if(Random::Int(1,2) == 1)
    {
        setAnimationState("Pick");
        return;
    }

    int posChickenX = tile->getX();
    int posChickenY = tile->getY();
    std::vector<Tile*> possibleTileMove;
    // We move chickens from 1 tile only to avoid slow creatures from running
    // for ages when the hatchery is big
    addTileToListIfPossible(posChickenX - 1, posChickenY, currentHatchery, possibleTileMove);
    addTileToListIfPossible(posChickenX + 1, posChickenY, currentHatchery, possibleTileMove);
    addTileToListIfPossible(posChickenX, posChickenY - 1, currentHatchery, possibleTileMove);
    addTileToListIfPossible(posChickenX, posChickenY + 1, currentHatchery, possibleTileMove);

    // We cannot move. That can happen if all the nearby tiles have been destroyed
    if(possibleTileMove.empty())
        return;

    const std::vector<RoomObjectPath::Obstacle> obstacles = RoomObjectNavigation::collect(*getGameMap(), 0.1f);
    const Ogre::Vector2 start(getPosition().x, getPosition().y);
    std::vector<Ogre::Vector2> positions;
    for(Tile* candidate : possibleTileMove)
    {
        Ogre::Vector2 point;
        if(RoomObjectNavigation::standingPosition(obstacles,
            Ogre::Vector2(candidate->getX(), candidate->getY()), point) &&
            RoomObjectPath::clearSegment(obstacles, start, point, true))
            positions.push_back(point);
    }
    if(positions.empty())
        return;
    const Ogre::Vector2 v = positions[Random::Uint(0, positions.size() - 1)];
    std::vector<Ogre::Vector2> path;
    path.push_back(v);
    const bool distortion = RoomObjectPath::clearSegment(
        RoomObjectNavigation::collect(*getGameMap(), 0.525f), start, v);
    const std::string& walkAnim = (mKind == ChickenKind::rooster) ? ChickenPose::strut : EntityAnimation::walk_anim;
    setWalkPath(walkAnim, EntityAnimation::idle_anim, true, true, path, distortion);
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

void ChickenEntity::hopToRoof(const Ogre::Vector3& position)
{
    mOnRoof = true;
    teleport(position);
}

void ChickenEntity::hopDown(const Ogre::Vector2& position)
{
    if(!mOnRoof)
        return;

    mOnRoof = false;
    teleport(Ogre::Vector3(position.x, position.y, 0.0f));
}

void ChickenEntity::teleport(const Ogre::Vector3& position)
{
    mBusyTurns = 0;
    clearDestinations(EntityAnimation::idle_anim, true, false);
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
    if(mReturningHome && isMoving())
        return true;
    if(mHomeSeat == nullptr)
        return false;

    std::vector<Room*> hatcheries = getGameMap()->getRoomsByTypeAndSeat(RoomType::hatchery, mHomeSeat);
    if(hatcheries.empty())
        return false;

    // Breadth-first search over the free tiles to the closest tile of a hatchery of the keeper
    const uint32_t maxTiles = 4000;
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
        if((room != nullptr) && (room->getType() == RoomType::hatchery) && (room->getSeat() == mHomeSeat))
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
        return false;

    std::vector<Tile*> reversed;
    for(Tile* step = goal; (step != nullptr) && (step != tile); step = parents[step])
        reversed.push_back(step);

    // Walk the first part of the way, the next turns go on from there
    std::vector<Ogre::Vector2> path;
    for(std::vector<Tile*>::reverse_iterator it = reversed.rbegin(); (it != reversed.rend()) && (path.size() < 30); ++it)
        path.push_back(Ogre::Vector2((*it)->getX(), (*it)->getY()));
    if(path.empty())
        return false;

    mReturningHome = true;
    setWalkPath(ChickenPose::flee, EntityAnimation::idle_anim, true, true, path, false);
    return true;
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

void ChickenEntity::pickup()
{
    mOnRoof = false;
    mBusyTurns = 0;
    mFollowing = false;
    mReturningHome = false;
    removeEntityFromPositionTile();
    RenderedMovableEntity::pickup();
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
    mKind = kind;
    mAge = 0;
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
}

void ChickenEntity::importFromPacket(ODPacket& is)
{
    RenderedMovableEntity::importFromPacket(is);
    uint32_t kind = 0;
    OD_ASSERT_TRUE(is >> kind);
    mKind = static_cast<ChickenKind>(kind);
}

void ChickenEntity::exportToStream(std::ostream& os) const
{
    RenderedMovableEntity::exportToStream(os);
    os << mPosition.x << "\t" << mPosition.y << "\t" << mPosition.z << "\t";
    os << static_cast<uint32_t>(mKind) << "\t" << mNbTurnLay << "\t" << mAge << "\t";
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

    format += "PosX\tPosY\tPosZ\tKind\tLayTimer\tAge";

    return format;
}


