#include "gamemap/RoomObjectNavigation.h"
#include "gamemap/RoomObjectBounds.h"
#include "gamemap/RoomObjectStep.h"
#include "creatureaction/CreatureAction.h"
#include "entities/BuildingObject.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "gamemap/GameMap.h"
#include "rooms/Room.h"
#include "rooms/RoomPrison.h"
#include "rooms/RoomType.h"
#include "utils/Helper.h"
#include <functional>

namespace
{
const BuildingObject* interactionObject(Creature& creature, const Ogre::Vector2& goal)
{
    Tile* tile = creature.getGameMap()->getTile(Helper::round(goal.x), Helper::round(goal.y));
    if(tile == nullptr || tile->getCoveringRoom() == nullptr)
        return nullptr;
    Room* room = tile->getCoveringRoom();
    bool entering = room->getType() == RoomType::dormitory &&
        tile == creature.getHomeTile() && creature.isActionInList(CreatureActionType::sleep);
    if(room->getType() == RoomType::portal && creature.isActionInList(CreatureActionType::leaveDungeon))
        entering = true;
    if(room->getType() == RoomType::torture && creature.isActionInList(CreatureActionType::useRoom))
        for(unsigned i = 0; room->getCreatureUsingRoom(i) != nullptr; ++i)
            if(room->getCreatureUsingRoom(i) == &creature)
                entering = true;
    if(!entering)
        return nullptr;
    const std::map<Tile*, BuildingObject*>& objects = room->getBuildingObjects();
    const std::map<Tile*, BuildingObject*>::const_iterator found = objects.find(tile);
    return found == objects.end() ? nullptr : found->second;
}

bool terrainClear(Creature& creature, const Ogre::Vector2& from, const Ogre::Vector2& to)
{
    GameMap& map = *creature.getGameMap();
    const std::function<bool(int, int)> passable = [&](int x, int y)
    {
        Tile* tile = map.getTile(x, y);
        return tile != nullptr && (creature.canGoThroughTile(tile) || tile == creature.getPositionTile());
    };
    int previousX = Helper::round(from.x), previousY = Helper::round(from.y);
    if(!passable(previousX, previousY))
        return false;
    const int steps = std::max(1, int(std::ceil(from.distance(to) * 16.0f)));
    for(int i = 1; i <= steps; ++i)
    {
        const Ogre::Vector2 point = from + (to - from) * (float(i) / steps);
        const int x = Helper::round(point.x), y = Helper::round(point.y);
        if(!passable(x, y))
            return false;
        if(x != previousX && y != previousY &&
            (!passable(x, previousY) || !passable(previousX, y)))
            return false;
        previousX = x;
        previousY = y;
    }
    return true;
}

bool removeLowStepObstacles(Creature& creature, std::vector<RoomObjectPath::Obstacle>& obstacles)
{
    const size_t count = obstacles.size();
    obstacles.erase(std::remove_if(obstacles.begin(), obstacles.end(), [&](RoomObjectPath::Obstacle obstacle)
    {
        return RoomObjectPath::prepareLowStep(obstacle, creature.getMeshName(),
            1.0f + 0.02f * creature.getLevel(), creature.getPosition().z) > 0.0f;
    }), obstacles.end());
    return obstacles.size() != count;
}
}

float RoomObjectNavigation::clearance(const Creature& creature)
{
    const float scale = 1.0f + 0.02f * creature.getLevel();
    for(const RoomObjectPath::WalkingRadius& model : RoomObjectPath::walkingRadii)
        if(creature.getMeshName() == model.name)
            return model.radius * scale;
    return 0.2f * scale;
}

