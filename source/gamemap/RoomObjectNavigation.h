#ifndef ROOMOBJECTNAVIGATION_H
#define ROOMOBJECTNAVIGATION_H

#include "gamemap/RoomObjectPath.h"

class GameMap;
class Creature;
class BuildingObject;

namespace RoomObjectNavigation
{
// Returns the radius of the circle that a creature needs around itself, including its level.
float clearance(const Creature& creature);
// Returns the furniture of all rooms as obstacles, widened by the clearance. The
// interaction object, which a creature may enter, and walkable landmarks are left out.
std::vector<RoomObjectPath::Obstacle> collect(GameMap& map, float clearance,
    const BuildingObject* interaction = nullptr);
// Like collect(), but with the real walking body of the creature instead of a plain clearance.
std::vector<RoomObjectPath::Obstacle> bodyObstacles(Creature& creature,
    const BuildingObject* interaction = nullptr);
// Finds the point nearest to the wanted one where nothing blocks a creature; returns
// false if there is none in the tile.
bool standingPosition(const std::vector<RoomObjectPath::Obstacle>& obstacles,
    const Ogre::Vector2& wanted, Ogre::Vector2& result);
// Searches a route to a point from which the creature can reach the food without
// furniture in between. The last point of the path is the position to eat at.
bool foodApproach(Creature& creature, const Ogre::Vector2& food, std::vector<Ogre::Vector2>& path);
// Searches a route to the wanted working position at the object, facing it with the
// facing offset. An empty path means that the creature already stands there.
bool workApproach(Creature& creature, const BuildingObject& object, const Ogre::Vector2& wanted,
    const Ogre::Vector2& facingOffset, std::vector<Ogre::Vector2>& path);
// Replaces the path by one that leads around furniture, if the path crosses any. Returns
// true if the path was changed or removed (no route) and false if it can be walked as it is.
bool refine(Creature& creature, std::vector<Ogre::Vector2>& path);
// Tells whether the path crosses furniture. With includeWalkDistortion, the client jitter
// of the walk is taken into account as well.
bool blocked(Creature& creature, const std::vector<Ogre::Vector2>& path, bool includeWalkDistortion = false);
}

#endif
