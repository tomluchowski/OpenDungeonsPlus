#ifndef ROOMOBJECTNAVIGATION_H
#define ROOMOBJECTNAVIGATION_H

#include "gamemap/RoomObjectPath.h"

class GameMap;
class Creature;
class BuildingObject;

namespace RoomObjectNavigation
{
float clearance(const Creature& creature);
std::vector<RoomObjectPath::Obstacle> collect(GameMap& map, float clearance,
    const BuildingObject* interaction = nullptr);
std::vector<RoomObjectPath::Obstacle> bodyObstacles(Creature& creature,
    const BuildingObject* interaction = nullptr);
bool standingPosition(const std::vector<RoomObjectPath::Obstacle>& obstacles,
    const Ogre::Vector2& wanted, Ogre::Vector2& result);
bool foodApproach(Creature& creature, const Ogre::Vector2& food, std::vector<Ogre::Vector2>& path);
bool workApproach(Creature& creature, const BuildingObject& object, const Ogre::Vector2& wanted,
    const Ogre::Vector2& facingOffset, std::vector<Ogre::Vector2>& path);
bool refine(Creature& creature, std::vector<Ogre::Vector2>& path);
bool blocked(Creature& creature, const std::vector<Ogre::Vector2>& path, bool includeWalkDistortion = false);
}

#endif
