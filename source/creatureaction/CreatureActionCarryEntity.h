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

#ifndef CREATUREACTIONCARRYENTITY_H
#define CREATUREACTIONCARRYENTITY_H

#include "creatureaction/CreatureAction.h"
#include "entities/GameEntity.h"

class Building;
class GameEntity;
class Tile;

class CreatureActionCarryEntity : public CreatureAction, public GameEntityListener
{
public:
    CreatureActionCarryEntity(Creature& creature, GameEntity& entityToCarry, Building& buildingDest);

    virtual ~CreatureActionCarryEntity();

    CreatureActionType getType() const override
    { return CreatureActionType::carryEntity; }

    std::function<bool()> action() override;

    std::string getListenerName() const override;
    bool notifyDead(GameEntity* entity) override;
    bool notifyRemovedFromGameMap(GameEntity* entity) override;
    bool notifyPickedUp(GameEntity* entity) override;
    bool notifyDropped(GameEntity* entity) override;

    static bool handleCarryEntity(Creature& creature, GameEntity* entityToCarry, Tile* tileDest);

    //! \brief True if the entity is not carried but pulled over the ground: a living creature of the seat of the
    //! carrier (hurt, knocked out for a while or knocked out to death) on its way to its own bed. Everything else
    //! is carried: gold, bodies, traps and the knocked out enemy creatures that are taken to a prison.
    static bool isPulledOverGround(const Creature& carrier, GameEntity& entity);

    //! \brief One turn of pulling a hurt creature (see mIsDrag). The worker walks backwards to the bed with
    //! the creature on the ground behind it: the creature is not in the carry node of the worker, it keeps
    //! its place on the map and follows the way of the worker at a fixed distance. This action is deleted
    //! by popAction, so nothing of it may be used after that call.
    bool handleDragCreature();

private:
    //! \brief Lets go of the carried or pulled entity (it died, was removed or was picked up) and logs the reason
    void releaseEntity(const std::string& reason);

    //! \brief Pulling: the creature lies down on the spot and the trail of the worker starts where it lay
    void startDrag();

    //! \brief Pulling: the worker is at the bed, the creature slides the rest of the trail and on to its place
    void startLaying(Creature& dragged);

    //! \brief Pulling: sends the new part of the trail of the worker up to targetArc to the creature as its walk path
    void followTrail(Creature& dragged, double targetArc);

    //! \brief Pulling: the worker stops and ends this action. The creature gets up where it lies if standUp is true.
    //! Always returns false (the action is gone after popAction).
    bool stopDragging(bool standUp);

    GameEntity* mEntityToCarry;
    Tile* mTileDest;
    Building* mBuildingDest;

    //! True if the entity is pulled instead of carried
    bool mIsDrag;
    //! Pulling: 0 while the worker walks, 1 while the creature is moved the last steps into the bed
    int32_t mDragPhase;
    //! Pulling: turns spent in phase 1
    int32_t mDragLayTurns;
    //! Pulling: the way of the worker so far (the first point is where the creature lay) and, for each point, the
    //! length of the way up to it
    std::vector<Ogre::Vector2> mDragTrail;
    std::vector<double> mDragTrailArc;
    //! Pulling: the length of the way up to the point the creature has been sent to
    double mDragFollowArc;
};

#endif // CREATUREACTIONCARRYENTITY_H
