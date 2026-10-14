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

#include "creatureaction/CreatureActionGoDefendHeart.h"

#include "creatureaction/CreatureActionWalkToTile.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "utils/LogManager.h"
#include "utils/MakeUnique.h"

#include <functional>
#include <list>
#include <vector>

std::function<bool()> CreatureActionGoDefendHeart::action()
{
    return std::bind(&CreatureActionGoDefendHeart::handleDefendHeart,
        std::ref(mCreature));
}

bool CreatureActionGoDefendHeart::handleDefendHeart(Creature& creature)
{
    Seat* seat = creature.getSeat();
    if(seat == nullptr || !seat->getHeartDefenceActive())
    {
        // The defence is over: the runner returns to normal work
        creature.popAction();
        return true;
    }

    Tile* targetTile = creature.getGameMap()->getHeartDefenceTargetTile(creature, seat);
    if(targetTile == nullptr)
    {
        // No heart to defend anymore
        creature.popAction();
        return true;
    }

    Tile* myTile = creature.getPositionTile();
    if(myTile == nullptr)
    {
        OD_LOG_ERR("name=" + creature.getName());
        return false;
    }

    if(myTile == targetTile)
        return false; // hold the defence point; the action stays in the queue

    if(creature.isMoving())
        return false; // still walking; check again next turn

    std::list<Tile*> result = creature.getGameMap()->path(&creature, targetTile);
    if(result.empty())
        return false; // no path yet: wait and retry

    if(result.size() > 3)
        result.resize(3);

    std::vector<Ogre::Vector2> path;
    creature.tileToVector2(result, path, true, 0.0);
    creature.setWalkPath(EntityAnimation::walk_anim, EntityAnimation::idle_anim, true, true, path, true);
    creature.pushAction(Utils::make_unique<CreatureActionWalkToTile>(creature));
    return false;
}
