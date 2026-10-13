#ifndef ROOMOBJECTPATH_H
#define ROOMOBJECTPATH_H

#include <OgreVector2.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_map>
#include <vector>

namespace RoomObjectPath
{
// Furniture bounds are expressed in mesh-local XY, before the placed object's
// rotation. Clearance belongs to the moving creature, not to the mesh asset.
struct Obstacle
{
    Ogre::Vector2 minimum, maximum, position;
    float cosine, sine;
    Ogre::Vector2 bodyMinimum = Ogre::Vector2::ZERO;
    Ogre::Vector2 bodyMaximum = Ogre::Vector2::ZERO;
    Ogre::Vector2 initialHeading = Ogre::Vector2(0, -1);
    // A nonnegative radius is a logical circular footprint with moving-body
    // clearance already included; negative retains the oriented-box geometry.
    float radius = -1.0f;
    Ogre::Vector2 bodyAxisMinimum = Ogre::Vector2::ZERO;
    Ogre::Vector2 bodyAxisMaximum = Ogre::Vector2::ZERO;
    float bodyCosine = 1.0f, bodySine = 0.0f;
    bool hasBodyAxes = false;
    Ogre::Vector2 worldMinimum, worldMaximum;
    bool hasWorldBounds = false;
    float maximumHeight = std::numeric_limits<float>::infinity();

    static Obstacle circle(const Ogre::Vector2& center, float clearanceRadius)
    {
        Obstacle result{Ogre::Vector2(-clearanceRadius), Ogre::Vector2(clearanceRadius), center, 1, 0};
        result.radius = clearanceRadius;
        return result;
    }

    Obstacle forHeading(Ogre::Vector2 direction) const
    {
        Obstacle result = *this;
        if(radius >= 0.0f || (bodyMinimum == Ogre::Vector2::ZERO && bodyMaximum == Ogre::Vector2::ZERO))
            return result;
        if(direction.squaredLength() < 0.000001f)
            direction = initialHeading;
        direction.normalise();
        Ogre::Vector2 low(std::numeric_limits<float>::infinity()), high(-std::numeric_limits<float>::infinity());
        for(float x : {bodyMinimum.x, bodyMaximum.x})
            for(float y : {bodyMinimum.y, bodyMaximum.y})
            {
                // The renderer turns native -Y toward the travel direction.
                const Ogre::Vector2 world(-direction.y * x - direction.x * y,
                    direction.x * x - direction.y * y);
                const Ogre::Vector2 point(cosine * world.x + sine * world.y,
                    -sine * world.x + cosine * world.y);
                low.makeFloor(point);
                high.makeCeil(point);
            }
        result.minimum -= high;
        result.maximum -= low;
        // Both rectangles contribute separating axes. Expanding only in the
        // furniture axes fills empty corners when furniture and body differ
        // in orientation, incorrectly sealing real gaps between rotated props.
        result.bodyCosine = -direction.y;
        result.bodySine = direction.x;
        low = Ogre::Vector2(std::numeric_limits<float>::infinity());
        high = -low;
        for(float x : {minimum.x, maximum.x})
            for(float y : {minimum.y, maximum.y})
            {
                const Ogre::Vector2 world(cosine * x - sine * y, sine * x + cosine * y);
                const Ogre::Vector2 point(result.bodyCosine * world.x + result.bodySine * world.y,
                    -result.bodySine * world.x + result.bodyCosine * world.y);
                low.makeFloor(point);
                high.makeCeil(point);
            }
        result.bodyAxisMinimum = low - bodyMaximum;
        result.bodyAxisMaximum = high - bodyMinimum;
        result.hasBodyAxes = true;
        result.bodyMinimum = result.bodyMaximum = Ogre::Vector2::ZERO;
        const Ogre::Vector2 center = (result.minimum + result.maximum) * 0.5f;
        const Ogre::Vector2 half = (result.maximum - result.minimum) * 0.5f;
        const Ogre::Vector2 worldCenter = position + Ogre::Vector2(cosine * center.x - sine * center.y,
            sine * center.x + cosine * center.y);
        const Ogre::Vector2 extent(std::abs(cosine) * half.x + std::abs(sine) * half.y + 0.00001f,
            std::abs(sine) * half.x + std::abs(cosine) * half.y + 0.00001f);
        result.worldMinimum = worldCenter - extent;
        result.worldMaximum = worldCenter + extent;
        result.hasWorldBounds = true;
        return result;
    }

