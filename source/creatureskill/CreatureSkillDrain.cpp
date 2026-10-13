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

#include "creatureskill/CreatureSkillDrain.h"

#include "creatureskill/CreatureSkillManager.h"
#include "entities/Creature.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "gamemap/GameMap.h"
#include "spells/Spell.h"
#include "utils/LogManager.h"

#include <istream>

const std::string CreatureSkillDrainName = "Drain";

namespace
{
class CreatureSkillDrainFactory : public CreatureSkillFactory
{
    CreatureSkill* createCreatureSkill() const override
    { return new CreatureSkillDrain; }

    const std::string& getCreatureSkillName() const override
    { return CreatureSkillDrainName; }
};

// Register the factory
static CreatureSkillRegister reg(new CreatureSkillDrainFactory);
}

const std::string& CreatureSkillDrain::getSkillName() const
{
    return CreatureSkillDrainName;
}

double CreatureSkillDrain::getRangeMax(const Creature* creature, GameEntity* entityAttack) const
{
    // Drain can be cast on creatures only
    if(entityAttack->getObjectType() != GameEntityType::creature)
        return 0.0;

    return mMaxRange;
}

bool CreatureSkillDrain::canBeUsedBy(const Creature* creature) const
{
    if(creature->getLevel() < mCreatureLevelMin)
        return false;

    return true;
}

bool CreatureSkillDrain::tryUseFight(GameMap& gameMap, Creature* creature, float range,
        GameEntity* attackedObject, Tile* attackedTile, bool ko, bool notifyPlayerIfHit) const
{
    if(attackedObject->getObjectType() != GameEntityType::creature)
    {
        OD_LOG_ERR("creature=" + creature->getName() + ", attackedObject=" + attackedObject->getName() + ", attackedTile=" + Tile::displayAsString(attackedTile));
        return false;
    }

    Creature* attackedCreature = static_cast<Creature*>(attackedObject);
    // Removes a percentage of the full health of the target and gives the same amount to the caster
    const double drained = attackedCreature->getMaxHp() * mDrainPercent / 100.0;
    const double damageDone = attackedCreature->takeDamage(creature, drained, 0.0, 0.0, 0.0, attackedTile, ko);
    if(damageDone > 0.0)
        creature->heal(damageDone);
    if(notifyPlayerIfHit)
        attackedCreature->notifyFightPlayer(attackedTile);

    return true;
}

CreatureSkillDrain* CreatureSkillDrain::clone() const
{
    return new CreatureSkillDrain(*this);
}

void CreatureSkillDrain::getFormatString(std::string& format) const
{
    CreatureSkill::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "RangeMax\tLevelMin\tDrainPercent";

}

void CreatureSkillDrain::exportToStream(std::ostream& os) const
{
    CreatureSkill::exportToStream(os);
    os << "\t" << mMaxRange;
    os << "\t" << mCreatureLevelMin;
    os << "\t" << mDrainPercent;
}

bool CreatureSkillDrain::importFromStream(std::istream& is)
{
    if(!CreatureSkill::importFromStream(is))
        return false;

    if(!(is >> mMaxRange))
        return false;
    if(!(is >> mCreatureLevelMin))
        return false;
    if(!(is >> mDrainPercent))
        return false;

    return true;
}

bool CreatureSkillDrain::isEqual(const CreatureSkill& creatureSkill) const
{
    if(!CreatureSkill::isEqual(creatureSkill))
        return false;

    const CreatureSkillDrain* skill = dynamic_cast<const CreatureSkillDrain*>(&creatureSkill);
    if(skill == nullptr)
        return false;

    if(mMaxRange != skill->mMaxRange)
        return false;
    if(mCreatureLevelMin != skill->mCreatureLevelMin)
        return false;
    if(mDrainPercent != skill->mDrainPercent)
        return false;

    return true;
}
