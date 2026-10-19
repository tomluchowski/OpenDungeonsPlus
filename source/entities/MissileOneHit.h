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
    //! \brief Creates a projectile and records the creature shooter's launch accuracy when available.
    MissileOneHit(GameMap* gameMap, Seat* seat, const std::string& senderName, const std::string& meshName,
        const std::string& particleScript, const Ogre::Vector3& direction, double speed, double physicalDamage, double magicalDamage,
        double elementDamage, GameEntity* entityTarget, bool damageAllies, bool koEnemyCreature, bool notifyPlayerIfHit);
    //! \brief Creates a projectile for loading, with no assumed shooter accuracy.
    MissileOneHit(GameMap* gameMap);

    virtual MissileObjectType getMissileType() const override
    { return MissileObjectType::oneHit; }

    virtual bool hitCreature(Tile* tile, GameEntity* entity) override;

    virtual void hitTargetEntity(Tile* tile, GameEntity* entityTarget) override;

    //! \brief True for single-target shots; area projectiles override this to bypass hit and avoidance rolls.
    virtual bool usesCreatureHitRoll() const { return true; }

    //! \brief Server side, cosmetic only. The missile was shot by this creature at that creature: the clients are
    //! told what the shot did (event hitResult). Preserved with launch accuracy in saves.
    void setShooter(const std::string& shooterName, const std::string& targetName)
    {
        mShooterName = shooterName;
        mTargetName = targetName;
    }

    static MissileOneHit* getMissileOneHitFromStream(GameMap* gameMap, std::istream& is);
    static MissileOneHit* getMissileOneHitFromPacket(GameMap* gameMap, ODPacket& is);
protected:
    //! \brief Saves damage and optional creature-shot launch accuracy.
    void exportToStream(std::ostream& os) const override;
    //! \brief Loads damage and optional launch accuracy without consuming derived projectile fields.
    bool importFromStream(std::istream& is) override;
    //! \brief Called when the shot ended: a shot at a creature that hurt nothing is reported as missed (cosmetic only)
    virtual void missileStopped() override;

    //! \brief Resolves single-target hit and avoidance before damage and reports the actual result.
    void hurt(Tile* tile, GameEntity* entity);

private:
    //! Shooter and target names for hit-result events, saved with creature-shot launch accuracy.
    std::string mShooterName;
    std::string mTargetName;
    bool mHasHit;
    double mPhysicalDamage;
    double mMagicalDamage;
    double mElementDamage;
    bool mNotifyPlayerIfHit;
    //! \brief Accuracy and level at launch; level zero means no accuracy snapshot exists in a legacy save.
    double mShotAttackRating;
    uint32_t mShotAttackerLevel;
};

#endif // MISSILEONEHIT_H
