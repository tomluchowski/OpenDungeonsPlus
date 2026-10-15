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

struct TrapProductionOrder
{
    std::string name;
    TrapType type;
    int32_t needed;
};

struct TrapProductionWorkshop
{
    std::string name;
    TrapType type;
    int32_t points;
    int32_t required;
};

struct TrapProductionData
{
    std::vector<TrapProductionOrder> orders;
    std::vector<TrapProductionWorkshop> workshops;
};

void exportTrapProductionData(ODPacket& packet, const TrapProductionData& data);
bool importTrapProductionData(ODPacket& packet, TrapProductionData& data);

#endif
