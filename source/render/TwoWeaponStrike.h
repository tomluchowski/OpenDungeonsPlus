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

#ifndef TWOWEAPONSTRIKE_H
#define TWOWEAPONSTRIKE_H

#include <cstdint>
#include <string>

/*! \brief The pure rules for creatures that carry two attack weapons (one in each hand). Client side only and
 * cosmetic: which arm shows the blow. No game state, no timing, no damage and no network are involved.
 */
namespace TwoWeaponStrike
{
//! Values of the setting TwoWeaponMode of config/creatureReactions.cfg
const uint32_t MODE_OFF = 0;       //!< always the right arm (what a creature with one weapon does)
const uint32_t MODE_ALTERNATE = 1; //!< left, right, left, right ...
const uint32_t MODE_RANDOM = 2;    //!< a fixed pseudo random side per blow, never more than two in a row

//! A weapon kind (see weaponKind in CreatureCombatReactions.cpp) that strikes. A shield, a bow or a crossbow and a
//! staff or wand are no attack weapons for this rule.
inline bool isStrikeKind(const std::string& kind)
{
    return (kind == "Sword") || (kind == "Axe") || (kind == "Hammer") || (kind == "Spear") || (kind == "Dagger");
}

//! True if the blow with this number is struck with the left arm
inline bool isLeftBlow(uint32_t mode, uint32_t blowCounter)
{
    if(mode == MODE_ALTERNATE)
        return (blowCounter % 2) != 0;

    if(mode == MODE_RANDOM)
    {
        // A short cycle of 8 blows with at most two equal sides in a row
        static const bool PATTERN[8] = {false, true, true, false, true, false, false, true};
        return PATTERN[blowCounter % 8];
    }

    return false;
}
} // namespace TwoWeaponStrike

#endif // TWOWEAPONSTRIKE_H
