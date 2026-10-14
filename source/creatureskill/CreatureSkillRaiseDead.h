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

#ifndef CREATURESKILLRAISEDEAD_H
#define CREATURESKILLRAISEDEAD_H

#include "creatureskill/CreatureSkill.h"

#include <cstdint>
#include <iosfwd>
#include <string>

class Creature;
class GameMap;

//! \brief Raise Dead: turns the dead bodies near the caster into temporary creatures (skeletons) that
//! fight for the seat of the caster. It is a support skill the creature uses by itself while enemies are
//! in sight (a possessed creature uses it when the player asks). The bodies of workers and of the raised
//! creature class cannot be raised.
class CreatureSkillRaiseDead : public CreatureSkill
{
public:
    // Constructors
    CreatureSkillRaiseDead() :
        mCreatureLevelMin(0),
        mRangeMax(0.0),
        mMaxRaised(0),
        mEffectDuration(0)
    {}

    virtual ~CreatureSkillRaiseDead()
    {}

    virtual const std::string& getSkillName() const override;

    virtual bool canBeUsedBy(const Creature* creature) const override;

    virtual bool tryUseSupport(GameMap& gameMap, Creature* creature) const override;

    virtual bool tryUseFight(GameMap& gameMap, Creature* creature, float range,
        GameEntity* attackedObject, Tile* attackedTile, bool ko, bool notifyPlayerIfHit) const override
    { return false; }

    virtual CreatureSkillRaiseDead* clone() const override;

    virtual bool isEqual(const CreatureSkill& creatureSkill) const override;

    virtual void getFormatString(std::string& format) const override;
    virtual void exportToStream(std::ostream& os) const override;
    virtual bool importFromStream(std::istream& is) override;

private:
    uint32_t mCreatureLevelMin;
    double mRangeMax;
    uint32_t mMaxRaised;
    uint32_t mEffectDuration;
    std::string mCreatureClass;
};

#endif // CREATURESKILLRAISEDEAD_H
