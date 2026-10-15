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


#ifndef CREATURESKILLTELEPORT_H
#define CREATURESKILLTELEPORT_H

#include "creatureskill/CreatureSkill.h"

#include <cstdint>
#include <iosfwd>
#include <string>

class Creature;
class GameMap;

//! \brief Teleport: the caster appears at once somewhere else in its own dungeon. It is a support skill the
//! creature uses by itself: when its health is low and enemies are in sight it jumps to the dungeon heart, and
//! when it walks to a place that is at least MinDistance tiles away it jumps there. A possessed creature only
//! teleports back to the dungeon heart.
class CreatureSkillTeleport : public CreatureSkill
{
public:
    // Constructors
    //! \brief Creates an empty skill that is filled by importFromStream.
    CreatureSkillTeleport() :
        mCreatureLevelMin(0),
        mMinDistance(0.0),
        mDangerPercent(0.0)
    {}

    //! \brief Destroys the skill.
    virtual ~CreatureSkillTeleport()
    {}

    //! \brief Returns the name under which the skill is registered.
    virtual const std::string& getSkillName() const override;

    //! \brief Returns true if the creature fulfils the requirements to use the skill.
    virtual bool canBeUsedBy(const Creature* creature) const override;

    //! \brief Tries to use the skill on the creature itself, without a target. Returns true if it was used.
    virtual bool tryUseSupport(GameMap& gameMap, Creature* creature) const override;

    //! \brief Tries to use the skill against the attacked object. Returns true if it was used.
    virtual bool tryUseFight(GameMap& gameMap, Creature* creature, float range,
        GameEntity* attackedObject, Tile* attackedTile, bool ko, bool notifyPlayerIfHit) const override
    { return false; }

    //! \brief Returns a copy of the skill with the same parameters.
    virtual CreatureSkillTeleport* clone() const override;

    //! \brief Returns true if the other skill is of the same type and has the same parameters.
    virtual bool isEqual(const CreatureSkill& creatureSkill) const override;

    //! \brief Writes the format of the definition line of the skill into the given string.
    virtual void getFormatString(std::string& format) const override;
    //! \brief Writes the skill to the stream.
    virtual void exportToStream(std::ostream& os) const override;
    //! \brief Reads the skill from the stream. Returns false if the stream is malformed.
    virtual bool importFromStream(std::istream& is) override;

private:
    uint32_t mCreatureLevelMin;
    double mMinDistance;
    double mDangerPercent;
};

#endif // CREATURESKILLTELEPORT_H
