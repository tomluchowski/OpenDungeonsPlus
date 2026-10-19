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

#ifndef CREATURESKILLEXPLOSION_H
#define CREATURESKILLEXPLOSION_H

#include "creatureskill/CreatureSkill.h"

#include <cstdint>
#include <iosfwd>
#include <string>

class Creature;
class GameMap;

//! \brief Applies an explosion effect with a configured damage range.
class CreatureSkillExplosion : public CreatureSkill
{
public:
    // Constructors
    //! \brief Creates a skill with empty creature damage ranges until its definition is loaded.
    CreatureSkillExplosion() :
        mMaxRange(0.0),
        mCreatureLevelMin(0),
        mEffectDuration(0),
        mEffectValue(0.0),
        mEffectValueMin(0.0)
    {}

    virtual ~CreatureSkillExplosion()
    {}

    virtual const std::string& getSkillName() const override;

    virtual double getRangeMax(const Creature* creature, GameEntity* entityAttack) const override;

    virtual bool canBeUsedBy(const Creature* creature) const override;

    virtual bool tryUseSupport(GameMap& gameMap, Creature* creature) const override
    { return false; }

    //! \brief Applies the configured creature damage range while preserving the building damage path.
    virtual bool tryUseFight(GameMap& gameMap, Creature* creature, float range,
        GameEntity* attackedObject, Tile* attackedTile, bool ko, bool notifyPlayerIfHit) const override;

    virtual CreatureSkillExplosion* clone() const override;

    //! \brief Compares the complete skill definition, including its creature damage range.
    virtual bool isEqual(const CreatureSkill& creatureSkill) const override;

    //! \brief Describes the legacy columns followed by the optional creature damage range.
    virtual void getFormatString(std::string& format) const override;
    //! \brief Writes all skill columns, including the creature damage range.
    virtual void exportToStream(std::ostream& os) const override;
    //! \brief Reads the skill, falling back to the old fixed damage when range columns are absent.
    virtual bool importFromStream(std::istream& is) override;

private:
    double mMaxRange;
    uint32_t mCreatureLevelMin;
    uint32_t mEffectDuration;
    double mEffectValue;
    //! \brief Minimum damage per application.
    double mEffectValueMin;

};

#endif // CREATURESKILLEXPLOSION_H
