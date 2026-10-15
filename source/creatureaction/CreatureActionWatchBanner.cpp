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

#include "creatureaction/CreatureActionWatchBanner.h"

#include "creatureaction/CreatureActionGoCallToWar.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "traps/Trap.h"
#include "traps/TrapType.h"
#include "utils/MakeUnique.h"
#include "utils/Random.h"

#include <list>
#include <memory>
#include <vector>

std::function<bool()> CreatureActionWatchBanner::action()
{
    return std::bind(&CreatureActionWatchBanner::handleWatchBanner,
        std::ref(mCreature), mPostTile, this);
}

bool CreatureActionWatchBanner::handleWatchBanner(Creature& creature, Tile* postTile, CreatureActionWatchBanner* watchBannerAction)
{
    Tile* myTile = creature.getPositionTile();
    if((myTile == nullptr) || (postTile == nullptr))
    {
        creature.popAction();
        return false;
    }

    // The post must still be there, belong to our keeper and be installed
    Trap* trap = postTile->getCoveringTrap();
    if((trap == nullptr) ||
       (trap->getType() != TrapType::watchBanner) ||
       (trap->getSeat() != creature.getSeat()) ||
       !trap->isActivated(postTile))
    {
        creature.popAction();
        return true;
    }

    // We leave the post to eat, sleep or get our fee
    if(creature.isTired() || creature.isHungry() ||
       ((creature.getGoldFee() > 0) && (creature.getSeat()->getGold() > 0)))
    {
        creature.popAction();
        return true;
    }

    if(myTile != postTile)
    {
        if(!creature.getGameMap()->pathExists(&creature, myTile, postTile))
        {
            creature.popAction();
            return true;
        }

        creature.setDestination(postTile);
        return false;
    }

    // We stand on the post until the stay is over, then we go back to our room
    int64_t turn = creature.getGameMap()->getTurnNumber();
    if(watchBannerAction->mArrivalTurn < 0)
        watchBannerAction->mArrivalTurn = turn;

    if(turn - watchBannerAction->mArrivalTurn >= watchBannerAction->mStayTurns)
    {
        creature.popAction();
        return true;
    }

    creature.setAnimationState(EntityAnimation::idle_anim);
    return false;
}

bool CreatureActionWatchBanner::isPostTaken(const Creature& creature, Tile* postTile)
{
    std::vector<Creature*> creatures = creature.getGameMap()->getCreaturesBySeat(creature.getSeat());
    for(Creature* other : creatures)
    {
        if(other == &creature)
            continue;

        for(const std::unique_ptr<CreatureAction>& act : other->getActions())
        {
            if(act->getType() != CreatureActionType::watchBanner)
                continue;

            if(static_cast<CreatureActionWatchBanner*>(act.get())->getPostTile() == postTile)
                return true;
        }
    }

    return false;
}

bool CreatureActionWatchBanner::tryPatrol(Creature& creature, int64_t stayTurns)
{
    Tile* myTile = creature.getPositionTile();
    if(myTile == nullptr)
        return false;

    std::vector<Tile*> posts;
    for(Trap* trap : creature.getGameMap()->getTraps())
    {
        if(trap->getType() != TrapType::watchBanner)
            continue;

        if(trap->getSeat() != creature.getSeat())
            continue;

        for(int i = 0; i < static_cast<int>(trap->numCoveredTiles()); ++i)
        {
            Tile* tile = trap->getCoveredTile(i);
            if(!trap->isActivated(tile))
                continue;

            if(isPostTaken(creature, tile))
                continue;

            if(!creature.getGameMap()->pathExists(&creature, myTile, tile))
                continue;

            posts.push_back(tile);
        }
    }

    if(posts.empty())
        return false;

    // The guards visit the posts in turn, so we do not always take the nearest one
    Tile* chosenTile = posts[Random::Uint(0, posts.size() - 1)];
    creature.pushAction(Utils::make_unique<CreatureActionWatchBanner>(creature, *chosenTile, stayTurns));
    return true;
}

bool CreatureActionWatchBanner::goToIntruder(Creature& creature, Tile* intruderTile)
{
    Tile* myTile = creature.getPositionTile();
    if((myTile == nullptr) || (intruderTile == nullptr))
        return false;

    if(creature.isActionInList(CreatureActionType::goCallToWar))
        return false;

    if(!creature.getGameMap()->pathExists(&creature, myTile, intruderTile))
        return false;

    std::list<Tile*> tempPath = creature.getGameMap()->path(&creature, intruderTile);
    if(tempPath.empty())
        return false;

    std::vector<Ogre::Vector2> path;
    Creature::tileToVector2(tempPath, path, true, 0.0);
    creature.setWalkPath(EntityAnimation::walk_anim, EntityAnimation::idle_anim, true, true, path, true);
    creature.pushAction(Utils::make_unique<CreatureActionGoCallToWar>(creature));
    return true;
}
