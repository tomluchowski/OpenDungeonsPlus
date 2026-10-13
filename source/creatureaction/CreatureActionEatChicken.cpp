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
#include "entities/CreatureDefinition.h"
#include "entities/Tile.h"
#include "gamemap/GameMap.h"
#include "gamemap/Pathfinding.h"
#include "gamemap/RoomObjectNavigation.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/MakeUnique.h"
#include "utils/Random.h"

#include <algorithm>

CreatureActionEatChicken::CreatureActionEatChicken(Creature& creature, ChickenEntity& chicken, bool gift) :
    CreatureAction(creature),
    mChicken(&chicken),
    mGift(gift)
{
    mChicken->addGameEntityListener(this);
    mChicken->setLockEat(mCreature, true);
    if(mGift)
    {
        // The creature sniffs at the chicken before it goes for it (the action waits for the cooldown)
        mCreature.setJobCooldown(static_cast<int>(
            ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatcheryGiftSniffTurns", 4.0)));
    }
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
    if(mGift)
        return std::bind(&CreatureActionEatChicken::handleGiftChicken, std::ref(mCreature), mChicken);

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

bool CreatureActionEatChicken::canAcceptGift(const Creature& creature)
{
    const CreatureDefinition* definition = creature.getDefinition();
    if(definition->isWorker())
        return false;

    // Only an idle creature takes the gift. A hungry one looks for food itself
    if(!creature.getActions().empty() || creature.isHungry())
        return false;

    if(!creature.getIsOnMap() || (creature.getPositionTile() == nullptr) || !creature.isAlive() ||
       creature.isKo() || creature.isInPrison())
    {
        return false;
    }

    return true;
}

bool CreatureActionEatChicken::handleGiftChicken(Creature& creature, ChickenEntity* chicken)
{
    const bool edibleBefore = (chicken != nullptr) && chicken->isEdible();
    const double hungerBefore = creature.getHunger();
    const double hpBefore = creature.getHP();
    const bool result = handleEatChicken(creature, chicken);

    // The chicken was not reached yet (or the meal did not happen)
    if(!edibleBefore || chicken->isEdible())
        return result;

    // The meal happened: the creature was not hungry, so it gets less out of it and rests longer
    ConfigManager& config = ConfigManager::getSingleton();
    creature.setHunger(hungerBefore - config.getRoomConfigDoubleOrDefault("HatcheryGiftHungerPerChicken", 4.0));
    creature.setHP(hpBefore + config.getRoomConfigDoubleOrDefault("HatcheryGiftHpRecoveredPerChicken", 2.0));
    const uint32_t cooldownMin = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryGiftCooldownMin", 12.0));
    const uint32_t cooldownMax = static_cast<uint32_t>(config.getRoomConfigDoubleOrDefault("HatcheryGiftCooldownMax", 20.0));
    creature.setJobCooldown(static_cast<int>(Random::Int(static_cast<int>(cooldownMin),
        static_cast<int>(std::max(cooldownMin, cooldownMax)))));
    return result;
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
