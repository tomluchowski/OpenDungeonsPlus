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


#include "creatureeffect/CreatureEffectTemporary.h"

#include "creatureeffect/CreatureEffectManager.h"
#include "entities/Creature.h"
#include "entities/Tile.h"

static const std::string CreatureEffectTemporaryName = "Temporary";

namespace
{
class CreatureEffectTemporaryFactory : public CreatureEffectFactory
{
    CreatureEffect* createCreatureEffect() const override
    { return new CreatureEffectTemporary; }

    const std::string& getCreatureEffectName() const override
    {
        return CreatureEffectTemporaryName;
    }
};

// Register the factory
static CreatureEffectRegister reg(new CreatureEffectTemporaryFactory);
}

const std::string& CreatureEffectTemporary::getEffectName() const
{
    return CreatureEffectTemporaryName;
}

void CreatureEffectTemporary::applyEffect(Creature& creature)
{
    // The time is up (the counter was decreased just before this call), the creature falls apart
    if(getNbTurnsEffect() > 0)
        return;

    if(!creature.isAlive())
        return;

    creature.takeDamage(nullptr, creature.getHP(), 0.0, 0.0, 0.0, creature.getPositionTile(), false);
}

CreatureEffectTemporary* CreatureEffectTemporary::load(std::istream& is)
{
    CreatureEffectTemporary* effect = new CreatureEffectTemporary;
    effect->importFromStream(is);
    return effect;
}
