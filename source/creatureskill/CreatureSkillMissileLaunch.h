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

#ifndef CREATURESKILLMISSILELAUNCH_H
#define CREATURESKILLMISSILELAUNCH_H

#include "creatureskill/CreatureSkill.h"

#include <cstdint>
#include <iosfwd>
#include <string>

class Creature;
class GameMap;

//! \brief Launches ranged attacks carrying creature ranges alongside deterministic building damage.
class CreatureSkillMissileLaunch : public CreatureSkill
{
public:
    // Constructors
    //! \brief Creates a skill with empty creature damage ranges until its definition is loaded.
    CreatureSkillMissileLaunch() :
        mRangeMax(0.0),
        mRangePerLvl(0.0),
        mCreatureLevelMin(0),
        mMissileSpeed(0.0),
        mPhyAtk(0.0),
        mPhyAtkPerLvl(0.0),
        mMagAtk(0.0),
        mMagAtkPerLvl(0.0),
        mEleAtk(0.0),
        mEleAtkPerLvl(0.0),
        mPhyMin(0.0), mPhyMax(0.0),
        mMagMin(0.0), mMagMax(0.0),
        mEleMin(0.0), mEleMax(0.0)
    {}

    virtual ~CreatureSkillMissileLaunch()
    {}

    virtual const std::string& getSkillName() const override;

    virtual double getRangeMax(const Creature* creature, GameEntity* entityAttack) const override;

    virtual bool canBeUsedBy(const Creature* creature) const override;

    virtual bool tryUseSupport(GameMap& gameMap, Creature* creature) const override
    { return false; }

    //! \brief Launches a combat projectile with the shooter possession state captured before its first hit.
    virtual bool tryUseFight(GameMap& gameMap, Creature* creature, float range,
        GameEntity* attackedObject, Tile* attackedTile, bool ko, bool notifyPlayerIfHit) const override;

    virtual CreatureSkillMissileLaunch* clone() const override;

    //! \brief Compares the complete skill definition, including its creature damage range.
    virtual bool isEqual(const CreatureSkill& creatureSkill) const override;

    //! \brief Describes the legacy columns followed by the optional creature damage range.
    virtual void getFormatString(std::string& format) const override;
    //! \brief Writes all skill columns, including the creature damage range.
    virtual void exportToStream(std::ostream& os) const override;
    //! \brief Reads the skill, falling back to the old fixed damage when range columns are absent.
    virtual bool importFromStream(std::istream& is) override;

private:
    double mRangeMax;
    double mRangePerLvl;
    uint32_t mCreatureLevelMin;
    std::string mMissileMesh;
    std::string mMissilePartScript;
    double mMissileSpeed;
    double mPhyAtk;
    double mPhyAtkPerLvl;
    double mMagAtk;
    double mMagAtkPerLvl;
    double mEleAtk;
    double mEleAtkPerLvl;
    //! \brief Level-one damage range against creatures; the old attack fields remain for buildings.
    double mPhyMin, mPhyMax, mMagMin, mMagMax, mEleMin, mEleMax;

};

#endif // CREATURESKILLMISSILELAUNCH_H
