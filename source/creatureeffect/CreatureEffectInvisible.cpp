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

#include "creatureeffect/CreatureEffectInvisible.h"

#include "creatureeffect/CreatureEffectManager.h"
#include "entities/Creature.h"

static const std::string CreatureEffectInvisibleName = "Invisible";

namespace
{
//! \brief Creates invisible effects when the configuration names "Invisible"
class CreatureEffectInvisibleFactory : public CreatureEffectFactory
{
    //! \brief Creates an empty invisible effect
    CreatureEffect* createCreatureEffect() const override
    { return new CreatureEffectInvisible; }

    //! \brief The effect name used in the configuration
    const std::string& getCreatureEffectName() const override
    {
        return CreatureEffectInvisibleName;
    }
};

// Register the factory
static CreatureEffectRegister reg(new CreatureEffectInvisibleFactory);
}

const std::string& CreatureEffectInvisible::getEffectName() const
{
    return CreatureEffectInvisibleName;
}

void CreatureEffectInvisible::applyEffect(Creature& creature)
{
    // The invisible state is deduced from the effect, nothing to update when it ends
}

CreatureEffectInvisible* CreatureEffectInvisible::load(std::istream& is)
{
    CreatureEffectInvisible* effect = new CreatureEffectInvisible;
    effect->importFromStream(is);
    return effect;
}
