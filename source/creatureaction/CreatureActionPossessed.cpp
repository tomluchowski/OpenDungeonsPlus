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
#include "entities/CreatureDefinition.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "gamemap/LevelScript.h"
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

    // The cast price covers the first seconds. After that the player pays each turn the share of
    // the drain per second of the creature type. The possession ends when the mana cannot pay
    // one second of it
    // A level that starts with a scripted possession does not charge for it
    if(creature.getGameMap()->getLevelScript().isFreePossession())
        return false;

    uint32_t turns = creature.nextPossessionTurn();
    double freeSeconds = ConfigManager::getSingleton().getSpellConfigDouble("PossessFreeSeconds");
    if(static_cast<double>(turns) <= (freeSeconds * ODApplication::turnsPerSecond))
        return false;

    double drainPerSecond = creature.getDefinition()->getPossessManaCost();
    double drainPerTurn = drainPerSecond / ODApplication::turnsPerSecond;
    if((creature.getSeat()->getMana() < drainPerSecond) || !creature.getSeat()->takeMana(drainPerTurn))
    {
        creature.endPossession();
        return false;
    }

    return false;
}
