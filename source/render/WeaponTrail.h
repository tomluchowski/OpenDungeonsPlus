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

#ifndef WEAPONTRAIL_H
#define WEAPONTRAIL_H

#include <string>

class Creature;
class CreatureReactions;

/*! \brief A short bright streak along the path of the weapon tip during a strong melee blow. It fades at once.
 *
 * Client side and cosmetic only: it is started from the hit result the server reported (the blow was a strong hit,
 * see CreatureWeaponVisuals::getLastHit), never guessed. No game state, timing, message or save is involved. The
 * streak is a billboard chain in the scene and exists only in the option 'Creature reactions: full'. Without the
 * server event (old server) nothing is shown. The values (duration, colour, width, length, minimum strength, number
 * of streaks at the same time, on/off) are in config/creatureReactions.cfg.
 */
namespace WeaponTrail
{
//! \brief A strong blow of the attacker lands after delay seconds: the streak follows the weapon tip around then
void noteStrongBlow(CreatureReactions& reactions, Creature* attacker, double delay);

//! \brief Samples the weapon tips, fades and removes the streaks (also removes all of them when the option is off)
void update(CreatureReactions& reactions, double timeSinceLastFrame);

//! \brief Removes the streak of the creature (the creature is destroyed or its weapons are rebuilt)
void removeCreature(const std::string& creatureName);

//! \brief Removes all the streaks
void stopAll();
} // namespace WeaponTrail

#endif // WEAPONTRAIL_H
