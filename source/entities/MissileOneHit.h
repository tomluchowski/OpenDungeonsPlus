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

#ifndef MISSILEONEHIT_H
#define MISSILEONEHIT_H

#include "entities/MissileObject.h"

#include <string>
#include <iosfwd>

class Creature;
class Room;
class GameMap;
class Tile;
class ODPacket;

class MissileOneHit: public MissileObject
{
public:
    MissileOneHit(GameMap* gameMap, Seat* seat, const std::string& senderName, const std::string& meshName,
        const std::string& particleScript, const Ogre::Vector3& direction, double speed, double physicalDamage, double magicalDamage,
        double elementDamage, GameEntity* entityTarget, bool damageAllies, bool koEnemyCreature, bool notifyPlayerIfHit);
    MissileOneHit(GameMap* gameMap);

    virtual MissileObjectType getMissileType() const override
    { return MissileObjectType::oneHit; }

    virtual bool hitCreature(Tile* tile, GameEntity* entity) override;

    virtual void hitTargetEntity(Tile* tile, GameEntity* entityTarget) override;

    //! \brief Server side, cosmetic only. The missile was shot by this creature at that creature: the clients are
    //! told what the shot did (event hitResult). Not saved. Without it the missile reports nothing.
    void setShooter(const std::string& shooterName, const std::string& targetName)
    {
        mShooterName = shooterName;
        mTargetName = targetName;
    }

    static MissileOneHit* getMissileOneHitFromStream(GameMap* gameMap, std::istream& is);
    static MissileOneHit* getMissileOneHitFromPacket(GameMap* gameMap, ODPacket& is);
protected:
    void exportToStream(std::ostream& os) const override;
    bool importFromStream(std::istream& is) override;
    //! \brief Called when the shot ended: a shot at a creature that hurt nothing is reported as missed (cosmetic only)
    virtual void missileStopped() override;

    //! \brief Deals the damage of the missile and, for a shot with a shooter, tells the clients what it did
    void hurt(Tile* tile, GameEntity* entity);

private:
    //! Cosmetic only (event hitResult), not saved: who shot, at whom, and whether the shot hurt a creature
    std::string mShooterName;
    std::string mTargetName;
    bool mHasHit;
    double mPhysicalDamage;
    double mMagicalDamage;
    double mElementDamage;
    bool mNotifyPlayerIfHit;
};

#endif // MISSILEONEHIT_H
