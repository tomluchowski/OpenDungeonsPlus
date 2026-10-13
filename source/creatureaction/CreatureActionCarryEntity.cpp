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

#include "creatureaction/CreatureActionCarryEntity.h"

#include "entities/Building.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <algorithm>
#include <deque>

namespace
{

//! \brief Appends the part of the trail between the two lengths to the path: the points of the trail that lie
//! strictly between them and then the point at the length 'to' itself
void appendTrailSection(const std::vector<Ogre::Vector2>& trail, const std::vector<double>& arcs,
    double from, double to, std::vector<Ogre::Vector2>& path)
{
    for(uint32_t i = 0; i < trail.size(); ++i)
    {
        if((arcs[i] > from) && (arcs[i] < to))
            path.push_back(trail[i]);
    }

    for(uint32_t i = 1; i < trail.size(); ++i)
    {
        if(arcs[i] < to)
            continue;

        double length = arcs[i] - arcs[i - 1];
        double ratio = (length > 0.0) ? ((to - arcs[i - 1]) / length) : 1.0;
        path.push_back(trail[i - 1] + (trail[i] - trail[i - 1]) * static_cast<Ogre::Real>(ratio));
        return;
    }

    if(!trail.empty())
        path.push_back(trail.back());
}

}

CreatureActionCarryEntity::CreatureActionCarryEntity(Creature& creature, GameEntity& entityToCarry, Building& buildingDest) :
    CreatureAction(creature),
    mEntityToCarry(&entityToCarry),
    mTileDest(nullptr),
    mBuildingDest(&buildingDest),
    mIsDrag(isPulledOverGround(entityToCarry)),
    mDragPhase(0),
    mDragLayTurns(0),
    mDragFollowArc(0.0)
{
    mEntityToCarry->addGameEntityListener(this);
    mBuildingDest->addGameEntityListener(this);
    mEntityToCarry->setCarryLock(mCreature, true);
    if(mIsDrag)
    {
        startDrag();
    }
    else
    {
        mEntityToCarry->notifyEntityCarryOn(&mCreature);
        mCreature.carryEntity(mEntityToCarry);
    }
    mTileDest = mBuildingDest->askSpotForCarriedEntity(mEntityToCarry);
    OD_LOG_INF("creature=" + mCreature.getName() + (mIsDrag ? " is pulling " : " is carrying ") + mEntityToCarry->getName() + " to tile=" + Tile::displayAsString(mTileDest));
}

CreatureActionCarryEntity::~CreatureActionCarryEntity()
{
    if(mEntityToCarry != nullptr)
    {
        mEntityToCarry->removeGameEntityListener(this);
        mEntityToCarry->setCarryLock(mCreature, false);
        const Ogre::Vector3& pos = mCreature.getPosition();
        if(mIsDrag)
        {
            OD_LOG_INF("creature=" + mCreature.getName() + " is letting go of pulled " + mEntityToCarry->getName() + ", pos=" + Helper::toString(pos));
            static_cast<Creature*>(mEntityToCarry)->notifyDragEnd();
        }
        else
        {
            OD_LOG_INF("creature=" + mCreature.getName() + " is releasing carried " + mEntityToCarry->getName() + ", pos=" + Helper::toString(pos));
            mEntityToCarry->notifyEntityCarryOff(pos);
        }
    }

    // A pulled creature was never in the carry node of the worker
    if(!mIsDrag)
        mCreature.releaseCarriedEntity();

    if(mBuildingDest != nullptr)
    {
        mBuildingDest->removeGameEntityListener(this);
        mBuildingDest->notifyCarryingStateChanged(&mCreature, mEntityToCarry);
    }
}

std::function<bool()> CreatureActionCarryEntity::action()
{
    if(mIsDrag)
        return std::bind(&CreatureActionCarryEntity::handleDragCreature, this);

    return std::bind(&CreatureActionCarryEntity::handleCarryEntity,
        std::ref(mCreature), mEntityToCarry, mTileDest);
}

bool CreatureActionCarryEntity::isPulledOverGround(GameEntity& entity)
{
    if(entity.getObjectType() != GameEntityType::creature)
        return false;

    // Creatures knocked out to death and dead ones are carried as before, only the hurt ones that are
    // still on their feet or knocked out for a while are pulled
    Creature& creature = static_cast<Creature&>(entity);
    return creature.isAlive() && (creature.getKoTurnCounter() >= 0);
}

