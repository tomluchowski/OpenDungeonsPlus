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

#include "creatureeffect/CreatureEffectDefector.h"

#include "creatureeffect/CreatureEffectManager.h"
#include "entities/Creature.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

static const std::string CreatureEffectDefectorName = "Defector";

namespace
{
class CreatureEffectDefectorFactory : public CreatureEffectFactory
{
    CreatureEffect* createCreatureEffect() const override
    { return new CreatureEffectDefector; }

    const std::string& getCreatureEffectName() const override
    {
        return CreatureEffectDefectorName;
    }
};

// Register the factory
static CreatureEffectRegister reg(new CreatureEffectDefectorFactory);
}

const std::string& CreatureEffectDefector::getEffectName() const
{
    return CreatureEffectDefectorName;
}

void CreatureEffectDefector::applyEffect(Creature& creature)
{
    // The creature only changes back when the effect ends
    if(mNbTurnsEffect > 0)
        return;

    if(!creature.isAlive())
        return;

    // If something else changed the creature seat in the meantime, we do not interfere
    if(creature.getSeat()->getId() != mNewSeatId)
        return;

    // We wait until the creature is back on the map (for example if it is held in a hand)
    if(!creature.getIsOnMap())
    {
        mNbTurnsEffect = 1;
        return;
    }

    Seat* originalSeat = creature.getGameMap()->getSeatById(mOriginalSeatId);
    if(originalSeat == nullptr)
    {
        OD_LOG_ERR("creature=" + creature.getName() + ", wrong originalSeatId=" + Helper::toString(mOriginalSeatId));
        return;
    }

    creature.changeSeat(originalSeat);
}

CreatureEffectDefector* CreatureEffectDefector::load(std::istream& is)
{
    CreatureEffectDefector* effect = new CreatureEffectDefector;
    effect->importFromStream(is);
    return effect;
}

void CreatureEffectDefector::exportToStream(std::ostream& os) const
{
    CreatureEffect::exportToStream(os);
    os << "\t" << mOriginalSeatId << "\t" << mNewSeatId;
}

bool CreatureEffectDefector::importFromStream(std::istream& is)
{
    if(!CreatureEffect::importFromStream(is))
        return false;
    if(!(is >> mOriginalSeatId))
        return false;
    if(!(is >> mNewSeatId))
        return false;

    return true;
}
