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

#include "creatureskill/CreatureSkillMeleeFight.h"

#include "creatureskill/CreatureSkillManager.h"
#include "entities/Creature.h"
#include "entities/CreatureProgression.h"
#include "entities/CombatDamage.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "entities/Weapon.h"
#include "gamemap/GameMap.h"
#include "network/CosmeticEvent.h"
#include "spells/Spell.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <algorithm>
#include <istream>

const std::string CreatureSkillMeleeFightName = "Melee";

//! Share of the damage below which a blow that did damage counts as glancing
static const double GLANCING_BLOW_SHARE = 0.34;
//! Value of a melee result event (see CosmeticEventType::meleeResult): the blow did its damage
static const int32_t MELEE_RESULT_HIT = 0;
//! Value of a melee result event: only a small part of the blow got through
static const int32_t MELEE_RESULT_GLANCING = 1;
//! Value of a melee result event: no damage at all
static const int32_t MELEE_RESULT_NO_DAMAGE = 2;
//! The share of the damage is sent in per mille
static const double PER_MILLE = 1000.0;
//! Added before the cast to an integer so that the share is rounded to the nearest per mille
static const double ROUND_HALF = 0.5;

namespace
{
class CreatureSkillMeleeFightFactory : public CreatureSkillFactory
{
    CreatureSkill* createCreatureSkill() const override
    { return new CreatureSkillMeleeFight; }

    const std::string& getCreatureSkillName() const override
    { return CreatureSkillMeleeFightName; }
};

// Register the factory
static CreatureSkillRegister reg(new CreatureSkillMeleeFightFactory);

//! Tells whether the creature carries a weapon in either hand.
bool hasWeapon(Creature* creature)
{
    return creature->getWeaponL() != nullptr || creature->getWeaponR() != nullptr;
}
}

const std::string& CreatureSkillMeleeFight::getSkillName() const
{
    return CreatureSkillMeleeFightName;
}

bool CreatureSkillMeleeFight::canBeUsedBy(const Creature* creature) const
{
    if(creature->getLevel() < mCreatureLevelMin)
        return false;

    return true;
}

//! \brief Resolves creature hit chance and typed avoidance without changing attack timing or building damage.
bool CreatureSkillMeleeFight::tryUseFight(GameMap& gameMap, Creature* creature, float range,
        GameEntity* attackedObject, Tile* attackedTile, bool ko, bool notifyPlayerIfHit) const
{
    const uint32_t level = creature->getLevel();
    double phyAtk = CreatureProgression::stat(mPhyAtk + mPhyAtkPerLvl, mPhyAtkPerLvl, level);
    double magAtk = CreatureProgression::stat(mMagAtk + mMagAtkPerLvl, mMagAtkPerLvl, level);
    double eleAtk = CreatureProgression::stat(mEleAtk + mEleAtkPerLvl, mEleAtkPerLvl, level);
    if(creature->getWeaponL() != nullptr)
    {
        phyAtk +=creature->getWeaponL()->getPhysicalDamage();
        magAtk +=creature->getWeaponL()->getMagicalDamage();
        eleAtk +=creature->getWeaponL()->getElementDamage();
    }
    if(creature->getWeaponR() != nullptr)
    {
        phyAtk +=creature->getWeaponR()->getPhysicalDamage();
        magAtk +=creature->getWeaponR()->getMagicalDamage();
        eleAtk +=creature->getWeaponR()->getElementDamage();
    }

    double creatureDamage = 0.0;
    double creatureRawDamage = 0.0;
    double modifier = creature->getModifierStrength();
    if(modifier != 1.0)
    {
        phyAtk *= modifier;
        magAtk *= modifier;
        eleAtk *= modifier;
    }
    // A hit roll comes first; only a landed blow allows the defender's configured avoidance kind.
    // The blow then does no damage; it is still made, so the animation, the warmup and the cooldown of the
    // attacker stay as they are, and takeDamage still runs with no damage for what it records besides the damage.
    DefenceChance::Outcome defence = DefenceChance::none;
    bool missed = false;
    if(attackedObject->getObjectType() == GameEntityType::creature)
    {
        Creature* target = static_cast<Creature*>(attackedObject);
        missed = !target->rollAttackHit(creature->getAttackRating(), creature->getLevel(), creature->isPossessed());
        if(!missed)
            defence = target->rollMeleeDefence(false);
        if(!missed && defence == DefenceChance::none)
        {
            double phyMin = mPhyMin, phyMax = mPhyMax;
            double magMin = mMagMin, magMax = mMagMax;
            double eleMin = mEleMin, eleMax = mEleMax;
            CombatDamage::scale(*creature, phyMin, phyMax, magMin, magMax, eleMin, eleMax);
            creatureDamage = CombatDamage::roll(*target,
                phyMin, phyMax, magMin, magMax, eleMin, eleMax, &creatureRawDamage) * modifier;
            creatureRawDamage *= modifier;
        }
        phyAtk = magAtk = eleAtk = 0.0;
    }
    if(missed || defence != DefenceChance::none)
    {
        creatureDamage = 0.0;
        phyAtk = 0.0;
        magAtk = 0.0;
        eleAtk = 0.0;
    }
    const double damageDone = attackedObject->takeDamage(creature, creatureDamage,
        phyAtk, magAtk, eleAtk, attackedTile, ko);
    if(attackedObject->getObjectType() == GameEntityType::creature)
    {
        Creature* target = static_cast<Creature*>(attackedObject);
        if(!missed)
            target->fireCombatImpact(hasWeapon(creature) && hasWeapon(target),
                damageDone > 0.0, creature->getPosition());

        // Tell the clients how much of the blow got through, so that they can show a hit, a glancing blow
        // or a blow that missed or was avoided. Reporting leaves the attack timing unchanged.
        const double rawDamage = creatureRawDamage + phyAtk + magAtk + eleAtk;
        double share = (rawDamage > 0.0) ? damageDone / rawDamage : 0.0;
        share = std::max(0.0, std::min(1.0, share));
        // The newer kind first (hit, glancing, blocked and how hard in relation to the health); the clients that
        // got it ignore the older kind below
        if(missed)
            target->fireHitMissed(creature->getName(), false);
        else if(defence != DefenceChance::none)
            target->fireHitDefended(creature->getName(), defence);
        else
            target->fireHitResult(creature->getName(), damageDone, rawDamage, false);
        CosmeticEvent event(CosmeticEventType::meleeResult);
        event.mSubject = creature->getName();
        event.mObject = target->getName();
        if(damageDone <= 0.0)
            event.mValue = MELEE_RESULT_NO_DAMAGE;
        else if(share < GLANCING_BLOW_SHARE)
            event.mValue = MELEE_RESULT_GLANCING;
        else
            event.mValue = MELEE_RESULT_HIT;
        event.mValue2 = static_cast<int32_t>(share * PER_MILLE + ROUND_HALF);
        event.mPosition = target->getPosition();
        target->fireCosmeticEvent(event, false);
    }
    if(notifyPlayerIfHit)
        attackedObject->notifyFightPlayer(attackedTile);

    return true;
}

