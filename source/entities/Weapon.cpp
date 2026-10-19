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

#include "entities/Weapon.h"

#include "network/ODPacket.h"

#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <fstream>

//! \brief Loads one equipment definition and rejects invalid fields.
Weapon* Weapon::load(std::stringstream& defFile)
{
    if (!defFile.good())
        return nullptr;

    Weapon* weapon = new Weapon();
    if(!update(weapon, defFile))
    {
        delete weapon;
        weapon = nullptr;
    }
    return weapon;
}
//! \brief Updates an equipment definition, including optional type and occupied hands.
bool Weapon::update(Weapon* weapon, std::stringstream& defFile)
{
    std::string nextParam;
    bool exit = false;
    // Parameters that should not be overriden if a Creature definition is extended. They will be set after
    // the class is copied if there is a base class
    std::string name = weapon->mName;
    std::string baseDefinition;
    while (defFile.good())
    {
        if (exit)
            break;

        if(!(defFile >> nextParam))
            break;

        if (nextParam == "[/Equipment]" || nextParam == "[/EquipmentDefinitions]")
            break;

        if (nextParam == "Name")
        {
            defFile >> name;
            continue;
        }

        if (nextParam == "BaseDefinition")
        {
            defFile >> baseDefinition;
            const Weapon* def = ConfigManager::getSingleton().getWeapon(baseDefinition);
            if(def == nullptr)
            {
                OD_LOG_ERR("Couldn't find base class " + baseDefinition);
                return false;
            }

            *weapon = *def;
            continue;
        }

        if (nextParam != "[Stats]")
            continue;

        while (defFile.good())
        {
            if (exit)
                break;

            if(!(defFile >> nextParam))
                break;

            if (nextParam == "[/Stats]")
                break;

            // Handle ill-formed files.
            if (nextParam == "[/Equipment]" || nextParam == "[/EquipmentDefinitions]")
            {
                exit = true;
                break;
            }

            if (nextParam == "PhysicalDamageMin")
            {
                defFile >> weapon->mPhysicalDamageMin;
                continue;
            }

            if (nextParam == "MagicalDamageMin")
            {
                defFile >> weapon->mMagicalDamageMin;
                continue;
            }

            if (nextParam == "ElementDamageMin")
            {
                defFile >> weapon->mElementDamageMin;
                continue;
            }

            if (nextParam == "AttackRating")
            {
                defFile >> weapon->mAttackRating;
                continue;
            }

            if (nextParam == "DefenseRating")
            {
                defFile >> weapon->mDefenseRating;
                continue;
            }

            if (nextParam == "WeaponType")
            {
                defFile >> weapon->mWeaponType;
                continue;
            }

            if (nextParam == "Hands")
            {
                defFile >> weapon->mHands;
                if(weapon->mHands != 1 && weapon->mHands != 2)
                    return false;
                continue;
            }

            if (nextParam == "MeshName")
            {
                defFile >> nextParam;
                weapon->mMeshName = nextParam;
                continue;
            }

            if (nextParam == "PhysicalDamage")
            {
                defFile >> nextParam;
                weapon->mPhysicalDamage = Helper::toDouble(nextParam);
                continue;
            }

            if (nextParam == "MagicalDamage")
            {
                defFile >> nextParam;
                weapon->mMagicalDamage = Helper::toDouble(nextParam);
                continue;
            }

            if (nextParam == "ElementDamage")
            {
                defFile >> nextParam;
                weapon->mElementDamage = Helper::toDouble(nextParam);
                continue;
            }

            if (nextParam == "PhysicalDefense")
            {
                defFile >> nextParam;
                weapon->mPhysicalDefense = Helper::toDouble(nextParam);
                continue;
            }

            if (nextParam == "MagicalDefense")
            {
                defFile >> nextParam;
                weapon->mMagicalDefense = Helper::toDouble(nextParam);
                continue;
            }

            if (nextParam == "ElementDefense")
            {
                defFile >> nextParam;
                weapon->mElementDefense = Helper::toDouble(nextParam);
                continue;
            }

            if (nextParam == "CooldownNbTurns")
            {
                defFile >> nextParam;
                weapon->mCooldownNbTurns = Helper::toUInt32(nextParam);
                continue;
            }

            if (nextParam == "ParryBase")
            {
                defFile >> nextParam;
                weapon->mParryBase = Helper::toDouble(nextParam);
                continue;
            }

            if (nextParam == "ParryPerLevel")
            {
                defFile >> nextParam;
                weapon->mParryPerLevel = Helper::toDouble(nextParam);
                continue;
            }

            if (nextParam == "ParryMax")
            {
                defFile >> nextParam;
                weapon->mParryMax = Helper::toDouble(nextParam);
                continue;
            }
        }
    }

    if (name.empty() || weapon->mMeshName.empty())
    {
        OD_LOG_ERR("name=" + name + ", weapon mesh name=" + weapon->mMeshName);
        return false;
    }

    weapon->mName = name;
    weapon->mBaseDefinition = baseDefinition;

    return true;
}

