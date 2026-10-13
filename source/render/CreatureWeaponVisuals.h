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

#ifndef CREATUREWEAPONVISUALS_H
#define CREATUREWEAPONVISUALS_H

#include <cstdint>
#include <string>

namespace Ogre
{
class Entity;
}

struct CosmeticEvent;
class Creature;
class CreatureReactions;
class GameMap;

/*! \brief Client side visuals for blows and shots, driven by the cosmetic events of the server:
 * - the result of a melee blow (dodged, glancing, strong) as reactions of the attacker and the target and a short
 *   trail behind the weapon after a strong blow,
 * - the arrow or bolt of archers: it lies on the string or the rail while an enemy is near. A bow arrow is taken from
 *   the pulling hand and follows that hand while the string is drawn (the bone is derived from the side of the
 *   weapon bone; without one it stays on the bow). It leaves at the moment the server launches the missile (the
 *   missile entity is the same arrow). A crossbow is reloaded visibly after a shot: the bolt comes out of the hand,
 *   slides onto the rail and the weapon tips when the string is cocked; the reload is never longer than the time
 *   between the shots. The times and distances are settings of config/creatureReactions.cfg.
 *
 * Nothing here touches the game: no animation state, no activity, no refresh and no message is sent. The arrow is
 * an extra entity on the bone of the bow and exists only in the option 'Creature reactions: full'.
 */
class CreatureWeaponVisuals
{
public:
    //! \brief Handles the kinds hitResult, meleeResult and missileLaunch. Returns true if the event was one of them.
    static bool noteCosmeticEvent(CreatureReactions& reactions, const CosmeticEvent& event);

    //! \brief Advances the delayed reactions, the trails and the arrows
    static void update(CreatureReactions& reactions, double timeSinceLastFrame);

    //! \brief Removes arrows, trails and everything that is waiting
    static void stopAll(CreatureReactions& reactions);

    //! \brief True if the server told lately that a blow on the creature was only a glancing one or did nothing.
    //! The guessed flinch of the combat reactions is not shown then.
    static bool wasBlowSoftened(const std::string& targetName, double now);

    //! \brief What the server last told about a blow or shot of a creature (event hitResult), for the looks that
    //! follow a strong hit (a weapon trail, later). Only what the server reported, never a guess.
    struct HitInfo
    {
        HitInfo() :
            mResult(0),
            mHealthPermille(0),
            mStrong(false),
            mMissile(false),
            mTime(0.0)
        {}

        //! The CosmeticHitResult (hit, glanced, blocked, missed)
        int32_t mResult;
        //! The damage that was done in per mille of the maximum health of the target
        int32_t mHealthPermille;
        //! True for a hit that took at least the share of the health that the configuration calls strong
        bool mStrong;
        //! True for a shot, false for a melee blow
        bool mMissile;
        std::string mTarget;
        //! Time of the event on the clock of the reactions
        double mTime;
    };

    //! \brief The last result of a blow or shot of the attacker. False if the server told nothing about it
    static bool getLastHit(const std::string& attackerName, HitInfo& info);

    //! \brief True once the server has sent the event hitResult: the hits, the dodges and the misses are then
    //! shown from it and not guessed from the attack animations
    static bool hasHitEvents();

    //! \brief Access to the internals of CreatureReactions for the helper functions of this file. They only read
    //! the state of the reactions (time, map, mode); show() starts an event the same way the combat reactions do.
    static bool isActive(const CreatureReactions& reactions);
    //! \brief Returns the time of the reactions in seconds
    static double getTime(const CreatureReactions& reactions);
    //! \brief Returns the map the reactions work on
    static GameMap* getGameMap(const CreatureReactions& reactions);
    //! \brief Returns the body entity of the creature, nullptr if it has none
    static Ogre::Entity* getBody(const CreatureReactions& reactions, const Creature* creature);
    //! \brief Starts a reaction on the creature unless it is dead, far from the camera or busy; returns true if it started
    static bool show(CreatureReactions& reactions, Creature* creature, const std::string& eventName);
};

#endif // CREATUREWEAPONVISUALS_H