    Ogre::Vector2 local(const Ogre::Vector2& point) const
    {
        const Ogre::Vector2 relative = point - position;
        return Ogre::Vector2(cosine * relative.x + sine * relative.y,
            -sine * relative.x + cosine * relative.y);
    }

    Ogre::Vector2 bodyLocal(const Ogre::Vector2& point) const
    {
        const Ogre::Vector2 relative = point - position;
        return Ogre::Vector2(bodyCosine * relative.x + bodySine * relative.y,
            -bodySine * relative.x + bodyCosine * relative.y);
    }

    bool contains(const Ogre::Vector2& point, const Ogre::Vector2& heading = Ogre::Vector2::ZERO) const
    {
        if(radius >= 0.0f)
            return point.squaredDistance(position) < radius * radius;
        const Obstacle bounds = forHeading(heading);
        const Ogre::Vector2 p = local(point);
        if(p.x <= bounds.minimum.x || p.x >= bounds.maximum.x ||
           p.y <= bounds.minimum.y || p.y >= bounds.maximum.y)
            return false;
        const Ogre::Vector2 bodyPoint = bounds.bodyLocal(point);
        return !bounds.hasBodyAxes || (bodyPoint.x > bounds.bodyAxisMinimum.x &&
            bodyPoint.x < bounds.bodyAxisMaximum.x && bodyPoint.y > bounds.bodyAxisMinimum.y &&
            bodyPoint.y < bounds.bodyAxisMaximum.y);
    }

