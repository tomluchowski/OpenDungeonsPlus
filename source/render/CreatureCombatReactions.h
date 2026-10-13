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

#ifndef CREATURECOMBATREACTIONS_H
#define CREATURECOMBATREACTIONS_H

#include <cstdint>
#include <string>

class Creature;
class CreatureReactions;

/*! \brief Cosmetic reactions of fighting creatures, client side only: drawing and sheathing the weapon, the stance
 * between two blows, a different look for each blow depending on the weapon, hit reactions of the creature that is
 * attacked, and the weapon that falls to the floor when its owner dies.
 *
 * Nothing here touches the game: the attack animation, its timing, the damage and the range are the ones the server
 * decided on, no animation state is set, no activity is changed and no message is sent. Everything is an overlay of
 * the reaction framework (events 'Attack*', 'Hit*', 'Weapon*' and 'Combat*' in config/creatureReactions.cfg) plus
 * the visibility of the weapon models. The functions are called by CreatureReactions at its client hooks.
 */
class CreatureCombatReactions
{
public:
    //! \brief The creature plays an attack animation (not the work in a room). Its blow gets a look of its own,
    //! the creature that is in front of it flinches, and weapons are drawn.
    static void noteAttack(CreatureReactions& reactions, Creature* attacker, const std::string& clip);

    //! \brief The server told that a blow or shot hit the target (event hitResult). The target flinches at the
    //! moment the blow lands, or staggers if the hit was a strong one. Replaces the guess made from the attack animation.
    static void noteHitEvent(CreatureReactions& reactions, Creature* attacker, Creature* target, bool strong, bool missile);

    //! \brief The creature runs from a fight: it draws its weapon
    static void noteAlarm(CreatureReactions& reactions, Creature* creature);

    //! \brief The creature plays its death animation: its weapons fall to the floor a moment later
    static void noteDeath(CreatureReactions& reactions, Creature* creature);

    //! \brief The server updated the creature. If its health stage got worse in a fight, it shows the hit.
    static void noteHealth(CreatureReactions& reactions, Creature* creature, uint32_t oldHealth);

    //! \brief Advances the delayed reactions, the fallen weapons and the drawing and sheathing of weapons
    static void update(CreatureReactions& reactions, double timeSinceLastFrame);

    //! \brief Forgets everything, shows the weapons again and removes the fallen ones
    static void stopAll(CreatureReactions& reactions);

private:
    //! \brief Returns true if the reactions are switched to full and their configuration is loaded
    static bool isActive(const CreatureReactions& reactions);
    //! \brief Starts a reaction on the creature unless it is dead, far from the camera or busy; returns true if it started
    static bool show(CreatureReactions& reactions, Creature* creature, const std::string& eventName);
    //! \brief Looks for the closest enemy in front of the attacker within the range; returns true and sets target and distance if there is one
    static bool findTarget(CreatureReactions& reactions, Creature* attacker, double range, Creature*& target,
        double& distance);
    //! \brief Returns the reaction that gives the blow its look, chosen by the weapon of the attacker and by ranged or not
    static std::string chooseAttackEvent(const CreatureReactions& reactions, const Creature* attacker, bool ranged);
    //! \brief Queues a hit reaction of the target (level 0 light, 1 and 2 strong) for the moment the blow lands
    static void scheduleHit(CreatureReactions& reactions, Creature* target, uint32_t level, bool shield, double delay);
    //! \brief Gives the blows that were seen a moment ago their look and queues the hit on the creature in front
    static void processAttacks(CreatureReactions& reactions);
    //! \brief Shows the hit reactions whose blow has landed
    static void processHits(CreatureReactions& reactions);
    //! \brief Lets the weapons of the creatures that died fall to the floor
    static void processDrops(CreatureReactions& reactions);
    //! \brief Moves the fallen weapons, lets them lie and shrinks them away at the end
    static void updateFallen(CreatureReactions& reactions, double timeSinceLastFrame);
    //! \brief Removes all fallen weapons from the scene
    static void removeFallen(CreatureReactions& reactions);
    //! \brief Follows the sheathing and drawing reactions so the weapon models are hidden or shown with them
    static void watchEvents(CreatureReactions& reactions);
    //! \brief Puts the weapons of calm creatures away, draws them when an enemy is near and starts the stance between blows
    static void tick(CreatureReactions& reactions);
    //! \brief Takes the weapons of the creature out of the sheath; with announce a visible creature also plays the drawing flourish
    static void draw(CreatureReactions& reactions, Creature* creature, bool announce);
    //! \brief Returns true if a living creature of a seat that is not allied is within the radius
    static bool isEnemyNear(CreatureReactions& reactions, const Creature* creature, double radius);
    //! \brief Remembers that the creature has put its weapons away (or drawn them) and hides or shows the models
    static void setSheathed(CreatureReactions& reactions, Creature* creature, bool sheathed);
    //! \brief Shows or hides the weapon models of the creature
    static void applyVisibility(CreatureReactions& reactions, const Creature* creature, bool visible);
    //! \brief Returns true if a weapon model of the creature exists in the scene
    static bool hasWeaponModel(CreatureReactions& reactions, const Creature* creature);
};

#endif // CREATURECOMBATREACTIONS_H