std::vector<RoomObjectPath::Obstacle> RoomObjectNavigation::collect(GameMap& map,
    float clearance, const BuildingObject* interaction)
{
    std::vector<RoomObjectPath::Obstacle> result;
    const std::function<void(const BuildingObject*)> append = [&](const BuildingObject* object)
    {
        // These walkable landmarks are not solid room furniture.
        if(object == interaction || object->getMeshName() == "PortalObject" ||
            object->getMeshName() == "DungeonTempleObject")
            return;
        for(const RoomObjectPath::MeshBounds& bounds : RoomObjectPath::meshBounds)
        {
            if(object->getMeshName() != bounds.name)
                continue;
            const float angle = float(object->getRotationAngle()) * 0.01745329252f;
            const Ogre::Vector2 placedScale = object->getFurnitureScale();
            const RoomObjectPath::FurnitureScale scale = placedScale == Ogre::Vector2::ZERO ? RoomObjectPath::furnitureScale(bounds) :
                RoomObjectPath::FurnitureScale{placedScale.x, placedScale.y};
            result.push_back({{bounds.minX * scale.x - clearance, bounds.minY * scale.y - clearance},
                {bounds.maxX * scale.x + clearance, bounds.maxY * scale.y + clearance},
                {object->getPosition().x, object->getPosition().y}, std::cos(angle), std::sin(angle)});
            result.back().maximumHeight = object->getPosition().z + bounds.maxZ;
            break;
        }
    };
    for(Room* room : map.getRooms())
    {
        for(const std::pair<Tile* const, BuildingObject*>& entry : room->getBuildingObjects())
            append(entry.second);
        if(room->getType() == RoomType::prison)
            for(const std::pair<Tile* const, BuildingObject*>& entry : static_cast<RoomPrison*>(room)->getFencingObjects())
                append(entry.second);
    }
    return result;
}

std::vector<RoomObjectPath::Obstacle> RoomObjectNavigation::bodyObstacles(Creature& creature,
    const BuildingObject* interaction)
{
    std::vector<RoomObjectPath::Obstacle> result = collect(*creature.getGameMap(), 0.0f, interaction);
    Ogre::Vector2 minimum(-0.2f), maximum(0.2f);
    for(const RoomObjectPath::WalkingRadius& model : RoomObjectPath::walkingRadii)
        if(creature.getMeshName() == model.name)
        {
            minimum = Ogre::Vector2(model.minX, model.minY);
            maximum = Ogre::Vector2(model.maxX, model.maxY);
            break;
        }
    const float scale = 1.0f + 0.02f * creature.getLevel();
    Ogre::Vector2 heading(creature.getWalkDirection().x, creature.getWalkDirection().y);
    if(heading.squaredLength() < 0.000001f)
        heading = Ogre::Vector2(0, -1);
    const RoomObjectPath::LowWalkingBounds* low = nullptr;
    for(const RoomObjectPath::LowWalkingBounds& model : RoomObjectPath::lowWalkingBounds)
        if(creature.getMeshName() == model.name)
        {
            low = &model;
            break;
        }
    for(std::vector<RoomObjectPath::Obstacle>::iterator it = result.begin(); it != result.end();)
    {
        RoomObjectPath::Obstacle& obstacle = *it;
        obstacle.bodyMinimum = minimum * scale;
        obstacle.bodyMaximum = maximum * scale;
        if(low != nullptr && obstacle.maximumHeight <= creature.getPosition().z + RoomObjectPath::lowWalkingHeight * scale)
        {
            // Body parts above the complete object cannot collide with it.
            if(low->empty)
            {
                it = result.erase(it);
                continue;
            }
            const float margin = RoomObjectPath::lowWalkingMargin;
            obstacle.bodyMinimum = Ogre::Vector2(low->minX - margin, low->minY - margin) * scale;
            obstacle.bodyMaximum = Ogre::Vector2(low->maxX + margin, low->maxY + margin) * scale;
        }
        obstacle.initialHeading = heading;
        ++it;
    }
    return result;
}

bool RoomObjectNavigation::standingPosition(const std::vector<RoomObjectPath::Obstacle>& obstacles,
    const Ogre::Vector2& wanted, Ogre::Vector2& result)
{
    if(RoomObjectPath::clearPoint(obstacles, wanted))
    {
        result = wanted;
        return true;
    }
    const Ogre::Vector2 center(float(Helper::round(wanted.x)), float(Helper::round(wanted.y)));
    float distance = std::numeric_limits<float>::infinity();
    for(int y = 0; y < 8; ++y)
        for(int x = 0; x < 8; ++x)
        {
            const Ogre::Vector2 point = center + Ogre::Vector2(-0.4375f + x * 0.125f, -0.4375f + y * 0.125f);
            if(point.squaredDistance(wanted) >= distance || !RoomObjectPath::clearPoint(obstacles, point))
                continue;
            distance = point.squaredDistance(wanted);
            result = point;
        }
    return std::isfinite(distance);
}

