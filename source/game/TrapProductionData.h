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

//! \brief A pending trap order of a seat, in priority order.
struct TrapProductionOrder
{
    std::string name;
    TrapType type;
    //! Number of crafted items still required.
    int32_t needed;
};

//! \brief Current work of one workshop; type is nullTrapType while idle.
struct TrapProductionWorkshop
{
    std::string name;
    TrapType type;
    //! Work points done so far.
    int32_t points;
    //! Work points needed for one item.
    int32_t required;
};

//! \brief Snapshot of the trap production state sent to the owner.
struct TrapProductionData
{
    std::vector<TrapProductionOrder> orders;
    std::vector<TrapProductionWorkshop> workshops;
};

//! \brief Writes the snapshot into the packet.
void exportTrapProductionData(ODPacket& packet, const TrapProductionData& data);
//! \brief Reads a snapshot; returns false and leaves data unchanged when the packet is truncated or invalid.
bool importTrapProductionData(ODPacket& packet, TrapProductionData& data);

#endif
