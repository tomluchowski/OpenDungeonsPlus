/*
 * Copyright (C) 2026 OpenDungeons Team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef TRAPPRODUCTIONDATA_H
#define TRAPPRODUCTIONDATA_H

#include "traps/TrapType.h"
#include <cstdint>
#include <string>
#include <vector>

class ODPacket;

//! \brief A trap of a seat that still needs crafted items. The name identifies the trap.
struct TrapProductionOrder
{
    std::string name;
    TrapType type;
    //! Number of crafted items the trap still needs.
    int32_t needed;
};

//! \brief The state of one workshop. type is TrapType::nullTrapType when it is idle.
struct TrapProductionWorkshop
{
    std::string name;
    TrapType type;
    //! Work points collected so far.
    int32_t points;
    //! Work points needed to finish the current trap, 0 when idle.
    int32_t required;
};

//! \brief What the production window shows: the pending orders in priority order and
//! the workshops of the player.
struct TrapProductionData
{
    std::vector<TrapProductionOrder> orders;
    std::vector<TrapProductionWorkshop> workshops;
};

//! \brief Writes the data to the packet.
void exportTrapProductionData(ODPacket& packet, const TrapProductionData& data);
//! \brief Reads the data from the packet. Returns false and leaves data unchanged if the
//! packet is incomplete or a value is invalid (empty or duplicate name, trap type out of
//! range, non-positive needed count, negative work points).
bool importTrapProductionData(ODPacket& packet, TrapProductionData& data);

#endif