bool RoomObjectNavigation::refine(Creature& creature, std::vector<Ogre::Vector2>& path)
{
    if(path.empty())
        return false;
    GameMap& map = *creature.getGameMap();
    const BuildingObject* interaction = interactionObject(creature, path.back());
    const float radius = clearance(creature);
    // Each client jitter coordinate is +/-0.3, so a rotated object needs the
    // full diagonal displacement as well as the creature's walking radius.
    std::vector<RoomObjectPath::Obstacle> obstacles = collect(map, radius + 0.425f, interaction);
    const Ogre::Vector2 start(creature.getPosition().x, creature.getPosition().y);
    Ogre::Vector2 previous = start;
    bool nearby = !RoomObjectPath::clearSegment(obstacles, start, path.back());
    for(const Ogre::Vector2& point : path)
    {
        if(!RoomObjectPath::clearSegment(obstacles, previous, point))
            nearby = true;
        previous = point;
    }
    if(!nearby)
        return false;

    obstacles = bodyObstacles(creature, interaction);
    Ogre::Vector2 goal = path.back();
    if(!standingPosition(obstacles, goal, goal))
    {
        path.clear();
        return true;
    }
    const std::function<bool(const Ogre::Vector2&, const Ogre::Vector2&)> terrain = [&](const Ogre::Vector2& from, const Ogre::Vector2& to)
    { return terrainClear(creature, from, to); };
    std::vector<Ogre::Vector2> result;
    // Coarse tile centers bound the search; they are not mandatory stops inside
    // a furnished room. Refine once to the actual destination through its gaps.
    Ogre::Vector2 minimum = start, maximum = start;
    for(const Ogre::Vector2& point : path)
    {
        minimum.makeFloor(point);
        maximum.makeCeil(point);
    }
    minimum.makeFloor(goal);
    maximum.makeCeil(goal);
    const int minX = std::max(0, int(std::floor(minimum.x)) - 2);
    const int minY = std::max(0, int(std::floor(minimum.y)) - 2);
    const int maxX = std::min(map.getMapSizeX() - 1, int(std::ceil(maximum.x)) + 2);
    const int maxY = std::min(map.getMapSizeY() - 1, int(std::ceil(maximum.y)) + 2);
    const std::function<bool(const std::vector<RoomObjectPath::Obstacle>&, std::vector<Ogre::Vector2>&)> findRoute = [&](const std::vector<RoomObjectPath::Obstacle>& solid, std::vector<Ogre::Vector2>& route)
    {
        return RoomObjectPath::route(start, goal, solid, minX, minY, maxX, maxY, terrain, route) ||
            RoomObjectPath::route(start, goal, solid, 0, 0,
                map.getMapSizeX() - 1, map.getMapSizeY() - 1, terrain, route);
    };
    const std::function<float(const std::vector<Ogre::Vector2>&)> length = [&](const std::vector<Ogre::Vector2>& route)
    {
        float distance = 0.0f;
        Ogre::Vector2 previous = start;
        for(const Ogre::Vector2& point : route)
        {
            distance += previous.distance(point);
            previous = point;
        }
        return distance;
    };
    bool found = findRoute(obstacles, result);
    if(found && result.size() == 1)
    {
        // A clear straight route cannot be improved by adding a step.
        path.swap(result);
        return true;
    }
    std::vector<RoomObjectPath::Obstacle> solid, steps;
    std::vector<float> rises;
    const float bestCost = found ? length(result) : std::numeric_limits<float>::infinity();
    for(RoomObjectPath::Obstacle obstacle : obstacles)
    {
        const float rise = RoomObjectPath::prepareLowStep(obstacle, creature.getMeshName(),
            1.0f + 0.02f * creature.getLevel(), creature.getPosition().z);
        if(rise <= 0.0f)
        {
            solid.push_back(obstacle);
            continue;
        }
        // Every crossing must reach the expanded nest and pay its rise/fall.
        // Distant nests whose lower bound cannot beat the existing route stay
        // solid instead of causing another whole-map alternative search.
        const std::function<float(const Ogre::Vector2&, const Ogre::Vector2&)> radius = [](const Ogre::Vector2& low, const Ogre::Vector2& high)
        {
            return std::hypot(std::max(std::abs(low.x), std::abs(high.x)),
                std::max(std::abs(low.y), std::abs(high.y)));
        };
        const float reach = radius(obstacle.minimum, obstacle.maximum) +
            radius(obstacle.bodyMinimum, obstacle.bodyMaximum);
        const float lowerBound = std::max(start.distance(goal),
            start.distance(obstacle.position) + goal.distance(obstacle.position) - 2.0f * reach) + 2.0f * rise;
        if(lowerBound >= bestCost)
            solid.push_back(obstacle);
        else
        {
            steps.push_back(obstacle);
            rises.push_back(rise);
        }
    }
    std::vector<Ogre::Vector2> crossing;
    if(!steps.empty() && findRoute(solid, crossing))
    {
        float cost = length(crossing);
        for(size_t i = 0; i < steps.size(); ++i)
        {
            Ogre::Vector2 previous = start;
            for(const Ogre::Vector2& point : crossing)
            {
                if(steps[i].intersects(previous, point))
                {
                    cost += 2.0f * rises[i];
                    break;
                }
                previous = point;
            }
        }
        if(!found || cost < length(result))
        {
            result.swap(crossing);
            found = true;
        }
    }
    if(!found)
    {
        path.clear();
        return true;
    }
    path.swap(result);
    return true;
}

