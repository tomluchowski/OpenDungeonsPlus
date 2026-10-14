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

#include "creatureeffect/CreatureEffectBurn.h"

#include "creatureeffect/CreatureEffectManager.h"
#include "entities/Creature.h"
#include "entities/Tile.h"

static const std::string CreatureEffectBurnName = "Burn";

namespace
{
class CreatureEffectBurnFactory : public CreatureEffectFactory
{
    CreatureEffect* createCreatureEffect() const override
    { return new CreatureEffectBurn; }

    const std::string& getCreatureEffectName() const override
    {
        return CreatureEffectBurnName;
    }
};

// Register the factory
static CreatureEffectRegister reg(new CreatureEffectBurnFactory);
}

const std::string& CreatureEffectBurn::getEffectName() const
{
    return CreatureEffectBurnName;
}

void CreatureEffectBurn::applyEffect(Creature& creature)
{
    if(!creature.isAlive())
        return;

    Tile* posTile = creature.getPositionTile();
    creature.takeDamage(nullptr, mDamagePerTurn, 0.0, 0.0, 0.0, posTile, false);
}

CreatureEffectBurn* CreatureEffectBurn::load(std::istream& is)
{
    CreatureEffectBurn* effect = new CreatureEffectBurn;
    effect->importFromStream(is);
    return effect;
}

void CreatureEffectBurn::exportToStream(std::ostream& os) const
{
    CreatureEffect::exportToStream(os);
    os << "\t" << mDamagePerTurn;
}

bool CreatureEffectBurn::importFromStream(std::istream& is)
{
    if(!CreatureEffect::importFromStream(is))
        return false;
    if(!(is >> mDamagePerTurn))
        return false;

    return true;
}
