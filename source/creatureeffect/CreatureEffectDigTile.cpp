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

#include "creatureeffect/CreatureEffectDigTile.h"

#include "creatureeffect/CreatureEffectManager.h"
#include "entities/Creature.h"
#include "utils/LogManager.h"

static const std::string CreatureEffectDigTileName = "DigTile";

namespace
{
class CreatureEffectDigTileFactory : public CreatureEffectFactory
{
    CreatureEffect* createCreatureEffect() const override
    { return new CreatureEffectDigTile; }

    const std::string& getCreatureEffectName() const override
    {
        return CreatureEffectDigTileName;
    }
};

// Register the factory
static CreatureEffectRegister reg(new CreatureEffectDigTileFactory);
}

const std::string& CreatureEffectDigTile::getEffectName() const
{
    return CreatureEffectDigTileName;
}

void CreatureEffectDigTile::applyEffect(Creature& creature)
{

}

CreatureEffectDigTile* CreatureEffectDigTile::load(std::istream& is)
{
    CreatureEffectDigTile* effect = new CreatureEffectDigTile;
    effect->importFromStream(is);
    return effect;
}

void CreatureEffectDigTile::exportToStream(std::ostream& os) const
{
    CreatureEffect::exportToStream(os);
    os << "\t" << mEffectValue;
}

bool CreatureEffectDigTile::importFromStream(std::istream& is)
{
    if(!CreatureEffect::importFromStream(is))
        return false;
    if(!(is >> mEffectValue))
        return false;

    return true;
}