bool RoomObjectNavigation::foodApproach(Creature& creature, const Ogre::Vector2& food,
    std::vector<Ogre::Vector2>& path)
{
    path.clear();
    GameMap& map = *creature.getGameMap();
    Tile* startTile = creature.getPositionTile();
    if(startTile == nullptr)
        return false;
    const std::vector<RoomObjectPath::Obstacle> furniture = collect(map, 0.0f);
    const std::vector<RoomObjectPath::Obstacle> body = bodyObstacles(creature);
    const Ogre::Vector2 start(creature.getPosition().x, creature.getPosition().y);
    const int foodX = Helper::round(food.x), foodY = Helper::round(food.y);
    std::vector<Ogre::Vector2> candidates;
    // Preserve the food action's same-tile/cardinal-neighbor reach, but require
    // an unobstructed approach on the chicken's side of the furniture.
    for(int dy = -1; dy <= 1; ++dy)
        for(int dx = -1; dx <= 1; ++dx)
        {
            if(std::abs(dx) + std::abs(dy) > 1)
                continue;
            Tile* tile = map.getTile(foodX + dx, foodY + dy);
            if(!creature.canGoThroughTile(tile) || !map.pathExists(&creature, startTile, tile))
                continue;
            for(int y = 0; y < 8; ++y)
                for(int x = 0; x < 8; ++x)
                {
                    const Ogre::Vector2 point(foodX + dx - 0.4375f + x * 0.125f,
                        foodY + dy - 0.4375f + y * 0.125f);
                    if(RoomObjectPath::clearPoint(body, point, food - point) &&
                        RoomObjectPath::clearSegment(furniture, point, food))
                        candidates.push_back(point);
                }
        }
    std::stable_sort(candidates.begin(), candidates.end(), [&](const Ogre::Vector2& a, const Ogre::Vector2& b)
    {
        const float da = a.squaredDistance(food), db = b.squaredDistance(food);
        return da == db ? a.squaredDistance(start) < b.squaredDistance(start) : da < db;
    });
    std::vector<Ogre::Vector2> stagingPoints, standingPoints;
    for(const Ogre::Vector2& point : candidates)
    {
        Ogre::Vector2 direction = food - point;
        if(direction.squaredLength() < 0.000001f)
            direction = Ogre::Vector2(0, -1);
        direction.normalise();
        // The last leg approaches in the orientation that made this standing
        // point usable; arbitrary grid diagonals can otherwise clip a wide body.
        for(float approach = clearance(creature) + 0.5f; approach >= 0.125f; approach *= 0.5f)
        {
            const Ogre::Vector2 staging = point - direction * approach;
            Tile* tile = map.getTile(Helper::round(staging.x), Helper::round(staging.y));
            if(!creature.canGoThroughTile(tile) || !terrainClear(creature, staging, point) ||
                !RoomObjectPath::clearSegment(body, staging, point))
                continue;
            stagingPoints.push_back(staging);
            standingPoints.push_back(point);
            break;
        }
    }
    // These are alternatives for one food target, not independent jobs. Search
    // their shared movement graph once instead of retrying it for every offset.
    size_t chosen = 0;
    const std::function<bool(const Ogre::Vector2&, const Ogre::Vector2&)> terrain = [&](const Ogre::Vector2& from, const Ogre::Vector2& to)
    { return terrainClear(creature, from, to); };
    if(!RoomObjectPath::routeToAnyAligned(start, stagingPoints, body, 0, 0,
        map.getMapSizeX() - 1, map.getMapSizeY() - 1, terrain, path, chosen))
    {
        std::vector<RoomObjectPath::Obstacle> transit = body;
        if(stagingPoints.empty() || !removeLowStepObstacles(creature, transit) ||
            !RoomObjectPath::routeToAnyAligned(start, stagingPoints, transit, 0, 0,
                map.getMapSizeX() - 1, map.getMapSizeY() - 1, terrain, path, chosen))
            return false;
    }
    path.push_back(standingPoints[chosen]);
    return true;
}

