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

//! \brief Defines held equipment damage ranges, defenses, ratings and weapon recovery.
class Weapon
{
public:
    //! \brief Creates an empty weapon with the given name: no damage, no defense, no cooldown and no own
    //! parry values (the global ones apply).
    Weapon(const std::string& name) :
       mName(name),
       mPhysicalDamage(0.0),
       mMagicalDamage(0.0),
       mElementDamage(0.0),
       mPhysicalDamageMin(-1.0), mMagicalDamageMin(-1.0), mElementDamageMin(-1.0),
       mAttackRating(0.0), mDefenseRating(0.0),
       mPhysicalDefense(0.0),
       mMagicalDefense(0.0),
       mElementDefense(0.0),
       mCooldownNbTurns(0),
       mParryBase(-1.0),
       mParryPerLevel(-1.0),
       mParryMax(-1.0)
    {}

    //! \brief Creates an empty unnamed weapon (filled from a packet): no damage, no defense, no cooldown and
    //! no own parry values.
    Weapon() :
       mPhysicalDamage(0.0),
       mMagicalDamage(0.0),
       mElementDamage(0.0),
       mPhysicalDamageMin(-1.0), mMagicalDamageMin(-1.0), mElementDamageMin(-1.0),
       mAttackRating(0.0), mDefenseRating(0.0),
       mPhysicalDefense(0.0),
       mMagicalDefense(0.0),
       mElementDefense(0.0),
       mCooldownNbTurns(0),
       mParryBase(-1.0),
       mParryPerLevel(-1.0),
       mParryMax(-1.0)
    {}

    virtual ~Weapon()
    {
    }

    //! \brief Loads a definition from the equipment file sub [Equipment][/Equipment] part
    //! \returns A Weapon if valid, nullptr otherwise.
    static Weapon* load(std::stringstream& defFile);
    //! \brief Reads the stats of one [Equipment] block into the given weapon, including the optional
    //! CooldownNbTurns field (missing = no weapon cooldown) and the optional ParryBase, ParryPerLevel and ParryMax fields.
    //! Optional damage minima default to the maxima; optional attack and defense ratings default to zero.
    //! \returns true if the block is valid, false otherwise.
    static bool update(Weapon* weapon, std::stringstream& defFile);
    //! \brief Writes the differences between def1 and def2 in the given file. Note that def1 can be null. In
    //! this case, every parameters in def2 will be written (CooldownNbTurns only if it is not 0, the parry values only if they are set).
    //! Otherwise CooldownNbTurns and the parry values are written only when they differ from def1. def2 cannot be null.
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

    //! \brief Minimum physical damage, defaulting to the maximum for legacy equipment.
    double getPhysicalDamageMin() const { return mPhysicalDamageMin < 0.0 ? mPhysicalDamage : mPhysicalDamageMin; }
    //! \brief Minimum magical damage, defaulting to the maximum for legacy equipment.
    double getMagicalDamageMin() const { return mMagicalDamageMin < 0.0 ? mMagicalDamage : mMagicalDamageMin; }
    //! \brief Minimum elemental damage, defaulting to the maximum for legacy equipment.
    double getElementDamageMin() const { return mElementDamageMin < 0.0 ? mElementDamage : mElementDamageMin; }
    //! \brief Equipment contribution to the attack rating.
    double getAttackRating() const { return mAttackRating; }
    //! \brief Equipment contribution to the defense rating.
    double getDefenseRating() const { return mDefenseRating; }

    inline double getPhysicalDefense() const
    { return mPhysicalDefense; }

    inline double getMagicalDefense() const
    { return mMagicalDefense; }

    inline double getElementDefense() const
    { return mElementDefense; }

    //! \brief Number of turns the melee cooldown lasts at level 1 when a creature carries this weapon; the
    //! creature level then shortens it (see Creature::getAppliedCooldownNbTurns).
    //! 0 means the weapon defines no cooldown (shields and other non-weapons), so it is ignored when the
    //! cooldown is chosen. When a creature carries two weapons, the larger value is used.
    inline uint32_t getCooldownNbTurns() const
    { return mCooldownNbTurns; }

    //! \brief Parry chance in percent of a creature that carries this weapon (without shield), base value.
    //! A weapon without its own ParryBase in the equipment file uses the global ParryBase.
    double getParryBase() const;

    //! \brief Parry chance in percent that every creature level adds for this weapon.
    //! A weapon without its own ParryPerLevel uses the global ParryPerLevel.
    double getParryPerLevel() const;

    //! \brief Highest parry chance in percent for this weapon.
    //! A weapon without its own ParryMax uses the global ParryMax.
    double getParryMax() const;

    //! \brief Writes the weapon to a packet, including cooldown, parry, damage minima and rating values.
    friend ODPacket& operator <<(ODPacket& os, const Weapon *weapon);
    //! \brief Reads the weapon from a packet in the order written by operator <<.
    friend ODPacket& operator >>(ODPacket& is, Weapon *weapon);

private:
    std::string     mName;
    //! \brief the Weapon name this class extends. Can be empty if no class extended
    std::string     mBaseDefinition;
    std::string     mMeshName;
    double          mPhysicalDamage;
    double          mMagicalDamage;
    double          mElementDamage;
    //! \brief Optional minimum channel damage; negative means use the maximum.
    double mPhysicalDamageMin, mMagicalDamageMin, mElementDamageMin;
    //! \brief Optional equipment rating contributions.
    double mAttackRating, mDefenseRating;
    double          mPhysicalDefense;
    double          mMagicalDefense;
    double          mElementDefense;
    //! \brief Melee cooldown in turns when carried (optional CooldownNbTurns field, 0 = none)
    uint32_t        mCooldownNbTurns;
    //! \brief Parry values in percent of the optional ParryBase / ParryPerLevel / ParryMax fields (negative = not set,
    //! the global values apply)
    double          mParryBase;
    double          mParryPerLevel;
    double          mParryMax;
};

#endif // WEAPON_H
