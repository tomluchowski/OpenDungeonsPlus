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

#ifndef WEAPON_H
#define WEAPON_H

#include <cstdint>
#include <string>
#include <iosfwd>

class Creature;
class ODPacket;
class WeaponDefinition;

class Weapon
{
public:
    Weapon(const std::string& name) :
       mName(name),
       mPhysicalDamage(0.0),
       mMagicalDamage(0.0),
       mElementDamage(0.0),
       mPhysicalDefense(0.0),
       mMagicalDefense(0.0),
       mElementDefense(0.0),
       mCooldownNbTurns(0)
    {}

    Weapon() :
       mPhysicalDamage(0.0),
       mMagicalDamage(0.0),
       mElementDamage(0.0),
       mPhysicalDefense(0.0),
       mMagicalDefense(0.0),
       mElementDefense(0.0),
       mCooldownNbTurns(0)
    {}

    virtual ~Weapon()
    {
    }

    //! \brief Loads a definition from the equipment file sub [Equipment][/Equipment] part
    //! \returns A Weapon if valid, nullptr otherwise.
    static Weapon* load(std::stringstream& defFile);
    //! \brief Reads the stats of one [Equipment] block into the given weapon, including the optional
    //! CooldownNbTurns field (missing = no weapon cooldown).
    //! \returns true if the block is valid, false otherwise.
    static bool update(Weapon* weapon, std::stringstream& defFile);
    //! \brief Writes the differences between def1 and def2 in the given file. Note that def1 can be null. In
    //! this case, every parameters in def2 will be written (CooldownNbTurns only if it is not 0). def2 cannot be null.
    static void writeWeaponDiff(const Weapon* def1, const Weapon* def2, std::ofstream& file);

    inline const std::string getOgreNamePrefix() const
    { return "Weapon_"; }

    inline const std::string& getName() const
    { return mName; }

    inline const std::string& getMeshName() const
    { return mMeshName; }

    inline double getPhysicalDamage() const
    { return mPhysicalDamage; }

    inline double getMagicalDamage() const
    { return mMagicalDamage; }

    inline double getElementDamage() const
    { return mElementDamage; }

    inline double getPhysicalDefense() const
    { return mPhysicalDefense; }

    inline double getMagicalDefense() const
    { return mMagicalDefense; }

    inline double getElementDefense() const
    { return mElementDefense; }

    //! \brief Number of turns the melee cooldown lasts when a creature carries this weapon.
    //! 0 means the weapon defines no cooldown (shields and other non-weapons), so it is ignored when the
    //! cooldown is chosen. When a creature carries two weapons, the larger value is used.
    inline uint32_t getCooldownNbTurns() const
    { return mCooldownNbTurns; }

    friend ODPacket& operator <<(ODPacket& os, const Weapon *weapon);
    friend ODPacket& operator >>(ODPacket& is, Weapon *weapon);

private:
    std::string     mName;
    //! \brief the Weapon name this class extends. Can be empty if no class extended
    std::string     mBaseDefinition;
    std::string     mMeshName;
    double          mPhysicalDamage;
    double          mMagicalDamage;
    double          mElementDamage;
    double          mPhysicalDefense;
    double          mMagicalDefense;
    double          mElementDefense;
    //! \brief Melee cooldown in turns when carried (optional CooldownNbTurns field, 0 = none)
    uint32_t        mCooldownNbTurns;
};

#endif // WEAPON_H
