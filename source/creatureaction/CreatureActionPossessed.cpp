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

#include "creatureaction/CreatureActionPossessed.h"

#include "entities/Creature.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "ODApplication.h"
#include "utils/ConfigManager.h"

#include <functional>

std::function<bool()> CreatureActionPossessed::action()
{
    return std::bind(&CreatureActionPossessed::handlePossessed,
        std::ref(mCreature));
}

bool CreatureActionPossessed::handlePossessed(Creature& creature)
{
    if(!creature.isPossessed())
    {
        // Nobody controls the creature anymore, it goes back to its normal actions
        creature.popAction();
        return true;
    }

    // The possession ends when the creature cannot be controlled anymore
    if(!creature.isAlive() || creature.isKo() || !creature.getIsOnMap() || creature.isInPrison() ||
       (creature.getPossessor()->getSeat() != creature.getSeat()))
    {
        creature.endPossession();
        return false;
    }

    // The player pays for the possession each turn. When there is no mana left, it ends
    double drainPerSecond = ConfigManager::getSingleton().getSpellConfigDouble("PossessDrainPerSecond");
    double drainPerTurn = drainPerSecond / ODApplication::turnsPerSecond;
    if(!creature.getSeat()->takeMana(drainPerTurn))
    {
        creature.endPossession();
        return false;
    }

    return false;
}
