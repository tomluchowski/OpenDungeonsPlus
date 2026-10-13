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


#include "creatureskill/CreatureSkillWhirlwind.h"

#include "creatureskill/CreatureSkillManager.h"
#include "entities/Creature.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "gamemap/GameMap.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <istream>

const std::string CreatureSkillWhirlwindName = "Whirlwind";

namespace
{
class CreatureSkillWhirlwindFactory : public CreatureSkillFactory
{
    CreatureSkill* createCreatureSkill() const override
    { return new CreatureSkillWhirlwind; }

    const std::string& getCreatureSkillName() const override
    { return CreatureSkillWhirlwindName; }
};

// Register the factory
static CreatureSkillRegister reg(new CreatureSkillWhirlwindFactory);
}

const std::string& CreatureSkillWhirlwind::getSkillName() const
{
    return CreatureSkillWhirlwindName;
}

double CreatureSkillWhirlwind::getRangeMax(const Creature* creature, GameEntity* entityAttack) const
{
    // Whirlwind can be cast on creatures only
    if(entityAttack->getObjectType() != GameEntityType::creature)
        return 0.0;

    return mMaxRange;
}

bool CreatureSkillWhirlwind::canBeUsedBy(const Creature* creature) const
{
    if(creature->getLevel() < mCreatureLevelMin)
        return false;

    return true;
}

bool CreatureSkillWhirlwind::tryUseFight(GameMap& gameMap, Creature* creature, float range,
        GameEntity* attackedObject, Tile* attackedTile, bool ko, bool notifyPlayerIfHit) const
{
    if(attackedObject->getObjectType() != GameEntityType::creature)
    {
        OD_LOG_ERR("creature=" + creature->getName() + ", attackedObject=" + attackedObject->getName() + ", attackedTile=" + Tile::displayAsString(attackedTile));
        return false;
    }

    Creature* attackedCreature = static_cast<Creature*>(attackedObject);
    Tile* startTile = attackedCreature->getPositionTile();
    if(startTile == nullptr)
        return false;

    // The wind blows away from the caster
    Ogre::Vector3 direction = attackedCreature->getPosition() - creature->getPosition();
    direction.z = 0.0f;
    if(direction.squaredLength() < 0.0001f)
        return false;

    direction.normalise();

    // We follow the line away from the caster until something blocks the way
    Tile* lastTile = startTile;
    for(uint32_t i = 1; i <= mPushTiles; ++i)
    {
        Ogre::Vector3 pos = attackedCreature->getPosition() + (direction * static_cast<Ogre::Real>(i));
        Tile* tile = gameMap.getTile(Helper::round(pos.x), Helper::round(pos.y));
        if((tile == nullptr) || tile->isFullTile() || !attackedCreature->canGoThroughTile(tile))
            break;

        lastTile = tile;
    }

    if(lastTile != startTile)
        attackedCreature->teleportTo(lastTile);

    // Creatures cannot act while the wind blows them
    attackedCreature->stun(static_cast<int32_t>(mStunTurns));

    return true;
}

CreatureSkillWhirlwind* CreatureSkillWhirlwind::clone() const
{
    return new CreatureSkillWhirlwind(*this);
}

void CreatureSkillWhirlwind::getFormatString(std::string& format) const
{
    CreatureSkill::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "RangeMax\tLevelMin\tPushTiles\tStunTurns";
}

void CreatureSkillWhirlwind::exportToStream(std::ostream& os) const
{
    CreatureSkill::exportToStream(os);
    os << "\t" << mMaxRange;
    os << "\t" << mCreatureLevelMin;
    os << "\t" << mPushTiles;
    os << "\t" << mStunTurns;
}

bool CreatureSkillWhirlwind::importFromStream(std::istream& is)
{
    if(!CreatureSkill::importFromStream(is))
        return false;

    if(!(is >> mMaxRange))
        return false;
    if(!(is >> mCreatureLevelMin))
        return false;
    if(!(is >> mPushTiles))
        return false;
    if(!(is >> mStunTurns))
        return false;

    return true;
}

bool CreatureSkillWhirlwind::isEqual(const CreatureSkill& creatureSkill) const
{
    if(!CreatureSkill::isEqual(creatureSkill))
        return false;

    const CreatureSkillWhirlwind* skill = dynamic_cast<const CreatureSkillWhirlwind*>(&creatureSkill);
    if(skill == nullptr)
        return false;

    if(mMaxRange != skill->mMaxRange)
        return false;
    if(mCreatureLevelMin != skill->mCreatureLevelMin)
        return false;
    if(mPushTiles != skill->mPushTiles)
        return false;
    if(mStunTurns != skill->mStunTurns)
        return false;

    return true;
}
