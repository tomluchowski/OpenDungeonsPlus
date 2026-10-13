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
    //! game has no random miss.
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
    portalArrival = 8
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
