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

#ifndef COSMETICEVENT_H
#define COSMETICEVENT_H

#include "network/ODPacket.h"

#include <cstdint>
#include <string>

#include <Ogre.h>

/*! \brief Kinds of cosmetic events. A cosmetic event is a short note from the server to a client
 *  that something happened which can be shown (an icon, a pose, a particle effect). It never
 *  changes the game and the game never depends on it: the clients only show it.
 *  The numbers are part of the network format. Never reorder or reuse them, only append.
 *  The fields of CosmeticEvent used by each kind are described next to the kind.
 */
enum class CosmeticEventType : int32_t
{
    //! The mood level of a creature changed. mSubject creature, mValue new CreatureMoodLevel,
    //! mValue2 old CreatureMoodLevel. Sent for the creatures of the receiving keeper and of its allies.
    moodStage = 0,
    //! The creature starts to flee out of fear. mSubject creature, mValue 0: it is outmatched or weak,
    //! 1: a fear trap scared it.
    scared = 1,
    //! The creature wanted to work for some time and found no job (the same count that lowers its mood).
    //! mSubject creature, mValue number of turns without a job. Sent for the creatures of the receiving
    //! keeper and of its allies. The number is reserved and the client already handles it, but the server
    //! does not send it yet: the count of turns without a job comes with the out of work mood.
    impatient = 2,
    //! A treasury just became full. mObject name of the room, mPosition the tile of the last deposit,
    //! mValue gold stored, mValue2 gold capacity.
    treasuryFull = 3,
    //! The gold a worker carries on its body changed. mSubject worker, mValue gold now (0: none),
    //! mValue2 the most the worker can carry.
    carriedGold = 4,
    //! A melee blow hit a creature. mSubject attacker, mObject target, mValue 0: the blow did its
    //! damage, 1: only a small part of it got through (glancing), 2: no damage at all (blocked or
    //! absorbed), mValue2 share of the damage that got through in per mille (0 to 1000),
    //! mPosition position of the target. This only reports what the damage calculation gave; the
    //! hitResult separately reports missed or avoided attacks.
    meleeResult = 5,
    //! A creature launched a missile. mSubject shooter, mObject name of the missile entity, mText mesh
    //! of the missile (empty: a magic missile without a mesh), mValue and mValue2 tile x and y of the
    //! target, mPosition where the missile starts.
    missileLaunch = 6,
    //! A worker dug a tile away. mSubject worker, mValue the TileType the tile had before,
    //! mPosition the tile.
    digFinished = 7,
    //! A creature of the keeper arrived through a portal. mSubject creature, mValue the CreatureMoodLevel
    //! the creature would have, mValue2 its mood points. Can arrive before the creature itself.
    portalArrival = 8,
    //! (9 to 16 are kept free: other kinds of other features use them.)
    //! What a blow or a shot of a creature really did to a creature, told when the damage was calculated (melee)
    //! or when the missile arrived (shot). mSubject attacker, mObject target, mValue the CosmeticHitResult,
    //! mValue2 the damage that was done in per mille of the maximum health of the target (0 to 1000, the base of
    //! a strong hit), mText "melee" or "missile", mPosition position of the target. Sent for the target to the
    //! keepers that see it. This only reports what the game decided or calculated; the possible results are
    //! those of CosmeticHitResult. For a melee blow the kind meleeResult follows it with
    //! the old fields, for clients that do not know this kind. Older clients skip the kind, an older server
    //! sends none.
    hitResult = 17,
    //! A creature starts a blow or a shot and turns to its target first. Sent just before the animation of the blow.
    //! mSubject attacker, mObject target, mPosition the direction from the attacker to the target (length 1, height 0),
    //! mValue and mValue2 are not used (0). The client turns the attacker smoothly with the angular speed of its
    //! configuration and starts the strike clip when the turn is done (at the latest after the configured delay, the turn
    //! then runs over the wind-up). Damage and its timing are decided by the server and are not touched. Older clients skip
    //! the kind and turn as before, an older server sends none.
    attackTurn = 18
};

//! \brief The result in mValue of the event hitResult
enum class CosmeticHitResult : int32_t
{
    //! The blow or shot did its damage
    hit = 0,
    //! Only a small part of the damage got through (armor or resistance took the rest)
    glanced = 1,
    //! No damage at all got through (armor or resistance took all of it)
    blocked = 2,
    //! A blow or single-target shot failed its hit roll, or a shot stopped without reaching its target.
    missed = 3,
    //! A blow or shot was dodged: the defender got out of the way, no damage (decided by the server before damage).
    dodged = 4,
    //! A melee blow was parried with a weapon: no damage (decided by the server before the damage). Clients that
    //! do not know the value 4 or 5 show nothing for it; the older kind meleeResult follows and tells them "no damage".
    parried = 5,
    //! A landed blow or shot was blocked by a carried shield, with no damage.
    shieldBlocked = 6
};

//! \brief The data of one cosmetic event. Every kind uses the same layout on the wire, so a
//! receiver always skips a kind it does not know without losing its place in the packet.
struct CosmeticEvent
{
    CosmeticEvent() :
        mType(-1),
        mValue(0),
        mValue2(0),
        mPosition(Ogre::Vector3::ZERO)
    {}

    CosmeticEvent(CosmeticEventType type) :
        mType(static_cast<int32_t>(type)),
        mValue(0),
        mValue2(0),
        mPosition(Ogre::Vector3::ZERO)
    {}

    //! \brief True if this build knows what the kind means
    bool isKnownType() const;

    //! \brief True if this event is of the given kind
    //! \param type The kind to compare with
    bool is(CosmeticEventType type) const
    { return mType == static_cast<int32_t>(type); }

    //! \brief Name of the kind for the log, "unknown" for a kind this build does not know
    std::string typeString() const;

    //! Raw number of the kind (see CosmeticEventType), kept as a number so that a kind of a newer
    //! build survives reading
    int32_t mType;
    std::string mSubject;
    std::string mObject;
    std::string mText;
    int32_t mValue;
    int32_t mValue2;
    Ogre::Vector3 mPosition;
};

//! \brief Writes the event: int32 kind, string subject, string object, string text, int32 value,
//! int32 value2, Vector3 position.
ODPacket& operator<<(ODPacket& os, const CosmeticEvent& event);
//! \brief Reads the event. The packet is in error state if it was too short.
ODPacket& operator>>(ODPacket& is, CosmeticEvent& event);

#endif // COSMETICEVENT_H
