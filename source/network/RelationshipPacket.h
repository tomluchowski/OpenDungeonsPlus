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

#ifndef RELATIONSHIPPACKET_H
#define RELATIONSHIPPACKET_H

#include "game/CreatureRelationships.h"
#include "network/ODPacket.h"

#include <cstdint>
#include <string>

//! \brief Packet layout of the relationship system, shared by server and client.

//! \brief Writes the payload of ServerNotificationType::relationshipTier: both creature names, the new tier and
//! whether the message is part of the replay for a client that joins or loads (replay messages show no posts or
//! emotes).
//! \param packet The packet to write to.
//! \param creatureA Name of the first creature of the pair.
//! \param creatureB Name of the second creature of the pair.
//! \param tier The new tier of the pair.
//! \param replay True if the message only repeats the current tier.
inline void writeRelationshipTier(ODPacket& packet, const std::string& creatureA, const std::string& creatureB,
    RelationshipTier tier, bool replay)
{
    packet << creatureA << creatureB << static_cast<int32_t>(tier) << replay;
}

//! \brief Reads the payload written by writeRelationshipTier. The tier is returned as sent (the receiver
//! checks the range).
//! \param replay Receives the replay flag.
//! \return False if the packet is too short.
inline bool readRelationshipTier(ODPacket& packet, std::string& creatureA, std::string& creatureB,
    int32_t& tier, bool& replay)
{
    packet >> creatureA >> creatureB >> tier >> replay;
    return static_cast<bool>(packet);
}

//! \brief Reads the optional trailing relationships flag of ServerNotificationType::startGameMode.
//! Servers without the option do not send it: the flag is then false.
inline bool readRelationshipsFlag(ODPacket& packet)
{
    bool relationships = false;
    if(!packet.endOfPacket())
        packet >> relationships;
    return relationships;
}

#endif // RELATIONSHIPPACKET_H
