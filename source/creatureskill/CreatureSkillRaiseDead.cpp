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

#include "creatureskill/CreatureSkillRaiseDead.h"

#include "creatureskill/CreatureSkillManager.h"
#include "creatureskill/CreatureSkillSummon.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/Tile.h"
#include "gamemap/GameMap.h"
#include "utils/LogManager.h"

#include <istream>
#include <vector>

const std::string CreatureSkillRaiseDeadName = "RaiseDead";

namespace
{
class CreatureSkillRaiseDeadFactory : public CreatureSkillFactory
{
    CreatureSkill* createCreatureSkill() const override
    { return new CreatureSkillRaiseDead; }

    const std::string& getCreatureSkillName() const override
    { return CreatureSkillRaiseDeadName; }
};

// Register the factory
static CreatureSkillRegister reg(new CreatureSkillRaiseDeadFactory);
}

const std::string& CreatureSkillRaiseDead::getSkillName() const
{
    return CreatureSkillRaiseDeadName;
}

bool CreatureSkillRaiseDead::canBeUsedBy(const Creature* creature) const
{
    if(creature->getLevel() < mCreatureLevelMin)
        return false;

    return true;
}

bool CreatureSkillRaiseDead::tryUseSupport(GameMap& gameMap, Creature* creature) const
{
    if(!creature->isAlive() || creature->isFrozen())
        return false;

    Tile* myTile = creature->getPositionTile();
    if(myTile == nullptr)
        return false;

    // The creature only casts it by itself when enemies are in sight. A possessed creature obeys the player
    const Creature* constCreature = creature;
    if(!creature->isPossessed() && constCreature->getVisibleEnemyObjects().empty())
        return false;

    const double rangeSquared = mRangeMax * mRangeMax;
    uint32_t nbRaised = 0;
    // We copy the list as new creatures are added to the map
    std::vector<Creature*> creatures = gameMap.getCreatures();
    for(Creature* body : creatures)
    {
        if(nbRaised >= mMaxRaised)
            break;

        if(body->getDefinition()->getClassName() == mCreatureClass)
            continue;

        Tile* bodyTile = body->getPositionTile();
        if(bodyTile == nullptr)
            continue;

        double dx = static_cast<double>(bodyTile->getX() - myTile->getX());
        double dy = static_cast<double>(bodyTile->getY() - myTile->getY());
        if(((dx * dx) + (dy * dy)) > rangeSquared)
            continue;

        if(!body->takeCorpse())
            continue;

        if(CreatureSkillSummon::summonTemporaryCreature(gameMap, *creature, mCreatureClass, bodyTile,
            static_cast<int32_t>(mEffectDuration)) == nullptr)
        {
            continue;
        }

        ++nbRaised;
    }

    return (nbRaised > 0);
}

CreatureSkillRaiseDead* CreatureSkillRaiseDead::clone() const
{
    return new CreatureSkillRaiseDead(*this);
}

void CreatureSkillRaiseDead::getFormatString(std::string& format) const
{
    CreatureSkill::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "LevelMin\tRangeMax\tMaxRaised\tEffectDuration\tCreatureClass";
}

void CreatureSkillRaiseDead::exportToStream(std::ostream& os) const
{
    CreatureSkill::exportToStream(os);
    os << "\t" << mCreatureLevelMin;
    os << "\t" << mRangeMax;
    os << "\t" << mMaxRaised;
    os << "\t" << mEffectDuration;
    os << "\t" << mCreatureClass;
}

bool CreatureSkillRaiseDead::importFromStream(std::istream& is)
{
    if(!CreatureSkill::importFromStream(is))
        return false;

    if(!(is >> mCreatureLevelMin))
        return false;
    if(!(is >> mRangeMax))
        return false;
    if(!(is >> mMaxRaised))
        return false;
    if(!(is >> mEffectDuration))
        return false;
    if(!(is >> mCreatureClass))
        return false;

    return true;
}

bool CreatureSkillRaiseDead::isEqual(const CreatureSkill& creatureSkill) const
{
    if(!CreatureSkill::isEqual(creatureSkill))
        return false;

    const CreatureSkillRaiseDead* skill = dynamic_cast<const CreatureSkillRaiseDead*>(&creatureSkill);
    if(skill == nullptr)
        return false;

    if(mCreatureLevelMin != skill->mCreatureLevelMin)
        return false;
    if(mRangeMax != skill->mRangeMax)
        return false;
    if(mMaxRaised != skill->mMaxRaised)
        return false;
    if(mEffectDuration != skill->mEffectDuration)
        return false;
    if(mCreatureClass != skill->mCreatureClass)
        return false;

    return true;
}
