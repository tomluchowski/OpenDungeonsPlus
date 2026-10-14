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

//! \brief The chance that a creature dodges or parries a melee blow. Pure numbers, no game state: every value
//! is a parameter (the game reads them from global.cfg), chances are in percent (0 to 100).
namespace DefenceChance
{
//! What happened to a blow before its damage was calculated
enum Outcome
{
    none = 0,   //!< the blow lands as usual
    dodged = 1, //!< the defender got out of the way, no damage
    parried = 2 //!< the defender stopped the blow with its weapon, no damage
};

//! Dodging: every creature, base chance plus a share per level, limited by max
inline double dodgeChance(uint32_t level, double base, double perLevel, double max)
{
    return std::max(0.0, std::min(max, base + perLevel * static_cast<double>(level)));
}

//! Parrying: only a creature that carries a weapon. With a shield as well the shield values are used.
//! A shield alone does not parry.
inline double parryChance(uint32_t level, bool hasWeapon, bool hasShield,
        double base, double perLevel, double max,
        double shieldBase, double shieldPerLevel, double shieldMax)
{
    if(!hasWeapon)
        return 0.0;

    if(hasShield)
        return std::max(0.0, std::min(shieldMax, shieldBase + shieldPerLevel * static_cast<double>(level)));

    return std::max(0.0, std::min(max, base + perLevel * static_cast<double>(level)));
}

//! Dodging is rolled first; only when the defender did not dodge, parrying is rolled. The rolls are
//! random numbers in [0, 100) (the server draws them, never a client).
inline Outcome decide(double dodgePercent, double parryPercent, double dodgeRoll, double parryRoll)
{
    if(dodgeRoll < dodgePercent)
        return dodged;

    if(parryRoll < parryPercent)
        return parried;

    return none;
}

//! Weapon kinds by the name of the mesh (lower case), same keywords as the client uses to show the weapon
inline bool contains(const std::string& text, const char* part)
{
    return text.find(part) != std::string::npos;
}

//! Returns true if the mesh name (lower case) is that of a shield
inline bool isShieldMesh(const std::string& meshLower)
{
    return contains(meshLower, "shield");
}

//! Any weapon that strikes (sword, axe, hammer, mace, spear, lance, pike, dagger, knife and every other mesh)
//! can parry. A shield, a bow or crossbow and a staff or wand cannot.
inline bool isParryWeaponMesh(const std::string& meshLower)
{
    if(isShieldMesh(meshLower) || contains(meshLower, "bow"))
        return false;

    if(contains(meshLower, "staff") || contains(meshLower, "wand"))
        return false;

    return true;
}
} // namespace DefenceChance

#endif // DEFENCECHANCE_H