//! \brief Writes equipment differences, preserving type and occupied hands.
void Weapon::writeWeaponDiff(const Weapon* def1, const Weapon* def2, std::ofstream& file)
{
    file << "[Equipment]" << std::endl;
    file << "    Name\t" << def2->mName << std::endl;
    if(!def2->mBaseDefinition.empty())
    {
        // If there is a base definition, we take it as the reference no matter what def1 is because
        // we want to write only the differences between reference and def2
        def1 = ConfigManager::getSingleton().getWeapon(def2->mBaseDefinition);
        OD_ASSERT_TRUE_MSG(def1 != nullptr, "BaseDefinition=" + def2->mBaseDefinition);
        file << "    BaseDefinition\t" << def2->mBaseDefinition << std::endl;
    }
    file << "    [Stats]" << std::endl;

    if(def1 == nullptr || (def1->mMeshName.compare(def2->mMeshName) != 0))
        file << "    MeshName\t" << def2->mMeshName << std::endl;

    if(def1 == nullptr || def1->mWeaponType != def2->mWeaponType ||
        def1->getWeaponType() != def2->getWeaponType())
        file << "    WeaponType\t" << def2->getWeaponType() << std::endl;
    if(def1 == nullptr || def1->mHands != def2->mHands)
        file << "    Hands\t" << def2->mHands << std::endl;

    if(def1 == nullptr || (def1->mPhysicalDamage != def2->mPhysicalDamage))
        file << "    PhysicalDamage\t" << def2->mPhysicalDamage << std::endl;

    if(def1 == nullptr || (def1->mMagicalDamage != def2->mMagicalDamage))
        file << "    MagicalDamage\t" << def2->mMagicalDamage << std::endl;

    if(def1 == nullptr || (def1->mElementDamage != def2->mElementDamage))
        file << "    ElementDamage\t" << def2->mElementDamage << std::endl;

    if(def1 == nullptr || (def1->mPhysicalDefense != def2->mPhysicalDefense))
        file << "    PhysicalDefense\t" << def2->mPhysicalDefense << std::endl;

    if(def1 == nullptr || (def1->mMagicalDefense != def2->mMagicalDefense))
        file << "    MagicalDefense\t" << def2->mMagicalDefense << std::endl;

    if(def1 == nullptr || (def1->mElementDefense != def2->mElementDefense))
        file << "    ElementDefense\t" << def2->mElementDefense << std::endl;

    // The field is optional: non-weapons (0) are never written without a differing base value
    const uint32_t cooldownRef = (def1 == nullptr) ? 0 : def1->mCooldownNbTurns;
    if(cooldownRef != def2->mCooldownNbTurns)
        file << "    CooldownNbTurns\t" << def2->mCooldownNbTurns << std::endl;

    // The parry values are optional too: a negative value means "not set" and is only written when it differs
    const double parryBaseRef = (def1 == nullptr) ? -1.0 : def1->mParryBase;
    if(parryBaseRef != def2->mParryBase)
        file << "    ParryBase\t" << def2->mParryBase << std::endl;

    const double parryPerLevelRef = (def1 == nullptr) ? -1.0 : def1->mParryPerLevel;
    if(parryPerLevelRef != def2->mParryPerLevel)
        file << "    ParryPerLevel\t" << def2->mParryPerLevel << std::endl;

    const double parryMaxRef = (def1 == nullptr) ? -1.0 : def1->mParryMax;
    if(parryMaxRef != def2->mParryMax)
        file << "    ParryMax\t" << def2->mParryMax << std::endl;

    if(def1 == nullptr || def1->mPhysicalDamageMin != def2->mPhysicalDamageMin)
        file << "    PhysicalDamageMin\t" << def2->mPhysicalDamageMin << std::endl;

    if(def1 == nullptr || def1->mMagicalDamageMin != def2->mMagicalDamageMin)
        file << "    MagicalDamageMin\t" << def2->mMagicalDamageMin << std::endl;

    if(def1 == nullptr || def1->mElementDamageMin != def2->mElementDamageMin)
        file << "    ElementDamageMin\t" << def2->mElementDamageMin << std::endl;

    if(def1 == nullptr || def1->mAttackRating != def2->mAttackRating)
        file << "    AttackRating\t" << def2->mAttackRating << std::endl;

    if(def1 == nullptr || def1->mDefenseRating != def2->mDefenseRating)
        file << "    DefenseRating\t" << def2->mDefenseRating << std::endl;

    file << "    [/Stats]" << std::endl;
    file << "[/Equipment]" << std::endl;
}

