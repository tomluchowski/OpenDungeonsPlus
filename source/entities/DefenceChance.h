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

#ifndef DEFENCECHANCE_H
#define DEFENCECHANCE_H

#include <algorithm>
#include <cstdint>
#include <string>

//! \brief The chance that a creature avoids a landed blow or shot. Pure numbers, no game state: every value
//! is a parameter (the game reads them from global.cfg), chances are in percent (0 to 100).
namespace DefenceChance
{
//! What happened to a blow before its damage was calculated
enum Outcome
{
    none = 0,   //!< the blow lands as usual
    dodged = 1, //!< the defender got out of the way, no damage
    parried = 2, //!< the defender stopped the blow with its weapon, no damage
    shieldBlocked = 3 //!< the defender stopped the blow or shot with its shield, no damage
};

//! \brief Returns a configured avoidance chance at the creature's level, limited by its maximum.
inline double dodgeChance(uint32_t level, double base, double perLevel, double max)
{
    return std::max(0.0, std::min(max, base + perLevel * static_cast<double>(level)));
}

//! \brief Returns weapon parry chance only when a parry-capable weapon is carried.
inline double parryChance(uint32_t level, bool hasWeapon, double base, double perLevel, double max)
{
    if(!hasWeapon)
        return 0.0;

    return std::max(0.0, std::min(max, base + perLevel * static_cast<double>(level)));
}

//! \brief Resolves the creature type's one avoidance mechanism from one server roll in percent.
inline Outcome decide(Outcome kind, double chance, double roll)
{
    return roll < chance ? kind : none;
}

//! Hit formula coefficients and clamps, all expressed in percent.
static const double HIT_SCALE = 200.0;
static const double MIN_HIT_PERCENT = 5.0;
static const double MAX_HIT_PERCENT = 95.0;

//! \brief Returns the level-ratio hit chance for a creature attacking another creature.
inline double hitChance(double attackRating, double defenseRating, uint32_t attackerLevel, uint32_t defenderLevel)
{
    if(attackRating + defenseRating <= 0.0)
        return MIN_HIT_PERCENT;
    const double chance = HIT_SCALE * attackRating / (attackRating + defenseRating) *
        attackerLevel / static_cast<double>(attackerLevel + defenderLevel);
    return std::max(MIN_HIT_PERCENT, std::min(MAX_HIT_PERCENT, chance));
}

//! \brief Identifies held shields from the equipment type rather than its mesh.
inline bool isShieldType(const std::string& type)
{
    return type == "shield";
}

//! \brief Identifies the equipment types eligible for configured weapon parry.
inline bool isParryWeaponType(const std::string& type)
{
    return type == "sword" || type == "axe" || type == "dagger" || type == "club" || type == "mace" ||
        type == "hammer" || type == "scepter" || type == "spear" || type == "polearm";
}
} // namespace DefenceChance

#endif // DEFENCECHANCE_H