bool CreatureActionCarryEntity::handleCarryEntity(Creature& creature, GameEntity* entityToCarry, Tile* tileDest)
{
    if(tileDest == nullptr)
    {
        OD_LOG_ERR("creature=" + creature.getName());
        creature.popAction();
        return false;
    }

    Tile* myTile = creature.getPositionTile();
    if(myTile == nullptr)
    {
        OD_LOG_ERR("creature=" + creature.getName() + ", pos=" + Helper::toString(creature.getPosition()));
        creature.popAction();
        return false;
    }

    if(entityToCarry == nullptr)
    {
        OD_LOG_ERR("creature=" + creature.getName());
        creature.popAction();
        return false;
    }

    // We check if we are on the entity position tile. If yes, we carry it to a building
    // that wants it (if any)
    if(myTile != tileDest)
    {
        if(!creature.setDestination(tileDest))
        {
            OD_LOG_ERR("creature=" + creature.getName() + ", myTile=" + Tile::displayAsString(myTile) + ", tileDest=" + Tile::displayAsString(tileDest));
            creature.popAction();
        }
        return true;
    }

    // We are at the destination tile
    creature.popAction();
    return false;
}

void CreatureActionCarryEntity::startDrag()
{
    Creature* dragged = static_cast<Creature*>(mEntityToCarry);
    dragged->notifyDragStart();

    // The creature lies down on the spot (the clip of the pulled creature, the clients show a fallback pose when
    // the skeleton has none). It keeps its place on the map.
    std::vector<Ogre::Vector2> noPath;
    dragged->setWalkPath(EntityAnimation::dragged_anim, EntityAnimation::dragged_anim, true, false, noPath, false);

    // The trail starts where the creature lies and goes on with the positions of the worker
    const Ogre::Vector3& draggedPos = dragged->getPosition();
    const Ogre::Vector3& workerPos = mCreature.getPosition();
    Ogre::Vector2 draggedPoint(draggedPos.x, draggedPos.y);
    Ogre::Vector2 workerPoint(workerPos.x, workerPos.y);
    mDragTrail.push_back(draggedPoint);
    mDragTrailArc.push_back(0.0);
    double distance = static_cast<double>(workerPoint.distance(draggedPoint));
    if(distance > 0.01)
    {
        mDragTrail.push_back(workerPoint);
        mDragTrailArc.push_back(distance);
    }
}

void CreatureActionCarryEntity::followTrail(Creature& dragged, double targetArc)
{
    if(targetArc <= (mDragFollowArc + 0.001))
        return;

    // What the creature still has to walk stays, the new part of the way of the worker is added behind it. The
    // whole path goes to the clients every turn, it is short because the creature slides faster than it is pulled
    const std::deque<Ogre::Vector2>& queue = dragged.getWalkQueue();
    std::vector<Ogre::Vector2> path(queue.begin(), queue.end());
    appendTrailSection(mDragTrail, mDragTrailArc, mDragFollowArc, targetArc, path);
    mDragFollowArc = targetArc;
    dragged.setWalkPath(EntityAnimation::dragged_anim, EntityAnimation::dragged_anim, true, false, path, false);
}

void CreatureActionCarryEntity::startLaying(Creature& dragged)
{
    mDragPhase = 1;
    mDragLayTurns = 0;

    // The worker is at the bed: the creature slides the rest of the way of the worker and then on to its place
    const std::deque<Ogre::Vector2>& queue = dragged.getWalkQueue();
    std::vector<Ogre::Vector2> path(queue.begin(), queue.end());
    appendTrailSection(mDragTrail, mDragTrailArc, mDragFollowArc, mDragTrailArc.back(), path);
    mDragFollowArc = mDragTrailArc.back();
    Tile* homeTile = dragged.getHomeTile();
    if(homeTile != nullptr)
    {
        path.push_back(Ogre::Vector2(static_cast<Ogre::Real>(homeTile->getX()),
            static_cast<Ogre::Real>(homeTile->getY())));
    }
    dragged.setWalkPath(EntityAnimation::dragged_anim, EntityAnimation::dragged_anim, true, false, path, false);
}

bool CreatureActionCarryEntity::stopDragging(bool standUp)
{
    // A creature that is let go in the middle of the way gets up where it lies (a dead one keeps its own
    // animation). At the bed the dormitory puts it to sleep.
    if(standUp && (mEntityToCarry != nullptr))
    {
        Creature* dragged = static_cast<Creature*>(mEntityToCarry);
        if(dragged->isAlive() && dragged->getIsOnMap())
            dragged->clearDestinations(EntityAnimation::idle_anim, true, true);
    }

    // The worker stops where it is
    mCreature.clearDestinations(EntityAnimation::idle_anim, true, true);
    mCreature.popAction();
    return false;
}

