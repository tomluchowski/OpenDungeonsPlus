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

#include "creatureskill/CreatureSkillSkeletonArmy.h"

#include "creatureskill/CreatureSkillManager.h"
#include "creatureskill/CreatureSkillSummon.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "gamemap/GameMap.h"
#include "utils/LogManager.h"
#include "utils/Random.h"

#include <istream>
#include <vector>

const std::string CreatureSkillSkeletonArmyName = "SkeletonArmy";

namespace
{
class CreatureSkillSkeletonArmyFactory : public CreatureSkillFactory
{
    CreatureSkill* createCreatureSkill() const override
    { return new CreatureSkillSkeletonArmy; }

    const std::string& getCreatureSkillName() const override
    { return CreatureSkillSkeletonArmyName; }
};

// Register the factory
static CreatureSkillRegister reg(new CreatureSkillSkeletonArmyFactory);

// The creatures rise from the ground within this number of tiles around the caster
const int SKELETON_ARMY_RADIUS = 2;
}

const std::string& CreatureSkillSkeletonArmy::getSkillName() const
{
    return CreatureSkillSkeletonArmyName;
}

bool CreatureSkillSkeletonArmy::canBeUsedBy(const Creature* creature) const
{
    if(creature->getLevel() < mCreatureLevelMin)
        return false;

    return true;
}

bool CreatureSkillSkeletonArmy::tryUseSupport(GameMap& gameMap, Creature* creature) const
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

    // We search the free tiles around the caster
    std::vector<Tile*> freeTiles;
    for(int xx = myTile->getX() - SKELETON_ARMY_RADIUS; xx <= myTile->getX() + SKELETON_ARMY_RADIUS; ++xx)
    {
        for(int yy = myTile->getY() - SKELETON_ARMY_RADIUS; yy <= myTile->getY() + SKELETON_ARMY_RADIUS; ++yy)
        {
            Tile* tile = gameMap.getTile(xx, yy);
            if((tile == nullptr) || tile->isFullTile())
                continue;

            freeTiles.push_back(tile);
        }
    }

    if(freeTiles.empty())
        return false;

    uint32_t nbSummoned = 0;
    for(uint32_t i = 0; i < mNbCreatures; ++i)
    {
        // Several creatures can share a tile if there are not enough of them
        uint32_t index = Random::Uint(0, static_cast<uint32_t>(freeTiles.size() - 1));
        if(CreatureSkillSummon::summonTemporaryCreature(gameMap, *creature, mCreatureClass, freeTiles[index],
            static_cast<int32_t>(mEffectDuration)) != nullptr)
        {
            ++nbSummoned;
        }
    }

    return (nbSummoned > 0);
}

CreatureSkillSkeletonArmy* CreatureSkillSkeletonArmy::clone() const
{
    return new CreatureSkillSkeletonArmy(*this);
}

void CreatureSkillSkeletonArmy::getFormatString(std::string& format) const
{
    CreatureSkill::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "LevelMin\tNbCreatures\tEffectDuration\tCreatureClass";
}

void CreatureSkillSkeletonArmy::exportToStream(std::ostream& os) const
{
    CreatureSkill::exportToStream(os);
    os << "\t" << mCreatureLevelMin;
    os << "\t" << mNbCreatures;
    os << "\t" << mEffectDuration;
    os << "\t" << mCreatureClass;
}

bool CreatureSkillSkeletonArmy::importFromStream(std::istream& is)
{
    if(!CreatureSkill::importFromStream(is))
        return false;

    if(!(is >> mCreatureLevelMin))
        return false;
    if(!(is >> mNbCreatures))
        return false;
    if(!(is >> mEffectDuration))
        return false;
    if(!(is >> mCreatureClass))
        return false;

    return true;
}

bool CreatureSkillSkeletonArmy::isEqual(const CreatureSkill& creatureSkill) const
{
    if(!CreatureSkill::isEqual(creatureSkill))
        return false;

    const CreatureSkillSkeletonArmy* skill = dynamic_cast<const CreatureSkillSkeletonArmy*>(&creatureSkill);
    if(skill == nullptr)
        return false;

    if(mCreatureLevelMin != skill->mCreatureLevelMin)
        return false;
    if(mNbCreatures != skill->mNbCreatures)
        return false;
    if(mEffectDuration != skill->mEffectDuration)
        return false;
    if(mCreatureClass != skill->mCreatureClass)
        return false;

    return true;
}
