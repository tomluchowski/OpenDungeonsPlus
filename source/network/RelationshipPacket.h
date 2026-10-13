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

//! \brief Writes the payload of ServerNotificationType::relationshipTier: both creature names and the new tier.
//! \param packet The packet to write to.
//! \param creatureA Name of the first creature of the pair.
//! \param creatureB Name of the second creature of the pair.
//! \param tier The new tier of the pair.
inline void writeRelationshipTier(ODPacket& packet, const std::string& creatureA, const std::string& creatureB,
    RelationshipTier tier)
{
    packet << creatureA << creatureB << static_cast<int32_t>(tier);
}

//! \brief Reads the payload written by writeRelationshipTier. The tier is returned as sent (the receiver
//! checks the range).
//! \return False if the packet is too short.
inline bool readRelationshipTier(ODPacket& packet, std::string& creatureA, std::string& creatureB,
    int32_t& tier)
{
    packet >> creatureA >> creatureB >> tier;
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