//! \brief Parry base value of this weapon in percent (0 - 100), the global value if the weapon sets none.
double Weapon::getParryBase() const
{
    if(mParryBase < 0.0)
        return ConfigManager::getSingleton().getParryBase();

    return std::max(0.0, std::min(100.0, mParryBase));
}

//! \brief Parry value per creature level of this weapon in percent (0 - 10), the global value if the weapon sets none.
double Weapon::getParryPerLevel() const
{
    if(mParryPerLevel < 0.0)
        return ConfigManager::getSingleton().getParryPerLevel();

    return std::max(0.0, std::min(10.0, mParryPerLevel));
}

//! \brief Highest parry value of this weapon in percent (0 - 100), the global value if the weapon sets none.
double Weapon::getParryMax() const
{
    if(mParryMax < 0.0)
        return ConfigManager::getSingleton().getParryMax();

    return std::max(0.0, std::min(100.0, mParryMax));
}

//! \brief Streams equipment values, type and occupied hands to the client.
ODPacket& operator <<(ODPacket& os, const Weapon *weapon)
{
    os << weapon->mName;
    os << weapon->mBaseDefinition;
    os << weapon->mMeshName;
    os << weapon->mPhysicalDamage;
    os << weapon->mMagicalDamage;
    os << weapon->mElementDamage;
    os << weapon->mPhysicalDefense;
    os << weapon->mMagicalDefense;
    os << weapon->mElementDefense;
    os << weapon->mCooldownNbTurns;
    os << weapon->mParryBase;
    os << weapon->mParryPerLevel;
    os << weapon->mParryMax;
    os << weapon->mPhysicalDamageMin;
    os << weapon->mMagicalDamageMin;
    os << weapon->mElementDamageMin;
    os << weapon->mAttackRating;
    os << weapon->mDefenseRating;
    os << weapon->mWeaponType;
    os << weapon->mHands;
    return os;
}

//! \brief Reads equipment values in the matching wire order.
ODPacket& operator >>(ODPacket& is, Weapon *weapon)
{
    OD_ASSERT_TRUE(is >> weapon->mName);
    OD_ASSERT_TRUE(is >> weapon->mBaseDefinition);
    OD_ASSERT_TRUE(is >> weapon->mMeshName);
    OD_ASSERT_TRUE(is >> weapon->mPhysicalDamage);
    OD_ASSERT_TRUE(is >> weapon->mMagicalDamage);
    OD_ASSERT_TRUE(is >> weapon->mElementDamage);
    OD_ASSERT_TRUE(is >> weapon->mPhysicalDefense);
    OD_ASSERT_TRUE(is >> weapon->mMagicalDefense);
    OD_ASSERT_TRUE(is >> weapon->mElementDefense);
    OD_ASSERT_TRUE(is >> weapon->mCooldownNbTurns);
    OD_ASSERT_TRUE(is >> weapon->mParryBase);
    OD_ASSERT_TRUE(is >> weapon->mParryPerLevel);
    OD_ASSERT_TRUE(is >> weapon->mParryMax);
    OD_ASSERT_TRUE(is >> weapon->mPhysicalDamageMin);
    OD_ASSERT_TRUE(is >> weapon->mMagicalDamageMin);
    OD_ASSERT_TRUE(is >> weapon->mElementDamageMin);
    OD_ASSERT_TRUE(is >> weapon->mAttackRating);
    OD_ASSERT_TRUE(is >> weapon->mDefenseRating);
    OD_ASSERT_TRUE(is >> weapon->mWeaponType);
    OD_ASSERT_TRUE(is >> weapon->mHands);
    return is;
}

//! \brief Uses the configured type or the former client mesh-keyword classification for old files.
std::string Weapon::getWeaponType() const
{
    if(!mWeaponType.empty())
        return mWeaponType;
    std::string mesh = mMeshName;
    std::transform(mesh.begin(), mesh.end(), mesh.begin(), [](unsigned char c) { return std::tolower(c); });
    if(mesh.find("shield") != std::string::npos) return "shield";
    if(mesh.find("bow") != std::string::npos) return "bow";
    if(mesh.find("axe") != std::string::npos) return "axe";
    if(mesh.find("hammer") != std::string::npos || mesh.find("mace") != std::string::npos) return "hammer";
    if(mesh.find("spear") != std::string::npos || mesh.find("lance") != std::string::npos || mesh.find("pike") != std::string::npos) return "spear";
    if(mesh.find("dagger") != std::string::npos || mesh.find("knife") != std::string::npos) return "dagger";
    if(mesh.find("staff") != std::string::npos || mesh.find("wand") != std::string::npos) return "staff";
    return "sword";
}
