#ifndef COMBATDAMAGE_H
#define COMBATDAMAGE_H

#include "entities/Creature.h"
#include "entities/CreatureProgression.h"
#include "entities/Weapon.h"
#include "utils/Random.h"

#include <algorithm>
#include <istream>
#include <string>

//! \brief Shared creature damage range parsing and the single roll used by all damage channels.
namespace CombatDamage
{
//! \brief Reads a marked minimum without consuming the next effect or a legacy save suffix.
inline bool readEffectMinimum(std::istream& is, double& minimum)
{
    const std::streampos position = is.tellg();
    std::string marker;
    if(!(is >> marker))
    {
        is.clear();
        return true;
    }
    if(marker != "DamageRangeMin")
    {
        is.seekg(position);
        return static_cast<bool>(is);
    }
    return static_cast<bool>(is >> minimum);
}

//! \brief Applies the creature curve and carried weapon ranges before any target defenses are subtracted.
inline void scale(const Creature& attacker, double& phyMin, double& phyMax,
    double& magMin, double& magMax, double& eleMin, double& eleMax)
{
    const double multiplier = CreatureProgression::damageMultiplier(attacker.getLevel());
    phyMin *= multiplier;
    phyMax *= multiplier;
    magMin *= multiplier;
    magMax *= multiplier;
    eleMin *= multiplier;
    eleMax *= multiplier;
    const Weapon* weapons[] = {attacker.getWeaponL(), attacker.getWeaponR()};
    for(const Weapon* weapon : weapons)
    {
        if(weapon == nullptr)
            continue;
        phyMin += weapon->getPhysicalDamageMin();
        phyMax += weapon->getPhysicalDamage();
        magMin += weapon->getMagicalDamageMin();
        magMax += weapon->getMagicalDamage();
        eleMin += weapon->getElementDamageMin();
        eleMax += weapon->getElementDamage();
    }
}

//! \brief Reads an optional appended six-column range, retaining the legacy values at end of line.
inline bool readRange(std::istream& is, double& phyMin, double& phyMax,
    double& magMin, double& magMax, double& eleMin, double& eleMax)
{
    is >> std::ws;
    if(is.eof())
    {
        is.clear();
        return true;
    }
    return static_cast<bool>(is >> phyMin >> phyMax >> magMin >> magMax >> eleMin >> eleMax);
}

//! \brief Rolls damage after subtracting each channel defense, sharing one random fraction across channels.
inline double roll(const Creature& target, double phyMin, double phyMax,
    double magMin, double magMax, double eleMin, double eleMax, double* rawDamage = nullptr)
{
    const double u = Random::Double(0.0, 1.0);
    if(rawDamage != nullptr)
        *rawDamage = phyMin + magMin + eleMin
            + u * (phyMax + magMax + eleMax - phyMin - magMin - eleMin);
    const double physicalLo = std::max(phyMin - target.getPhysicalDefense(), 0.0);
    const double magicalLo = std::max(magMin - target.getMagicalDefense(), 0.0);
    const double elementalLo = std::max(eleMin - target.getElementDefense(), 0.0);
    const double lo = physicalLo + magicalLo + elementalLo;
    const double hi = std::max(phyMax - target.getPhysicalDefense(), 0.0)
        + std::max(magMax - target.getMagicalDefense(), 0.0)
        + std::max(eleMax - target.getElementDefense(), 0.0);
    return lo + u * (hi - lo);
}
}

#endif // COMBATDAMAGE_H