bool RoomObjectNavigation::workApproach(Creature& creature, const BuildingObject& object,
    const Ogre::Vector2& wanted, const Ogre::Vector2& facingOffset, std::vector<Ogre::Vector2>& path)
{
    path.clear();
    GameMap& map = *creature.getGameMap();
    const Ogre::Vector2 objectPosition(object.getPosition().x, object.getPosition().y);
    Tile* objectTile = map.getTile(Helper::round(objectPosition.x), Helper::round(objectPosition.y));
    if(objectTile == nullptr)
        return false;
    const Room* room = objectTile->getCoveringRoom();
    const std::vector<RoomObjectPath::Obstacle> body = bodyObstacles(creature);
    const Ogre::Vector2 facing = objectPosition + facingOffset;
    Ogre::Vector2 away = wanted - facing;
    if(away.squaredLength() < 0.000001f)
        away = Ogre::Vector2(0, -1);
    away.normalise();
    const Ogre::Vector2 start(creature.getPosition().x, creature.getPosition().y);
    // Preserve the assigned side of the workstation. Only increase its stand-
    // off enough for this creature; work cannot require standing inside it.
    const int steps = int(std::ceil((2.0f * clearance(creature) + 1.0f) * 8.0f));
    for(int step = 0; step <= steps; ++step)
    {
        const Ogre::Vector2 point = wanted + away * (step * 0.125f);
        Tile* tile = map.getTile(Helper::round(point.x), Helper::round(point.y));
        if(!creature.canGoThroughTile(tile) || tile->getCoveringRoom() != room ||
            !RoomObjectPath::clearPoint(body, point, facing - point))
            continue;
        if(start.squaredDistance(point) < 0.0025f &&
            RoomObjectPath::clearPoint(body, start, facing - start))
            return true;
        for(float distance : {clearance(creature) + 0.5f, 0.5f})
        {
            const Ogre::Vector2 staging = point + away * distance;
            Tile* stagingTile = map.getTile(Helper::round(staging.x), Helper::round(staging.y));
            if(!creature.canGoThroughTile(stagingTile) || !terrainClear(creature, staging, point) ||
                !RoomObjectPath::clearSegment(body, staging, point))
                continue;
            const std::list<Tile*> tiles = map.path(&creature, stagingTile);
            if(tiles.empty())
                continue;
            Creature::tileToVector2(tiles, path, true, 0.0f);
            path.push_back(staging);
            refine(creature, path);
            // The final leg was checked with the actual working direction.
            // Generic endpoint refinement tests eight walking headings and
            // must not displace this precise, already valid interaction point.
            if(!path.empty() && path.back() == staging)
            {
                path.push_back(point);
                return true;
            }
            path.clear();
        }
    }
    return false;
}

bool RoomObjectNavigation::blocked(Creature& creature, const std::vector<Ogre::Vector2>& path,
    bool includeWalkDistortion)
{
    if(path.empty())
        return false;
    const BuildingObject* interaction = interactionObject(creature, path.back());
    std::vector<RoomObjectPath::Obstacle> obstacles = includeWalkDistortion ? collect(*creature.getGameMap(), clearance(creature) + 0.425f, interaction) :
        bodyObstacles(creature, interaction);
    if(!includeWalkDistortion)
        removeLowStepObstacles(creature, obstacles);
    Ogre::Vector2 previous(creature.getPosition().x, creature.getPosition().y);
    bool first = true;
    for(const Ogre::Vector2& point : path)
    {
        if(!RoomObjectPath::clearSegment(obstacles, previous, point, first && !includeWalkDistortion))
            return true;
        first = false;
        previous = point;
    }
    return false;
}