bool CreatureActionCarryEntity::handleDragCreature()
{
    // The creature that was pulled is gone (dead, removed or picked up: the listener let it go already), or
    // the dormitory is gone, or the worker is not on the map
    Tile* myTile = mCreature.getPositionTile();
    if((mEntityToCarry == nullptr) || (mBuildingDest == nullptr) || (mTileDest == nullptr) || (myTile == nullptr))
        return stopDragging(true);

    Creature* dragged = static_cast<Creature*>(mEntityToCarry);
    ConfigManager& config = ConfigManager::getSingleton();

    if(mDragPhase == 1)
    {
        // The creature slides the last steps into its bed. Once it is there (or when that takes too long) the
        // worker lets go and the dormitory puts the creature to sleep
        ++mDragLayTurns;
        double layTurns = config.getRoomConfigDoubleOrDefault("DormitoryWoundedDragLayTurns", 12.0);
        if(!dragged->isAlive() || !dragged->isMoving() || (static_cast<double>(mDragLayTurns) > layTurns))
            return stopDragging(false);

        return false;
    }

    // The creature is let go where it lies when it died or got knocked out to death, when a hostile creature
    // comes close, when pulling takes too long or when the creature is left too far behind
    double enemyRadius = config.getRoomConfigDoubleOrDefault("DormitoryWoundedCarryEnemyRadius", 6.0);
    double maxTurns = config.getRoomConfigDoubleOrDefault("DormitoryWoundedCarryMaxTurns", 400.0);
    double maxDistance = config.getRoomConfigDoubleOrDefault("DormitoryWoundedDragMaxDistance", 3.5);
    const Ogre::Vector3& workerPos = mCreature.getPosition();
    const Ogre::Vector3& draggedPos = dragged->getPosition();
    Ogre::Vector2 workerPoint(workerPos.x, workerPos.y);
    Ogre::Vector2 draggedPoint(draggedPos.x, draggedPos.y);
    if(!dragged->isAlive() || (dragged->getKoTurnCounter() < 0) || !dragged->getIsOnMap() ||
       mCreature.isHostileNear(enemyRadius) || (static_cast<double>(getNbTurnsActive()) > maxTurns) ||
       (static_cast<double>(workerPoint.distance(draggedPoint)) > maxDistance))
    {
        return stopDragging(true);
    }

    // The creature follows the way of the worker at a fixed distance behind it (config)
    double step = static_cast<double>(workerPoint.distance(mDragTrail.back()));
    if(step > 0.01)
    {
        mDragTrail.push_back(workerPoint);
        mDragTrailArc.push_back(mDragTrailArc.back() + step);
    }
    double gap = std::max(0.0, config.getRoomConfigDoubleOrDefault("DormitoryWoundedDragGap", 0.9));
    followTrail(*dragged, std::max(0.0, mDragTrailArc.back() - gap));

    // The worker walks backwards to the bed. The way is walked in one go, so it keeps its turn and this check
    // runs every turn
    if(mCreature.isMoving())
        return false;

    if(myTile != mTileDest)
    {
        if(!mCreature.setDragDestination(mTileDest))
        {
            OD_LOG_ERR("creature=" + mCreature.getName() + ", myTile=" + Tile::displayAsString(myTile) + ", tileDest=" + Tile::displayAsString(mTileDest));
            return stopDragging(true);
        }
        return false;
    }

    // The worker is at the bed
    startLaying(*dragged);
    return false;
}

void CreatureActionCarryEntity::releaseEntity(const std::string& reason)
{
    mEntityToCarry->setCarryLock(mCreature, false);
    const Ogre::Vector3& pos = mCreature.getPosition();
    OD_LOG_INF("creature=" + mCreature.getName() + " is releasing " + reason + " carried " + mEntityToCarry->getName() + ", pos=" + Helper::toString(pos));
    if(mIsDrag)
    {
        // Pulled creatures were never in the carry node of the worker
        static_cast<Creature*>(mEntityToCarry)->notifyDragEnd();
    }
    else
    {
        mEntityToCarry->notifyEntityCarryOff(pos);
        mCreature.releaseCarriedEntity();
    }
    mEntityToCarry = nullptr;
}

std::string CreatureActionCarryEntity::getListenerName() const
{
    return toString(getType()) + ", creature=" + mCreature.getName();
}

bool CreatureActionCarryEntity::notifyDead(GameEntity* entity)
{
    if(entity == mEntityToCarry)
    {
        releaseEntity("dead");
        return false;
    }
    if(entity == mBuildingDest)
    {
        mBuildingDest = nullptr;
        return false;
    }
    return true;
}

bool CreatureActionCarryEntity::notifyRemovedFromGameMap(GameEntity* entity)
{
    if(entity == mEntityToCarry)
    {
        releaseEntity("removed");
        return false;
    }
    if(entity == mBuildingDest)
    {
        mBuildingDest = nullptr;
        return false;
    }
    return true;
}

bool CreatureActionCarryEntity::notifyPickedUp(GameEntity* entity)
{
    if(entity == mEntityToCarry)
    {
        releaseEntity("picked");
        return false;
    }
    return true;
}

bool CreatureActionCarryEntity::notifyDropped(GameEntity* entity)
{
    // That should not happen. For now, we only require events for attacked creatures. And when they
    // are picked up, we should have cleared the action queue
    OD_LOG_ERR(toString(getType()) + ", creature=" + mCreature.getName() + ", entity=" + entity->getName());
    return true;
}
