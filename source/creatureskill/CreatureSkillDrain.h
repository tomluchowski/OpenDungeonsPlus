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

#ifndef CREATURESKILLDRAIN_H
#define CREATURESKILLDRAIN_H

#include "creatureskill/CreatureSkill.h"

#include <cstdint>
#include <iosfwd>
#include <string>

class Creature;
class GameMap;

class CreatureSkillDrain : public CreatureSkill
{
public:
    // Constructors
    //! \brief Creates an empty skill that is filled by importFromStream.
    CreatureSkillDrain() :
        mMaxRange(0.0),
        mCreatureLevelMin(0),
        mDrainPercent(0.0)
    {}

    //! \brief Destroys the skill.
    virtual ~CreatureSkillDrain()
    {}

    //! \brief Returns the name under which the skill is registered.
    virtual const std::string& getSkillName() const override;

    //! \brief Returns the maximum range at which the skill can be used on its target.
    virtual double getRangeMax(const Creature* creature, GameEntity* entityAttack) const override;

    //! \brief Returns true if the creature fulfils the requirements to use the skill.
    virtual bool canBeUsedBy(const Creature* creature) const override;

    //! \brief Tries to use the skill on the creature itself, without a target. Returns true if it was used.
    virtual bool tryUseSupport(GameMap& gameMap, Creature* creature) const override
    { return false; }

    //! \brief Tries to use the skill against the attacked object. Returns true if it was used.
    virtual bool tryUseFight(GameMap& gameMap, Creature* creature, float range,
        GameEntity* attackedObject, Tile* attackedTile, bool ko, bool notifyPlayerIfHit) const override;

    //! \brief Returns a copy of the skill with the same parameters.
    virtual CreatureSkillDrain* clone() const override;

    //! \brief Returns true if the other skill is of the same type and has the same parameters.
    virtual bool isEqual(const CreatureSkill& creatureSkill) const override;

    //! \brief Writes the format of the definition line of the skill into the given string.
    virtual void getFormatString(std::string& format) const override;
    //! \brief Writes the skill to the stream.
    virtual void exportToStream(std::ostream& os) const override;
    //! \brief Reads the skill from the stream. Returns false if the stream is malformed.
    virtual bool importFromStream(std::istream& is) override;

private:
    double mMaxRange;
    uint32_t mCreatureLevelMin;
    double mDrainPercent;

};

#endif // CREATURESKILLDRAIN_H
