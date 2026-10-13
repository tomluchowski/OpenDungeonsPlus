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

#ifndef MISSILEBOULDER_H
#define MISSILEBOULDER_H

#include "entities/MissileObject.h"

#include <string>
#include <iosfwd>

class Creature;
class Room;
class GameMap;
class Tile;
class ODPacket;

class MissileBoulder: public MissileObject
{
public:
    MissileBoulder(GameMap* gameMap, Seat* seat, const std::string& senderName, const std::string& meshName,
        const Ogre::Vector3& direction, double speed, double damage, GameEntity* entityTarget,
        bool notifyPlayerIfHit);
    MissileBoulder(GameMap* gameMap);

    virtual MissileObjectType getMissileType() const override
    { return MissileObjectType::boulder; }

    virtual bool hitCreature(Tile* tile, GameEntity* entity) override;
    virtual bool wallHitNextDirection(const Ogre::Vector3& actDirection, Tile* tile, Ogre::Vector3& nextDirection) override;

    //! \brief A golf ball is a boulder that the keeper slaps: it rolls, slows down and stays where it stops.
    //! It does not hurt anybody and falls into the holes of the level script. A golf ball has a negative damage.
    bool isGolfBall() const
    { return mDamage < 0.0; }

    //! \brief Creates a golf ball (not added to the map yet)
    static MissileBoulder* createGolfBall(GameMap* gameMap, Seat* seat, const std::string& name);

    //! \brief The ball lies still and can be slapped
    bool isResting();

    //! \brief Slaps the ball away from the point (fromX, fromY) in tile coordinates
    void slapFrom(float fromX, float fromY);

    //! \brief Only a golf ball of the seat that lies still outside of a hole can be slapped (the client only checks
    //! that it does not move)
    virtual bool canSlap(Seat* seat) override;
    //! \brief Slaps the ball without the place of the hand: it rolls to the east
    virtual void slap() override;
    //! \brief A golf ball stops in the holes of the level script
    virtual bool stopsOnTile(Tile* tile) override;
    //! \brief A golf ball stays on the map when it stopped, so that it can be slapped again
    virtual bool staysWhenStopped() const override
    { return isGolfBall(); }

    static MissileBoulder* getMissileBoulderFromStream(GameMap* gameMap, std::istream& is);
    static MissileBoulder* getMissileBoulderFromPacket(GameMap* gameMap, ODPacket& is);
protected:
    //! \brief A golf ball slows down every turn until it stops
    virtual void updateDirection() override;
    void exportToStream(std::ostream& os) const override;
    bool importFromStream(std::istream& is) override;

private:
    double mDamage;
    int mNbHits;
    bool mNotifyPlayerIfHit;
};

#endif // MISSILEBOULDER_H
