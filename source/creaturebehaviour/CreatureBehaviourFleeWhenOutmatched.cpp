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

#include "creaturebehaviour/CreatureBehaviourFleeWhenOutmatched.h"

#include "creatureaction/CreatureAction.h"
#include "creaturebehaviour/CreatureBehaviourManager.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/GameEntityType.h"

const std::string CreatureBehaviourFleeWhenOutmatched::mNameCreatureBehaviourFleeWhenOutmatched = "FleeWhenOutmatched";

namespace
{
class CreatureBehaviourFleeWhenOutmatchedFactory : public CreatureBehaviourFactory
{
    CreatureBehaviour* createCreatureBehaviour() const override
    { return new CreatureBehaviourFleeWhenOutmatched; }

    const std::string& getCreatureBehaviourName() const override
    {
        return CreatureBehaviourFleeWhenOutmatched::mNameCreatureBehaviourFleeWhenOutmatched;
    }
};

// Register the factory
static CreatureBehaviourRegister reg(new CreatureBehaviourFleeWhenOutmatchedFactory);

// Sums the threat of the fighting creatures in the given list. Workers do not count
// and the creature given as skip is ignored.
double sumThreat(const std::vector<GameEntity*>& entities, const Creature* skip)
{
    double threat = 0.0;
    for(GameEntity* entity : entities)
    {
        if(entity->getObjectType() != GameEntityType::creature)
            continue;

        const Creature* other = static_cast<const Creature*>(entity);
        if(other == skip)
            continue;

        if(other->getDefinition()->isWorker())
            continue;

        threat += other->getThreat();
    }
    return threat;
}
}

CreatureBehaviourFleeWhenOutmatched::CreatureBehaviourFleeWhenOutmatched(const CreatureBehaviourFleeWhenOutmatched& behaviour) :
    CreatureBehaviour(),
    mFearCoef(behaviour.mFearCoef)
{
}

CreatureBehaviour* CreatureBehaviourFleeWhenOutmatched::clone() const
{
    return new CreatureBehaviourFleeWhenOutmatched(*this);
}

bool CreatureBehaviourFleeWhenOutmatched::processBehaviour(Creature& creature) const
{
    // If we are having a friendly fight (in the arena or in a bar) we should not flee
    if(creature.isActionInList(CreatureActionType::fightFriendly))
        return true;

    // If we are already fleeing, there is nothing more to do
    if(creature.isActionInList(CreatureActionType::flee))
        return true;

    double enemyThreat = sumThreat(creature.getVisibleEnemyObjects(), nullptr);
    if(enemyThreat <= 0.0)
        return true;

    // Our side is the creature itself plus the allies it can see
    double ownThreat = creature.getThreat() + sumThreat(creature.getVisibleAlliedObjects(), &creature);
    if(enemyThreat <= ownThreat * mFearCoef)
        return true;

    creature.flee();
    return false;
}

void CreatureBehaviourFleeWhenOutmatched::getFormatString(std::string& format) const
{
    if(!format.empty())
        format += "\t";

    format += "FearCoef";
}

bool CreatureBehaviourFleeWhenOutmatched::isEqual(const CreatureBehaviour& creatureBehaviour) const
{
    if(!CreatureBehaviour::isEqual(creatureBehaviour))
        return false;

    const CreatureBehaviourFleeWhenOutmatched* cb = dynamic_cast<const CreatureBehaviourFleeWhenOutmatched*>(&creatureBehaviour);
    if(cb == nullptr)
        return false;

    if(mFearCoef != cb->mFearCoef)
        return false;

    return true;
}

void CreatureBehaviourFleeWhenOutmatched::exportToStream(std::ostream& os) const
{
    CreatureBehaviour::exportToStream(os);
    os << "\t" << mFearCoef;
}

bool CreatureBehaviourFleeWhenOutmatched::importFromStream(std::istream& is)
{
    if(!CreatureBehaviour::importFromStream(is))
        return false;

    if(!(is >> mFearCoef))
        return false;

    return true;
}
