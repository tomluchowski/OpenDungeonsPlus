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
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include <functional>

namespace
{
// Samples per tile when a straight line is tested against the terrain.
const float TERRAIN_SAMPLES_PER_TILE = 16.0f;
// Each client jitter coordinate is +/-0.3, so a rotated object needs the full diagonal
// displacement (0.3 * sqrt(2), rounded up) as well as the creature's walking radius.
const float WALK_DISTORTION_REACH = 0.425f;
// Free standing points are searched on a grid of STANDING_GRID_SIZE x STANDING_GRID_SIZE
// points per tile; the points are STANDING_GRID_STEP apart and centred on the tile.
const int STANDING_GRID_SIZE = 8;
const float STANDING_GRID_STEP = 0.125f;
const float STANDING_GRID_START = -0.4375f;
// Walking radius of a creature whose mesh is not in the catalog, before it is scaled by its level.
const float DEFAULT_WALKING_RADIUS = 0.2f;
// Squared distance below which a creature already stands at the wanted work position.
const float WORK_POSITION_TOLERANCE = 0.0025f;

// Returns the point of the standing grid in the tile centred on center.
Ogre::Vector2 standingGridPoint(const Ogre::Vector2& center, int x, int y)
{
    return center + Ogre::Vector2(STANDING_GRID_START + x * STANDING_GRID_STEP,
        STANDING_GRID_START + y * STANDING_GRID_STEP);
}

// Returns the biggest distance of a corner of the box from its origin.
float farthestCorner(const Ogre::Vector2& low, const Ogre::Vector2& high)
{
    return std::hypot(std::max(std::abs(low.x), std::abs(high.x)),
        std::max(std::abs(low.y), std::abs(high.y)));
}

// Returns the furniture of the room that a creature interacts with at the goal (its
// own bed, the portal it leaves through, the torture device it uses), or null. That
// furniture must not block the creature while it does so.
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
    const int steps = std::max(1, int(std::ceil(from.distance(to) * TERRAIN_SAMPLES_PER_TILE)));
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

// Removes the obstacles that the creature can step onto and returns whether there were any.
bool removeLowStepObstacles(Creature& creature, std::vector<RoomObjectPath::Obstacle>& obstacles)
{
    const size_t count = obstacles.size();
    obstacles.erase(std::remove_if(obstacles.begin(), obstacles.end(), [&](RoomObjectPath::Obstacle obstacle)
    {
        return RoomObjectPath::prepareLowStep(obstacle, creature.getMeshName(),
            RoomObjectPath::creatureScale(creature.getLevel()), creature.getPosition().z,
            ConfigManager::getSingleton().getBedStepMaxHeight()) > 0.0f;
    }), obstacles.end());
    return obstacles.size() != count;
}
}

