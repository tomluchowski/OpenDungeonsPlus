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


#ifndef CREATURESKILLAREADAMAGE_H
#define CREATURESKILLAREADAMAGE_H

#include "creatureskill/CreatureSkill.h"

#include <cstdint>
#include <iosfwd>
#include <string>

class Creature;
class GameMap;

//! \brief Skill that puts a zone of damage around the tile of the target creature (see CreatureEffectAreaDamage).
//! It is registered under the names "GasCloud" and "HailStorm", which only differ by their values in the
//! creature definition: the radius, how many turns the zone lasts and the damage per turn.
class CreatureSkillAreaDamage : public CreatureSkill
{
public:
    // Constructors
    CreatureSkillAreaDamage(const std::string& skillName) :
        mSkillName(skillName),
        mMaxRange(0.0),
        mCreatureLevelMin(0),
        mRadius(0.0),
        mNbTurns(0),
        mDamagePerTurn(0.0)
    {}

    virtual ~CreatureSkillAreaDamage()
    {}

    virtual const std::string& getSkillName() const override
    { return mSkillName; }

    virtual double getRangeMax(const Creature* creature, GameEntity* entityAttack) const override;

    virtual bool canBeUsedBy(const Creature* creature) const override;

    virtual bool tryUseSupport(GameMap& gameMap, Creature* creature) const override
    { return false; }

    virtual bool tryUseFight(GameMap& gameMap, Creature* creature, float range,
        GameEntity* attackedObject, Tile* attackedTile, bool ko, bool notifyPlayerIfHit) const override;

    virtual CreatureSkillAreaDamage* clone() const override;

    virtual bool isEqual(const CreatureSkill& creatureSkill) const override;

    virtual void getFormatString(std::string& format) const override;
    virtual void exportToStream(std::ostream& os) const override;
    virtual bool importFromStream(std::istream& is) override;

private:
    std::string mSkillName;
    double mMaxRange;
    uint32_t mCreatureLevelMin;
    double mRadius;
    uint32_t mNbTurns;
    double mDamagePerTurn;
    std::string mParticleScript;
};

#endif // CREATURESKILLAREADAMAGE_H
