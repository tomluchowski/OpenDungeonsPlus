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

#include "creatureeffect/CreatureEffectHexenHen.h"

#include "creatureeffect/CreatureEffectManager.h"
#include "entities/Creature.h"

static const std::string CreatureEffectHexenHenName = "HexenHen";

namespace
{
class CreatureEffectHexenHenFactory : public CreatureEffectFactory
{
    CreatureEffect* createCreatureEffect() const override
    { return new CreatureEffectHexenHen; }

    const std::string& getCreatureEffectName() const override
    {
        return CreatureEffectHexenHenName;
    }
};

// Register the factory
static CreatureEffectRegister reg(new CreatureEffectHexenHenFactory);
}

const std::string& CreatureEffectHexenHen::getEffectName() const
{
    return CreatureEffectHexenHenName;
}

void CreatureEffectHexenHen::applyEffect(Creature& creature)
{
    // The creature gets its normal form back when the effect ends. The state is
    // deduced from the effect, we only have to tell the clients that it changed.
    if(mNbTurnsEffect > 0)
        return;

    creature.requestRefresh();
}

CreatureEffectHexenHen* CreatureEffectHexenHen::load(std::istream& is)
{
    CreatureEffectHexenHen* effect = new CreatureEffectHexenHen;
    effect->importFromStream(is);
    return effect;
}
