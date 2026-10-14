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

#include "creatureskill/CreatureSkillInvisible.h"

#include "creatureeffect/CreatureEffectInvisible.h"
#include "creatureskill/CreatureSkillManager.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "gamemap/GameMap.h"
#include "spells/Spell.h"

#include <istream>

const std::string CreatureSkillInvisibleName = "Invisible";

namespace
{
class CreatureSkillInvisibleFactory : public CreatureSkillFactory
{
    CreatureSkill* createCreatureSkill() const override
    { return new CreatureSkillInvisible; }

    const std::string& getCreatureSkillName() const override
    { return CreatureSkillInvisibleName; }
};

// Register the factory
static CreatureSkillRegister reg(new CreatureSkillInvisibleFactory);
}

const std::string& CreatureSkillInvisible::getSkillName() const
{
    return CreatureSkillInvisibleName;
}

bool CreatureSkillInvisible::canBeUsedBy(const Creature* creature) const
{
    if(creature->getLevel() < mCreatureLevelMin)
        return false;

    return true;
}

bool CreatureSkillInvisible::tryUseSupport(GameMap& gameMap, Creature* creature) const
{
    if(!creature->isAlive())
        return false;

    CreatureEffectInvisible* effect = new CreatureEffectInvisible(mEffectDuration);
    creature->addCreatureEffect(effect);

    return true;
}

CreatureSkillInvisible* CreatureSkillInvisible::clone() const
{
    return new CreatureSkillInvisible(*this);
}

void CreatureSkillInvisible::getFormatString(std::string& format) const
{
    CreatureSkill::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "LevelMin\tEffectDuration";
}

void CreatureSkillInvisible::exportToStream(std::ostream& os) const
{
    CreatureSkill::exportToStream(os);
    os << "\t" << mCreatureLevelMin;
    os << "\t" << mEffectDuration;
}

bool CreatureSkillInvisible::importFromStream(std::istream& is)
{
    if(!CreatureSkill::importFromStream(is))
        return false;

    if(!(is >> mCreatureLevelMin))
        return false;
    if(!(is >> mEffectDuration))
        return false;

    return true;
}

bool CreatureSkillInvisible::isEqual(const CreatureSkill& creatureSkill) const
{
    if(!CreatureSkill::isEqual(creatureSkill))
        return false;

    const CreatureSkillInvisible* skill = dynamic_cast<const CreatureSkillInvisible*>(&creatureSkill);
    if(skill == nullptr)
        return false;

    if(mCreatureLevelMin != skill->mCreatureLevelMin)
        return false;
    if(mEffectDuration != skill->mEffectDuration)
        return false;

    return true;
}