    bool intersects(const Ogre::Vector2& from, const Ogre::Vector2& to) const
    {
        // Cached for each grid heading: distant furniture cannot intersect the
        // segment, so reserve the exact oriented-body test for nearby objects.
        if(hasWorldBounds && ((from.x < worldMinimum.x && to.x < worldMinimum.x) ||
            (from.x > worldMaximum.x && to.x > worldMaximum.x) ||
            (from.y < worldMinimum.y && to.y < worldMinimum.y) ||
            (from.y > worldMaximum.y && to.y > worldMaximum.y)))
            return false;
        if(radius >= 0.0f)
        {
            const Ogre::Vector2 delta = to - from;
            const float lengthSquared = delta.squaredLength();
            const float along = lengthSquared > 0.0f ?
                std::max(0.0f, std::min(1.0f, (position - from).dotProduct(delta) / lengthSquared)) : 0.0f;
            return contains(from + delta * along);
        }
        const Obstacle bounds = forHeading(to - from);
        const Ogre::Vector2 localFrom = local(from), localDelta = local(to) - localFrom;
        const Ogre::Vector2 bodyFrom = bounds.bodyLocal(from), bodyDelta = bounds.bodyLocal(to) - bodyFrom;
        const std::array<float, 4> a{{localFrom.x, localFrom.y, bodyFrom.x, bodyFrom.y}};
        const std::array<float, 4> delta{{localDelta.x, localDelta.y, bodyDelta.x, bodyDelta.y}};
        const std::array<float, 4> minimum{{bounds.minimum.x, bounds.minimum.y,
            bounds.bodyAxisMinimum.x, bounds.bodyAxisMinimum.y}};
        const std::array<float, 4> maximum{{bounds.maximum.x, bounds.maximum.y,
            bounds.bodyAxisMaximum.x, bounds.bodyAxisMaximum.y}};
        float enter = 0.0f, leave = 1.0f;
        for(int axis = 0; axis < (bounds.hasBodyAxes ? 4 : 2); ++axis)
        {
            if(std::abs(delta[axis]) < 0.00001f)
            {
                if(a[axis] <= minimum[axis] || a[axis] >= maximum[axis])
                    return false;
                continue;
            }
            float low = (minimum[axis] - a[axis]) / delta[axis];
            float high = (maximum[axis] - a[axis]) / delta[axis];
            if(low > high)
                std::swap(low, high);
            enter = std::max(enter, low);
            leave = std::min(leave, high);
            if(enter >= leave)
                return false;
        }
        return enter < leave;
    }
};

inline bool clearPoint(const std::vector<Obstacle>& obstacles, const Ogre::Vector2& point,
    const Ogre::Vector2& heading = Ogre::Vector2::ZERO)
{
    if(heading == Ogre::Vector2::ZERO)
    {
        for(int y = -1; y <= 1; ++y)
            for(int x = -1; x <= 1; ++x)
                if((x != 0 || y != 0) && clearPoint(obstacles, point, Ogre::Vector2(float(x), float(y))))
                    return true;
        return false;
    }
    for(const Obstacle& obstacle : obstacles)
        if(obstacle.contains(point, heading))
            return false;
    return true;
}

inline bool clearSegment(const std::vector<Obstacle>& obstacles,
    const Ogre::Vector2& from, const Ogre::Vector2& to, bool allowExit = false)
{
    for(const Obstacle& obstacle : obstacles)
    {
        // A creature loaded/dropped into furniture must be able to leave it,
        // but no subsequent waypoint may re-enter that furniture.
        if(allowExit && obstacle.contains(from, obstacle.initialHeading) && !obstacle.contains(to, to - from))
            continue;
        if(obstacle.intersects(from, to))
            return false;
    }
    return true;
}

// The existing map supplies terrain/door checks for an entire segment. Furniture
// is checked analytically, so even thin and rotated objects cannot be skipped.
using TerrainSegment = std::function<bool(const Ogre::Vector2&, const Ogre::Vector2&)>;

inline bool routeToAny(const Ogre::Vector2& start, const std::vector<Ogre::Vector2>& goals,
    const std::vector<Obstacle>& obstacles, int minX, int minY, int maxX, int maxY,
    const TerrainSegment& terrain, std::vector<Ogre::Vector2>& result, size_t& chosenGoal, bool allowStartExit = true,
    const Ogre::Vector2& gridOffset = Ogre::Vector2::ZERO,
    float maximumCost = std::numeric_limits<float>::infinity())
{
    result.clear();
    if(goals.empty())
        return false;
    if(goals.size() == 1 && start.distance(goals.front()) >= maximumCost)
        return false;
    for(size_t i = 0; i < goals.size(); ++i)
    {
        const Ogre::Vector2& goal = goals[i];
        if(start.distance(goal) < maximumCost && clearPoint(obstacles, goal) && terrain(goal, goal) && clearSegment(obstacles, start, goal, allowStartExit) && terrain(start, goal))
        {
            chosenGoal = i;
            result.push_back(goal);
            return true;
        }
    }

    if(goals.size() == 1 && std::isfinite(maximumCost))
    {
        // Any improving route lies inside the start/goal distance ellipse.
        // Cropping its bounding box avoids allocating a whole-map grid for
        // every short lane comparison without imposing a search budget.
        const Ogre::Vector2 center = (start + goals.front()) * 0.5f;
        const Ogre::Vector2 delta = goals.front() - start;
        const float halfX = 0.5f * std::sqrt(std::max(0.0f, maximumCost * maximumCost - delta.y * delta.y));
        const float halfY = 0.5f * std::sqrt(std::max(0.0f, maximumCost * maximumCost - delta.x * delta.x));
        minX = std::max(minX, int(std::floor(center.x - halfX - gridOffset.x)));
        minY = std::max(minY, int(std::floor(center.y - halfY - gridOffset.y)));
        maxX = std::min(maxX, int(std::ceil(center.x + halfX - gridOffset.x)));
        maxY = std::min(maxY, int(std::ceil(center.y + halfY - gridOffset.y)));
    }
    // Quarter-tile nodes refine the existing tile path only where furniture
    // obstructs it; endpoints retain the precise room-interaction offsets.
    const int width = (maxX - minX) * 4 + 1;
    const int height = (maxY - minY) * 4 + 1;
    if(width <= 0 || height <= 0)
        return false;
    struct SearchNode
    {
        float cost = std::numeric_limits<float>::infinity();
        float forwardCost = std::numeric_limits<float>::infinity();
        int parent = -1, forwardParent = -1;
        size_t terminal = 0;
    };
    // A local or enclosed approach visits only a small fraction of a large map.
    // Store visited nodes rather than initialize whole-map arrays per lane.
    std::unordered_map<int, SearchNode> nodes;
    using Entry = std::pair<float, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> forwardOpen;
    const std::function<Ogre::Vector2(int)> position = [&](int index)
    {
        return Ogre::Vector2(minX + (index % width) * 0.25f,
            minY + (index / width) * 0.25f) + gridOffset;
    };
    // Opposite consistent potentials keep the two A* frontiers comparable;
    // their sum is a lower bound on the remaining complete route cost.
    const std::function<float(int)> potential = [&](int index)
    { return goals.size() == 1 ? 0.5f * (position(index).distance(goals.front()) - position(index).distance(start)) : 0.0f; };
    const std::function<void(int, float, int)> add = [&](int index, float cost, int parent)
    {
        if(cost >= nodes[index].cost)
            return;
        nodes[index].cost = cost;
        nodes[index].parent = parent;
        open.emplace(cost - potential(index), index);
    };

    // Usually only the surrounding grid nodes are needed. If old save data or
    // newly placed furniture overlaps the start, connect an outward exit first.
    const Ogre::Vector2 initialHeading = obstacles.empty() ? Ogre::Vector2(0, -1) : obstacles.front().initialHeading;
    const float reach = !allowStartExit || clearPoint(obstacles, start, initialHeading) ? 0.4f : 3.0f;
    const int firstX = std::max(0, int(std::floor((start.x - reach - minX - gridOffset.x) * 4)));
    const int lastX = std::min(width - 1, int(std::ceil((start.x + reach - minX - gridOffset.x) * 4)));
    const int firstY = std::max(0, int(std::floor((start.y - reach - minY - gridOffset.y) * 4)));
    const int lastY = std::min(height - 1, int(std::ceil((start.y + reach - minY - gridOffset.y) * 4)));
    for(int y = firstY; y <= lastY; ++y)
        for(int x = firstX; x <= lastX; ++x)
        {
            const int index = y * width + x;
            const Ogre::Vector2 point = position(index);
            if(clearPoint(obstacles, point) && clearSegment(obstacles, start, point, allowStartExit) && terrain(start, point))
            {
                nodes[index].forwardCost = start.distance(point);
                forwardOpen.emplace(nodes[index].forwardCost + potential(index), index);
            }
        }
    if(forwardOpen.empty())
        return false;
    // Connect both endpoints: either may be isolated by furniture, independently
    // of the terrain-only coarse route. Exhausting either frontier proves failure.
    for(size_t i = 0; i < goals.size(); ++i)
    {
        const Ogre::Vector2& goal = goals[i];
        if(!clearPoint(obstacles, goal) || !terrain(goal, goal))
            continue;
        const int goalFirstX = std::max(0, int(std::floor((goal.x - 0.5f - minX - gridOffset.x) * 4)));
        const int goalLastX = std::min(width - 1, int(std::ceil((goal.x + 0.5f - minX - gridOffset.x) * 4)));
        const int goalFirstY = std::max(0, int(std::floor((goal.y - 0.5f - minY - gridOffset.y) * 4)));
        const int goalLastY = std::min(height - 1, int(std::ceil((goal.y + 0.5f - minY - gridOffset.y) * 4)));
        for(int y = goalFirstY; y <= goalLastY; ++y)
            for(int x = goalFirstX; x <= goalLastX; ++x)
            {
                const int index = y * width + x;
                const Ogre::Vector2 point = position(index);
                if(point.distance(goal) < nodes[index].cost && point.squaredDistance(goal) <= 0.25f && clearSegment(obstacles, point, goal) && terrain(point, goal))
                {
                    nodes[index].terminal = i;
                    add(index, point.distance(goal), -1);
                }
            }
    }

    if(open.empty())
        return false;
    // A grid edge has only eight headings; project a creature's walking body
    // once per heading, not once per obstacle for every expanded search node.
    std::array<std::vector<Obstacle>, 9> edgeObstacles;
    for(int dy = -1; dy <= 1; ++dy)
        for(int dx = -1; dx <= 1; ++dx)
        {
            if(dx == 0 && dy == 0)
                continue;
            std::vector<Obstacle>& edge = edgeObstacles[(dy + 1) * 3 + dx + 1];
            edge.reserve(obstacles.size());
            for(const Obstacle& obstacle : obstacles)
                edge.push_back(obstacle.forHeading(Ogre::Vector2(float(-dx), float(-dy))));
        }
    int destination = -1;
    float best = maximumCost;
    bool forward = false;
    while(!open.empty() && !forwardOpen.empty())
    {
        while(!open.empty() && open.top().first > nodes[open.top().second].cost - potential(open.top().second) + 0.0001f)
            open.pop();
        while(!forwardOpen.empty() && forwardOpen.top().first > nodes[forwardOpen.top().second].forwardCost + potential(forwardOpen.top().second) + 0.0001f)
            forwardOpen.pop();
        if(open.empty() || forwardOpen.empty() ||
            open.top().first + forwardOpen.top().first >= best)
            break;
        forward = !forward;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>>& frontier = forward ? forwardOpen : open;
        float SearchNode::* const distance = forward ? &SearchNode::forwardCost : &SearchNode::cost;
        int SearchNode::* const link = forward ? &SearchNode::forwardParent : &SearchNode::parent;
        const Entry entry = frontier.top();
        frontier.pop();
        const int current = entry.second;
        const Ogre::Vector2 point = position(current);
        const int x = current % width, y = current / width;
        if(nodes[current].forwardCost + nodes[current].cost < best)
        {
            destination = current;
            best = nodes[current].forwardCost + nodes[current].cost;
        }
        for(int dy = -1; dy <= 1; ++dy)
            for(int dx = -1; dx <= 1; ++dx)
            {
                if((dx == 0 && dy == 0) || x + dx < 0 || x + dx >= width || y + dy < 0 || y + dy >= height)
                    continue;
                const int next = (y + dy) * width + x + dx;
                const Ogre::Vector2 target = position(next);
                const float nextCost = nodes[current].*distance + point.distance(target);
                if(nextCost >= nodes[next].*distance)
                    continue;
                const Ogre::Vector2 from = forward ? point : target;
                const Ogre::Vector2 to = forward ? target : point;
                const int heading = forward ? (1 - dy) * 3 + 1 - dx : (dy + 1) * 3 + dx + 1;
                if(!clearSegment(edgeObstacles[heading], from, to) || !terrain(from, to))
                    continue;
                nodes[next].*distance = nextCost;
                nodes[next].*link = current;
                frontier.emplace(nextCost + (forward ? potential(next) : -potential(next)), next);
                if(nodes[next].forwardCost + nodes[next].cost < best)
                {
                    destination = next;
                    best = nodes[next].forwardCost + nodes[next].cost;
                }
            }
    }
    if(destination < 0)
        return false;
    for(int index = destination; index >= 0; index = nodes[index].forwardParent)
        result.push_back(position(index));
    std::reverse(result.begin(), result.end());
    for(int index = nodes[destination].parent; index >= 0; index = nodes[index].parent)
        result.push_back(position(index));
    int terminal = destination;
    while(nodes[terminal].parent >= 0)
        terminal = nodes[terminal].parent;
    chosenGoal = nodes[terminal].terminal;
    result.push_back(goals[chosenGoal]);

    // Collapse only collinear runs: a visual/client interpolation must not cut a
    // corner through an obstacle or silently bypass an existing terrain rule.
    for(size_t i = 1; i + 1 < result.size();)
    {
        const Ogre::Vector2 a = result[i] - result[i - 1];
        const Ogre::Vector2 b = result[i + 1] - result[i];
        if(std::abs(a.crossProduct(b)) < 0.00001f && a.dotProduct(b) >= 0.0f)
            result.erase(result.begin() + i);
        else
            ++i;
    }
    return true;
}

inline bool routeToAnyAligned(const Ogre::Vector2& start, const std::vector<Ogre::Vector2>& goals,
    const std::vector<Obstacle>& obstacles, int minX, int minY, int maxX, int maxY,
    const TerrainSegment& terrain, std::vector<Ogre::Vector2>& result, size_t& chosenGoal, bool allowStartExit = true)
{
    bool found = routeToAny(start, goals, obstacles, minX, minY, maxX, maxY,
        terrain, result, chosenGoal, allowStartExit);
    if(found && result.size() == 1)
        return true;
    // Mesh roots are not necessarily centered on the body or furniture. A valid
    // narrow lane can fall between every root-aligned quarter-tile node even
    // though a body-centered lane fits. Keep the same exact edge/terrain checks.
    if(obstacles.empty() || goals.empty())
        return found;
    const Ogre::Vector2 goal = found ? goals[chosenGoal] : *std::min_element(goals.begin(), goals.end(),
        [&](const Ogre::Vector2& a, const Ogre::Vector2& b)
        { return start.squaredDistance(a) < start.squaredDistance(b); });
    const std::function<float(const std::vector<Ogre::Vector2>&)> length = [&](const std::vector<Ogre::Vector2>& path)
    {
        float cost = 0;
        Ogre::Vector2 previous = start;
        for(const Ogre::Vector2& point : path)
        {
            cost += previous.distance(point);
            previous = point;
        }
        return cost;
    };
    float bestCost = found ? length(result) : std::numeric_limits<float>::infinity();
    // Once a shared search selects a safe interaction endpoint, compare lane
    // alternatives to that endpoint with A*, not repeated multi-goal Dijkstra.
    Ogre::Vector2 center = (obstacles.front().bodyMinimum + obstacles.front().bodyMaximum) * 0.5f;
    Ogre::Vector2 furnitureCenter = Ogre::Vector2::ZERO;
    float nearest = std::numeric_limits<float>::infinity();
    for(const Obstacle& obstacle : obstacles)
    {
        if(!obstacle.intersects(start, goal) || obstacle.position.squaredDistance(start) >= nearest)
            continue;
        nearest = obstacle.position.squaredDistance(start);
        center = (obstacle.bodyMinimum + obstacle.bodyMaximum) * 0.5f;
        const Ogre::Vector2 localCenter = (obstacle.minimum + obstacle.maximum) * 0.5f;
        furnitureCenter = obstacle.position + Ogre::Vector2(
            localCenter.x * obstacle.cosine - localCenter.y * obstacle.sine,
            localCenter.x * obstacle.sine + localCenter.y * obstacle.cosine);
    }
    furnitureCenter.x -= std::round(furnitureCenter.x * 4.0f) * 0.25f;
    furnitureCenter.y -= std::round(furnitureCenter.y * 4.0f) * 0.25f;
    if(center.squaredLength() < 0.000001f && furnitureCenter.squaredLength() < 0.000001f)
        return found;
    for(int y = -1; y <= 1; ++y)
        for(int x = -1; x <= 1; ++x)
        {
            if(x == 0 && y == 0)
                continue;
            const Ogre::Vector2 heading = Ogre::Vector2(float(x), float(y)).normalisedCopy();
            const Ogre::Vector2 offset = furnitureCenter + Ogre::Vector2(center.x * heading.y + center.y * heading.x,
                -center.x * heading.x + center.y * heading.y);
            std::vector<Ogre::Vector2> candidate;
            // A valid outside route must not suppress a shorter usable lane.
            // Bound each alternative by the best complete route already found.
            size_t candidateGoal = 0;
            const bool selectedEndpoint = found && goals.size() > 1;
            const size_t selectedGoal = chosenGoal;
            const std::vector<Ogre::Vector2> targets = selectedEndpoint ? std::vector<Ogre::Vector2>{goals[chosenGoal]} : goals;
            if(routeToAny(start, targets, obstacles, minX, minY, maxX, maxY,
                terrain, candidate, candidateGoal, allowStartExit, offset, bestCost))
            {
                bestCost = length(candidate);
                result.swap(candidate);
                chosenGoal = selectedEndpoint ? selectedGoal : candidateGoal;
                found = true;
            }
        }
    return found;
}

inline bool route(const Ogre::Vector2& start, const Ogre::Vector2& goal,
    const std::vector<Obstacle>& obstacles, int minX, int minY, int maxX, int maxY,
    const TerrainSegment& terrain, std::vector<Ogre::Vector2>& result, bool allowStartExit = true)
{
    size_t chosenGoal = 0;
    return routeToAnyAligned(start, {goal}, obstacles, minX, minY, maxX, maxY,
        terrain, result, chosenGoal, allowStartExit);
}
}

#endif
