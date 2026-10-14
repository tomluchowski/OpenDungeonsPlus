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


#include "creatureeffect/CreatureEffectAreaDamage.h"

#include "creatureeffect/CreatureEffectManager.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"

#include <vector>

static const std::string CreatureEffectAreaDamageName = "AreaDamage";

namespace
{
class CreatureEffectAreaDamageFactory : public CreatureEffectFactory
{
    CreatureEffect* createCreatureEffect() const override
    { return new CreatureEffectAreaDamage; }

    const std::string& getCreatureEffectName() const override
    {
        return CreatureEffectAreaDamageName;
    }
};

// Register the factory
static CreatureEffectRegister reg(new CreatureEffectAreaDamageFactory);
}

const std::string& CreatureEffectAreaDamage::getEffectName() const
{
    return CreatureEffectAreaDamageName;
}

void CreatureEffectAreaDamage::applyEffect(Creature& creature)
{
    // The zone only exists on the server and while its caster is alive
    if(!creature.isAlive() || !creature.getIsOnServerMap())
        return;

    const double radiusSquared = mRadius * mRadius;
    // We copy the list as dying creatures might be removed from the map
    std::vector<Creature*> creatures = creature.getGameMap()->getCreatures();
    for(Creature* target : creatures)
    {
        if(!target->isAlive())
            continue;

        if(target->getSeat()->isAlliedSeat(creature.getSeat()))
            continue;

        Tile* targetTile = target->getPositionTile();
        if(targetTile == nullptr)
            continue;

        double dx = static_cast<double>(targetTile->getX() - mTileX);
        double dy = static_cast<double>(targetTile->getY() - mTileY);
        if(((dx * dx) + (dy * dy)) > radiusSquared)
            continue;

        target->takeDamage(&creature, mDamagePerTurn, 0.0, 0.0, 0.0, targetTile, false);
    }
}

CreatureEffectAreaDamage* CreatureEffectAreaDamage::load(std::istream& is)
{
    CreatureEffectAreaDamage* effect = new CreatureEffectAreaDamage;
    effect->importFromStream(is);
    return effect;
}

void CreatureEffectAreaDamage::exportToStream(std::ostream& os) const
{
    CreatureEffect::exportToStream(os);
    os << "\t" << mTileX;
    os << "\t" << mTileY;
    os << "\t" << mRadius;
    os << "\t" << mDamagePerTurn;
}

bool CreatureEffectAreaDamage::importFromStream(std::istream& is)
{
    if(!CreatureEffect::importFromStream(is))
        return false;
    if(!(is >> mTileX))
        return false;
    if(!(is >> mTileY))
        return false;
    if(!(is >> mRadius))
        return false;
    if(!(is >> mDamagePerTurn))
        return false;

    return true;
}