CreatureSkillMeleeFight* CreatureSkillMeleeFight::clone() const
{
    return new CreatureSkillMeleeFight(*this);
}

void CreatureSkillMeleeFight::getFormatString(std::string& format) const
{
    CreatureSkill::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "LevelMin\tPhyAtk\tPhyAtkPerLvl\tMagAtk\tMagAtkPerLvl\tEleAtk\tEleAtkPerLvl";
    format += "\tPhyMin\tPhyMax\tMagMin\tMagMax\tEleMin\tEleMax";

}

void CreatureSkillMeleeFight::exportToStream(std::ostream& os) const
{
    CreatureSkill::exportToStream(os);
    os << "\t" << mCreatureLevelMin;
    os << "\t" << mPhyAtk;
    os << "\t" << mPhyAtkPerLvl;
    os << "\t" << mMagAtk;
    os << "\t" << mMagAtkPerLvl;
    os << "\t" << mEleAtk;
    os << "\t" << mEleAtkPerLvl;
    os << "\t" << mPhyMin;
    os << "\t" << mPhyMax;
    os << "\t" << mMagMin;
    os << "\t" << mMagMax;
    os << "\t" << mEleMin;
    os << "\t" << mEleMax;
}

bool CreatureSkillMeleeFight::importFromStream(std::istream& is)
{
    if(!CreatureSkill::importFromStream(is))
        return false;

    if(!(is >> mCreatureLevelMin))
        return false;
    if(!(is >> mPhyAtk))
        return false;
    if(!(is >> mPhyAtkPerLvl))
        return false;
    if(!(is >> mMagAtk))
        return false;
    if(!(is >> mMagAtkPerLvl))
        return false;
    if(!(is >> mEleAtk))
        return false;
    if(!(is >> mEleAtkPerLvl))
        return false;

    mPhyMin = mPhyMax = mPhyAtk + mPhyAtkPerLvl;
    mMagMin = mMagMax = mMagAtk + mMagAtkPerLvl;
    mEleMin = mEleMax = mEleAtk + mEleAtkPerLvl;
    return CombatDamage::readRange(is, mPhyMin, mPhyMax, mMagMin, mMagMax, mEleMin, mEleMax);
}

bool CreatureSkillMeleeFight::isEqual(const CreatureSkill& creatureSkill) const
{
    if(!CreatureSkill::isEqual(creatureSkill))
        return false;

    const CreatureSkillMeleeFight* skill = dynamic_cast<const CreatureSkillMeleeFight*>(&creatureSkill);
    if(skill == nullptr)
        return false;

    if(mCreatureLevelMin != skill->mCreatureLevelMin)
        return false;
    if(mPhyAtk != skill->mPhyAtk)
        return false;
    if(mPhyAtkPerLvl != skill->mPhyAtkPerLvl)
        return false;
    if(mMagAtk != skill->mMagAtk)
        return false;
    if(mMagAtkPerLvl != skill->mMagAtkPerLvl)
        return false;
    if(mEleAtk != skill->mEleAtk)
        return false;
    if(mEleAtkPerLvl != skill->mEleAtkPerLvl)
        return false;
    if(mPhyMin != skill->mPhyMin || mPhyMax != skill->mPhyMax
        || mMagMin != skill->mMagMin || mMagMax != skill->mMagMax
        || mEleMin != skill->mEleMin || mEleMax != skill->mEleMax)
        return false;

    return true;
}
