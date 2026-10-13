/*
 * Copyright (C) 2026 OpenDungeons Team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "game/TrapProductionData.h"
#include "network/ODPacket.h"
#include <set>
#include <utility>

void exportTrapProductionData(ODPacket& packet, const TrapProductionData& data)
{
    packet << static_cast<uint32_t>(data.orders.size());
    for(const TrapProductionOrder& order : data.orders)
        packet << order.name << order.type << order.needed;
    packet << static_cast<uint32_t>(data.workshops.size());
    for(const TrapProductionWorkshop& workshop : data.workshops)
        packet << workshop.name << workshop.type << workshop.points << workshop.required;
}

bool importTrapProductionData(ODPacket& packet, TrapProductionData& data)
{
    TrapProductionData received;
    std::set<std::string> names;
    uint32_t count;
    if(!(packet >> count))
        return false;
    while(count-- > 0)
    {
        TrapProductionOrder order;
        if(!(packet >> order.name >> order.type >> order.needed) || order.name.empty() ||
           !names.insert(order.name).second || order.type <= TrapType::nullTrapType ||
           order.type >= TrapType::nbTraps || order.needed <= 0)
            return false;
        received.orders.push_back(std::move(order));
    }
    names.clear();
    if(!(packet >> count))
        return false;
    while(count-- > 0)
    {
        TrapProductionWorkshop workshop;
        if(!(packet >> workshop.name >> workshop.type >> workshop.points >> workshop.required) ||
           workshop.name.empty() || !names.insert(workshop.name).second ||
           workshop.type < TrapType::nullTrapType || workshop.type >= TrapType::nbTraps ||
           workshop.points < 0 || workshop.required < 0)
            return false;
        received.workshops.push_back(std::move(workshop));
    }
    data = std::move(received);
    return true;
}
