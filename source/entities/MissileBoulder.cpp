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

#include "entities/MissileBoulder.h"

#include "entities/Tile.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "gamemap/LevelScript.h"
#include "network/ODPacket.h"
#include "utils/LogManager.h"
#include "utils/Random.h"

#include <iostream>

MissileBoulder::MissileBoulder(GameMap* gameMap, Seat* seat, const std::string& senderName, const std::string& meshName,
        const Ogre::Vector3& direction, double speed, double damage, GameEntity* entityTarget, bool notifyPlayerIfHit) :
    MissileObject(gameMap, seat, senderName, meshName, direction, speed, entityTarget, true, false),
    mDamage(damage),
    mNbHits(0),
    mNotifyPlayerIfHit(notifyPlayerIfHit)
{
}

MissileBoulder::MissileBoulder(GameMap* gameMap) :
    MissileObject(gameMap),
    mDamage(0.0),
    mNbHits(0),
    mNotifyPlayerIfHit(false)
{
}

//! A golf ball leaves the hand with this speed (tiles per turn) and loses this share of it each turn
static const double GOLF_START_SPEED = 1.5;
static const double GOLF_FRICTION = 0.85;
static const double GOLF_MIN_SPEED = 0.15;

MissileBoulder* MissileBoulder::createGolfBall(GameMap* gameMap, Seat* seat, const std::string& name)
{
    MissileBoulder* ball = new MissileBoulder(gameMap, seat, name, "Boulder", Ogre::Vector3::ZERO, 0.0, -1.0, nullptr, false);
    // The ball lies still until it is slapped
    ball->stopMissile();
    return ball;
}

bool MissileBoulder::isResting()
{
    return isGolfBall() && !isMoving();
}

bool MissileBoulder::canSlap(Seat* seat)
{
    if(!getIsOnMap() || (seat != getSeat()))
        return false;

    // The client does not know the damage that marks a golf ball: the server decides
    if(!getIsOnServerMap())
        return !isMoving();

    if(!isResting())
        return false;

    Tile* tile = getPositionTile();
    return (tile != nullptr) && !getGameMap()->getLevelScript().isBoulderHole(tile->getX(), tile->getY());
}

void MissileBoulder::slap()
{
    // Without the place of the hand the ball rolls to the east
    slapFrom(getPosition().x - 1.0f, getPosition().y);
}

void MissileBoulder::slapFrom(float fromX, float fromY)
{
    Ogre::Vector3 direction(getPosition().x - fromX, getPosition().y - fromY, 0.0f);
    if(direction.length() < 0.01f)
        direction = Ogre::Vector3(1.0f, 0.0f, 0.0f);

    direction.normalise();
    launch(direction, GOLF_START_SPEED);
}

bool MissileBoulder::stopsOnTile(Tile* tile)
{
    return isGolfBall() && getGameMap()->getLevelScript().isBoulderHole(tile->getX(), tile->getY());
}

void MissileBoulder::updateDirection()
{
    if(!isGolfBall())
        return;

    double speed = getMoveSpeed() * GOLF_FRICTION;
    if(speed < GOLF_MIN_SPEED)
    {
        setSpeed(0.0);
        stopMissile();
        return;
    }

    setSpeed(speed);
}

bool MissileBoulder::hitCreature(Tile* tile, GameEntity* entity)
{
    // A golf ball does not hurt
    if(isGolfBall())
        return true;

    entity->takeDamage(this, 0.0, mDamage, 0.0, 0.0, tile, false);
    if(mNotifyPlayerIfHit)
        entity->notifyFightPlayer(tile);

    ++mNbHits;
    if(Random::Uint(0, 10 - mNbHits) <= 0)
        return false;

    return true;
}

bool MissileBoulder::wallHitNextDirection(const Ogre::Vector3& actDirection, Tile* tile, Ogre::Vector3& nextDirection)
{
    // A golf ball stops at the wall
    if(isGolfBall())
        return false;

    // When we hit a wall, we might break
    if(Random::Uint(1, 2) == 1)
        return false;

    if(Random::Uint(1, 2) == 1)
    {
        nextDirection.x = actDirection.y;
        nextDirection.y = actDirection.x;
        nextDirection.z = actDirection.z;
    }
    else
    {
        nextDirection.x = -actDirection.y;
        nextDirection.y = -actDirection.x;
        nextDirection.z = actDirection.z;
    }
    return true;
}

MissileBoulder* MissileBoulder::getMissileBoulderFromStream(GameMap* gameMap, std::istream& is)
{
    MissileBoulder* obj = new MissileBoulder(gameMap);
    obj->importFromStream(is);
    return obj;
}

MissileBoulder* MissileBoulder::getMissileBoulderFromPacket(GameMap* gameMap, ODPacket& is)
{
    MissileBoulder* obj = new MissileBoulder(gameMap);
    obj->importFromPacket(is);
    return obj;
}

void MissileBoulder::exportToStream(std::ostream& os) const
{
    MissileObject::exportToStream(os);
    os << mDamage << "\t";
    os << mNbHits << "\t";
}

bool MissileBoulder::importFromStream(std::istream& is)
{
    if(!MissileObject::importFromStream(is))
        return false;
    if(!(is >> mDamage))
        return false;
    if(!(is >> mNbHits))
        return false;

    return true;
}
