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

#include "creatureaction/CreatureActionReloadTrap.h"

#include "entities/Creature.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "gamemap/Pathfinding.h"
#include "traps/Trap.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/MakeUnique.h"

#include <algorithm>

namespace
{
//! The trap that covers the tile, nullptr if there is none
Trap* getTrapOnTile(Tile& tile)
{
    Building* building = tile.getCoveringBuilding();
    if((building == nullptr) || (building->getObjectType() != GameEntityType::trap))
        return nullptr;

    return static_cast<Trap*>(building);
}
}

CreatureActionReloadTrap::CreatureActionReloadTrap(Creature& creature, Tile& tileReload) :
    CreatureAction(creature),
    mTileReload(tileReload),
    mWorkTurns(0)
{
    // Counts in the worker share rules of the player
    mCreature.getSeat()->getPlayer()->notifyWorkerAction(mCreature, getType());

    // One worker per tile
    Trap* trap = getTrapOnTile(mTileReload);
    if(trap != nullptr)
        trap->setReloadWorker(&mTileReload, &mCreature);
}

CreatureActionReloadTrap::~CreatureActionReloadTrap()
{
    mCreature.getSeat()->getPlayer()->notifyWorkerStopsAction(mCreature, getType());

    Trap* trap = getTrapOnTile(mTileReload);
    if((trap != nullptr) && (trap->getReloadWorker(&mTileReload) == &mCreature))
        trap->setReloadWorker(&mTileReload, nullptr);
}

std::function<bool()> CreatureActionReloadTrap::action()
{
    return std::bind(&CreatureActionReloadTrap::handleReloadTrap,
        std::ref(mCreature), std::ref(mTileReload), std::ref(mWorkTurns));
}

bool CreatureActionReloadTrap::handleReloadTrap(Creature& creature, Tile& tileReload, int32_t& workTurns)
{
    Tile* myTile = creature.getPositionTile();
    if(myTile == nullptr)
    {
        OD_LOG_ERR("creature=" + creature.getName() + ", pos=" + Helper::toString(creature.getPosition()));
        creature.popAction();
        return false;
    }

    // The trap may be gone, loaded by somebody else or claimed meanwhile
    Trap* trap = getTrapOnTile(tileReload);
    if((trap == nullptr) || (trap->getSeat() != creature.getSeat()) ||
       !trap->canBeReloadedByWorker(&tileReload, &creature))
    {
        creature.popAction();
        return true;
    }

    // We check if we are on the expected tile. If not, we go there
    if(myTile != &tileReload)
    {
        if(!creature.setDestination(&tileReload))
        {
            creature.popAction();
            return false;
        }

        return true;
    }

    // The work: hammering and tightening until the tile is armed again
    creature.setAnimationState(EntityAnimation::dig_anim);
    ++workTurns;
    int32_t workNeeded = static_cast<int32_t>(ConfigManager::getSingleton().getTrapConfigDoubleOrDefault("TrapReloadWorkTurns", 12.0));
    if(workTurns < workNeeded)
        return false;

    // The gold is paid when the work is done. If the seat cannot pay, nobody tries again for a while
    int32_t price = trap->getReloadPrice();
    if((price > 0) && !creature.getGameMap()->withdrawFromTreasuries(price, creature.getSeat()))
    {
        double retry = ConfigManager::getSingleton().getTrapConfigDoubleOrDefault("TrapReloadRetryTurns", 100.0);
        trap->postponeReload(&tileReload, creature.getGameMap()->getTurnNumber() + static_cast<int64_t>(std::max(0.0, retry)));
        creature.popAction();
        return false;
    }

    trap->activate(&tileReload);
    creature.receiveExp(ConfigManager::getSingleton().getTrapConfigDoubleOrDefault("TrapReloadExperience", 1.0));
    creature.popAction();
    return false;
}

bool CreatureActionReloadTrap::tryStart(Creature& creature)
{
    ConfigManager& config = ConfigManager::getSingleton();
    if(config.getTrapConfigDoubleOrDefault("TrapReloadByWorkers", 1.0) <= 0.0)
        return false;

    Tile* myTile = creature.getPositionTile();
    if((myTile == nullptr) || !creature.getGameMap()->isServerGameMap())
        return false;

    double radius = config.getTrapConfigDoubleOrDefault("TrapReloadSearchRadius", 30.0);
    Tile* bestTile = nullptr;
    float bestDist = 0.0f;
    for(Trap* trap : creature.getGameMap()->getTraps())
    {
        if(trap->getSeat() != creature.getSeat())
            continue;

        // Not enough gold in the treasuries: no use to walk there
        if(creature.getSeat()->getGold() < trap->getReloadPrice())
            continue;

        for(Tile* tile : trap->getCoveredTiles())
        {
            if(!trap->canBeReloadedByWorker(tile, &creature))
                continue;

            float dist = Pathfinding::squaredDistanceTile(*myTile, *tile);
            if(dist > (radius * radius))
                continue;

            if((bestTile != nullptr) && (bestDist <= dist))
                continue;

            if(!creature.getGameMap()->pathExists(&creature, myTile, tile))
                continue;

            bestTile = tile;
            bestDist = dist;
        }
    }

    if(bestTile == nullptr)
        return false;

    creature.pushAction(Utils::make_unique<CreatureActionReloadTrap>(creature, *bestTile));
    return true;
}
