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

#ifndef MISSILEBLAST_H
#define MISSILEBLAST_H

#include "entities/MissileOneHit.h"

#include <string>
#include <iosfwd>

class GameMap;
class Tile;
class ODPacket;

//! \brief A missile that can follow its target (guided bolt) and/or explode when it hits something, hurting every
//! enemy creature within the blast radius (grenade). With a blast radius of 0 it behaves like a MissileOneHit.
class MissileBlast: public MissileOneHit
{
public:
    MissileBlast(GameMap* gameMap, Seat* seat, const std::string& senderName, const std::string& meshName,
        const std::string& particleScript, const Ogre::Vector3& direction, double speed, double physicalDamage, double magicalDamage,
        double elementDamage, GameEntity* entityTarget, bool damageAllies, bool koEnemyCreature, bool notifyPlayerIfHit,
        double blastRadius, bool homing);
    MissileBlast(GameMap* gameMap);

    virtual MissileObjectType getMissileType() const override
    { return MissileObjectType::blast; }

    virtual bool wallHitNextDirection(const Ogre::Vector3& actDirection, Tile* tile, Ogre::Vector3& nextDirection) override;

    virtual bool hitCreature(Tile* tile, GameEntity* entity) override;

    virtual void hitTargetEntity(Tile* tile, GameEntity* entityTarget) override;

    static MissileBlast* getMissileBlastFromStream(GameMap* gameMap, std::istream& is);
    static MissileBlast* getMissileBlastFromPacket(GameMap* gameMap, ODPacket& is);

protected:
    virtual void updateDirection() override;

    void exportToStream(std::ostream& os) const override;
    bool importFromStream(std::istream& is) override;

private:
    //! \brief Hurts every enemy creature within the blast radius around the given tile
    void explode(Tile* tile);

    double mBlastRadius;
    bool mHoming;
};

#endif // MISSILEBLAST_H