float RoomObjectNavigation::clearance(const Creature& creature)
{
    const float scale = RoomObjectPath::creatureScale(creature.getLevel());
    for(const RoomObjectPath::WalkingRadius& model : RoomObjectPath::walkingRadii)
        if(creature.getMeshName() == model.name)
            return model.radius * scale;
    return DEFAULT_WALKING_RADIUS * scale;
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
            const float angle = float(object->getRotationAngle()) * RoomObjectPath::degreesToRadians;
            const RoomObjectPath::FurnitureScale scale = RoomObjectPath::placedFurnitureScale(bounds,
                object->getFurnitureScale());
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
    Ogre::Vector2 minimum(-DEFAULT_WALKING_RADIUS), maximum(DEFAULT_WALKING_RADIUS);
    for(const RoomObjectPath::WalkingRadius& model : RoomObjectPath::walkingRadii)
        if(creature.getMeshName() == model.name)
        {
            minimum = Ogre::Vector2(model.minX, model.minY);
            maximum = Ogre::Vector2(model.maxX, model.maxY);
            break;
        }
    const float scale = RoomObjectPath::creatureScale(creature.getLevel());
    Ogre::Vector2 heading(creature.getWalkDirection().x, creature.getWalkDirection().y);
    if(heading.squaredLength() < 0.000001f)
        heading = Ogre::Vector2(0, -1);
    for(std::vector<RoomObjectPath::Obstacle>::iterator it = result.begin(); it != result.end();)
    {
        RoomObjectPath::Obstacle& obstacle = *it;
        obstacle.bodyMinimum = minimum * scale;
        obstacle.bodyMaximum = maximum * scale;
        // Body parts above the complete object cannot collide with it, so only the
        // body below its top counts.
        const RoomObjectPath::BodyBand* band = RoomObjectPath::bodyBand(creature.getMeshName(),
            obstacle.maximumHeight - creature.getPosition().z);
        if(band != nullptr)
        {
            if(band->empty)
            {
                it = result.erase(it);
                continue;
            }
            const float margin = RoomObjectPath::lowWalkingMargin;
            obstacle.bodyMinimum = Ogre::Vector2(band->minX - margin, band->minY - margin) * scale;
            obstacle.bodyMaximum = Ogre::Vector2(band->maxX + margin, band->maxY + margin) * scale;
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
    for(int y = 0; y < STANDING_GRID_SIZE; ++y)
        for(int x = 0; x < STANDING_GRID_SIZE; ++x)
        {
            const Ogre::Vector2 point = standingGridPoint(center, x, y);
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
    std::vector<RoomObjectPath::Obstacle> obstacles = collect(map, radius + WALK_DISTORTION_REACH, interaction);
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
    bool found = findRoute(obstacles, result);
    if(found && result.size() == 1)
    {
        // A clear straight route cannot be improved by adding a step.
        path.swap(result);
        return true;
    }
    std::vector<RoomObjectPath::Obstacle> solid, steps;
    std::vector<float> rises;
    const float bestCost = found ? RoomObjectPath::pathLength(start, result) : std::numeric_limits<float>::infinity();
    for(RoomObjectPath::Obstacle obstacle : obstacles)
    {
        const float rise = RoomObjectPath::prepareLowStep(obstacle, creature.getMeshName(),
            RoomObjectPath::creatureScale(creature.getLevel()), creature.getPosition().z,
            ConfigManager::getSingleton().getBedStepMaxHeight());
        if(rise <= 0.0f)
        {
            solid.push_back(obstacle);
            continue;
        }
        // Every crossing must reach the expanded nest and pay its rise/fall.
        // Distant nests whose lower bound cannot beat the existing route stay
        // solid instead of causing another whole-map alternative search.
        const float reach = farthestCorner(obstacle.minimum, obstacle.maximum) +
            farthestCorner(obstacle.bodyMinimum, obstacle.bodyMaximum);
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
        float cost = RoomObjectPath::pathLength(start, crossing);
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
        if(!found || cost < RoomObjectPath::pathLength(start, result))
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
            for(int y = 0; y < STANDING_GRID_SIZE; ++y)
                for(int x = 0; x < STANDING_GRID_SIZE; ++x)
                {
                    const Ogre::Vector2 point = standingGridPoint(Ogre::Vector2(float(foodX + dx), float(foodY + dy)), x, y);
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
    const int steps = int(std::ceil((2.0f * clearance(creature) + 1.0f) / STANDING_GRID_STEP));
    for(int step = 0; step <= steps; ++step)
    {
        const Ogre::Vector2 point = wanted + away * (step * STANDING_GRID_STEP);
        Tile* tile = map.getTile(Helper::round(point.x), Helper::round(point.y));
        if(!creature.canGoThroughTile(tile) || tile->getCoveringRoom() != room ||
            !RoomObjectPath::clearPoint(body, point, facing - point))
            continue;
        if(start.squaredDistance(point) < WORK_POSITION_TOLERANCE &&
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
    std::vector<RoomObjectPath::Obstacle> obstacles = includeWalkDistortion ? collect(*creature.getGameMap(), clearance(creature) + WALK_DISTORTION_REACH, interaction) :
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
