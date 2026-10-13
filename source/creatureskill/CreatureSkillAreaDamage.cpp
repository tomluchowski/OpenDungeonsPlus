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


#include "creatureskill/CreatureSkillAreaDamage.h"

#include "creatureeffect/CreatureEffectAreaDamage.h"
#include "creatureskill/CreatureSkillManager.h"
#include "entities/Creature.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "gamemap/GameMap.h"
#include "utils/LogManager.h"

#include <istream>

const std::string CreatureSkillGasCloudName = "GasCloud";
const std::string CreatureSkillHailStormName = "HailStorm";

namespace
{
class CreatureSkillAreaDamageFactory : public CreatureSkillFactory
{
public:
    CreatureSkillAreaDamageFactory(const std::string& skillName) :
        mSkillName(skillName)
    {}

    CreatureSkill* createCreatureSkill() const override
    { return new CreatureSkillAreaDamage(mSkillName); }

    const std::string& getCreatureSkillName() const override
    { return mSkillName; }

private:
    const std::string mSkillName;
};

// Register the factories
static CreatureSkillRegister regGasCloud(new CreatureSkillAreaDamageFactory(CreatureSkillGasCloudName));
static CreatureSkillRegister regHailStorm(new CreatureSkillAreaDamageFactory(CreatureSkillHailStormName));
}

double CreatureSkillAreaDamage::getRangeMax(const Creature* creature, GameEntity* entityAttack) const
{
    // The zone is cast on creatures only
    if(entityAttack->getObjectType() != GameEntityType::creature)
        return 0.0;

    return mMaxRange;
}

bool CreatureSkillAreaDamage::canBeUsedBy(const Creature* creature) const
{
    if(creature->getLevel() < mCreatureLevelMin)
        return false;

    return true;
}

bool CreatureSkillAreaDamage::tryUseFight(GameMap& gameMap, Creature* creature, float range,
        GameEntity* attackedObject, Tile* attackedTile, bool ko, bool notifyPlayerIfHit) const
{
    if((attackedObject->getObjectType() != GameEntityType::creature) || (attackedTile == nullptr))
    {
        OD_LOG_ERR("creature=" + creature->getName() + ", attackedObject=" + attackedObject->getName() + ", attackedTile=" + Tile::displayAsString(attackedTile));
        return false;
    }

    // The zone is centred on the target and is kept by the caster
    CreatureEffectAreaDamage* effect = new CreatureEffectAreaDamage(static_cast<int32_t>(mNbTurns),
        attackedTile->getX(), attackedTile->getY(), mRadius, mDamagePerTurn, mParticleScript);
    creature->addCreatureEffect(effect);

    return true;
}

CreatureSkillAreaDamage* CreatureSkillAreaDamage::clone() const
{
    return new CreatureSkillAreaDamage(*this);
}

void CreatureSkillAreaDamage::getFormatString(std::string& format) const
{
    CreatureSkill::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "RangeMax\tLevelMin\tRadius\tNbTurns\tDamagePerTurn\tParticleScript";
}

void CreatureSkillAreaDamage::exportToStream(std::ostream& os) const
{
    CreatureSkill::exportToStream(os);
    os << "\t" << mMaxRange;
    os << "\t" << mCreatureLevelMin;
    os << "\t" << mRadius;
    os << "\t" << mNbTurns;
    os << "\t" << mDamagePerTurn;
    if(mParticleScript.empty())
        os << "\tnone";
    else
        os << "\t" << mParticleScript;
}

bool CreatureSkillAreaDamage::importFromStream(std::istream& is)
{
    if(!CreatureSkill::importFromStream(is))
        return false;

    if(!(is >> mMaxRange))
        return false;
    if(!(is >> mCreatureLevelMin))
        return false;
    if(!(is >> mRadius))
        return false;
    if(!(is >> mNbTurns))
        return false;
    if(!(is >> mDamagePerTurn))
        return false;
    if(!(is >> mParticleScript))
        return false;
    if(mParticleScript == "none")
        mParticleScript.clear();

    return true;
}

bool CreatureSkillAreaDamage::isEqual(const CreatureSkill& creatureSkill) const
{
    if(!CreatureSkill::isEqual(creatureSkill))
        return false;

    const CreatureSkillAreaDamage* skill = dynamic_cast<const CreatureSkillAreaDamage*>(&creatureSkill);
    if(skill == nullptr)
        return false;

    if(mSkillName != skill->mSkillName)
        return false;
    if(mMaxRange != skill->mMaxRange)
        return false;
    if(mCreatureLevelMin != skill->mCreatureLevelMin)
        return false;
    if(mRadius != skill->mRadius)
        return false;
    if(mNbTurns != skill->mNbTurns)
        return false;
    if(mDamagePerTurn != skill->mDamagePerTurn)
        return false;
    if(mParticleScript != skill->mParticleScript)
        return false;

    return true;
}
