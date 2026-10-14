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

#include "creatureaction/CreatureActionEatChicken.h"

#include "creatureaction/CreatureActionWalkToTile.h"
#include "entities/ChickenEntity.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "gamemap/GameMap.h"
#include "gamemap/Pathfinding.h"
#include "gamemap/RoomObjectNavigation.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/MakeUnique.h"
#include "utils/Random.h"

CreatureActionEatChicken::CreatureActionEatChicken(Creature& creature, ChickenEntity& chicken) :
    CreatureAction(creature),
    mChicken(&chicken)
{
    mChicken->addGameEntityListener(this);
    mChicken->setLockEat(mCreature, true);
}

CreatureActionEatChicken::~CreatureActionEatChicken()
{
    if(mChicken != nullptr)
    {
        mChicken->setLockEat(mCreature, false);
        mChicken->removeGameEntityListener(this);
    }
}

std::function<bool()> CreatureActionEatChicken::action()
{
    return std::bind(&CreatureActionEatChicken::handleEatChicken,
        std::ref(mCreature), mChicken);
}

bool CreatureActionEatChicken::handleEatChicken(Creature& creature, ChickenEntity* chicken)
{
    if(!creature.decreaseJobCooldown())
        return false;

    Tile* myTile = creature.getPositionTile();
    if(myTile == nullptr)
    {
        OD_LOG_ERR("name=" + creature.getName() + ", position=" + Helper::toString(creature.getPosition()));
        creature.popAction();
        return false;
    }

    if(chicken == nullptr)
    {
        creature.popAction();
        return false;
    }

    Tile* chickenTile = chicken->getPositionTile();
    if(chickenTile == nullptr)
    {
        OD_LOG_ERR("name=" + creature.getName() + ", chicken=" + chicken->getName() + ", chicken position=" + Helper::toString(chicken->getPosition()));
        creature.popAction();
        return false;
    }

    float dist = Pathfinding::squaredDistanceTile(*myTile, *chickenTile);
    const Ogre::Vector2 foodPosition(chicken->getPosition().x, chicken->getPosition().y);
    const bool clearReach = RoomObjectPath::clearSegment(
        RoomObjectNavigation::collect(*creature.getGameMap(), 0.0f),
        Ogre::Vector2(creature.getPosition().x, creature.getPosition().y), foodPosition);
    const std::vector<RoomObjectPath::Obstacle> bodyObstacles = RoomObjectNavigation::bodyObstacles(creature);
    const bool clearBody = RoomObjectPath::clearPoint(bodyObstacles,
        Ogre::Vector2(creature.getPosition().x, creature.getPosition().y),
        foodPosition - Ogre::Vector2(creature.getPosition().x, creature.getPosition().y));
    if(dist > 1 || !clearReach || !clearBody)
    {
        std::vector<Ogre::Vector2> path;
        const bool furnitureApproach = !clearReach || !clearBody ||
            !RoomObjectPath::clearPoint(bodyObstacles, foodPosition);
        if(furnitureApproach)
        {
            if(!RoomObjectNavigation::foodApproach(creature, foodPosition, path))
            {
                creature.popAction();
                return false;
            }
        }
        else
        {
            const std::list<Tile*> tiles = creature.getGameMap()->path(&creature, chickenTile);
            if(tiles.empty())
            {
                creature.popAction();
                return true;
            }
            // Preserve the original tile-based chase away from furniture.
            std::list<Tile*> chase = tiles;
            if(chase.size() > 2)
                chase.resize(8 * chase.size() / 10);
            creature.tileToVector2(chase, path, true, 0.0);
        }

        // We make sure we don't go too far as the chicken is also moving
        if(furnitureApproach && path.size() > 2)
        {
            // We only keep 80% of the path
            path.resize(8 * path.size() / 10);
        }

        // The route is already clearance-checked; independent client offsets
        // would be able to move the eater back into the coop.
        creature.setWalkPath(EntityAnimation::walk_anim, EntityAnimation::idle_anim, true, true, path, !furnitureApproach);
        creature.pushAction(Utils::make_unique<CreatureActionWalkToTile>(creature));
        return false;
    }

    // We can eat the chicken
    if(!chicken->eatChicken(&creature))
    {
        creature.popAction();
        return false;
    }
    creature.foodEaten(ConfigManager::getSingleton().getRoomConfigDouble("HatcheryHungerPerChicken"));
    creature.setJobCooldown(Random::Int(ConfigManager::getSingleton().getRoomConfigUInt32("HatcheryCooldownChickenMin"),
        ConfigManager::getSingleton().getRoomConfigUInt32("HatcheryCooldownChickenMax")));
    creature.setHP(creature.getHP() + ConfigManager::getSingleton().getRoomConfigDouble("HatcheryHpRecoveredPerChicken"));
    creature.computeCreatureOverlayHealthValue();
    Ogre::Vector3 walkDirection = chicken->getPosition() - creature.getPosition();
    walkDirection.z = 0;
    walkDirection.normalise();
    // Stop any remaining client interpolation before attaching the consumed chicken.
    creature.clearDestinations(EntityAnimation::eat_chicken_anim, false, false);
    creature.setAnimationState(EntityAnimation::eat_chicken_anim, false,
        walkDirection, false);
    creature.fireChickenFeeding(chicken->getName(), chicken->getPosition());
    return false;
}

std::string CreatureActionEatChicken::getListenerName() const
{
    return toString(getType()) + ", creature=" + mCreature.getName();
}

bool CreatureActionEatChicken::notifyDead(GameEntity* entity)
{
    if(entity == mChicken)
    {
        mChicken->setLockEat(mCreature, false);
        mChicken = nullptr;
        return false;
    }
    return true;
}

bool CreatureActionEatChicken::notifyRemovedFromGameMap(GameEntity* entity)
{
    if(entity == mChicken)
    {
        mChicken->setLockEat(mCreature, false);
        mChicken = nullptr;
        return false;
    }
    return true;
}

bool CreatureActionEatChicken::notifyPickedUp(GameEntity* entity)
{
    if(entity == mChicken)
    {
        mChicken->setLockEat(mCreature, false);
        mChicken = nullptr;
        return false;
    }
    return true;
}

bool CreatureActionEatChicken::notifyDropped(GameEntity* entity)
{
    // That should not happen because we should have stopped listening when the creature was picked up
    OD_LOG_ERR(toString(getType()) + ", creature=" + mCreature.getName() + ", chicken=" + mChicken->getName());
    return true;
}
